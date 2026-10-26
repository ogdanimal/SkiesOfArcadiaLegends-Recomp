// Audio interface: the control register (sample rates, the streaming sample counter and its
// interrupt) as AudioInterface.cpp. Disc streaming itself is off (DTK), so the counter only
// counts time while PSTAT is set.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "dolphin_state.h"
#include "host.h"
#include "state.h"

namespace host
{
namespace
{
enum : u32
{
  AICR_PSTAT = 1u << 0,
  AICR_AISFR = 1u << 1,  // streaming rate: 1 = 48 kHz
  AICR_AIINTMSK = 1u << 2,
  AICR_AIINT = 1u << 3,
  AICR_AIINTVLD = 1u << 4,
  AICR_SCRESET = 1u << 5,
  AICR_AIDFR = 1u << 6,  // DSP rate: 1 = 32 kHz
};
u32 s_control = AICR_AISFR | AICR_AIDFR;
u32 s_volume = 0;
u32 s_sample_counter = 0;
u32 s_interrupt_timing = 0;
u64 s_last_cycles = 0;

u64 CyclesPerSample()
{
  return CPU_HZ / ((s_control & AICR_AISFR) ? 48000 : 32000);
}

u32 Counter()
{
  if (!(s_control & AICR_PSTAT))
    return s_sample_counter;
  return s_sample_counter + static_cast<u32>((Now() - s_last_cycles) / CyclesPerSample());
}

void UpdateInterrupts()
{
  PiSetInterrupt(PI_INT_AI, (s_control & AICR_AIINT) && (s_control & AICR_AIINTMSK));
}

void Tick(u64)
{
  if (!(s_control & AICR_PSTAT))
    return;
  const u32 now = Counter();
  if ((s_control & AICR_AIINTVLD) && s_sample_counter < s_interrupt_timing && now >= s_interrupt_timing)
  {
    s_control |= AICR_AIINT;
    UpdateInterrupts();
  }
  s_sample_counter = now;
  s_last_cycles = Now();
  Schedule(EV_AI, CyclesPerSample() * 64, Tick);
}
}  // namespace

void AiInit()
{
  s_control = AICR_AISFR | AICR_AIDFR;
  s_volume = 0;
  s_sample_counter = s_interrupt_timing = 0;
  s_last_cycles = 0;
  SchedRegister(EV_AI, Tick);
}

void AiDoState(StateIO& p)
{
  p.Do(s_control);
  p.Do(s_volume);
  p.Do(s_sample_counter);
  p.Do(s_interrupt_timing);
  p.Do(s_last_cycles);
}

void AiFromDolphin(const DolphinState& d)
{
  s_control = static_cast<u32>(d.U("hw/ai/control"));
  s_volume = static_cast<u32>(d.U("hw/ai/volume"));
  s_sample_counter = static_cast<u32>(d.U("hw/ai/sample_counter"));
  s_interrupt_timing = static_cast<u32>(d.U("hw/ai/interrupt_timing"));
  s_last_cycles = d.U("hw/ai/last_cpu_time");
  UpdateInterrupts();
}

u32 AiDspSampleRate()
{
  return (s_control & AICR_AIDFR) ? 32000 : 48000;
}

u32 AiRead32(u32 offset)
{
  switch (offset)
  {
  case 0x00:
    return s_control;
  case 0x04:
    return s_volume;
  case 0x08:
    return Counter();
  case 0x0C:
    return s_interrupt_timing;
  default:
    return 0;
  }
}

void AiWrite32(u32 offset, u32 value)
{
  switch (offset)
  {
  case 0x00:
  {
    const u32 keep = AICR_AIINT;
    const u32 old = s_control;
    s_control = (s_control & keep) | (value & ~(keep | AICR_SCRESET));
    if (value & AICR_AIINT)
      s_control &= ~AICR_AIINT;
    if ((old ^ s_control) & AICR_PSTAT)
    {
      if (old & AICR_PSTAT)
        s_sample_counter = static_cast<u32>(s_sample_counter + (Now() - s_last_cycles) / CyclesPerSample());
      s_last_cycles = Now();
      Deschedule(EV_AI);
      if (s_control & AICR_PSTAT)
        Schedule(EV_AI, CyclesPerSample() * 64, Tick);
    }
    if (value & AICR_SCRESET)
    {
      s_sample_counter = 0;
      s_last_cycles = Now();
    }
    UpdateInterrupts();
    return;
  }
  case 0x04:
    s_volume = value;
    return;
  case 0x08:
    s_sample_counter = value;
    s_last_cycles = Now();
    return;
  case 0x0C:
    s_interrupt_timing = value;
    return;
  default:
    return;
  }
}
}  // namespace host
