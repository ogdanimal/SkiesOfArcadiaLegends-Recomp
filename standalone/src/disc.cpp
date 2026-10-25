// The disc, served from an extracted game folder (sys/ + files/, Dolphin's "Extract Entire Disc"
// layout). The disc's own FST (sys/fst.bin) keeps the original file offsets, and the game reads
// files at those offsets, so a read is mapped back to the file that held those bytes.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

namespace host
{
namespace
{
struct Extent
{
  u64 offset;
  u64 size;
  std::string path;
  std::vector<u8> data;  // system files are kept in memory
};
std::vector<Extent> s_extents;  // sorted by offset
std::vector<u8> s_boot;
std::vector<u8> s_dol;  // sys/main.dol, whole: a patched DOL can outgrow its slot on the disc

struct OpenFile
{
  const Extent* extent = nullptr;
  std::ifstream stream;
};
OpenFile s_open[8];
u32 s_open_next = 0;

std::vector<u8> ReadAll(const std::filesystem::path& path)
{
  std::ifstream f(path, std::ios::binary);
  return std::vector<u8>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void AddSystemFile(u64 offset, std::vector<u8> data, const char* name)
{
  Extent e;
  e.offset = offset;
  e.size = data.size();
  e.path = name;
  e.data = std::move(data);
  s_extents.push_back(std::move(e));
}

std::ifstream* Stream(const Extent& e)
{
  for (OpenFile& f : s_open)
  {
    if (f.extent == &e)
      return &f.stream;
  }
  OpenFile& f = s_open[s_open_next++ % std::size(s_open)];
  f.stream.close();
  f.stream.clear();
  f.stream.open(e.path, std::ios::binary);
  f.extent = f.stream ? &e : nullptr;
  return f.stream ? &f.stream : nullptr;
}
}  // namespace

bool DiscOpen(const std::string& game_dir, const std::string& dol_path)
{
  namespace fs = std::filesystem;
  const fs::path sys = fs::path(game_dir) / "sys";
  const fs::path files = fs::path(game_dir) / "files";
  s_boot = ReadAll(sys / "boot.bin");
  std::vector<u8> bi2 = ReadAll(sys / "bi2.bin");
  std::vector<u8> apploader = ReadAll(sys / "apploader.img");
  std::vector<u8> dol = ReadAll(dol_path.empty() ? sys / "main.dol" : std::filesystem::path(dol_path));
  std::vector<u8> fst = ReadAll(sys / "fst.bin");
  if (s_boot.size() != 0x440 || bi2.size() != 0x2000 || apploader.empty() || dol.empty() || fst.empty())
  {
    Log("[disc] %s is not an extracted GameCube disc (sys/boot.bin, bi2.bin, apploader.img, "
        "main.dol, fst.bin)\n",
        game_dir.c_str());
    return false;
  }
  const u32 dol_offset = BE32(&s_boot[0x420]);
  const u32 fst_offset = BE32(&s_boot[0x424]);
  AddSystemFile(0, s_boot, "boot.bin");
  AddSystemFile(0x440, std::move(bi2), "bi2.bin");
  AddSystemFile(0x2440, std::move(apploader), "apploader.img");
  s_dol = dol;
  // The disc's DOL slot ends where the FST begins; a game-patched DOL is longer (its patch
  // sections come last), so the slot serves only what fits and boot loads s_dol itself.
  if (fst_offset > dol_offset && dol.size() > fst_offset - dol_offset)
    dol.resize(fst_offset - dol_offset);
  AddSystemFile(dol_offset, std::move(dol), "main.dol");

  // FST: 12-byte entries (flags/name offset, file offset or parent, size or next index), then the
  // name table. Directory entries nest until their "next" index.
  const u32 count = BE32(&fst[8]);
  if (count * 12 > fst.size())
  {
    Log("[disc] fst.bin is truncated\n");
    return false;
  }
  const char* names = reinterpret_cast<const char*>(fst.data() + count * 12);
  std::vector<std::pair<u32, fs::path>> dirs{{count, files}};
  u32 added = 0;
  for (u32 i = 1; i < count; ++i)
  {
    while (dirs.size() > 1 && i >= dirs.back().first)
      dirs.pop_back();
    const u8* entry = &fst[i * 12];
    const u32 name_offset = BE32(entry) & 0x00FFFFFF;
    const fs::path path = dirs.back().second / (names + name_offset);
    if (entry[0] & 1)
    {
      dirs.push_back({BE32(entry + 8), path});
      continue;
    }
    Extent e;
    e.offset = BE32(entry + 4);
    e.size = BE32(entry + 8);
    e.path = path.string();
    if (!fs::exists(path))
    {
      Log("[disc] missing %s\n", e.path.c_str());
      continue;
    }
    s_extents.push_back(std::move(e));
    ++added;
  }
  AddSystemFile(fst_offset, std::move(fst), "fst.bin");
  std::sort(s_extents.begin(), s_extents.end(),
            [](const Extent& a, const Extent& b) { return a.offset < b.offset; });
  Log("[disc] %.6s: %u files from %s\n", reinterpret_cast<const char*>(s_boot.data()), added,
      game_dir.c_str());
  return true;
}

const u8* DiscHeader()
{
  return s_boot.data();
}

const std::vector<u8>& DiscDol()
{
  return s_dol;
}

bool DiscRead(u64 offset, void* out, u32 length)
{
  u8* dst = static_cast<u8*>(out);
  std::memset(dst, 0, length);
  const u64 end = offset + length;
  // First extent that ends after offset
  auto it = std::upper_bound(s_extents.begin(), s_extents.end(), offset,
                             [](u64 value, const Extent& e) { return value < e.offset; });
  if (it != s_extents.begin())
    --it;
  for (; it != s_extents.end() && it->offset < end; ++it)
  {
    const u64 from = std::max(offset, it->offset);
    const u64 to = std::min(end, it->offset + it->size);
    if (from >= to)
      continue;
    const u64 in_file = from - it->offset;
    const u64 n = to - from;
    if (!it->data.empty())
    {
      std::memcpy(dst + (from - offset), it->data.data() + in_file, n);
      continue;
    }
    std::ifstream* f = Stream(*it);
    if (!f)
      return false;
    f->clear();
    f->seekg(static_cast<std::streamoff>(in_file));
    f->read(reinterpret_cast<char*>(dst + (from - offset)), static_cast<std::streamsize>(n));
  }
  return true;
}
}  // namespace host
