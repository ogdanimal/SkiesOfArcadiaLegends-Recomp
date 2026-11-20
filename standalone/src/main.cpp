// skies-standalone: the recompiled game on this host, no emulator core.
//   skies-standalone --game DIR [--module FILE] [--headless] [--unthrottled] [--seconds N]
//                    [--mmio-log] [--idle-pc ADDR] [--dsp-coef FILE] [--user USERDIR] [--dol FILE]
//                    [--load-state FILE] [--continue N|card] [--sys DIR]
// DIR is an extracted disc (sys/ + files/); FILE the g<GAMEID>_recomp module built for its DOL (by
// default the disc ID's, from the library path: Android's app libraries, or beside the executable).
// USERDIR holds the memory card saves as the Dolphin build keeps them (GC/<region>/Card A/*.gci);
// without it no card is inserted. --dol boots that DOL (a game-patched main.dol) instead of sys/main.dol.
// --load-state starts from a save state (state.cpp); --seconds then counts the state's guest clock
// (--seconds +N: N seconds after the state's time).
// --continue boots straight into card save file N (1-99) or the newest one ("card"), as the launchers'
// Continue ([Skies] AutoLoadCard; needs a ws20+ game patch).
// The launchers start it as they started the Dolphin build's runner (skies-run): --game G --user-dir U
// [--load-state F] [-X11 | --wayland] [--title T], or with no --game; that form does the runner's
// part first (frontend.cpp: data folder, default-game.txt, config.ini, the release's DOL patch, the
// states' folder, the module beside the program) and ignores the runner's --graphics / --audio /
// --mods / --no-mods / --allow-interpreter.
//   skies-standalone --user USERDIR --import-texture-pack PACK | --remove-texture-packs
// imports an HD texture pack (a .7z, .zip or folder, texture_pack.cpp) into USERDIR/Load/Textures/GEA,
// adding to the ones before, or removes them all, and exits.
//   skies-standalone --user USERDIR --import-dc DREAMCAST_DISC [--game DIR] [--sys DIR]
// imports a Dreamcast disc (GDI or CHD; dreamcast.cpp): its music into USERDIR/DreamcastMusic, and from
// Eternal Arcadia (the Japanese discs) the files of the Uncensored setting, converted for --game.
// --sys is the folder with Skies/uncensor-manifest.json (by default the source tree's standalone/data/Sys).
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

#include "dsphle/ax.h"
#include "dsphle/dsp_hle.h"

#include <algorithm>
#include <cmath>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#if defined(__linux__)
#include <sched.h>
#endif

#if defined(_WIN32)
// Laptops with two GPUs (NVIDIA Optimus, AMD switchable graphics): their drivers read these exports and put the
// program on the discrete GPU. Dawn's D3D12 / Vulkan adapter already asks for high performance; this covers
// the D3D11 fallback and drivers' own profiles too (issue #32: 15-17 fps on a GTX 1650 laptop).
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif
#if defined(__APPLE__)
#include <pthread.h>
#endif

