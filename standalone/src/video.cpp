// Output through GXRuntime's Aurora backend: the GX command stream, presentation at the game's
// display copy, controllers and sound. Headless builds and --headless runs keep the same entry
// points with no window.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"
#include "state.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#ifdef SKIES_STANDALONE_AURORA
#include "gxruntime/aurora_backend.h"
#include "gxruntime/platform.h"

#include <aurora/aurora.h>
#include <aurora/gfx.h>
#include <dolphin/gx.h>
#include <dolphin/vi.h>

extern bool aurora_debug_draw_log;  // GXRuntime: Aurora's per-draw log (AURORA_DRAW_LOG)

extern "C" {
void aurora_backend_present(void);
bool aurora_backend_should_quit(void);
void aurora_backend_write_raw(const void* data, u32 size);
bool aurora_backend_drain_if_over(u32 bytes);
int aurora_backend_fifo_over(u32 bytes);
void aurora_backend_set_guest_address_resolver(DolPlatformGuestAddressResolverFn resolve, void* user);
int aurora_backend_surface_size(unsigned* width, unsigned* height);
void aurora_backend_set_present_rect(int left, int top, int width, int height);
void aurora_backend_set_present_time(long long steady_ns);
void aurora_backend_set_present_callback(void (*callback)(void));
void aurora_backend_set_frame_buffer_aspect_fit(int fit);
}
#endif

namespace host
{
namespace
{
bool s_active = false;
u64 s_frames = 0;
std::string s_dump_dir;
u64 s_dump_every = 0;
u64 s_dump_from = 0;  // SKIES_DUMP_FROM: first frame dumped
std::vector<u64> s_dump_frames;  // SKIES_DUMP_FRAMES=N,N,...: just these
std::vector<s16> s_audio;
double s_drain_ms = 0;  // SKIES_PRESENT_PACING: time this frame in aurora_backend_drain_if_over

#ifdef SKIES_STANDALONE_AURORA
// The picture's aspect: the widescreen game patch keeps its target in ws_config at the start of its
// data section (patches/game/cave.ld, widescreen.c: magic 'WSCF', then display_aspect; 0 = 4:3).
// It defaults to 16:9 and nothing here writes it, so a ws DOL shows 16:9 and the plain DOL 4:3.
constexpr u32 WS_CONFIG = 0x80002A00;
double PictureAspect()
{
  if (Read32(WS_CONFIG) != 0x57534346)
    return 4.0 / 3.0;
  const u32 bits = Read32(WS_CONFIG + 8);
  float aspect;
  std::memcpy(&aspect, &bits, sizeof(aspect));
  return aspect > 0.5f && aspect < 8.0f ? aspect : 4.0 / 3.0;
}

bool ResolveGuestAddress(void*, u32 address, u32, DolGuestAddressSpace, DolGuestResourceKind,
                         const void** data, u32* available)
{
  const u32 physical = address & 0x1FFFFFFFu;
  if (physical >= RAM_SIZE)
  {
    static int s_logged = 0;
    if (s_logged < 8)
    {
      ++s_logged;
      Log("[video] unresolved guest address %08x\n", address);
    }
    return false;
  }
  *data = g_ram + physical;
  *available = RAM_SIZE - physical;
  return true;
}

// On the render worker after each present: the interval between presents, as the Dolphin build's
// [present-pacing] (its >20 / >25 ms counts suit 60 fps; Skies presents 30 frames a second in both builds,
// so these count >40 / >50 ms)
void PresentDone()
{
  static std::chrono::steady_clock::time_point last{};
  static PacingWindow window;
  const auto now = std::chrono::steady_clock::now();
  if (last.time_since_epoch().count() != 0)
    PacingAdd(window, std::chrono::duration<double, std::milli>(now - last).count(), "present-pacing", "interval",
              40.0, 50.0);
  last = now;
}

// Presents on the throttle's clock: the render worker presents each frame at its copy's guest time plus a
// delay instead of when the emulation thread got there, so the intervals keep the game's rhythm while the
// thread's own timing wobbles (on the Pocket 5 the copy comes 6-11 ms after its guest time, p99 up to 18:
// the drawing field runs slower than real time and the next one catches up; Windows 0.7-1.5 ms, p99 6-8).
// SKIES_PRESENT_DELAY_MS=auto (the default) keeps the delay at the last 300 copies' 99th percentile lateness
// + 1.5 ms (at most 30), N fixes it, off presents at once. Hitches over 50 ms (state loads) are left out;
// unthrottled runs present at once.
void SchedulePresent()
{
  static const double s_mode = [] {  // -2 auto, -1 off, else fixed ms
    const char* v = std::getenv("SKIES_PRESENT_DELAY_MS");
    const std::string mode = v ? v : "auto";
    return mode == "auto" ? -2.0 : mode == "off" ? -1.0 : std::atof(mode.c_str());
  }();
  const auto wall = WallForGuest(Now());
  if (wall.time_since_epoch().count() == 0)
    return;
  const double late = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - wall).count();
  if (PacingOn())
  {
    static PacingWindow window;
    PacingAdd(window, late, "present-late", "copy after its guest time", 5.0, 10.0);
  }
  if (s_mode == -1.0 || late > 50.0)
    return;
  double delay = s_mode;
  if (s_mode == -2.0)
  {
    constexpr unsigned HISTORY = 300;
    static double s_late[HISTORY];
    static unsigned s_count = 0;
    static double s_delay = 20.0;  // from above: a delay that grows makes late presents, one that shrinks doesn't
    s_late[s_count++ % HISTORY] = late;
    if (s_count >= 60 && s_count % 30 == 0)
    {
      const unsigned n = std::min(s_count, HISTORY);
      std::vector<double> sorted(s_late, s_late + n);
      const size_t at = static_cast<size_t>(n * 0.99);
      std::nth_element(sorted.begin(), sorted.begin() + at, sorted.end());
      const double next = std::clamp(sorted[at] + 1.5, 0.0, 30.0);
      if (std::abs(next - s_delay) >= 1.0 && PacingOn())
        Log("[video] present delay %.1f ms\n", next);
      s_delay = next;
    }
    delay = s_delay;
  }
  aurora_backend_set_present_time(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          (wall + std::chrono::microseconds(static_cast<s64>(delay * 1000))).time_since_epoch())
          .count());
}

