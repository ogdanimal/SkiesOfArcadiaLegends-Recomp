// The state the IPL and the disc's apploader leave before the DOL's entry point: Dolphin's BS2
// emulation (Boot_BS2Emu.cpp SetupGCMemory / EmulatedBS2_GC) plus what the apploader itself
// loads (the DOL's sections, bi2 and the FST at the top of MEM1, their pointers in low memory).
// The apploader is not run: this host has no interpreter for its code.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

namespace host
{
namespace
{
// From sys/main.dol, not the disc's DOL slot: a game-patched DOL is longer than the slot.
bool LoadDol()
{
  const std::vector<u8>& dol = DiscDol();
  if (dol.size() < 0x100)
    return false;
  const u8* header = dol.data();
  // 7 text + 11 data sections: offsets, addresses, sizes; then bss address/size, entry point
  for (u32 i = 0; i < 18; ++i)
  {
    const u32 offset = BE32(header + i * 4);
    const u32 address = BE32(header + 0x48 + i * 4);
    const u32 size = BE32(header + 0x90 + i * 4);
    if (size == 0)
      continue;
    u8* dst = RamPtr(address, size);
    if (!dst || u64(offset) + size > dol.size())
    {
      Log("[boot] DOL section %u (%08x, %u bytes) does not fit\n", i, address, size);
      return false;
    }
    std::memcpy(dst, dol.data() + offset, size);
  }
  // No BSS clearing: the header's BSS range spans .sdata between the BSS sections, and the
  // apploader leaves it to __start (RAM starts zeroed anyway).
  g_cpu.pc = BE32(header + 0xE0);
  return true;
}

u64 RtcBase()
{
  static const u64 base = []() -> u64 {
    if (const char* fixed = std::getenv("SKIES_RTC"))
      return static_cast<u64>(std::strtoull(fixed, nullptr, 0));
    return static_cast<u64>(std::time(nullptr)) - 946684800ull;
  }();
  return base;
}
}  // namespace

u32 RtcSeconds()
{
  return static_cast<u32>(RtcBase() + g_cycles / CPU_HZ);
}

bool Boot(const std::string& game_dir)
{
  const u8* disc = DiscHeader();
  const bool ntsc = disc[3] == 'E' || disc[3] == 'J';

  // The disc ID block (DVDReadDiscID: the first 0x20 bytes of the disc)
  std::memcpy(g_ram, disc, 0x20);

  // SetupGCMemory
  Write32(0x80000020, 0x0D15EA5E);  // booted from the boot ROM
  Write32(0x80000028, RAM_SIZE);
  Write32(0x8000002C, 0x10000006);  // console type: latest devkit (Dolphin's choice)
  Write32(0x800000CC, ntsc ? 0 : 1);
  Write32(0x800000D0, ARAM_SIZE);
  Write32(0x800000F8, 0x09A7EC80);  // bus clock
  Write32(0x800000FC, 0x1CF7C580);  // core clock
  Write32(0x80000300, 0x4C000064);  // default DSI, FPU and syscall handlers: rfi
  Write32(0x80000800, 0x4C000064);
  Write32(0x80000C00, 0x4C000064);

  // Time base from the real-time clock (seconds since 2000-01-01), as Dolphin presets it.
  const u64 seconds = RtcBase();
  const u64 ticks = seconds * (CPU_HZ / TIMER_RATIO);
  g_cpu.timebase = ticks;
  Write32(0x800030D8, static_cast<u32>(ticks >> 32));
  Write32(0x800030DC, static_cast<u32>(ticks));

  // The apploader's work: the DOL, then bi2 and the FST at the top of MEM1.
  const u32 fst_offset = BE32(disc + 0x424);
  const u32 fst_size = BE32(disc + 0x428);
  const u32 fst_max = BE32(disc + 0x42C);
  if (!LoadDol())
    return false;
  const u32 fst_address = (0x80000000u + RAM_SIZE - fst_max) & ~0x1Fu;
  const u32 bi2_address = (fst_address - 0x2000) & ~0x1Fu;
  if (!DiscRead(fst_offset, RamPtr(fst_address, fst_size), fst_size) ||
      !DiscRead(0x440, RamPtr(bi2_address, 0x2000), 0x2000))
    return false;
  Write32(0x80000038, fst_address);
  Write32(0x8000003C, fst_max);
  Write32(0x800000F4, bi2_address);

  // BS2's registers: stack, small data areas (the DOL's __start sets its own)
  g_cpu.gpr[1] = ntsc ? 0x81566550 : 0x815EDCA8;
  g_cpu.gpr[2] = ntsc ? 0x81465CC0 : 0x814B5B20;
  g_cpu.gpr[13] = ntsc ? 0x81465320 : 0x814B4FC0;
  Log("[boot] %.6s from %s: entry %08x, FST %08x (%u bytes), bi2 %08x\n",
      reinterpret_cast<const char*>(disc), game_dir.c_str(), g_cpu.pc, fst_address, fst_size,
      bi2_address);
  return true;
}
}  // namespace host
