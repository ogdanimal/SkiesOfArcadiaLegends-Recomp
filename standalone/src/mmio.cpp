// Address decoding for everything outside MEM1: the hardware registers at 0xCC00xxxx (16-bit
// register files on CP/PE/VI/MI/DSP, 32-bit on PI/DI/SI/EXI/AI; other widths are split or
// merged), the EFB at 0xC8000000 and the locked cache at 0xE0000000.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

#include <cstdlib>

namespace host
{
namespace
{
u16 s_mi[0x80];

// --mmio-log with SKIES_MMIO_RANGE=lo-hi (hex) logs only that window
bool Logged(u32 address)
{
  static u32 lo = 0, hi = ~0u;
  static bool parsed = false;
  if (!parsed)
  {
    parsed = true;
    if (const char* r = std::getenv("SKIES_MMIO_RANGE"))
    {
      char* end = nullptr;
      lo = static_cast<u32>(std::strtoul(r, &end, 16));
      if (end && *end == '-')
        hi = static_cast<u32>(std::strtoul(end + 1, nullptr, 16));
    }
  }
  return g_log_mmio && address >= lo && address < hi;
}

u16 Read16Reg(u32 address)
{
  const u32 offset = address & 0xFFF;
  switch ((address >> 12) & 0xF)
  {
  case 0x0:
    return CpRead16(offset);
  case 0x1:
    return PeRead16(offset);
  case 0x2:
    return ViRead16(offset);
  case 0x4:
    return s_mi[(offset >> 1) & 0x7F];
  case 0x5:
    return DspRead16(offset);
  default:
    break;
  }
  Log("[mmio] unhandled read16 %08x\n", address);
  return 0;
}

void Write16Reg(u32 address, u16 value)
{
  const u32 offset = address & 0xFFF;
  switch ((address >> 12) & 0xF)
  {
  case 0x0:
    CpWrite16(offset, value);
    return;
  case 0x1:
    PeWrite16(offset, value);
    return;
  case 0x2:
    ViWrite16(offset, value);
    return;
  case 0x4:
    // MI: memory protection and bandwidth counters. Kept, not acted on.
    s_mi[(offset >> 1) & 0x7F] = value;
    return;
  case 0x5:
    DspWrite16(offset, value);
    return;
  default:
    break;
  }
  Log("[mmio] unhandled write16 %08x = %04x\n", address, value);
}

bool Is16BitBlock(u32 address)
{
  const u32 block = (address >> 12) & 0xF;
  return block <= 2 || block == 4 || block == 5;
}

u32 Read32Reg(u32 address)
{
  const u32 offset = address & 0x3FF;
  if ((address & 0xF000) == 0x3000)
    return PiRead(address & 0xFFF);
  switch ((address >> 10) & 0x3F)
  {
  case 0x18:  // 0x6000
    return DiRead32(offset);
  case 0x19:  // 0x6400
    return SiRead32(offset);
  case 0x1A:  // 0x6800
    return ExiRead32(offset);
  case 0x1B:  // 0x6C00
    return AiRead32(offset);
  default:
    break;
  }
  Log("[mmio] unhandled read32 %08x\n", address);
  return 0;
}

void Write32Reg(u32 address, u32 value)
{
  const u32 offset = address & 0x3FF;
  if ((address & 0xF000) == 0x3000)
  {
    PiWrite(address & 0xFFF, value);
    return;
  }
  switch ((address >> 10) & 0x3F)
  {
  case 0x18:
    DiWrite32(offset, value);
    return;
  case 0x19:
    SiWrite32(offset, value);
    return;
  case 0x1A:
    ExiWrite32(offset, value);
    return;
  case 0x1B:
    AiWrite32(offset, value);
    return;
  default:
    break;
  }
  Log("[mmio] unhandled write32 %08x = %08x\n", address, value);
}
}  // namespace

u64 MmioRead(u32 address, u8 size)
{
  u64 value = 0;
  if ((address & 0xFFFF0000u) == 0xCC000000u)
  {
    if (Is16BitBlock(address))
    {
      if (size == 2)
        value = Read16Reg(address & ~1u);
      else if (size == 4)
        value = (u32(Read16Reg(address & ~3u)) << 16) | Read16Reg((address & ~3u) + 2);
      else if (size == 1)
        value = (Read16Reg(address & ~1u) >> ((address & 1) ? 0 : 8)) & 0xFF;
      else
        value = (u64(MmioRead(address, 4)) << 32) | MmioRead(address + 4, 4);
    }
    else
    {
      if (size == 4)
        value = Read32Reg(address & ~3u);
      else if (size == 2)
        value = (Read32Reg(address & ~3u) >> ((address & 2) ? 0 : 16)) & 0xFFFF;
      else if (size == 1)
        value = (Read32Reg(address & ~3u) >> (8 * (3 - (address & 3)))) & 0xFF;
      else
        value = (u64(Read32Reg(address & ~3u)) << 32) | Read32Reg((address & ~3u) + 4);
    }
  }
  else if ((address & 0xFF000000u) == 0xC8000000u)
  {
    value = EfbRead(address);
  }
  else if ((address & 0xFFFFC000u) == 0xE0000000u)
  {
    const u8* p = LockedCache() + (address & 0x3FFF);
    for (u8 i = 0; i < size && (address & 0x3FFF) + i < 0x4000; ++i)
      value = (value << 8) | p[i];
  }
  else
  {
    static int s_logged = 0;
    if (s_logged < 32)
    {
      ++s_logged;
      Log("[mmio] read%u from unmapped %08x (pc %08x)\n", size * 8, address, g_cpu.pc);
    }
  }
  if (Logged(address))
    Log("[mmio] r%u %08x -> %llx (pc %08x)\n", size * 8, address,
        static_cast<unsigned long long>(value), g_cpu.pc);
  return value;
}

void MmioWrite(u32 address, u64 value, u8 size)
{
  if (Logged(address))
    Log("[mmio] w%u %08x <- %llx (pc %08x)\n", size * 8, address,
        static_cast<unsigned long long>(value), g_cpu.pc);
  if ((address & 0xFFFF0000u) == 0xCC000000u)
  {
    if ((address & 0xF000) == 0x8000)
    {
      Log("[mmio] gather pipe write outside the pipe buffer at %08x\n", address);
      return;
    }
    if (Is16BitBlock(address))
    {
      if (size == 2)
        Write16Reg(address & ~1u, static_cast<u16>(value));
      else if (size == 4)
      {
        Write16Reg(address & ~3u, static_cast<u16>(value >> 16));
        Write16Reg((address & ~3u) + 2, static_cast<u16>(value));
      }
      else if (size == 8)
      {
        MmioWrite(address, value >> 32, 4);
        MmioWrite(address + 4, value & 0xFFFFFFFFu, 4);
      }
      else
        Log("[mmio] byte write %08x = %02x ignored\n", address, static_cast<u32>(value));
    }
    else
    {
      if (size == 4)
        Write32Reg(address & ~3u, static_cast<u32>(value));
      else if (size == 2)
      {
        // Half a 32-bit register: merge with what it holds.
        const u32 old = Read32Reg(address & ~3u);
        const u32 merged = (address & 2) ? (old & 0xFFFF0000u) | static_cast<u16>(value)
                                         : (old & 0x0000FFFFu) | (static_cast<u32>(value) << 16);
        Write32Reg(address & ~3u, merged);
      }
      else if (size == 8)
      {
        Write32Reg(address & ~3u, static_cast<u32>(value >> 32));
        Write32Reg((address & ~3u) + 4, static_cast<u32>(value));
      }
      else
        Log("[mmio] byte write %08x = %02x ignored\n", address, static_cast<u32>(value));
    }
    return;
  }
  if ((address & 0xFF000000u) == 0xC8000000u)
    return;  // EFB poke: not modelled
  if ((address & 0xFFFFC000u) == 0xE0000000u)
  {
    u8* p = LockedCache() + (address & 0x3FFF);
    for (u8 i = 0; i < size && (address & 0x3FFF) + i < 0x4000; ++i)
      p[i] = static_cast<u8>(value >> (8 * (size - 1 - i)));
    return;
  }
  static int s_logged = 0;
  if (s_logged < 32)
  {
    ++s_logged;
    Log("[mmio] write%u to unmapped %08x = %llx (pc %08x)\n", size * 8, address,
        static_cast<unsigned long long>(value), g_cpu.pc);
  }
}
}  // namespace host