// The picture centred in the window at PictureAspect (Dolphin's aspect ratio letterbox).
void ApplyPresentRect()
{
  unsigned width = 0, height = 0;
  if (!aurora_backend_surface_size(&width, &height))
    return;
  const double aspect = PictureAspect();
  int w = static_cast<int>(width), h = static_cast<int>(height);
  if (static_cast<double>(width) / height > aspect)
    w = static_cast<int>(height * aspect + 0.5);
  else
    h = static_cast<int>(width / aspect + 0.5);
  aurora_backend_set_present_rect((static_cast<int>(width) - w) / 2,
                                  (static_cast<int>(height) - h) / 2, w, h);
}

void DumpFrame()
{
  const u8* rgba = nullptr;
  u32 width = 0, height = 0;
  for (int attempt = 0; attempt < 20000; ++attempt)
  {
    if (aurora_take_framebuffer_readback(&rgba, &width, &height))
      break;
    aurora_pump_framebuffer_readback();
    std::this_thread::sleep_for(std::chrono::microseconds(250));
    rgba = nullptr;
  }
  if (!rgba)
  {
    Log("[video] frame %llu: readback never completed\n", static_cast<unsigned long long>(s_frames));
    return;
  }
  char path[512];
  std::snprintf(path, sizeof(path), "%s/frame_%06llu.ppm", s_dump_dir.c_str(),
                static_cast<unsigned long long>(s_frames));
  FILE* f = std::fopen(path, "wb");
  if (!f)
    return;
  std::fprintf(f, "P6\n%u %u\n255\n", width, height);
  std::vector<u8> rgb(static_cast<size_t>(width) * height * 3);
  for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i)
  {
    rgb[i * 3] = rgba[i * 4];
    rgb[i * 3 + 1] = rgba[i * 4 + 1];
    rgb[i * 3 + 2] = rgba[i * 4 + 2];
  }
  std::fwrite(rgb.data(), 1, rgb.size(), f);
  std::fclose(f);
}
#endif
}  // namespace

