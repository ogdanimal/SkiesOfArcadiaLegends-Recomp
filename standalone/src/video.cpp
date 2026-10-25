// Output through GXRuntime's Aurora backend: the GX command stream, presentation at the game's
// display copy, controllers and sound. Headless builds and --headless runs keep the same entry
// points with no window.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#ifdef SKIES_STANDALONE_AURORA
#include "gxruntime/aurora_backend.h"
#include "gxruntime/platform.h"

#include <aurora/aurora.h>
#include <aurora/gfx.h>
#include <dolphin/gx.h>

extern "C" {
void aurora_backend_present(void);
bool aurora_backend_should_quit(void);
void aurora_backend_write_raw(const void* data, u32 size);
bool aurora_backend_drain_if_over(u32 bytes);
void aurora_backend_set_guest_address_resolver(DolPlatformGuestAddressResolverFn resolve, void* user);
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
std::vector<s16> s_audio;

#ifdef SKIES_STANDALONE_AURORA
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
#ifdef SKIES_STANDALONE_AURORA
  if (headless)
    return true;
  const char* w = std::getenv("SKIES_WIDTH");
  const char* h = std::getenv("SKIES_HEIGHT");
  const AuroraBackendConfig config = {
      .app_name = "Skies of Arcadia Legends (standalone)",
      .window_width = w ? static_cast<unsigned>(std::atoi(w)) : 1280u,
      .window_height = h ? static_cast<unsigned>(std::atoi(h)) : 960u,
      .vsync = std::getenv("SKIES_VSYNC") != nullptr,
      .allow_texture_dumps = false,
      .info_logging = std::getenv("SKIES_AURORA_LOG") != nullptr,
      .graphics_logging = std::getenv("SKIES_AURORA_LOG") != nullptr,
      .force_untextured = false,
      .native_window = nullptr,
      .native_window_width = 0,
      .native_window_height = 0,
      .native_window_scale = 0.f,
      .user_path = nullptr,
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
  // SDK baseline before the game's own GXInit, as the Dolphin-side backend does.
  static std::vector<u8> s_gx_fifo(64 * 1024);
  GXInit(s_gx_fifo.data(), static_cast<u32>(s_gx_fifo.size()));
  GXFlush();
  dol_platform_pad_init();
  dol_platform_audio_set_sample_rate(32000);
  s_active = true;
  return true;
#else
  (void)headless;
  return true;
#endif
}

void VideoShutdown()
{
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
  aurora_backend_write_raw(data, size);
  aurora_backend_drain_if_over(64 * 1024);
#else
  (void)data;
  (void)size;
#endif
}

void VideoFrameEnd()
{
  ++s_frames;
  if ((s_frames % 600) == 1)
    Log("[video] frame %llu at %.2f s guest\n", static_cast<unsigned long long>(s_frames),
        static_cast<double>(Now()) / CPU_HZ);
#ifdef SKIES_STANDALONE_AURORA
  if (!s_active)
    return;
  const bool dump = s_dump_every != 0 && (s_frames % s_dump_every) == 0;
  if (dump)
    aurora_request_framebuffer_readback();
  aurora_backend_present();
  if (dump)
    DumpFrame();
  if (aurora_backend_should_quit())
    RequestQuit();
#endif
}

void VideoRetrace()
{
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
// (START A B X Y Z L R UP DOWN LEFT RIGHT), for unattended runs.
u16 ScriptedButtons()
{
  struct Press
  {
    double at;
    u16 button;
  };
  static std::vector<Press> s_presses;
  static bool s_parsed = false;
  if (!s_parsed)
  {
    s_parsed = true;
    const char* script = std::getenv("SKIES_PRESS");
    static const struct
    {
      const char* name;
      u16 bit;
    } kNames[] = {{"LEFT", 0x1}, {"RIGHT", 0x2}, {"DOWN", 0x4}, {"UP", 0x8}, {"Z", 0x10},
                  {"R", 0x20},   {"L", 0x40},    {"A", 0x100},  {"B", 0x200}, {"X", 0x400},
                  {"Y", 0x800},  {"START", 0x1000}};
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
  u16 held = 0;
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
  buttons[0] = ScriptedButtons();
#ifdef SKIES_STANDALONE_AURORA
  if (!s_active)
    return;
  DolPadState state[4];
  dol_platform_pad_read(state);
  for (int i = 0; i < 4; ++i)
  {
    if (state[i].error != 0)
      continue;
    connected[i] = true;
    buttons[i] = static_cast<u16>(state[i].button | (i == 0 ? buttons[0] : 0));
    sticks[i][0] = state[i].stick_x;
    sticks[i][1] = state[i].stick_y;
    sticks[i][2] = state[i].substick_x;
    sticks[i][3] = state[i].substick_y;
    triggers[i][0] = state[i].trigger_left;
    triggers[i][1] = state[i].trigger_right;
  }
#endif
}

void AudioPushSamples(const s16* samples, u32 frames)
{
#ifdef SKIES_STANDALONE_AURORA
  static const bool s_mute = std::getenv("SKIES_MUTE") != nullptr;
  if (!s_active || s_mute)
    return;
  s_audio.insert(s_audio.end(), samples, samples + frames * 2);
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
