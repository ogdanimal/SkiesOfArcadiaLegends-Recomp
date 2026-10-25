// Processor interface: the interrupt cause/mask pair that drives the CPU's external interrupt
// line, and the CPU-side FIFO pointers the gather pipe writes through (ProcessorInterface.cpp).
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"
#include "state.h"

namespace host
{
u32 g_pi_fifo_base = 0, g_pi_fifo_end = 0, g_pi_fifo_wptr = 0;

namespace
{
u32 s_cause = 0;
u32 s_mask = 0;
u32 s_reset_code = 0;
constexpr u32 FLIPPER_REV_C = 0x246500B1;

void Update()
{
  SetExternalInterrupt((s_cause & s_mask) != 0);
}
}  // namespace

void PiInit()
{
  s_cause = PI_INT_RESET;  // the reset switch reads as not pressed
  s_mask = 0;
  s_reset_code = 0;
  g_pi_fifo_base = g_pi_fifo_end = g_pi_fifo_wptr = 0;
  Update();
}

void PiDoState(StateIO& p)
{
  p.Do(s_cause);
  p.Do(s_mask);
  p.Do(s_reset_code);
  p.Do(g_pi_fifo_base);
  p.Do(g_pi_fifo_end);
  p.Do(g_pi_fifo_wptr);
  if (p.Reading())
    Update();
}

void PiSetInterrupt(u32 cause, bool set)
{
  if (set)
    s_cause |= cause;
  else
    s_cause &= ~cause;
  Update();
}

u32 PiRead(u32 offset)
{
  switch (offset & ~3u)
  {
  case 0x00:
    return s_cause;
  case 0x04:
    return s_mask;
  case 0x0C:
    return g_pi_fifo_base;
  case 0x10:
    return g_pi_fifo_end;
  case 0x14:
    return g_pi_fifo_wptr;
  case 0x24:
    return s_reset_code;
  case 0x2C:
    return FLIPPER_REV_C;
  default:
    Log("[pi] read %03x\n", offset);
    return 0;
  }
}

void PiWrite(u32 offset, u32 value)
{
  switch (offset & ~3u)
  {
  case 0x00:
    s_cause &= ~value;
    Update();
    return;
  case 0x04:
    s_mask = value;
    Update();
    return;
  case 0x0C:
    g_pi_fifo_base = value & 0xFFFFFFE0u;
    return;
  case 0x10:
    g_pi_fifo_end = value & 0xFFFFFFE0u;
    return;
  case 0x14:
    g_pi_fifo_wptr = value & 0xFFFFFFE0u;
    return;
  case 0x18:
    // PI_FIFO_RESET (GXAbortFrame): drop what the gather pipe holds
    if (value & 1)
    {
      if (g_cpu.gather_pipe_ptr_ref)
        *g_cpu.gather_pipe_ptr_ref = g_cpu.gather_pipe_base;
    }
    return;
  case 0x24:
    s_reset_code = value;
    return;
  default:
    Log("[pi] write %03x = %08x\n", offset, value);
    return;
  }
}
}  // namespace host
