// skies-standalone: the recompiled game on this host, no emulator core.
//   skies-standalone --game DIR --module FILE [--headless] [--unthrottled] [--seconds N]
//                    [--mmio-log] [--idle-pc ADDR] [--dsp-coef FILE] [--user USERDIR]
// DIR is an extracted disc (sys/ + files/); FILE the g<GAMEID>_recomp module built for its DOL.
// USERDIR holds the memory card saves as the Dolphin build keeps them (GC/<region>/Card A/*.gci);
// without it no card is inserted.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

#include "dsphle/ax.h"
#include "dsphle/dsp_hle.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

namespace host
{
void GpuStats(u64* frames, u64* draw_dones, u64* tokens);  // hw_gpu.cpp
void DiStats(u64* reads, u64* bytes);                        // hw_di.cpp

namespace
{
bool s_throttle = true;
u64 s_stop_cycles = 0;
std::chrono::steady_clock::time_point s_wall_start;
u64 s_last_report_cycles = 0;
std::chrono::steady_clock::time_point s_last_report_wall;

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

// Called by the VI at every field: pace guest time to the wall clock, stop at --seconds.
void Throttle()
{
  const auto now = std::chrono::steady_clock::now();
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
  if (!s_throttle)
    return;
  const auto target = s_wall_start + std::chrono::nanoseconds(static_cast<u64>(
                                         static_cast<double>(g_cycles) * 1e9 / CPU_HZ));
  if (target > now + std::chrono::milliseconds(1))
    std::this_thread::sleep_until(target);
  else if (now - target > std::chrono::milliseconds(200))
    s_wall_start += now - target - std::chrono::milliseconds(200);  // don't race to catch up
}
}  // namespace host

int main(int argc, char** argv)
{
  using namespace host;
  setvbuf(stderr, nullptr, _IONBF, 0);
  std::string game, module;
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
    else if (a == "--user")
      ExiSetUserDir(next());
    else
    {
      Log("usage: %s --game DIR --module FILE [--headless] [--unthrottled] [--seconds N] "
          "[--mmio-log] [--idle-pc ADDR] [--dsp-coef FILE] [--user USERDIR]\n",
          argv[0]);
      return 2;
    }
  }
  if (game.empty() || module.empty())
  {
    Log("--game and --module are required\n");
    return 2;
  }

  SchedInit();
  CpuInit();
  if (!LoadModule(module.c_str()) || !DiscOpen(game))
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
  if (!VideoInit(headless))
    return 1;
  if (seconds > 0)
    s_stop_cycles = static_cast<u64>(seconds * CPU_HZ);

  s_wall_start = s_last_report_wall = std::chrono::steady_clock::now();
  RunCpu();
  Report();
  ExiShutdown();
  VideoShutdown();
  return 0;
}
