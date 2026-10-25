// skies-standalone: the recompiled game on this host, no emulator core.
//   skies-standalone --game DIR --module FILE [--headless] [--unthrottled] [--seconds N]
//                    [--mmio-log] [--idle-pc ADDR] [--dsp-coef FILE] [--user USERDIR] [--dol FILE]
//                    [--load-state FILE]
// DIR is an extracted disc (sys/ + files/); FILE the g<GAMEID>_recomp module built for its DOL.
// USERDIR holds the memory card saves as the Dolphin build keeps them (GC/<region>/Card A/*.gci);
// without it no card is inserted. --dol boots that DOL (a game-patched main.dol) instead of sys/main.dol.
// --load-state starts from a save state (state.cpp); --seconds then counts the state's guest clock.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

#include "dsphle/ax.h"
#include "dsphle/dsp_hle.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#if defined(__linux__)
#include <sched.h>
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
std::chrono::steady_clock::time_point s_wall_start;
u64 s_last_report_cycles = 0;
std::chrono::steady_clock::time_point s_last_report_wall;
// The emulation thread's time per field outside the throttle's sleep (PacingOn): its headroom at full speed
std::chrono::steady_clock::time_point s_last_wake;
PacingWindow s_busy;

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

double EmulationSpeed()
{
  return s_speed;
}

void ThrottleRestart()
{
  const auto now = std::chrono::steady_clock::now();
  s_wall_start = now - std::chrono::nanoseconds(static_cast<u64>(static_cast<double>(g_cycles) * 1e9 / CPU_HZ));
  s_last_report_cycles = g_cycles;
  s_last_report_wall = now;
  s_speed_next = 0;
  s_speed = 1.0;
  s_last_wake = now;
}

// Field pacing on phones: schedutil clocks a thread that sleeps a fifth of every field down until
// it is busy again, so the field that draws runs late on a slow clock (Pocket 5: p99 44 ms between
// presents at full speed, where the same thread uncapped makes 1.8x). Android therefore keeps the
// emulation thread on the fastest cores and busy-waits the last 6 ms before each field's target
// (battle p99 37.5 ms, save point 35.8; the Dolphin build 37.7 / 36.2 at about the same power;
// docs/handoffs/HANDOFF-emulator-free.md, *Pacing on the Pocket 5*).
// SKIES_PIN_CPU=fast|none|N picks the cores, SKIES_THROTTLE_SPIN_MS=N the busy wait (-1: all of it).
#if defined(__ANDROID__)
constexpr const char* DEFAULT_PIN = "fast";
constexpr double DEFAULT_SPIN_MS = 6;
#else
constexpr const char* DEFAULT_PIN = "none";
constexpr double DEFAULT_SPIN_MS = 0;
#endif

void PinEmulationThread()
{
#if defined(__linux__)
  const char* pin = std::getenv("SKIES_PIN_CPU");
  const std::string want = pin ? pin : DEFAULT_PIN;
  if (want == "none")
    return;
  cpu_set_t set;
  CPU_ZERO(&set);
  if (want == "fast")
  {
    // The cores with the highest top clock (the Pocket 5's prime core 7)
    long best = 0;
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
      if (std::fscanf(f, "%ld", &khz) == 1 && khz >= best)
      {
        if (khz > best)
          CPU_ZERO(&set);
        best = khz;
        CPU_SET(cpu, &set);
      }
      std::fclose(f);
    }
    if (best == 0)
      return;
  }
  else
    CPU_SET(std::atoi(want.c_str()), &set);
  std::string cpus;
  for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
    if (CPU_ISSET(cpu, &set))
      cpus += (cpus.empty() ? "" : ",") + std::to_string(cpu);
  const int rc = sched_setaffinity(0, sizeof(set), &set);
  Log("[host] emulation thread on cpu %s: %s\n", cpus.c_str(), rc == 0 ? "ok" : "failed");
