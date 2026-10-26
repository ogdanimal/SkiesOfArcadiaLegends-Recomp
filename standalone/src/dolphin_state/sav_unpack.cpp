// sav_unpack.cpp -- Dolphin .sav container: legacy header + version + extended header + LZ4 chunks.
// (Core/State.h, Core/StateFile.cpp.)  Needs lz4 (compile lz4.c alongside).
#include <cstdio>
#include <cstring>

#include "dolphin_state_reader.h"
#include "lz4.h"

namespace dsr
{
std::vector<uint8_t> unpack_sav(const std::string& path, SavInfo* info)
{
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) throw ParseError("cannot open " + path);
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::vector<uint8_t> d(static_cast<size_t>(n));
  if (n > 0 && std::fread(d.data(), 1, d.size(), f) != d.size())
  {
    std::fclose(f);
    throw ParseError("short read " + path);
  }
  std::fclose(f);
  return unpack_sav_data(d, info);
}

std::vector<uint8_t> unpack_sav_data(const std::vector<uint8_t>& d, SavInfo* info)
{
  auto need = [&](uint64_t o, uint64_t len, const char* what) {
    if (o + len > d.size()) throw ParseError(std::string("truncated .sav: ") + what);
  };
  // StateHeaderLegacy: char game_id[6]; char reserved1[2]; u32 lzo_size; char reserved2[4]; double time  (24 bytes)
  need(0, 32, "headers");
  uint32_t lzo_size, cookie, vlen;
  double ts;
  std::memcpy(&lzo_size, &d[8], 4);
  std::memcpy(&ts, &d[16], 8);
  std::memcpy(&cookie, &d[24], 4);
  std::memcpy(&vlen, &d[28], 4);
  if (lzo_size != 0) throw ParseError("legacy LZO state not supported");
  need(32, vlen, "version string");
  uint64_t o = 32 + vlen;
  // StateExtendedBaseHeader: u16 header_version; u16 compression_type; u32 payload_offset; u64 uncompressed_size
  need(o, 16, "extended header");
  uint16_t hv, comp;
  uint32_t payload_off;
  uint64_t us;
  std::memcpy(&hv, &d[o], 2);
  std::memcpy(&comp, &d[o + 2], 2);
  std::memcpy(&payload_off, &d[o + 4], 4);
  std::memcpy(&us, &d[o + 8], 8);
  o += 16 + payload_off;
  if (info)
  {
    info->game_id.assign(reinterpret_cast<const char*>(&d[0]), strnlen(reinterpret_cast<const char*>(&d[0]), 6));
    info->version_string.assign(reinterpret_cast<const char*>(&d[32]), vlen);
    info->version_cookie = cookie;
    info->state_version = cookie - 0xBAADBABEu;
    info->timestamp = ts;
    info->compression = comp;
    info->uncompressed_size = us;
  }
  std::vector<uint8_t> out;
  if (comp == 0)
  {
    need(o, 0, "payload");
    out.assign(d.begin() + static_cast<long>(o), d.end());
    return out;
  }
  if (comp != 1) throw ParseError("unknown compression type " + std::to_string(comp));
  // A GameCube state is about 90-150 MB; more is a damaged header
  if (us > (512ull << 20)) throw ParseError("implausible state size " + std::to_string(us));
  out.resize(us);
  uint64_t got = 0;
  while (got < us)
  {
    need(o, 4, "chunk length");
    int32_t cl;
    std::memcpy(&cl, &d[o], 4);
    o += 4;
    if (cl < 0) throw ParseError("negative chunk length");
    need(o, static_cast<uint64_t>(cl), "chunk data");
    const uint64_t left = us - got;
    const int maxd = static_cast<int>(left > 0x7E000000ull ? 0x7E000000ull : left);
    int r = LZ4_decompress_safe(reinterpret_cast<const char*>(&d[o]), reinterpret_cast<char*>(&out[got]), cl, maxd);
    if (r < 0) throw ParseError("LZ4 decompression failed");
    o += static_cast<uint64_t>(cl);
    got += static_cast<uint64_t>(r);
  }
  return out;
}
}  // namespace dsr