bool VideoInit(bool headless)
{
  if (const char* dir = std::getenv("SKIES_DUMP_DIR"))
    s_dump_dir = dir;
  s_dump_every = s_dump_dir.empty() ? 0 : 60;
  if (const char* every = std::getenv("SKIES_DUMP_EVERY"))
    s_dump_every = std::strtoull(every, nullptr, 0);
  if (const char* from = std::getenv("SKIES_DUMP_FROM"))
    s_dump_from = std::strtoull(from, nullptr, 0);
  if (const char* list = std::getenv("SKIES_DUMP_FRAMES"))
  {
    for (const char* p = list; *p;)
    {
      char* end = nullptr;
      s_dump_frames.push_back(std::strtoull(p, &end, 0));
      p = *end ? end + 1 : end;
    }
    s_dump_every = 1;
  }
#ifdef SKIES_STANDALONE_AURORA
  if (headless)
    return true;
  // Aurora's controller and keyboard bindings (keyboard_bindings.dat and the controller
  // mappings) in USER/Config/Aurora beside the Dolphin build's GCPadNew.ini; its shader caches stay
  // in the system's per-user app folder.
  static std::string s_aurora_user;
  if (!SettingsConfigDir().empty())
  {
    s_aurora_user = (std::filesystem::path(SettingsConfigDir()) / "Aurora").string();
    std::error_code ec;
    std::filesystem::create_directories(s_aurora_user, ec);
  }
  const char* w = std::getenv("SKIES_WIDTH");
  const char* h = std::getenv("SKIES_HEIGHT");
  // The DOL is in memory by now, so the window can open at the picture's shape (960 lines high).
  const AuroraBackendConfig config = {
      .app_name = "Skies of Arcadia Legends (standalone)",
      .window_width = w ? static_cast<unsigned>(std::atoi(w))
                        : static_cast<unsigned>(960.0 * PictureAspect() + 0.5),
      .window_height = h ? static_cast<unsigned>(std::atoi(h)) : 960u,
      .vsync = std::getenv("SKIES_VSYNC") != nullptr ||
               SettingGet(SettingFile::Gfx, "Hardware", "VSync", 0) != 0,
      .allow_texture_dumps = false,
      .info_logging = std::getenv("SKIES_AURORA_LOG") != nullptr,
      .graphics_logging = std::getenv("SKIES_AURORA_LOG") != nullptr,
      .force_untextured = false,
      .native_window = nullptr,
      .native_window_width = 0,
      .native_window_height = 0,
      .native_window_scale = 0.f,
      .user_path = s_aurora_user.empty() ? nullptr : s_aurora_user.c_str(),
      .cache_path = nullptr,
  };
  char arg0[] = "skies-standalone";
  char* argv[] = {arg0, nullptr};
  if (!dol_aurora_initialize(1, argv, &config))
  {
    Log("[video] Aurora could not initialize its window or GPU device\n");
    return false;
  }
  aurora_backend_set_guest_address_resolver(ResolveGuestAddress, nullptr);
  if (PacingOn())
    aurora_backend_set_present_callback(PresentDone);
  // As the Dolphin-side backend (ApplyEfbScale): the EFB keeps 640x480 times the scale whatever the
  // window's shape (the ws patch draws anamorphic) and the picture is stretched at the present.
  // The scale is the Resolution setting (menu.cpp; SKIES_EFB_SCALE=N overrides it), 0 = the window's.
  aurora_backend_set_frame_buffer_aspect_fit(1);
  // SDK baseline before the game's own GXInit, as the Dolphin-side backend does.
  static std::vector<u8> s_gx_fifo(64 * 1024);
  GXInit(s_gx_fifo.data(), static_cast<u32>(s_gx_fifo.size()));
  GXFlush();
  dol_platform_pad_init();
  dol_platform_audio_set_sample_rate(32000);
  s_active = true;
  MenuApplyGame();
  return true;
#else
  (void)headless;
  return true;
#endif
}

