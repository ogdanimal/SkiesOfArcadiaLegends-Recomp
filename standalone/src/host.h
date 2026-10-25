// The standalone host: shared state and the interfaces between its parts.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Hardware models follow Dolphin's (GPL-2.0-or-later; HW/*.cpp, VideoCommon/CommandProcessor.cpp,
// PixelEngine.cpp), cut down to what a retail game drives.
#pragma once

#include "core/cpu.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>

namespace host
{
// ---- clocks ------------------------------------------------------------------------------------
constexpr u64 CPU_HZ = 486000000;  // Gekko core clock; all event times are in these cycles
constexpr u32 TIMER_RATIO = 12;    // core cycles per time base tick (40.5 MHz)

// ---- logging -----------------------------------------------------------------------------------
extern bool g_log_mmio;
void Log(const char* fmt, ...);
[[noreturn]] void Fatal(const char* fmt, ...);

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
void Schedule(EventId id, u64 cycles_from_now, EventFn fn);
void Deschedule(EventId id);
bool IsScheduled(EventId id);
u64 Now();
// The run loop's clock: Advance() moves time by dispatched cycles; RunDue() fires due events.
extern u64 g_cycles;
extern u64 g_next_event;
void RunDue();

// ---- CPU (cpu.cpp) -----------------------------------------------------------------------------
bool LoadModule(const char* path);
const char* ModuleGameId();
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

// ---- disc (disc.cpp) ---------------------------------------------------------------------------
bool DiscOpen(const std::string& game_dir);
bool DiscRead(u64 offset, void* out, u32 length);
const u8* DiscHeader();  // boot.bin (0x440 bytes)

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
}  // namespace host
