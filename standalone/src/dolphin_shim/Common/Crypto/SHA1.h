// Dolphin's Common/Crypto/SHA1.h for the Dreamcast modules (dolphin_shim/README.h): SHA-1 of a
// buffer (FIPS 180-4), for the file checks against the uncensor manifest and the 60 Hz tables.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace Common::SHA1
{
using Digest = std::array<std::uint8_t, 20>;

inline Digest CalculateDigest(const std::uint8_t* data, std::size_t size)
{
  std::uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
  const auto rol = [](std::uint32_t x, int n) { return (x << n) | (x >> (32 - n)); };
  const auto block = [&](const std::uint8_t* p) {
    std::uint32_t w[80];
    for (int i = 0; i < 16; ++i)
      w[i] = std::uint32_t{p[i * 4]} << 24 | p[i * 4 + 1] << 16 | p[i * 4 + 2] << 8 | p[i * 4 + 3];
    for (int i = 16; i < 80; ++i)
      w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i)
    {
      std::uint32_t f, k;
      if (i < 20)
        f = (b & c) | (~b & d), k = 0x5A827999;
      else if (i < 40)
        f = b ^ c ^ d, k = 0x6ED9EBA1;
      else if (i < 60)
        f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
      else
        f = b ^ c ^ d, k = 0xCA62C1D6;
      const std::uint32_t t = rol(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = rol(b, 30);
      b = a;
      a = t;
    }
    h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
  };
  std::size_t whole = size / 64 * 64;
  for (std::size_t i = 0; i < whole; i += 64)
    block(data + i);
  // The tail, the 0x80 marker and the length in bits, in one or two blocks
  std::uint8_t tail[128] = {};
  const std::size_t rest = size - whole;
  if (rest)
    std::memcpy(tail, data + whole, rest);
  tail[rest] = 0x80;
  const std::size_t tail_size = rest < 56 ? 64 : 128;
  const std::uint64_t bits = static_cast<std::uint64_t>(size) * 8;
  for (int i = 0; i < 8; ++i)
    tail[tail_size - 1 - i] = static_cast<std::uint8_t>(bits >> (8 * i));
  for (std::size_t i = 0; i < tail_size; i += 64)
    block(tail + i);
  Digest digest;
  for (int i = 0; i < 5; ++i)
  {
    digest[i * 4] = static_cast<std::uint8_t>(h[i] >> 24);
    digest[i * 4 + 1] = static_cast<std::uint8_t>(h[i] >> 16);
    digest[i * 4 + 2] = static_cast<std::uint8_t>(h[i] >> 8);
    digest[i * 4 + 3] = static_cast<std::uint8_t>(h[i]);
  }
  return digest;
}

inline Digest CalculateDigest(std::string_view data)
{
  return CalculateDigest(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
}

inline Digest CalculateDigest(const std::string& data)
{
  return CalculateDigest(std::string_view(data));
}

template <typename T>
inline Digest CalculateDigest(const std::vector<T>& data)
{
  static_assert(sizeof(T) == 1);
  return CalculateDigest(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
}
}  // namespace Common::SHA1