void WavClose();

void VideoShutdown()
{
  WavClose();
#ifdef SKIES_STANDALONE_AURORA
  if (s_active)
    dol_aurora_shutdown();
#endif
  s_active = false;
}

void VideoCommands(const u8* data, u32 size)
{
#ifdef SKIES_STANDALONE_AURORA
  if (!s_active)
    return;
  // SKIES_DRAIN=N: let Aurora's worker fall at most N bytes behind (default 64 KB; debug)
  static const u32 drain = [] {
    const char* v = std::getenv("SKIES_DRAIN");
    return v ? static_cast<u32>(std::strtoul(v, nullptr, 0)) : 64u * 1024;
  }();
  aurora_backend_write_raw(data, size);
  // Timed only when it drains: a clock read per command chunk cost the Pocket 5 15% of its speed
  if (PacingOn() && aurora_backend_fifo_over(drain))
  {
    const auto t0 = std::chrono::steady_clock::now();
    aurora_backend_drain_if_over(drain);
    s_drain_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  }
  else
    aurora_backend_drain_if_over(drain);
#else
  (void)data;
  (void)size;
#endif
}

// SKIES_POKE=ADDR=WORDS[;ADDR=WORDS...] (hex; WORDS: 8 hex digits each, big-endian guest words)
// written to RAM at every frame, as a frontend writes the game patch's input blocks every poll
// (e.g. qol_input at 80002EA0: 'QOLC', encounter percent, ...).
void ApplyPokes()
{
  struct Poke
  {
    u32 address;
    std::vector<u32> words;
  };
  static const std::vector<Poke> pokes = [] {
    std::vector<Poke> list;
    const char* spec = std::getenv("SKIES_POKE");
    for (std::string rest = spec ? spec : ""; !rest.empty();)
    {
      const size_t semi = rest.find(';');
      const std::string one = rest.substr(0, semi);
      rest = semi == std::string::npos ? "" : rest.substr(semi + 1);
      const size_t eq = one.find('=');
      if (eq == std::string::npos)
        continue;
      Poke p{static_cast<u32>(std::strtoul(one.substr(0, eq).c_str(), nullptr, 16)), {}};
      const std::string hex = one.substr(eq + 1);
      for (size_t i = 0; i + 8 <= hex.size(); i += 8)
        p.words.push_back(static_cast<u32>(std::strtoul(hex.substr(i, 8).c_str(), nullptr, 16)));
      list.push_back(std::move(p));
    }
    return list;
  }();
  for (const Poke& p : pokes)
    for (size_t i = 0; i < p.words.size(); ++i)
      Write32(p.address + static_cast<u32>(4 * i), p.words[i]);
}

#ifdef SKIES_STANDALONE_AURORA
static void ArmDrawLog();
#endif