namespace host
{
bool PacingOn()
{
  static const bool on = [] {
    for (const char* name : {"SKIES_PRESENT_PACING", "RECOMP_PRESENT_PACING"})
      if (const char* env = std::getenv(name); env && env[0] == '1')
        return true;
    return false;
  }();
  return on;
}

void PacingAdd(PacingWindow& window, double ms, const char* tag, const char* what, double over_a, double over_b)
{
  constexpr unsigned N = sizeof(window.values) / sizeof(window.values[0]);
  window.values[window.count++] = ms;
  if (window.count < N)
    return;
  window.count = 0;
  std::sort(window.values, window.values + N);
  unsigned a = 0, b = 0;
  for (double v : window.values)
  {
    a += v > over_a;
    b += v > over_b;
  }
  Log("[%s] %s ms over %u: p50 %.2f p90 %.2f p99 %.2f max %.2f; >%g ms %u, >%g ms %u\n", tag, what, N,
      window.values[N / 2], window.values[N * 9 / 10], window.values[N * 99 / 100], window.values[N - 1], over_a, a,
      over_b, b);
}

void GpuStats(u64* frames, u64* draw_dones, u64* tokens);  // hw_gpu.cpp
void DiStats(u64* reads, u64* bytes);                        // hw_di.cpp

namespace
{
bool s_throttle = true;
u64 s_stop_cycles = 0;
// --seconds +N: N seconds after the run's start (g_run_start_seconds: the state's time)
double s_stop_relative = 0.0;
// The menu asked for the game to start again (a new language): main runs itself again
bool s_restart = false;
std::chrono::steady_clock::time_point s_wall_start;
// Guest seconds per wall second: 1, or the display's rate over the game's when it is paced to the display
// (FollowDisplay)
double s_rate = 1.0;

std::chrono::nanoseconds GuestToWall(u64 cycles)
{
  return std::chrono::nanoseconds(static_cast<u64>(static_cast<double>(cycles) * 1e9 / (CPU_HZ * s_rate)));
}
u64 s_last_report_cycles = 0;
std::chrono::steady_clock::time_point s_last_report_wall;
// The emulation thread's time per field outside the throttle's sleep (PacingOn): its headroom at full speed
std::chrono::steady_clock::time_point s_last_wake;
PacingWindow s_busy;
// The throttle's sleeps and spins in all: the time the emulation thread had to spare
std::chrono::steady_clock::duration s_idle{};

void Report()
{
  u64 frames, draw_dones, tokens, reads, bytes;
  GpuStats(&frames, &draw_dones, &tokens);
  DiStats(&reads, &bytes);
  Log("[host] %.1f s guest: %llu frames, %llu draw-dones, %llu tokens, %llu disc reads (%.1f MB), "
      "pc %08x, DSP ucode %08x\n",
      static_cast<double>(g_cycles) / CPU_HZ, static_cast<unsigned long long>(frames),
      static_cast<unsigned long long>(draw_dones), static_cast<unsigned long long>(tokens),
      static_cast<unsigned long long>(reads), static_cast<double>(bytes) / (1 << 20), g_cpu.pc,
      dsphle::CurrentUcodeCrc());
}
}  // namespace

// Guest time over wall time for the last half second, from the fields' times (the speed options
// step down when the host can't keep up).
namespace
{
struct SpeedSample
{
  std::chrono::steady_clock::time_point wall;
  u64 cycles;
};
SpeedSample s_speed_samples[32];
u32 s_speed_next = 0;
double s_speed = 1.0;

void SampleSpeed(std::chrono::steady_clock::time_point now)
{
  const SpeedSample& oldest = s_speed_samples[s_speed_next % 32];
  if (s_speed_next >= 32)
  {
    const double wall = std::chrono::duration<double>(now - oldest.wall).count();
    if (wall > 0)
      s_speed = static_cast<double>(g_cycles - oldest.cycles) / CPU_HZ / wall;
  }
  s_speed_samples[s_speed_next % 32] = {now, g_cycles};
  ++s_speed_next;
}
}  // namespace

// The wall time the throttle gives guest time CYCLES (0 when unthrottled): presents are scheduled on it
std::chrono::steady_clock::time_point WallForGuest(u64 cycles)
{
  if (!s_throttle)
    return {};
  return s_wall_start + GuestToWall(cycles);
}

double EmulationSpeed()
{
  return s_speed;
}

std::chrono::steady_clock::duration ThrottleIdle()
{
  return s_idle;
}

bool Throttled()
{
  return s_throttle;
}

void ThrottleRestart()
{
  const auto now = std::chrono::steady_clock::now();
  s_wall_start = now - GuestToWall(g_cycles);
  s_last_report_cycles = g_cycles;
  s_last_report_wall = now;
  s_speed_next = 0;
  s_speed = 1.0;
  s_last_wake = now;
}

// Field pacing on phones: schedutil clocks a thread that sleeps part of every field down until it is busy
// again, so the field that draws runs late on a slow clock (Pocket 5: the emulation thread ~80% busy on a
// gold core at 1.6-2.4 GHz at full speed, where uncapped it runs on the prime core at 2.84). Android keeps
// the emulation thread on the fastest cores; presents follow the throttle's clock (video.cpp
// SchedulePresent), which takes up the rest. docs/handoffs/HANDOFF-emulator-free.md, *Pacing on the Pocket 5*.
// SKIES_PIN_CPU=fast|none|N picks the cores; SKIES_THROTTLE_SPIN_MS=N busy-waits the last N ms before each
// field's target instead of sleeping them (-1: all of it; it raises the clock, at a power cost).
#if defined(__ANDROID__) || defined(__APPLE__)
constexpr const char* DEFAULT_PIN = "fast";
constexpr double DEFAULT_SPIN_MS = 0;
#else
constexpr const char* DEFAULT_PIN = "none";
constexpr double DEFAULT_SPIN_MS = 0;
#endif

#if defined(__linux__)
// The cores by top clock (cpuinfo_max_freq), fastest first
std::vector<cpu_set_t> CoreTiers()
{
  std::vector<std::pair<long, int>> cores;
  for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
  {
    char path[96];
    std::snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", cpu);
    FILE* f = std::fopen(path, "r");
    if (!f)
    {
      if (cpu >= 64)
        break;
      continue;
    }
    long khz = 0;
    if (std::fscanf(f, "%ld", &khz) == 1 && khz > 0)
      cores.emplace_back(khz, cpu);
    std::fclose(f);
  }
  std::sort(cores.begin(), cores.end(), std::greater<>());
  std::vector<cpu_set_t> tiers;
  long tier_khz = -1;
  for (const auto& [khz, cpu] : cores)
  {
    if (khz != tier_khz)
    {
      tiers.emplace_back();
      CPU_ZERO(&tiers.back());
      tier_khz = khz;
    }
    CPU_SET(cpu, &tiers.back());
  }
  return tiers;
}
#endif

// Keeps the calling thread on some cores: ENV=fast|none|N (fast: the cores of the given clock tier, 0
// the fastest; a device with one tier has only that; on macOS any value but none is the user-interactive
// QoS class), else the platform's default.
void PinThread(const char* env, const char* default_pin, size_t tier, const char* name)
{
#if defined(__linux__)
  const char* pin = std::getenv(env);
  const std::string want = pin ? pin : default_pin;
  if (want == "none")
    return;
  cpu_set_t set;
  CPU_ZERO(&set);
  if (want == "fast")
  {
    const std::vector<cpu_set_t> tiers = CoreTiers();
    if (tiers.empty())
      return;
    set = tiers[std::min(tier, tiers.size() - 1)];
  }
  else
  {
    char* end = nullptr;
    const long cpu = std::strtol(want.c_str(), &end, 10);
    if (want.empty() || *end != '\0' || cpu < 0 || cpu >= CPU_SETSIZE)
    {
      Log("[host] %s: %s=%s is not fast, none or a core number, left unpinned\n", name, env, want.c_str());
      return;
    }
    CPU_SET(static_cast<int>(cpu), &set);
  }
  std::string cpus;
  for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
    if (CPU_ISSET(cpu, &set))
      cpus += (cpus.empty() ? "" : ",") + std::to_string(cpu);
  const int rc = sched_setaffinity(0, sizeof(set), &set);
  Log("[host] %s on cpu %s: %s\n", name, cpus.c_str(), rc == 0 ? "ok" : "failed");
#elif defined(__APPLE__)
  // No affinity on macOS: the user-interactive QoS class keeps a thread on the performance cores (an SDL
  // thread starts at the default class, which the scheduler moves to the efficiency cores under load: on
  // an M4 beside 8 busy default-class processes, 2.18x without and 3.45x with it, as fast as with none)
  (void)tier;
  const char* pin = std::getenv(env);
  if (std::strcmp(pin ? pin : default_pin, "none") == 0)
    return;
  qos_class_t before = QOS_CLASS_UNSPECIFIED;
  pthread_get_qos_class_np(pthread_self(), &before, nullptr);
  const int rc = pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
  Log("[host] %s at user-interactive QoS (was 0x%x): %s\n", name, static_cast<unsigned>(before),
      rc == 0 ? "ok" : "failed");
#else
  (void)env;
  (void)default_pin;
  (void)tier;
  (void)name;
#endif
}

void PinEmulationThread()
{
  // The fastest cores (the Pocket 5's prime core 7)
  PinThread("SKIES_PIN_CPU", DEFAULT_PIN, 0, "emulation thread");
}

// The video thread (video.cpp): on phones the next fastest cores, apart from the game's (the Pocket 5's
// gold cores 4-6), so it never shares the game's core nor lands on a little one. (Its clock is the
// other half: video.cpp's polling, SKIES_VIDEO_SPIN.) SKIES_PIN_VIDEO=fast|none|N.
void PinVideoThread()
{
  PinThread("SKIES_PIN_VIDEO", DEFAULT_PIN, 1, "video thread");
}

// The game paced to the display: when the display's refresh (VideoRefreshPeriodNs) is within 0.5% of a whole
// number of refreshes per game frame (the GameCube's 59.94 Hz on a 60 Hz screen: 1.001), guest time runs at
// that rate (the audio's playback rate follows its queue, Aurora's aurora_audio.cpp), so frames keep the same
// place against the refreshes. At 59.94 Hz they
// drift by a refresh about every 17 s, and each time a frame stays up a refresh longer (a 50 ms frame at
// 30 fps, a repeated one at 60). A 50 Hz game on a 60 Hz screen stays at its own rate. Checked every two
// seconds; SKIES_DISPLAY_RATE=0 turns it off.
void FollowDisplay()
{
  static const bool s_on = [] {
    const char* v = std::getenv("SKIES_DISPLAY_RATE");
    return !v || std::atoi(v) != 0;
  }();
  static u32 s_fields = 0;
  if (!s_on || ++s_fields % 120 != 0)
    return;
  const s64 period = VideoRefreshPeriodNs();
  const u64 frame_cycles = ViFrameCycles();
  if (period <= 0 || frame_cycles == 0)
    return;
  const double frame_ns = static_cast<double>(frame_cycles) * 1e9 / CPU_HZ;
  const double refreshes = std::round(frame_ns / static_cast<double>(period));
  double rate = refreshes >= 1 ? frame_ns / (refreshes * static_cast<double>(period)) : 1.0;
  if (std::abs(rate - 1.0) > 0.005)
    rate = 1.0;
  if (rate == s_rate)
    return;
  if (std::abs(rate - s_rate) > 1e-4)
    Log("[host] paced to the display (%.3f Hz): the game at %.4fx\n", 1e9 / static_cast<double>(period), rate);
  const auto at = s_wall_start + GuestToWall(g_cycles);  // the wall time now's guest time keeps
  s_rate = rate;
  s_wall_start = at - GuestToWall(g_cycles);
}

// Called by the VI at every field: pace guest time to the wall clock, stop at --seconds.
void Throttle()
{
  static const double spin_ms = [] {
    PinEmulationThread();
    const char* v = std::getenv("SKIES_THROTTLE_SPIN_MS");
    return v ? std::atof(v) : DEFAULT_SPIN_MS;
  }();
  const auto now = std::chrono::steady_clock::now();
#ifdef SKIES_STANDALONE_AURORA
  DreamcastFieldTick();
#endif
  SampleSpeed(now);
  if (PacingOn() && s_last_wake.time_since_epoch().count() != 0)
    PacingAdd(s_busy, std::chrono::duration<double, std::milli>(now - s_last_wake).count(), "host-busy",
              "emulation thread per field", 1000.0 / 60, 1000.0 / 30);
  if (g_cycles - s_last_report_cycles >= 5 * CPU_HZ)
  {
    const double wall = std::chrono::duration<double>(now - s_last_report_wall).count();
    const double guest = static_cast<double>(g_cycles - s_last_report_cycles) / CPU_HZ;
    Report();
    Log("[host] speed %.2fx\n", wall > 0 ? guest / wall : 0.0);
    s_last_report_cycles = g_cycles;
    s_last_report_wall = now;
  }
  // Relative: recomputed each time, since the state (and so the start) loads after the first fields
  if (s_stop_relative > 0)
    s_stop_cycles = static_cast<u64>((g_run_start_seconds + s_stop_relative) * CPU_HZ);
  if (s_stop_cycles && g_cycles >= s_stop_cycles)
    RequestQuit();
  if (VideoQuitRequested())
    RequestQuit();
  if (MenuTakeRestartRequest())
  {
    s_restart = true;
    RequestQuit();
  }
  s_last_wake = now;
  if (!s_throttle)
    return;
  FollowDisplay();
  const auto target = s_wall_start + GuestToWall(g_cycles);
  if (target > now + std::chrono::milliseconds(1))
  {
    if (spin_ms != 0)
    {
      const auto spin_from =
          spin_ms < 0 ? now : target - std::chrono::microseconds(static_cast<s64>(spin_ms * 1000));
      if (spin_from > now)
        std::this_thread::sleep_until(spin_from);
      while (std::chrono::steady_clock::now() < target)
      {
      }
    }
    else
      std::this_thread::sleep_until(target);
    s_last_wake = std::chrono::steady_clock::now();
    s_idle += s_last_wake - now;
  }
  else if (now - target > std::chrono::milliseconds(200))
    s_wall_start += now - target - std::chrono::milliseconds(200);  // don't race to catch up
}
}  // namespace host

