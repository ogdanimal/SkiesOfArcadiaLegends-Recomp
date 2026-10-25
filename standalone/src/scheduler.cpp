// Event scheduler in Gekko core cycles. A handful of fixed event slots: the run loop compares the
// clock against one cached deadline per dispatch and scans the slots only when something is due.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

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

void Schedule(EventId id, u64 cycles_from_now, EventFn fn)
{
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
}  // namespace host