void VideoFrameEnd()
{
  ++s_frames;
  ApplyPokes();
  MenuVideoFrame();
  if ((s_frames % 600) == 1)
    Log("[video] frame %llu at %.2f s guest\n", static_cast<unsigned long long>(s_frames),
        static_cast<double>(Now()) / CPU_HZ);
#ifdef SKIES_STANDALONE_AURORA
  if (!s_active)
    return;
  MenuApplyGame();
  ApplyPresentRect();
  MenuFrame();
  ArmDrawLog();
  bool dump = s_dump_every != 0 && s_frames >= s_dump_from && (s_frames % s_dump_every) == 0;
  if (!s_dump_frames.empty())
    dump = std::find(s_dump_frames.begin(), s_dump_frames.end(), s_frames) != s_dump_frames.end();
  if (dump)
    aurora_request_framebuffer_readback();
  SchedulePresent();
  if (PacingOn())
  {
    const auto now = std::chrono::steady_clock::now();
    static PacingWindow drains;
    PacingAdd(drains, s_drain_ms, "present-drain", "in drains per frame", 2.0, 8.0);
    s_drain_ms = 0;
    static PacingWindow call;
    aurora_backend_present();
    PacingAdd(call, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - now).count(),
              "present-call", "in aurora_backend_present", 2.0, 8.0);
  }
  else
    aurora_backend_present();
  if (dump)
    DumpFrame();
  if (aurora_backend_should_quit())
    RequestQuit();
#endif
}

// The EFB copies Aurora holds (the picture the game copied to a texture lives on the GPU only,
// the memory under it is zeroed: hw_gpu.cpp MarkEfbCopy), read back into the state; a load hands
// them back once the rest is in place (VideoRestoreCopies).
static std::vector<u8> s_loaded_copies;

#ifdef SKIES_STANDALONE_AURORA
// SKIES_DRAW_LOG_FRAME=N: Aurora's draw log for frame N only (run with SKIES_DRAIN=0 so the
// frame's commands are parsed before the switch). Set at each frame end, and after a state load.
static void ArmDrawLog()
{
  static const u64 log_frame = [] {
    const char* v = std::getenv("SKIES_DRAW_LOG_FRAME");
    return v ? std::strtoull(v, nullptr, 0) : 0ull;
  }();
  if (log_frame != 0)
  {
    aurora_backend_drain_if_over(0);
    aurora_debug_draw_log = s_frames + 1 == log_frame;
  }
}
#endif

void VideoDoState(StateIO& p)
{
  p.Do(s_frames);  // the frame dumps' numbers
  std::vector<u8> copies;
#ifdef SKIES_STANDALONE_AURORA
  if (!p.Reading() && p.with_gpu && s_active)
  {
    const u32 count = aurora_backend_copies_snapshot(
        g_ram, RAM_SIZE,
        [](const void* data, u32 size, void* user) {
          const u8* bytes = static_cast<const u8*>(data);
          static_cast<std::vector<u8>*>(user)->assign(bytes, bytes + size);
        },
        &copies);
    Log("[video] state: %u EFB copies (%.1f MB)\n", count, copies.size() / 1048576.0);
  }
#endif
  p.Do(copies);
  if (p.Reading())
    s_loaded_copies = std::move(copies);
}

void VideoStateLoaded()
{
#ifdef SKIES_STANDALONE_AURORA
  if (s_active && !s_loaded_copies.empty())
  {
    const int count = aurora_backend_copies_restore(g_ram, RAM_SIZE, s_loaded_copies.data(),
                                                    static_cast<u32>(s_loaded_copies.size()));
    Log("[video] state: %d EFB copies restored\n", count);
  }
  if (s_active)
    ArmDrawLog();
#endif
  s_loaded_copies = {};
}

void VideoRetrace()
{
}

bool VideoSurfaceSize(unsigned* width, unsigned* height)
{
#ifdef SKIES_STANDALONE_AURORA
  unsigned w = 0, h = 0;
  if (!s_active || !aurora_backend_surface_size(&w, &h))
    return false;
  if (width)
    *width = w;
  if (height)
    *height = h;
  return true;
#else
  (void)width;
  (void)height;
  return false;
#endif
}

bool VideoQuitRequested()
{
#ifdef SKIES_STANDALONE_AURORA
  return s_active && aurora_backend_should_quit();
#else
  return false;
#endif
}

