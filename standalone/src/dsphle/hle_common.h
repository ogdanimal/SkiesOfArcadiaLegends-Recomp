// Host-side glue for the DSP HLE port: logging and guest memory access.
// Replaces Dolphin's Common/Logging, Core::System and Memory::MemoryManager plumbing.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>

#include "core/types.h"
#include "dsphle/dsp_hle.h"

namespace host::dsphle
{
// Warnings, errors and notices always print; debug-level messages (Dolphin's DEBUG_LOG_FMT and
// the per-mail traces) only when this is set.
inline bool g_debug_log = false;

#define DSPHLE_NOTICE(...)                      \
  do                                            \
  {                                             \
    std::fprintf(stderr, "[dsphle] " __VA_ARGS__); \
    std::fputc('\n', stderr);                   \
  } while (0)
#define DSPHLE_WARN(...) DSPHLE_NOTICE(__VA_ARGS__)
#define DSPHLE_ERROR(...) DSPHLE_NOTICE(__VA_ARGS__)
#define DSPHLE_DEBUG(...)                       \
  do                                            \
  {                                             \
    if (::host::dsphle::g_debug_log)            \
      DSPHLE_NOTICE(__VA_ARGS__);               \
  } while (0)

namespace swap
{
#ifdef _MSC_VER
inline u16 swap16(u16 v) { return _byteswap_ushort(v); }
inline u32 swap32(u32 v) { return _byteswap_ulong(v); }
#else
inline u16 swap16(u16 v) { return __builtin_bswap16(v); }
inline u32 swap32(u32 v) { return __builtin_bswap32(v); }
#endif
// Guest data is big-endian, the host is little-endian.
template <typename T>
inline T FromBigEndian(T v)
{
  static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4);
  if constexpr (sizeof(T) == 2)
    return static_cast<T>(swap16(static_cast<u16>(v)));
  else if constexpr (sizeof(T) == 4)
    return static_cast<T>(swap32(static_cast<u32>(v)));
  else
    return v;
}
}  // namespace swap

// Stand-in for Memory::MemoryManager over the host's MEM1. Stateless. Guest addresses are
// physical or 0x80xxxxxx/0xC0xxxxxx (address & 0x01FFFFFF); ranges must lie inside HostRam().
class MemoryManager
{
public:
  // Pointer into host RAM for [address, address + size), or nullptr if out of bounds.
  u8* GetPointerForRange(u32 address, size_t size) const
  {
    const u32 phys = address & 0x01FFFFFF;
    const u32 ram_size = HostRamSize();
    if (HostRam() == nullptr || size > ram_size || phys > ram_size - size)
      return nullptr;
    return HostRam() + phys;
  }

  // Read pointer that never fails: out-of-bounds ranges (<= 4096 bytes) read as zeros.
  // (Dolphin would hand back nullptr and crash; the AX code below dereferences directly.)
  const u8* GetPointerForRead(u32 address, size_t size) const
  {
    static const u8 zeros[4096] = {};
    if (const u8* p = GetPointerForRange(address, size))
      return p;
    return size <= sizeof(zeros) ? zeros : nullptr;
  }

  template <typename T>
  void CopyFromEmuSwapped(T* data, u32 address, size_t size) const
  {
    const u8* src = GetPointerForRange(address, size);
    if (src == nullptr)
      return;
    for (size_t i = 0; i < size / sizeof(T); i++)
    {
      T v;
      std::memcpy(&v, src + i * sizeof(T), sizeof(T));
      data[i] = swap::FromBigEndian(v);
    }
  }

  template <typename T>
  void CopyToEmuSwapped(u32 address, const T* data, size_t size) const
  {
    u8* dest = GetPointerForRange(address, size);
    if (dest == nullptr)
      return;
    for (size_t i = 0; i < size / sizeof(T); i++)
    {
      const T v = swap::FromBigEndian(data[i]);
      std::memcpy(dest + i * sizeof(T), &v, sizeof(T));
    }
  }

  void CopyToEmu(u32 address, const void* data, size_t size) const
  {
    u8* dest = GetPointerForRange(address, size);
    if (dest != nullptr)
      std::memcpy(dest, data, size);
  }
};
}  // namespace host::dsphle
