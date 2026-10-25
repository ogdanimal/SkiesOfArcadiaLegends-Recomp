// The standalone host: shared state and the interfaces between its parts.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Hardware models follow Dolphin's (GPL-2.0-or-later; HW/*.cpp, VideoCommon/CommandProcessor.cpp,
// PixelEngine.cpp), cut down to what a retail game drives.
#pragma once

#include "core/cpu.h"

#include <cstdarg>
#include <cstdint>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace host
{
// ---- clocks ------------------------------------------------------------------------------------
constexpr u64 CPU_HZ = 486000000;  // Gekko core clock; all event times are in these cycles
constexpr u32 TIMER_RATIO = 12;    // core cycles per time base tick (40.5 MHz)

// ---- logging -----------------------------------------------------------------------------------
extern bool g_log_mmio;
void Log(const char* fmt, ...);
[[noreturn]] void Fatal(const char* fmt, ...);

// ---- frame timing (SKIES_PRESENT_PACING=1, or the Dolphin build's RECOMP_PRESENT_PACING=1) -----
// Every 600 values a line "[TAG] WHAT ms over 600: p50 p90 p99 max; >A ms N, >B ms N", the Dolphin
// build's [present-pacing] format.
bool PacingOn();
struct PacingWindow
{
  double values[600];
  unsigned count = 0;
};
void PacingAdd(PacingWindow& window, double ms, const char* tag, const char* what, double over_a, double over_b);

// ---- guest memory ------------------------------------------------------------------------------
constexpr u32 RAM_SIZE = 0x01800000;  // 24 MB MEM1
extern u8* g_ram;
extern CPUState g_cpu;

inline u8* RamPtr(u32 address, u32 size)
{
  const u32 physical = address & 0x1FFFFFFFu;
  return physical + size <= RAM_SIZE ? g_ram + physical : nullptr;
}
u32 Read32(u32 address);
u16 Read16(u32 address);
u8 Read8(u32 address);
void Write32(u32 address, u32 value);
void Write16(u32 address, u16 value);
void Write8(u32 address, u8 value);
inline u32 BE32(const u8* p)
{
  return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | p[3];
}
inline u16 BE16(const u8* p)
{
  return u16((p[0] << 8) | p[1]);
}

// ---- scheduler (scheduler.cpp) -------------------------------------------------------------------
enum EventId
{
  EV_VI,          // next VI half-line
  EV_DEC,         // decrementer underflow
  EV_DI,          // disc command completion
  EV_DSP,         // DSP HLE: 1 ms update
  EV_DSP_INT,     // DSP interrupts the HLE raised with a delay
  EV_ARAM,        // ARAM DMA completion
  EV_AUDIO_DMA,   // AI DMA block
  EV_AI,          // AI sample counter interrupt
  EV_SI,          // SI transfer completion
  EV_EXI,         // EXI transfer completion
  EV_CARD_CMD,    // memory card: erase / program done
  EV_CARD_XFER,   // memory card: DMA transfer complete
  EV_CARD_FLUSH,  // memory card: write changed saves back to the GCI folder
  EV_THROTTLE,    // wall-clock pacing
  EV_COUNT,
};
using EventFn = void (*)(u64 late);
void SchedInit();
// Each slot has one handler; registering it at init lets a loaded state's pending events fire.
void SchedRegister(EventId id, EventFn fn);
void Schedule(EventId id, u64 cycles_from_now, EventFn fn);
void Deschedule(EventId id);
bool IsScheduled(EventId id);
u64 Now();
std::chrono::steady_clock::time_point WallForGuest(u64 cycles);  // {} unthrottled
// The run loop's clock: Advance() moves time by dispatched cycles; RunDue() fires due events.
extern u64 g_cycles;
extern u64 g_next_event;
void RunDue();

// ---- CPU (cpu.cpp) -----------------------------------------------------------------------------
bool LoadModule(const char* path);
const char* ModuleGameId();
bool VerifyModule();  // after Boot: the module's chunk hashes against the DOL in RAM
void CpuInit();
void RunCpu();          // until quit
void RequestQuit();
bool QuitRequested();
void SetIdlePc(u32 pc);
u32 GetSpr(u16 spr);
void SetSpr(u16 spr, u32 value);
// Interrupt lines from the PI (level) and the decrementer (edge).
void SetExternalInterrupt(bool asserted);
void RaiseDecrementer();
void DecrementerWritten();
u32 DecrementerValue();
u8* LockedCache();  // 16 KB at 0xE0000000

// ---- boot (boot.cpp) ---------------------------------------------------------------------------
bool Boot(const std::string& game_dir);
// The real-time clock in seconds since 2000-01-01: the wall clock at boot (or SKIES_RTC, for
// repeatable runs) plus the guest time since, as Dolphin's emulated RTC.
u32 RtcSeconds();

// ---- disc (disc.cpp) ---------------------------------------------------------------------------
// dol_path: a DOL to boot instead of sys/main.dol (a game-patched one), or empty
bool DiscOpen(const std::string& game_dir, const std::string& dol_path);
bool DiscRead(u64 offset, void* out, u32 length);
const u8* DiscHeader();  // boot.bin (0x440 bytes)
const std::vector<u8>& DiscDol();  // sys/main.dol

// ---- MMIO (mmio.cpp) ---------------------------------------------------------------------------
u64 MmioRead(u32 address, u8 size);
void MmioWrite(u32 address, u64 value, u8 size);

// ---- devices -------------------------------------------------------------------------------------
// PI: interrupt cause/mask (bit numbers as Dolphin's ProcessorInterface)
enum PiCause : u32
{
  PI_INT_PI = 0x1,
  PI_INT_RSW = 0x2,
  PI_INT_DI = 0x4,
  PI_INT_SI = 0x8,
  PI_INT_EXI = 0x10,
  PI_INT_AI = 0x20,
  PI_INT_DSP = 0x40,
  PI_INT_MEM = 0x80,
  PI_INT_VI = 0x100,
  PI_INT_PE_TOKEN = 0x200,
  PI_INT_PE_FINISH = 0x400,
  PI_INT_CP = 0x800,
  PI_INT_DEBUG = 0x1000,
  PI_INT_HSP = 0x2000,
  PI_INT_RESET = 0x10000,
};
void PiInit();
void PiSetInterrupt(u32 cause, bool set);
u32 PiRead(u32 offset);
void PiWrite(u32 offset, u32 value);
// The CPU-side FIFO pointers (the gather pipe writes there).
extern u32 g_pi_fifo_base, g_pi_fifo_end, g_pi_fifo_wptr;

void ViInit();
u16 ViRead16(u32 offset);
void ViWrite16(u32 offset, u16 value);
bool ViIsPal();

// GPU: command processor, pixel engine, the gather pipe and the GX command stream into Aurora.
void GpuInit();
u64 GpuFrameCount();  // XFB copies so far
// dff.cpp (debug): SKIES_DFF frame capture as a Dolphin FIFO log
void DffInit();
void DffCommand(const u8* p, u32 len, bool top_level);
void DffFrameEnd(u64 frame);
u16 CpRead16(u32 offset);
void CpWrite16(u32 offset, u16 value);
u16 PeRead16(u32 offset);
void PeWrite16(u32 offset, u16 value);
void GatherPipeFlush(CPUState* cpu);
void GatherPipeInstall(CPUState* cpu);
u32 EfbRead(u32 address);

void DspInit();
u16 DspRead16(u32 offset);
void DspWrite16(u32 offset, u16 value);
u8* Aram();
constexpr u32 ARAM_SIZE = 0x01000000;

void DiInit();
u32 DiRead32(u32 offset);
void DiWrite32(u32 offset, u32 value);

void SiInit();
u32 SiRead32(u32 offset);
void SiWrite32(u32 offset, u32 value);
void SiPollDevices();

// EXI with the IPL (SRAM, RTC, UART) and a memory card in slot A over USER/GC/<region>/Card A
// (no card when no user folder is set).
void ExiSetUserDir(const std::string& user_dir);
void ExiInit();
void ExiShutdown();  // flushes the memory card
u32 ExiRead32(u32 offset);
void ExiWrite32(u32 offset, u32 value);

void AiInit();
u32 AiRead32(u32 offset);
void AiWrite32(u32 offset, u32 value);
void AiSetSampleRateFromDsp();
u32 AiDspSampleRate();

// ---- video (video.cpp) ---------------------------------------------------------------------------
bool VideoInit(bool headless);
void VideoShutdown();
void VideoCommands(const u8* data, u32 size);  // whole GP commands, top level
void VideoFrameEnd();                         // the game's display copy (EFB to XFB)
void VideoRetrace();                          // a VI field
bool VideoQuitRequested();
void VideoPadRead(u16 buttons[4], s8 sticks[4][4], u8 triggers[4][2], bool connected[4]);
// The window's surface in pixels (false: no window; either pointer may be null)
bool VideoSurfaceSize(unsigned* width, unsigned* height);

// ---- settings (settings.cpp) -----------------------------------------------------------------------
// The Dolphin build's Config/Dolphin.ini and Config/GFX.ini in the user folder (read at start,
// changed lines written back by SettingsSave).
enum class SettingFile
{
  Main,
  Gfx,
};
void SettingsInit(const std::string& user_dir);
int SettingGet(SettingFile file, const char* section, const char* key, int fallback);
void SettingSet(SettingFile file, const char* section, const char* key, int value, bool boolean = false);
void SettingsSave();

// ---- the in-game menu and the player options (menu.cpp) --------------------------------------------
// A GameCube controller's state as the SI sends it (sticks centred at 0x80).
struct PadStatus
{
  bool connected = false;
  u16 buttons = 0;
  u8 stick_x = 0x80, stick_y = 0x80, sub_x = 0x80, sub_y = 0x80, trig_l = 0, trig_r = 0;
};
// Applies the picture settings to the game patch's config words (before the window opens, and at
// every frame end).
void MenuApplyGame();
// Controller 1 at each SI poll: the menu (Z+Start, Esc, a controller's Back) takes the input while
// it's open; the camera, speed and QOL options read it while it's closed.
void MenuFilterPad(PadStatus& pad);
// Before each present: the menu, messages and the FPS counter through Aurora's overlay.
void MenuFrame();
void MenuMessage(const std::string& text, int milliseconds);
int MenuVolume();  // percent
// The emulated CPU's clock factor for the speed options (1 = the Gekko's; cpu.cpp divides the
// cycles it charges by it)
extern float g_overclock;
// Guest time over wall time for the last half second (1 = full speed; main.cpp)
double EmulationSpeed();
// After a state load moved the guest clock: wall-clock pacing restarts from now (main.cpp)
void ThrottleRestart();

// ---- save states (state.cpp) -------------------------------------------------------------------
// The whole machine in one file, taken and put back between two dispatches of the run loop, after
// a frame's display copy (the picture is complete and the next frame not started).
class StateIO;
void StateSetUserDir(const std::string& user_dir);
// USER/StateSaves/Standalone (empty without --user)
std::string StateDirectory();
// Done at the next frame end; the message is shown once it worked.
void StateRequestSave(const std::string& path, const std::string& message);
void StateRequestLoad(const std::string& path, const std::string& message);
// The command line's --load-state: at the game's first frame end, as the menu's loads are; a state
// that can't be loaded ends the run
void StateLoadAtStart(const std::string& path);
extern bool g_state_request;  // the run loop calls StateRun while set
void StateRun();
void StateFrameEnd();  // hw_gpu.cpp, after each display copy
// The parts' state (each file's own), in the order state.cpp runs them
void SchedDoState(StateIO& p);
void CpuDoState(StateIO& p);
void PiDoState(StateIO& p);
void ViDoState(StateIO& p);
void GpuDoState(StateIO& p);
void DffDoState(StateIO& p);  // the GP registers Aurora has (BP, CP, XF, TLUT loads)
void DffReloadAurora();       // after a load: those registers to Aurora again
void DspDoState(StateIO& p);  // and ARAM, the DSP HLE
void DiDoState(StateIO& p);
void SiDoState(StateIO& p);
void ExiDoState(StateIO& p);  // and the memory card
void AiDoState(StateIO& p);
void VideoDoState(StateIO& p);  // and Aurora's EFB copies
void VideoStateLoaded();        // after a load: the state's EFB copies to Aurora
u64 ModuleFingerprint();  // the recompiled module's chunk hashes (a state needs the same module)
double CardSecondsSinceLastAccess();  // guest seconds; auto-saves wait for a quiet card
}  // namespace host