// SKIES_PRESS="12.5:START,14:A,...": port 1 holds that button for 0.2 guest seconds from each time
// (START A B X Y Z L R UP DOWN LEFT RIGHT; CUP CDOWN CLEFT CRIGHT push the C-stick fully over, LT RT
// the analog triggers fully in), for unattended runs. The low 16 bits are buttons.
constexpr u32 SCRIPT_C_UP = 1u << 16, SCRIPT_C_DOWN = 1u << 17, SCRIPT_C_LEFT = 1u << 18,
              SCRIPT_C_RIGHT = 1u << 19, SCRIPT_LT = 1u << 20, SCRIPT_RT = 1u << 21;
u32 ScriptedButtons()
{
  struct Press
  {
    double at;
    u32 button;
  };
  static std::vector<Press> s_presses;
  static bool s_parsed = false;
  if (!s_parsed)
  {
    s_parsed = true;
    // SKIES_PRESS_FILE=FILE: the same script read from a file (long scripts; Windows' cmd cuts
    // environment values read with set /p at 1023 characters)
    static std::string s_file_script;
    const char* script = std::getenv("SKIES_PRESS");
    if (const char* path = std::getenv("SKIES_PRESS_FILE"))
    {
      if (std::FILE* f = std::fopen(path, "rb"))
      {
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
          s_file_script.append(buf, n);
        std::fclose(f);
        while (!s_file_script.empty() && std::isspace(static_cast<unsigned char>(s_file_script.back())))
          s_file_script.pop_back();
        script = s_file_script.c_str();
      }
      else
        Log("[video] cannot read SKIES_PRESS_FILE %s\n", path);
    }
    static const struct
    {
      const char* name;
      u32 bit;
    } kNames[] = {{"LEFT", 0x1},           {"RIGHT", 0x2},      {"DOWN", 0x4},
                  {"UP", 0x8},             {"Z", 0x10},         {"R", 0x20},
                  {"L", 0x40},             {"A", 0x100},        {"B", 0x200},
                  {"X", 0x400},            {"Y", 0x800},        {"START", 0x1000},
                  {"CUP", SCRIPT_C_UP},    {"CDOWN", SCRIPT_C_DOWN}, {"CLEFT", SCRIPT_C_LEFT},
                  {"CRIGHT", SCRIPT_C_RIGHT}, {"LT", SCRIPT_LT}, {"RT", SCRIPT_RT}};
    while (script && *script)
    {
      char* end = nullptr;
      const double at = std::strtod(script, &end);
      if (!end || *end != ':')
        break;
      std::string name;
      for (script = end + 1; *script && *script != ','; ++script)
        name += *script;
      if (*script == ',')
        ++script;
      for (const auto& n : kNames)
        if (name == n.name)
          s_presses.push_back({at, n.bit});
    }
  }
  const double now = static_cast<double>(Now()) / CPU_HZ;
  u32 held = 0;
  for (const Press& p : s_presses)
    if (now >= p.at && now < p.at + 0.2)
      held |= p.button;
  return held;
}