#endif
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
  if (s_stop_cycles && g_cycles >= s_stop_cycles)
    RequestQuit();
  if (VideoQuitRequested())
    RequestQuit();
  s_last_wake = now;
  if (!s_throttle)
    return;
  const auto target = s_wall_start + std::chrono::nanoseconds(static_cast<u64>(
                                         static_cast<double>(g_cycles) * 1e9 / CPU_HZ));
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
  }
  else if (now - target > std::chrono::milliseconds(200))
    s_wall_start += now - target - std::chrono::milliseconds(200);  // don't race to catch up
}
}  // namespace host

#if defined(__ANDROID__) && defined(SKIES_STANDALONE_AURORA)
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_system.h>

#include <fstream>
#include <vector>

// The Android shell (standalone/android): SDLActivity calls main (SDL_main) without arguments. They come
// from args.txt in the app's external files folder, one per line ($FILES in them is that folder), the
// environment from env.txt there (NAME=VALUE lines), and the log (stderr) goes to log.txt there.
namespace
{
std::vector<std::string> s_android_args;
std::vector<char*> s_android_argv;

void AndroidSetup(int& argc, char**& argv)
{
  const char* dir = SDL_GetAndroidExternalStoragePath();
  if (!dir)
    return;
  const std::string files = dir;
  std::freopen((files + "/log.txt").c_str(), "w", stderr);
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
  lines("env.txt", [](const std::string& line) {
    if (const size_t eq = line.find('='); eq != std::string::npos)
      setenv(line.substr(0, eq).c_str(), line.substr(eq + 1).c_str(), 1);
  });
  if (argc > 1)
    return;
  s_android_args.push_back("skies-standalone");
  lines("args.txt", [](const std::string& line) { s_android_args.push_back(line); });
  for (auto& arg : s_android_args)
    s_android_argv.push_back(arg.data());
  s_android_argv.push_back(nullptr);
  argc = static_cast<int>(s_android_args.size());
  argv = s_android_argv.data();
}
}  // namespace
#endif

int main(int argc, char** argv)
{
  using namespace host;
#if defined(__ANDROID__) && defined(SKIES_STANDALONE_AURORA)
  AndroidSetup(argc, argv);
#endif
  setvbuf(stderr, nullptr, _IONBF, 0);
  std::string game, module, dol, user, load_state;
  std::string dsp_coef = SKIES_DSP_COEF_DEFAULT;
  bool headless = false;
  u32 idle_pc = ~0u;
  double seconds = 0;
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
      seconds = std::atof(next());
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
    else if (a == "--load-state")
      load_state = next();
    else
    {
      Log("usage: %s --game DIR --module FILE [--headless] [--unthrottled] [--seconds N] "
          "[--mmio-log] [--idle-pc ADDR] [--dsp-coef FILE] [--user USERDIR] [--dol FILE] "
          "[--load-state FILE]\n",
          argv[0]);
      return 2;
    }
  }
  if (game.empty() || module.empty())
  {
    Log("--game and --module are required\n");
    return 2;
  }

  ExiSetUserDir(user);
  SettingsInit(user);
  StateSetUserDir(user);
  SchedInit();
  CpuInit();
  if (!LoadModule(module.c_str()) || !DiscOpen(game, dol))
    return 1;
  const std::string disc_id(reinterpret_cast<const char*>(DiscHeader()), 6);
  if (disc_id != ModuleGameId())
    Log("[host] warning: disc %s, module built for %s\n", disc_id.c_str(), ModuleGameId());
  if (!Boot(game) || !VerifyModule())
    return 1;
  // The SDK scheduler's idle spin (release/GEAE8P.ini StaticRecompIdlePC)
  if (idle_pc == ~0u)
    idle_pc = disc_id == "GEAE8P" ? 0x80237BA8u : 0;
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
  if (seconds > 0)
    s_stop_cycles = static_cast<u64>(seconds * CPU_HZ);

  s_wall_start = s_last_report_wall = std::chrono::steady_clock::now();
  if (!load_state.empty())
    StateLoadAtStart(load_state);
  RunCpu();
  Report();
  ExiShutdown();
  VideoShutdown();
  return 0;
}
