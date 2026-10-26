// Video interface: the register file, the half-line beam clock and its display interrupts
// (VideoInterface.cpp: Preset, UpdateParameters, Update, UpdateInterrupts).
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"
#include "state.h"

#include <cstring>

namespace host
{
void Throttle();  // main.cpp

namespace
{
u16 s_regs[0x40];  // 16-bit registers, offset / 2
u32 s_half_line = 0;
u64 s_line_start = 0;
u32 s_odd_first = 0, s_odd_last = 0, s_even_first = 0, s_even_last = 0;
bool s_pal = false;

u16& Reg(u32 offset)
{
  return s_regs[(offset >> 1) & 0x3F];
}
u32 Reg32(u32 hi_offset)
{
  return (u32(Reg(hi_offset)) << 16) | Reg(hi_offset + 2);
}

u32 Equ() { return Reg(0x00) & 0xF; }
u32 Acv() { return (Reg(0x00) >> 4) & 0x3FF; }
u32 Hlw() { return Reg(0x06) & 0x3FF; }
u32 PrbOdd() { return Reg(0x0E) & 0x3FF; }
u32 PsbOdd() { return Reg(0x0C) & 0x3FF; }
u32 PrbEven() { return Reg(0x12) & 0x3FF; }
u32 PsbEven() { return Reg(0x10) & 0x3FF; }

u32 HalfLinesOdd() { return 3 * Equ() + PrbOdd() + 2 * Acv() + PsbOdd(); }
u32 HalfLinesEven() { return 3 * Equ() + PrbEven() + 2 * Acv() + PsbEven(); }

u64 TicksPerHalfLine()
{
  const u64 clock = (Reg(0x6C) & 1) ? 54000000 : 27000000;
  const u64 per_sample = 2 * CPU_HZ / clock;
  const u32 hlw = Hlw() ? Hlw() : 429;
  return per_sample * hlw;
}

void UpdateParameters()
{
  const u32 equ_hl = 3 * Equ();
  const u32 acv_hl = 2 * Acv();
  s_odd_first = equ_hl + PrbOdd();
  const u32 odd_end = s_odd_first + acv_hl;
  s_odd_last = odd_end - 1;
  const s32 psb_diff = static_cast<s32>(PsbOdd()) - static_cast<s32>(PsbEven());
  s_even_first = odd_end + PsbOdd() + equ_hl + PrbEven() - psb_diff;
  s_even_last = s_even_first + acv_hl - 1;
}

void UpdateInterrupts()
{
  bool any = false;
  for (u32 i = 0; i < 4; ++i)
  {
    const u16 hi = Reg(0x30 + 4 * i);
    if ((hi & 0x8000) && (hi & 0x1000))
      any = true;
  }
  PiSetInterrupt(PI_INT_VI, any);
}

void HalfLine(u64 late)
{
  const u32 total = HalfLinesOdd() + HalfLinesEven();
  const bool field_boundary = s_half_line == 0 || s_half_line == HalfLinesOdd();
  if (field_boundary)
  {
    SiPollDevices();
    VideoRetrace();
    Throttle();
  }

  ++s_half_line;
  if (s_half_line >= total)
    s_half_line = 0;
  if (!(s_half_line & 1))
    s_line_start = Now() - late;

  for (u32 i = 0; i < 4; ++i)
  {
    u16& hi = Reg(0x30 + 4 * i);
    const u16 lo = Reg(0x32 + 4 * i);
    const u32 vct = hi & 0x7FF;
    const u32 hct = lo & 0x7FF;
    const u32 target_halfline = hct > Hlw() ? 1 : 0;
    if (1 + s_half_line / 2 == vct && (s_half_line & 1) == target_halfline)
      hi |= 0x8000;
  }
  UpdateInterrupts();

  const u64 period = TicksPerHalfLine();
  Schedule(EV_VI, period > late ? period - late : 1, HalfLine);
}
}  // namespace

void ViDoState(StateIO& p)
{
  p.DoArray(s_regs, 0x40);
  p.Do(s_half_line);
  p.Do(s_line_start);
  if (p.Reading())
    UpdateParameters();
}

bool ViIsPal()
{
  return s_pal;
}

void ViInit()
{
  std::memset(s_regs, 0, sizeof(s_regs));
  s_pal = !DiscRunsAsNtsc();
  // Preset(): what the IPL leaves
  Reg(0x00) = 6;                    // EQU 6, ACV 0
  Reg(0x02) = s_pal ? 0x0101 : 1;   // ENB, FMT
  Reg(0x04) = (71 << 8) | 105;      // HCS, HCE
  Reg(0x06) = 429;                  // HLW
  Reg(0x08) = (373 >> 1) << 0;      // HTR1: kept approximate (not used for timing)
  Reg(0x0E) = 502;
  Reg(0x0C) = 5;
  Reg(0x12) = 503;
  Reg(0x10) = 4;
  Reg(0x30) = 0x1000 | 263;  // DI0: line 263, enabled
  Reg(0x32) = 430;
  Reg(0x34) = 0x1000 | 1;  // DI1: line 1, enabled
  Reg(0x36) = 1;
  Reg(0x6C) = s_pal ? 0 : 1;  // 54 MHz on NTSC (progressive capable)
  s_half_line = 0;
  s_line_start = 0;
  UpdateParameters();
  Schedule(EV_VI, TicksPerHalfLine(), HalfLine);
}

u16 ViRead16(u32 offset)
{
  switch (offset)
  {
  case 0x2C:  // vertical beam position
    return static_cast<u16>(1 + s_half_line / 2);
  case 0x2E:  // horizontal beam position
  {
    const u64 per_half = TicksPerHalfLine();
    u64 into = Now() - s_line_start;
    u32 h = static_cast<u32>(1 + Hlw() * into / (per_half ? per_half : 1));
    if (h > 2 * Hlw())
      h = 2 * Hlw();
    return static_cast<u16>(h);
  }
  case 0x6E:  // DTV status: no component cable
    return 0;
  default:
    return Reg(offset);
  }
}

void ViWrite16(u32 offset, u16 value)
{
  Reg(offset) = value;
  switch (offset)
  {
  case 0x00:
  case 0x0C:
  case 0x0E:
  case 0x10:
  case 0x12:
  case 0x06:
  case 0x6C:
    UpdateParameters();
    break;
  case 0x02:
    if (value & 2)  // RST: the interrupt registers go back to zero
    {
      for (u32 i = 0; i < 4; ++i)
        Reg(0x30 + 4 * i) = Reg(0x32 + 4 * i) = 0;
    }
    UpdateInterrupts();
    break;
  case 0x30:
  case 0x34:
  case 0x38:
  case 0x3C:
    UpdateInterrupts();
    break;
  default:
    break;
  }
}

// The frame buffer the VI scans out (top field), for presentation diagnostics.
u32 ViXfbAddress()
{
  const u32 top = Reg32(0x1C);
  const u32 fbb = top & 0x00FFFFFF;
  return (top & 0x10000000) ? fbb << 5 : fbb;
}
}  // namespace host