void VideoPadRead(u16 buttons[4], s8 sticks[4][4], u8 triggers[4][2], bool connected[4])
{
  for (int i = 0; i < 4; ++i)
  {
    buttons[i] = 0;
    sticks[i][0] = sticks[i][1] = sticks[i][2] = sticks[i][3] = 0;
    triggers[i][0] = triggers[i][1] = 0;
    connected[i] = i == 0;  // port 1 always holds a controller
  }
  const u32 scripted = ScriptedButtons();
  buttons[0] = static_cast<u16>(scripted);
  sticks[0][2] = scripted & SCRIPT_C_LEFT ? -127 : scripted & SCRIPT_C_RIGHT ? 127 : 0;
  sticks[0][3] = scripted & SCRIPT_C_DOWN ? -127 : scripted & SCRIPT_C_UP ? 127 : 0;
  triggers[0][0] = scripted & SCRIPT_LT ? 255 : 0;
  triggers[0][1] = scripted & SCRIPT_RT ? 255 : 0;
#ifdef SKIES_STANDALONE_AURORA
  if (!s_active)
    return;
  DolPadState state[4];
  dol_platform_pad_read(state);
  InputPoll();
  for (int i = 0; i < 4; ++i)
  {
    if (state[i].error != 0)
      continue;
    connected[i] = true;
    buttons[i] = static_cast<u16>(state[i].button | (i == 0 ? buttons[0] : 0));
    sticks[i][0] = state[i].stick_x;
    sticks[i][1] = state[i].stick_y;
    if (i != 0 || !(scripted & (SCRIPT_C_UP | SCRIPT_C_DOWN | SCRIPT_C_LEFT | SCRIPT_C_RIGHT)))
    {
      sticks[i][2] = state[i].substick_x;
      sticks[i][3] = state[i].substick_y;
    }
    triggers[i][0] = std::max(triggers[i][0], state[i].trigger_left);
    triggers[i][1] = std::max(triggers[i][1], state[i].trigger_right);
  }
#endif
}

void VideoPadMotor(u32 channel, u32 command)
{
#ifdef SKIES_STANDALONE_AURORA
  if (s_active)
    dol_platform_pad_control_motor(channel, command);
#else
  (void)channel;
  (void)command;
#endif
}

// SKIES_WAV=FILE: the audio DMA's output (32 kHz stereo, left first) as a WAV file, muted or not.
std::FILE* s_wav = nullptr;
u32 s_wav_bytes = 0;

void WavHeader(std::FILE* f, u32 data_bytes)
{
  auto u32le = [&](u32 v) { std::fwrite(&v, 4, 1, f); };
  auto u16le = [&](u16 v) { std::fwrite(&v, 2, 1, f); };
  std::fwrite("RIFF", 1, 4, f);
  u32le(36 + data_bytes);
  std::fwrite("WAVEfmt ", 1, 8, f);
  u32le(16);
  u16le(1);  // PCM
  u16le(2);
  u32le(32000);
  u32le(32000 * 4);
  u16le(4);
  u16le(16);
  std::fwrite("data", 1, 4, f);
  u32le(data_bytes);
}

void WavClose()
{
  if (!s_wav)
    return;
  std::fseek(s_wav, 0, SEEK_SET);
  WavHeader(s_wav, s_wav_bytes);
  std::fclose(s_wav);
  s_wav = nullptr;
}

void AudioPushSamples(const s16* samples, u32 frames)
{
  static const char* s_wav_path = std::getenv("SKIES_WAV");
  if (s_wav_path && !s_wav && s_wav_bytes == 0)
  {
    s_wav = std::fopen(s_wav_path, "wb");
    if (s_wav)
      WavHeader(s_wav, 0);
    else
      s_wav_path = nullptr;
  }
  if (s_wav)
  {
    std::fwrite(samples, sizeof(s16), frames * 2, s_wav);  // the host is little-endian
    s_wav_bytes += frames * 4;
  }
#ifdef SKIES_STANDALONE_AURORA
  static const bool s_mute = std::getenv("SKIES_MUTE") != nullptr;
  if (!s_active || s_mute)
    return;
  const int volume = MenuVolume();
  if (volume >= 100)
  {
    s_audio.insert(s_audio.end(), samples, samples + frames * 2);
  }
  else
  {
    // The Volume setting (the WAV dump above stays the DSP's own output)
    for (u32 i = 0; i < frames * 2; ++i)
      s_audio.push_back(static_cast<s16>(samples[i] * volume / 100));
  }
  if (s_audio.size() >= 512 * 2)
  {
    dol_platform_audio_push(s_audio.data(), static_cast<u32>(s_audio.size() / 2));
    s_audio.clear();
  }
#else
  (void)samples;
  (void)frames;
#endif
}
}  // namespace host