#if defined(__ANDROID__) && defined(SKIES_ADRENOTOOLS)
#include <adrenotools/driver.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
// A custom GPU driver (Mesa Turnip on Adreno) the player installed in the launcher (Settings, Graphics, GPU driver:
// GFX.ini [Settings] DriverLibName, extracted to files/GPUDrivers/Extracted), opened through libadrenotools as the
// Dolphin build's VulkanLoader does; Dawn then gets its vkGetInstanceProcAddr through libskiesvk.so
// (android_vkshim.c), which a libvulkan.so symlink in GPUDrivers/Shim names and Aurora searches first
// (AURORA_VULKAN_LIBRARY_DIR). SkiesGameActivity passes the two folders (SKIES_NATIVE_LIB_DIR,
// SKIES_GPU_DRIVER_DIR). Anything that fails leaves the device's own driver.
void LoadCustomGpuDriver()
{
  const std::string name = host::SettingGetString(host::SettingFile::Gfx, "Settings", "DriverLibName");
  if (name.empty())
    return;
  if (const char* api = std::getenv("DOL_AURORA_BACKEND"); api && std::string(api) == "gles")
  {
    host::Log("[host] GPU driver %s: not used with OpenGL ES\n", name.c_str());
    return;
  }
  const char* lib_dir = std::getenv("SKIES_NATIVE_LIB_DIR");
  const char* driver_dir = std::getenv("SKIES_GPU_DRIVER_DIR");
  if (!lib_dir || !driver_dir)
  {
    host::Log("[host] GPU driver %s: the app didn't pass its folders, the device's own driver\n", name.c_str());
    return;
  }
  const std::string base = driver_dir, hooks = std::string(lib_dir) + "/";
  const std::string tmp = base + "/Tmp/", extracted = base + "/Extracted/", redirect = base + "/FileRedirect/";
  void* vulkan = adrenotools_open_libvulkan(RTLD_NOW, ADRENOTOOLS_DRIVER_FILE_REDIRECT | ADRENOTOOLS_DRIVER_CUSTOM,
                                            tmp.c_str(), hooks.c_str(), extracted.c_str(), name.c_str(),
                                            redirect.c_str(), nullptr);
  void* get_proc = vulkan ? dlsym(vulkan, "vkGetInstanceProcAddr") : nullptr;
  const std::string shim_lib = hooks + "libskiesvk.so";
  void* shim = get_proc ? dlopen(shim_lib.c_str(), RTLD_NOW) : nullptr;
  using SetFn = void (*)(void*);
  const auto set = shim ? reinterpret_cast<SetFn>(dlsym(shim, "skies_vkshim_set")) : nullptr;
  if (!set)
  {
    host::Log("[host] GPU driver %s didn't load (%s): the device's own driver\n", name.c_str(),
              !vulkan ? "libadrenotools" : !get_proc ? "no vkGetInstanceProcAddr" : "no shim");
    return;
  }
  set(get_proc);
  const std::string shim_dir = base + "/Shim";
  mkdir(shim_dir.c_str(), 0700);
  const std::string link = shim_dir + "/libvulkan.so";
  unlink(link.c_str());
  if (symlink(shim_lib.c_str(), link.c_str()) != 0)
  {
    host::Log("[host] GPU driver %s: no shim link, the device's own driver\n", name.c_str());
    return;
  }
  // With the slash: Dawn joins its search paths and the file name as they are (DynamicLib::Open)
  setenv("AURORA_VULKAN_LIBRARY_DIR", (shim_dir + "/").c_str(), 1);
  host::Log("[host] GPU driver %s (custom, through libadrenotools)\n", name.c_str());
}
}  // namespace
#endif

