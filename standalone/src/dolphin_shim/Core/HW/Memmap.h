// Dolphin's Core/HW/Memmap.h for the Dreamcast modules (dolphin_shim/README.h): guest RAM
#pragma once

#include <cstddef>

#include "Common/CommonTypes.h"
#include "dreamcast_host.h"

namespace Memory
{
class MemoryManager
{
public:
  bool IsInitialized() const { return dchost::RamReady(); }
  u32 Read_U32(u32 address) const { return dchost::Read32(address); }
  u8 Read_U8(u32 address) const { return dchost::Read8(address); }
  void Write_U32(u32 value, u32 address) { dchost::Write32(value, address); }
  void CopyFromEmu(void* out, u32 address, std::size_t size) const
  {
    dchost::CopyFromRam(out, address, size);
  }
};
}  // namespace Memory
