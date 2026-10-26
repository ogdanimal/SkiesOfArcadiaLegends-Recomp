// Event scheduler in Gekko core cycles. A handful of fixed event slots: the run loop compares the
// clock against one cached deadline per dispatch and scans the slots only when something is due.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "dolphin_state.h"
#include "host.h"
#include "state.h"

#include <limits>

namespace host
{
u64 g_cycles = 0;
u64 g_next_event = std::numeric_limits<u64>::max();

namespace
{
struct Slot
{
  u64 when = std::numeric_limits<u64>::max();
  EventFn fn = nullptr;
};
Slot s_slots[EV_COUNT];
// Each slot's handler (one per slot), for slots a loaded state has pending
EventFn s_known[EV_COUNT];

void Recompute()
{
  u64 next = std::numeric_limits<u64>::max();
  for (const Slot& s : s_slots)
    next = s.when < next ? s.when : next;
  g_next_event = next;
}
}  // namespace

void SchedInit()
{
  for (Slot& s : s_slots)
    s = {};
  g_cycles = 0;
  g_next_event = std::numeric_limits<u64>::max();
}

u64 Now()
{
  return g_cycles;
}

void SchedRegister(EventId id, EventFn fn)
{
  s_known[id] = fn;
}

void Schedule(EventId id, u64 cycles_from_now, EventFn fn)
{
  s_known[id] = fn;
  s_slots[id].when = g_cycles + cycles_from_now;
  s_slots[id].fn = fn;
  if (s_slots[id].when < g_next_event)
    g_next_event = s_slots[id].when;
}

void Deschedule(EventId id)
{
  if (s_slots[id].when == std::numeric_limits<u64>::max())
    return;
  s_slots[id].when = std::numeric_limits<u64>::max();
  Recompute();
}

bool IsScheduled(EventId id)
{
  return s_slots[id].when != std::numeric_limits<u64>::max();
}

// The clock and when each slot is due; handlers are this run's (SchedRegister).
void SchedDoState(StateIO& p)
{
  p.Do(g_cycles);
  for (int i = 0; i < EV_COUNT; ++i)
  {
    u64 when = s_slots[i].when;
    p.Do(when);
    if (!p.Reading())
      continue;
    s_slots[i].when = when;
    s_slots[i].fn = s_known[i];
    if (when != std::numeric_limits<u64>::max() && !s_known[i])
      p.Fail("event " + std::to_string(i) + " is pending but has no handler");
  }
  if (p.Reading())
    Recompute();
}

void RunDue()
{
  while (g_next_event <= g_cycles)
  {
    // Fire the earliest due slot; its handler may reschedule itself or others.
    int best = -1;
    for (int i = 0; i < EV_COUNT; ++i)
    {
      if (s_slots[i].when <= g_cycles && (best < 0 || s_slots[i].when < s_slots[best].when))
        best = i;
    }
    if (best < 0)
    {
      Recompute();
      break;
    }
    const u64 late = g_cycles - s_slots[best].when;
    EventFn fn = s_slots[best].fn;
    s_slots[best].when = std::numeric_limits<u64>::max();
    Recompute();
    if (fn)
      fn(late);
  }
}

// A Dolphin state: its clock, and its events in the slots that do the same here (the parts that keep
// more than a time for an event take it themselves: the DSP's delayed interrupts, a disc read in
// flight). This run's wall-clock pacing and memory card (whose writes the state doesn't hold) keep
// their own.
void SchedFromDolphin(const DolphinState& d)
{
  g_cycles = d.Now();
  for (int i = 0; i < EV_COUNT; ++i)
  {
    if (i != EV_THROTTLE && i != EV_CARD_CMD && i != EV_CARD_XFER && i != EV_CARD_FLUSH)
      s_slots[i].when = std::numeric_limits<u64>::max();
  }
  const auto slot_for = [](const DolphinState::Event& e) -> int {
    if (e.name == "VICallback")
      return EV_VI;
    if (e.name == "DecCallback")
      return EV_DEC;
    if (e.name == "DSPCallback")
      return EV_DSP;
    if (e.name == "AudioDMACallback")
      return EV_AUDIO_DMA;
    if (e.name == "AICallback")
      return EV_AI;
    if (e.name == "ARAMint")
      return EV_ARAM;
    if (e.name == "SITransferPending")
      return EV_SI;
    // A disc command's completion; the drive's streaming audio (DTK, reply type 3) isn't modelled
    if (e.name == "FinishExecutingCommand" && (e.userdata >> 32) != 3)
      return EV_DI;
    return -1;
  };
  for (const DolphinState::Event& e : d.Events())
  {
    const int slot = slot_for(e);
    if (slot < 0)
      continue;
    if (!s_known[slot])
      throw dsr::ParseError("no handler here for the state's " + e.name);
    s_slots[slot].when = e.time;
    s_slots[slot].fn = s_known[slot];
  }
  Recompute();
}
}  // namespace host