#if defined(__ANDROID__) && defined(SKIES_STANDALONE_AURORA)
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_system.h>
#include <jni.h>

#include <cstdio>
#include <fstream>
#include <vector>

// The Android shell (standalone/android): SDLActivity calls main (SDL_main) without arguments. They come
// from args.txt in the app's external files folder, one per line ($FILES in them is that folder), the
// environment from env.txt there (NAME=VALUE lines), and the log (stderr) goes to log.txt there. After a
// restart (restart.flag there, Restart) the state / Continue arguments and a test's presses are left out.
namespace
{
std::vector<std::string> s_android_args;
std::vector<char*> s_android_argv;

}  // namespace

// Android's Back (the app's SkiesGameActivity): the menu, as Esc on the desktop; Quit game in it ends the game
// A touch on the game's screen that no touch control took: the in-game menu's (x, y fractions of the view)
extern "C" JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_skies_SkiesGameActivity_nativeMenuTap(JNIEnv*, jclass,
                                                                                                       jfloat x, jfloat y)
{
  return host::MenuTap(x, y) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_skies_SkiesGameActivity_nativeBack(JNIEnv*, jclass)
{
  host::MenuRequestBack();
}

// The touch controls' setting ([Skies] TouchControls: 0 Auto, 1 Always, 2 Off), polled by the view
extern "C" JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_skies_SkiesTouchControls_nativeTouchMode(JNIEnv*, jclass)
{
  return host::TouchMode();
}

// Android's touch controls (the app's SkiesTouchControls): controller 1, beside a real one
extern "C" JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_skies_SkiesTouchControls_nativeTouchPad(
    JNIEnv*, jclass, jint buttons, jint stick_x, jint stick_y, jint c_x, jint c_y, jint trigger_l, jint trigger_r)
{
  const auto s8_of = [](jint v) { return static_cast<s8>(v < -127 ? -127 : v > 127 ? 127 : v); };
  const auto u8_of = [](jint v) { return static_cast<u8>(v < 0 ? 0 : v > 255 ? 255 : v); };
  host::TouchPadSet(static_cast<u16>(buttons), s8_of(stick_x), s8_of(stick_y), s8_of(c_x), s8_of(c_y),
                    u8_of(trigger_l), u8_of(trigger_r));
}

namespace
{
// The test hooks below (env.txt, SKIES_EXTRA_ARGS, args.txt) are files in the app's external folder, which on
// Android 9-10 any app with the storage permission can write; they set variables (library folders among them) and
// add arguments (states to load). Only test builds read them: the test app (package ending in ".test") and the
// bare shell (com.skiesofarcadia.standalone). The package is the process name, less the ":game" suffix.
bool AndroidTestHooks()
{
  std::ifstream in("/proc/self/cmdline", std::ios::binary);
  std::string name;
  std::getline(in, name, '\0');
  if (const size_t colon = name.find(':'); colon != std::string::npos)
    name.resize(colon);
  const auto ends_with = [&](const std::string& tail) {
    return name.size() >= tail.size() && name.compare(name.size() - tail.size(), tail.size(), tail) == 0;
  };
  return ends_with(".test") || name == "com.skiesofarcadia.standalone";
}

// Vulkan's crash safeguard (issue #34): a driver that crashes while Vulkan starts kills the process, so the
// fallback to OpenGL ES (taken when Vulkan merely fails) never runs. A start with Vulkan leaves a marker in the
// app's files that goes once the game has shown its first frames (GpuStartSurvived) or main returns; a marker
// still there at the next start means that one died first, and the Renderer setting goes to OpenGL ES (the
// launcher reads the settings again when the game returns), with a line in the log.
std::string s_vulkan_marker;

void VulkanCrashGuard()
{
  if (host::SettingGetString(host::SettingFile::Main, "Core", "GFXBackend") == "OGL")
    return;
  const char* dir = SDL_GetAndroidExternalStoragePath();
  if (!dir)
    return;
  const std::string marker = std::string(dir) + "/vulkan-start-pending";
  if (std::remove(marker.c_str()) == 0)
  {
    host::SettingSetString(host::SettingFile::Main, "Core", "GFXBackend", "OGL");
    host::SettingsSave();
    host::Log("[host] renderer: the last start with Vulkan ended before its first frames (a graphics driver crash?), so "
        "the Renderer setting is now OpenGL ES (Settings > Graphics > Renderer to go back)\n");
    return;
  }
  if (std::FILE* f = std::fopen(marker.c_str(), "w"))
  {
    std::fclose(f);
    s_vulkan_marker = marker;
  }
}

void AndroidSetup(int& argc, char**& argv)
{
  const char* dir = SDL_GetAndroidExternalStoragePath();
  if (!dir)
    return;
  const std::string files = dir;
  std::freopen((files + "/log.txt").c_str(), "w", stderr);
  const bool restarted = std::remove((files + "/restart.flag").c_str()) == 0;
  if (!AndroidTestHooks())
  {
    for (const char* hook : {"env.txt", "args.txt"})
    {
      if (std::ifstream(files + "/" + hook))
        std::fprintf(stderr, "[host] %s ignored: test builds only\n", hook);
    }
    return;
  }
  const auto lines = [&](const char* name, auto&& each) {
    std::ifstream in(files + "/" + name);
    for (std::string line; std::getline(in, line);)
    {
      while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
        line.pop_back();
      if (line.empty() || line[0] == '#')
        continue;
      for (size_t at; (at = line.find("$FILES")) != std::string::npos;)
        line.replace(at, 6, files);
      each(line);
    }
  };
  lines("env.txt", [&](const std::string& line) {
    const size_t eq = line.find('=');
    if (eq == std::string::npos)
      return;
    const std::string name = line.substr(0, eq);
    if (restarted && (name == "SKIES_PRESS" || name == "SKIES_PRESS_FILE"))
      return;
    setenv(name.c_str(), line.substr(eq + 1).c_str(), 1);
  });
  // SKIES_EXTRA_ARGS (env.txt, device tests): words added after the launcher's command line, so a later
  // --load-state or --seconds wins (e.g. "--load-state $FILES/sweep/a.sav --seconds +40"); not after a restart
  const char* extra = restarted ? nullptr : std::getenv("SKIES_EXTRA_ARGS");
  if (argc > 1)
  {
    if (!extra || !*extra)
      return;
    for (int i = 0; i < argc; ++i)
      s_android_args.push_back(argv[i]);
    for (std::string rest = extra; !rest.empty();)
    {
      const size_t space = rest.find(' ');
      const std::string word = rest.substr(0, space);
      rest = space == std::string::npos ? "" : rest.substr(space + 1);
      if (!word.empty())
        s_android_args.push_back(word);
    }
    for (auto& arg : s_android_args)
      s_android_argv.push_back(arg.data());
    s_android_argv.push_back(nullptr);
    argc = static_cast<int>(s_android_args.size());
    argv = s_android_argv.data();
    return;
  }
  s_android_args.push_back("skies-standalone");
  bool skip_next = false;
  lines("args.txt", [&](const std::string& line) {
    if (skip_next)
    {
      skip_next = false;
      return;
    }
    if (restarted && (line == "--load-state" || line == "--continue"))
    {
      skip_next = true;
      return;
    }
    s_android_args.push_back(line);
  });
  for (auto& arg : s_android_args)
    s_android_argv.push_back(arg.data());
  s_android_argv.push_back(nullptr);
  argc = static_cast<int>(s_android_args.size());
  argv = s_android_argv.data();
}
}  // namespace
#endif

#if !defined(__ANDROID__) && !defined(__APPLE__)
namespace
{
// The renderer's crash safeguard on Windows and Linux (Android: VulkanCrashGuard): a graphics driver that crashes
// while its API starts kills the process, so Auto's fallback (taken when an API merely fails) never runs and every
// start dies the same way. A start leaves USER/Cache/renderer-start-pending, with the API in use written in once
// the first frame is out (GpuStartApi); it goes after 300 frames, when main returns, or at a Fatal (not the
// driver's doing). One still there at the next start moves the Renderer setting past the API that crashed
// (Windows: Direct3D 12, Vulkan, Direct3D 11; Linux: Vulkan, OpenGL ES), with a line in the log.
std::string s_renderer_marker;

// [Skies] Renderer values: 1 Direct3D 12, 2 Vulkan, 3 Direct3D 11, 4 OpenGL ES
#if defined(_WIN32)
constexpr int RENDERER_ORDER[] = {1, 2, 3};
#else
constexpr int RENDERER_ORDER[] = {2, 4};
#endif

int RendererValue(const std::string& name)
{
  return name == "Direct3D 12" ? 1 : name == "Vulkan" ? 2 : name == "Direct3D 11" ? 3 : name == "OpenGL ES" ? 4 : 0;
}

void RendererCrashGuard(const std::string& user)
{
  if (user.empty())
    return;
  const std::filesystem::path cache = std::filesystem::path(user) / "Cache";
  std::error_code ec;
  std::filesystem::create_directories(cache, ec);
  const std::string marker = (cache / "renderer-start-pending").string();
  if (std::FILE* f = std::fopen(marker.c_str(), "r"))
  {
    char name[32] = {};
    const size_t n = std::fread(name, 1, sizeof(name) - 1, f);
    std::fclose(f);
    std::remove(marker.c_str());
    // The API that crashed: the one written in, or (dead before its first frame) the first one this start tried
    int crashed = RendererValue(std::string(name, n));
    const int setting = host::SettingGet(host::SettingFile::Main, "Skies", "Renderer", 0);
    if (crashed == 0)
      crashed = setting > 0 ? setting : RENDERER_ORDER[0];
    const int* const end = std::end(RENDERER_ORDER);
    const int* const at = std::find(std::begin(RENDERER_ORDER), end, crashed);
    if (at != end && at + 1 != end)
    {
      host::SettingSet(host::SettingFile::Main, "Skies", "Renderer", *(at + 1), false);
      host::SettingsSave();
      static const char* const names[] = {"Auto", "Direct3D 12", "Vulkan", "Direct3D 11", "OpenGL ES"};
      host::Log("[host] renderer: the last start with %s ended before its first frames (a graphics driver crash?), "
                "so the Renderer setting is now %s (Settings > Graphics > Renderer to change it)\n", names[crashed],
                names[*(at + 1)]);
    }
    else
      host::Log("[host] renderer: the last start ended before its first frames, with no other graphics API left "
                "to try\n");
  }
  if (std::FILE* f = std::fopen(marker.c_str(), "w"))
  {
    std::fclose(f);
    s_renderer_marker = marker;
  }
}
}  // namespace
#endif

namespace host
{
void GpuStartApi(const char* name)
{
#if !defined(__ANDROID__) && !defined(__APPLE__)
  if (!s_renderer_marker.empty() && name && *name)
  {
    if (std::FILE* f = std::fopen(s_renderer_marker.c_str(), "w"))
    {
      std::fputs(name, f);
      std::fclose(f);
    }
  }
#else
  (void)name;
#endif
}

void GpuStartSurvived()
{
#if defined(__ANDROID__)
  if (!s_vulkan_marker.empty())
  {
    std::remove(s_vulkan_marker.c_str());
    s_vulkan_marker.clear();
  }
#elif !defined(__APPLE__)
  if (!s_renderer_marker.empty())
  {
    std::remove(s_renderer_marker.c_str());
    s_renderer_marker.clear();
  }
#endif
}
}  // namespace host

namespace
{
// The SDK scheduler's idle spin in SelectThread, per game (release/<disc ID>.ini StaticRecompIdlePC)
u32 DefaultIdlePc(const std::string& disc_id)
{
  if (disc_id == "GEAE8P")
    return 0x80237BA8u;
  if (disc_id == "GEAP8P")
    return 0x8023B02Cu;
  if (disc_id == "GEAJ8P")
    return 0x8023772Cu;
  return 0;
}

// The disc's module by name: Android loads it from the APK's libraries; the desktop takes the one
// beside the executable, else leaves it to the library path.
std::string DefaultModule(const std::string& disc_id, const char* argv0)
{
#if defined(__ANDROID__)
  (void)argv0;
  return "libg" + disc_id + "_recomp.so";
#else
#if defined(_WIN32)
  const std::string name = "g" + disc_id + "_recomp.dll";
#elif defined(__APPLE__)
  const std::string name = "g" + disc_id + "_recomp.dylib";
#else
  const std::string name = "g" + disc_id + "_recomp.so";
#endif
  std::error_code ec;
  const std::filesystem::path beside = std::filesystem::path(argv0).parent_path() / name;
  return std::filesystem::exists(beside, ec) ? beside.string() : name;
#endif
}
}  // namespace

namespace
{
// The same game started again, as the Dolphin build's frontend does for a new language: this
// executable with the same arguments, less a save state or a Continue (the game starts at its title
// screen). Only where CanRestart (menu.cpp) offers it. Android: the activity starts the game again in a new
// process (SkiesActivity.restartGame; SDL runs main once per process) and restart.flag tells AndroidSetup.
void Restart(int argc, char** argv)
{
#if defined(__ANDROID__)
  (void)argc;
  (void)argv;
  host::Log("[host] starting again\n");
  if (const char* dir = SDL_GetAndroidExternalStoragePath())
    std::ofstream(std::string(dir) + "/restart.flag") << "1\n";
  JNIEnv* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
  jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
  if (env && activity)
  {
    jclass cls = env->GetObjectClass(activity);
    if (jmethodID restart = env->GetMethodID(cls, "restartGame", "()V"))
      env->CallVoidMethod(activity, restart);
    if (env->ExceptionCheck())
    {
      env->ExceptionDescribe();
      env->ExceptionClear();
    }
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
  }
#else
  std::vector<char*> args;
  for (int i = 0; i < argc; ++i)
  {
    const std::string a = argv[i];
    if ((a == "--load-state" || a == "--continue") && i + 1 < argc)
    {
      ++i;
      continue;
    }
    args.push_back(argv[i]);
  }
  args.push_back(nullptr);
  host::Log("[host] starting again\n");
  // A scripted run's presses (SKIES_PRESS) are for its first start: replayed, they would change the
  // language and restart again
#if defined(_WIN32)
  _putenv("SKIES_PRESS=");
  _putenv("SKIES_PRESS_FILE=");
#else
  unsetenv("SKIES_PRESS");
  unsetenv("SKIES_PRESS_FILE");
#endif
#if defined(_WIN32)
  // _execv joins the arguments with spaces as they are: quote each for the C runtime's parser (a
  // user folder with a space in it), backslashes before a quote doubled
  std::vector<std::string> quoted;
  for (char* const* a = args.data(); *a; ++a)
  {
    const std::string arg = *a;
    if (!arg.empty() && arg.find_first_of(" \t\"") == std::string::npos)
    {
      quoted.push_back(arg);
      continue;
    }
    std::string q = "\"";
    size_t slashes = 0;
    for (const char c : arg)
    {
      if (c == '\\')
      {
        ++slashes;
        continue;
      }
      q.append(c == '"' ? slashes * 2 + 1 : slashes, '\\');
      slashes = 0;
      q += c;
    }
    q.append(slashes * 2, '\\');
    q += '"';
    quoted.push_back(q);
  }
  std::vector<char*> win_args;
  for (std::string& q : quoted)
    win_args.push_back(q.data());
  win_args.push_back(nullptr);
  _execv(argv[0], win_args.data());
#elif defined(__linux__)
  // The executable's real path (exec'ing /proc/self/exe itself would name the process "exe")
  std::error_code ec;
  const std::string self = std::filesystem::read_symlink("/proc/self/exe", ec).string();
  execv(self.empty() ? argv[0] : self.c_str(), args.data());
#else
  execv(argv[0], args.data());
#endif
  host::Log("[host] couldn't start again (errno %d)\n", errno);
#endif
}
}  // namespace

int main(int argc, char** argv)
{
  using namespace host;
#if defined(__ANDROID__) && defined(SKIES_STANDALONE_AURORA)
  AndroidSetup(argc, argv);
#endif
  setvbuf(stderr, nullptr, _IONBF, 0);
  // Aurora's info lines go to stdout: unbuffered too, so a crash doesn't lose the last of them
  setvbuf(stdout, nullptr, _IONBF, 0);
  std::string game, module, dol, user, load_state, import_pack, import_dc, sys_dir, dsp_coef, state_dir;
  bool remove_packs = false;
  bool runner_form = false;  // --user-dir: the launchers' command line
  LaunchOptions::WindowSystem window_system = LaunchOptions::WindowSystem::Default;
  std::string title;
  bool headless = false;
  u32 idle_pc = ~0u;
  int continue_file = 0;
  double seconds = 0;
  bool seconds_relative = false;  // --seconds +N
  for (int i = 1; i < argc; ++i)
  {
    const std::string a = argv[i];
    auto next = [&]() -> const char* {
      if (i + 1 >= argc)
      {
        Log("%s needs a value\n", a.c_str());
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--game")
      game = next();
    else if (a == "--module")
      module = next();
    else if (a == "--headless")
      headless = true;
    else if (a == "--unthrottled")
      s_throttle = false;
    else if (a == "--seconds")
    {
      const std::string v = next();
      seconds_relative = !v.empty() && v[0] == '+';
      seconds = std::atof(v.c_str() + (seconds_relative ? 1 : 0));
    }
    else if (a == "--mmio-log")
      g_log_mmio = true;
    else if (a == "--idle-pc")
      idle_pc = static_cast<u32>(std::strtoul(next(), nullptr, 0));
    else if (a == "--dsp-coef")
      dsp_coef = next();
    else if (a == "--dol")
      dol = next();
    else if (a == "--user")
      user = next();
    else if (a == "--user-dir")
    {
      user = next();
      runner_form = true;
    }
    else if (a == "--title")
      title = next();
    else if (a == "-X11" || a == "--x11")
      window_system = LaunchOptions::WindowSystem::X11;
    else if (a == "--wayland")
      window_system = LaunchOptions::WindowSystem::Wayland;
    else if (a == "--mute")
    {
#if defined(_WIN32)
      _putenv("SKIES_MUTE=1");
#else
      setenv("SKIES_MUTE", "1", 1);
#endif
    }
    else if (a == "--graphics" || a == "--audio" || a == "--mods")
      next();  // the runner's: Aurora picks its own GPU backend; no mods
    else if (a == "--no-mods" || a == "--allow-interpreter")
    {
    }
    else if (a == "--load-state")
      load_state = next();
    else if (a == "--continue")
    {
      const std::string v = next();
      continue_file = v == "card" ? CONTINUE_NEWEST : std::atoi(v.c_str());
    }
    else if (a == "--import-texture-pack")
      import_pack = next();
    else if (a == "--remove-texture-packs")
      remove_packs = true;
    else if (a == "--import-dc")
      import_dc = next();
    else if (a == "--sys")
      sys_dir = next();
    else if (a == "--state-dir")
      state_dir = next();  // the Android launcher's (a romhack's or another version's folder)
    else
    {
      Log("usage: %s --game DIR [--module FILE] [--headless] [--unthrottled] [--seconds N] "
          "[--mmio-log] [--idle-pc ADDR] [--dsp-coef FILE] [--user USERDIR] [--dol FILE] "
          "[--load-state FILE] [--continue N|card] [--sys DIR] [--state-dir DIR]\n"
          "       %s --user USERDIR --import-texture-pack PACK | --remove-texture-packs\n"
          "       %s --user USERDIR --import-dc DREAMCAST_DISC [--game DIR] [--sys DIR]\n",
          argv[0], argv[0], argv[0]);
      return 2;
    }
  }
#ifdef SKIES_STANDALONE_AURORA
  if (!sys_dir.empty())
    DreamcastSetSysDir(sys_dir);
#endif
  if (!import_dc.empty())
  {
    // The Dreamcast disc (GDI or CHD): its music, and with Eternal Arcadia the uncensored files,
    // converted for --game when it is given (dreamcast.cpp)
    if (user.empty())
    {
      Log("--user is required\n");
      return 2;
    }
    SettingsInit(user);
#ifdef SKIES_STANDALONE_AURORA
    return DreamcastImportCommand(import_dc, game);
#else
    Log("This build has no Dreamcast import (built without Aurora)\n");
    return 2;
#endif
  }
  if (!import_pack.empty() || remove_packs)
  {
    if (user.empty())
    {
      Log("--user is required\n");
      return 2;
    }
    SettingsInit(user);
#ifdef SKIES_STANDALONE_AURORA
    return TexturePackCommand(import_pack, remove_packs);
#else
    Log("This build has no HD textures (built without Aurora)\n");
    return 2;
#endif
  }
  LaunchOptions launch;
#if !defined(__ANDROID__)
  if (runner_form || game.empty())
  {
    launch.game = game;
    launch.user = user;
    launch.module = module;
    launch.dsp_coef = dsp_coef;
    launch.sys_dir = sys_dir;
    launch.window_system = window_system;
    if (const int code = FrontendPrepare(argv[0], launch); code >= 0)
      return code;
    game = launch.game;
    user = launch.user;
    module = launch.module;
    dsp_coef = launch.dsp_coef;
    sys_dir = launch.sys_dir;
  }
#endif
  if (!title.empty())
    g_window.title = title;
  if (dsp_coef.empty())
    dsp_coef = SKIES_DSP_COEF_DEFAULT;
#ifdef SKIES_STANDALONE_AURORA
  if (!sys_dir.empty())
    DreamcastSetSysDir(sys_dir);
#endif
  if (game.empty())
  {
    Log("--game is required\n");
    return 2;
  }
  if (!load_state.empty() && !std::filesystem::is_regular_file(load_state))
  {
    // As the runner: a state that isn't there would boot to the title screen and look like it worked
    Log("savestate not found: %s\n", load_state.c_str());
    return 2;
  }

#if defined(_WIN32)
  // A GUI program started without redirected output (the launcher, a double click) has no stderr: the log
  // goes to USER/Logs/skies-standalone.log, as the Dolphin build's to User/Logs
  if (_fileno(stderr) < 0 && !user.empty())
  {
    const std::filesystem::path logs = std::filesystem::path(user) / "Logs";
    std::error_code ec;
    std::filesystem::create_directories(logs, ec);
    const std::string log = (logs / "skies-standalone.log").string();
    // Emptied once, then both streams append (each write at the end, so neither overwrites the other)
    if (FILE* f = std::fopen(log.c_str(), "w"))
      std::fclose(f);
    if (std::freopen(log.c_str(), "a", stderr))
      setvbuf(stderr, nullptr, _IONBF, 0);
    if (std::freopen(log.c_str(), "a", stdout))
      setvbuf(stdout, nullptr, _IONBF, 0);
  }
  else
#endif
  // Everywhere else (a terminal, the launcher on Linux and macOS, the Android app's log.txt) the log also goes
  // to USER/Logs/skies-standalone.log, the file the README asks players to attach
  if (!user.empty())
  {
    const std::filesystem::path logs = std::filesystem::path(user) / "Logs";
    std::error_code ec;
    std::filesystem::create_directories(logs, ec);
    LogAlsoToFile((logs / "skies-standalone.log").string());
  }
  ExiSetUserDir(user);
  SettingsInit(user);
#if defined(__ANDROID__)
  // The app's Graphics > Renderer row (the Dolphin build's [Core] GFXBackend): OpenGL ES is the way
  // out of a Vulkan driver that misbehaves. DOL_AURORA_BACKEND (env.txt) still wins for tests.
  if (!std::getenv("DOL_AURORA_BACKEND") && seconds <= 0)
    VulkanCrashGuard();
  if (!std::getenv("DOL_AURORA_BACKEND") && SettingGetString(SettingFile::Main, "Core", "GFXBackend") == "OGL")
  {
    setenv("DOL_AURORA_BACKEND", "gles", 1);
    Log("[host] renderer: OpenGL ES (Settings)\n");
  }
#if defined(SKIES_ADRENOTOOLS)
  LoadCustomGpuDriver();
#endif
#else
  // Desktop: [Skies] Renderer (the launcher's and the in-game menu's Renderer row): 0 Auto (Direct3D 12, then
  // Vulkan, then Direct3D 11 on Windows; Vulkan, then OpenGL ES on Linux; Metal on macOS), 1 Direct3D 12,
  // 2 Vulkan, 3 Direct3D 11, 4 OpenGL ES: tried first, the usual order after it if it can't start. The way
  // out of a graphics driver that misbehaves with the automatic choice. DOL_AURORA_BACKEND still wins.
#if !defined(__APPLE__)  // macOS has Metal only: nothing to move on to
  if (!std::getenv("DOL_AURORA_BACKEND") && seconds <= 0)  // not unattended test runs (often cut short on purpose)
    RendererCrashGuard(user);
#endif
  if (!std::getenv("DOL_AURORA_BACKEND"))
  {
    static const char* const apis[] = {nullptr, "d3d12", "vulkan", "d3d11", "gles"};
    const int renderer = SettingGet(SettingFile::Main, "Skies", "Renderer", 0);
    if (renderer > 0 && renderer < 5)
    {
#if defined(_WIN32)
      _putenv_s("DOL_AURORA_BACKEND", apis[renderer]);
#else
      setenv("DOL_AURORA_BACKEND", apis[renderer], 1);
#endif
      Log("[host] renderer: %s first (Settings)\n", apis[renderer]);
    }
  }
#endif
  StateSetUserDir(user, state_dir.empty() ? launch.state_dir : state_dir);
  // config.ini's resolution at every start, as the runner set it; its 60 fps for a first start
  // that has no Frame rate yet
  if (launch.efb_scale > 0)
    SettingSet(SettingFile::Gfx, "Settings", "InternalResolution", launch.efb_scale);
  if (launch.frame_interpolation && SettingGet(SettingFile::Main, "Skies", "FrameMode", -1) < 0)
    SettingSet(SettingFile::Main, "Skies", "FrameMode", 1);
  SchedInit();
  CpuInit();
  if (!DiscOpen(game, dol))
    return 1;
  const std::string disc_id(reinterpret_cast<const char*>(DiscHeader()), 6);
  if (module.empty())
    module = DefaultModule(disc_id, argv[0]);
  if (!LoadModule(module.c_str()))
    return 1;
  if (disc_id != ModuleGameId())
    Log("[host] warning: disc %s, module built for %s\n", disc_id.c_str(), ModuleGameId());
  if (!Boot(game) || !VerifyModule())
    return 1;
  if (idle_pc == ~0u)
    idle_pc = DefaultIdlePc(disc_id);
  SetIdlePc(idle_pc);
  dsphle::SetResamplingCoefficientsPath(dsp_coef.c_str());

  PiInit();
  ViInit();
  GpuInit();
  DffInit();
  DspInit();
  DiInit();
  SiInit();
  ExiInit();
  AiInit();
  MenuApplyGame();  // the picture's shape before the window opens at it
  if (!VideoInit(headless))
    return 1;
  if (seconds > 0 && !seconds_relative)
    s_stop_cycles = static_cast<u64>(seconds * CPU_HZ);
  s_stop_relative = seconds_relative ? seconds : 0.0;

  // The game on its own thread, this one the video thread (video.cpp)
  VideoRun([&] {
    s_wall_start = s_last_report_wall = std::chrono::steady_clock::now();
    // A state replaces a Continue (and clears one the key still holds, as the launchers do)
    MenuContinue(load_state.empty() ? continue_file : CONTINUE_OFF);
    if (!load_state.empty())
      StateLoadAtStart(load_state);
    RunCpu();
  });
  Report();
#ifdef SKIES_STANDALONE_AURORA
  DreamcastShutdown();
#endif
  ExiShutdown();
  VideoShutdown();
  GpuStartSurvived();  // quit before its first frames: not a crash
  if (s_restart)
    Restart(argc, argv);
  return 0;
}
