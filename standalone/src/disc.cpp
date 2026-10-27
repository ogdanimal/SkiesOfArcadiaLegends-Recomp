// The disc, served from an extracted game folder (sys/ + files/, Dolphin's "Extract Entire Disc"
// layout). The disc's own FST (sys/fst.bin) keeps the original file offsets, and the game reads
// files at those offsets, so a read is mapped back to the file that held those bytes. A file can be
// served from another of the same size (dreamcast.cpp: a Dreamcast stream, the European disc's 60 Hz
// copy, an uncensored model), as the Dolphin build's DirectoryBlob does.
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
  std::string path;
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

std::ifstream* Stream(const std::string& path)
{
  for (OpenFile& f : s_open)
  {
    if (f.path == path)
      return &f.stream;
  }
  OpenFile& f = s_open[s_open_next++ % std::size(s_open)];
  f.stream.close();
  f.stream.clear();
  f.stream.open(path, std::ios::binary);
  f.path = f.stream ? path : std::string();
  return f.stream ? &f.stream : nullptr;
}

// n bytes of path at in_file (what the file doesn't have stays zero); false when it can't be opened
bool ReadFile(const std::string& path, u64 in_file, u8* out, u64 n)
{
  std::ifstream* f = Stream(path);
  if (!f)
    return false;
  f->clear();
  f->seekg(static_cast<std::streamoff>(in_file));
  f->read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(n));
  return true;
}
}  // namespace

namespace
{
// SKIES_DISC_DUMP=DIR:REL[,REL...] (a test): those files (relative to files/) read through DiscRead as
// the game would read them, into DIR: what the disc serves (dreamcast.cpp's redirects included)
void DumpFiles(const std::string& files_dir)
{
  const char* spec = std::getenv("SKIES_DISC_DUMP");
  if (!spec)
    return;
  const std::string all = spec;
  const size_t colon = all.find(':', all.size() > 2 && all[1] == ':' ? 2 : 0);  // C:\... on Windows
  if (colon == std::string::npos)
    return;
  const std::filesystem::path out_dir = all.substr(0, colon);
  std::filesystem::create_directories(out_dir);
  for (size_t at = colon + 1; at < all.size();)
  {
    const size_t comma = all.find(',', at);
    const std::string rel = all.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    at = comma == std::string::npos ? all.size() : comma + 1;
    const std::string path = (std::filesystem::path(files_dir) / rel).string();
    const auto it = std::find_if(s_extents.begin(), s_extents.end(),
                                 [&](const Extent& e) { return e.path == path; });
    if (it == s_extents.end())
    {
      Log("[disc] dump: no %s\n", rel.c_str());
      continue;
    }
    std::vector<u8> data(it->size);
    DiscRead(it->offset, data.data(), static_cast<u32>(data.size()));
    std::ofstream(out_dir / std::filesystem::path(rel).filename(), std::ios::binary)
        .write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    Log("[disc] dump: %s, %zu bytes\n", rel.c_str(), data.size());
  }
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
#ifdef SKIES_STANDALONE_AURORA
    DreamcastDiscFile(e.path);
#endif
    s_extents.push_back(std::move(e));
    ++added;
  }
  AddSystemFile(fst_offset, std::move(fst), "fst.bin");
  std::sort(s_extents.begin(), s_extents.end(),
            [](const Extent& a, const Extent& b) { return a.offset < b.offset; });
  Log("[disc] %.6s: %u files from %s\n", reinterpret_cast<const char*>(s_boot.data()), added,
      game_dir.c_str());
  DumpFiles(files.string());
  return true;
}

// After a state load: the file table the game has in RAM (OSBootInfo's FST, 0x80000038 / 0x8000003C)
// is the one its reads use. The Dolphin build lays an extracted game out anew (DiscIO's
// DirectoryBlob: other file offsets than sys/fst.bin's), so a state it made reads files at those
// offsets: the files are served at the RAM table's offsets, matched by path. The system files the
// game reads only at boot (main.dol, fst.bin) go when the layout changes, so they can't shadow a
// file. A table the same as the disc's (this host's own states) changes nothing.
void DiscAdoptGuestFst()
{
  namespace fs = std::filesystem;
  const u32 start = Read32(0x80000038), max = Read32(0x8000003C);
  const u8* fst = RamPtr(start, 12);
  if (!fst || max < 12 || max > 0x400000 || !RamPtr(start, max))
    return;
  fst = RamPtr(start, max);
  const u32 count = BE32(fst + 8);
  if (count < 1 || u64{count} * 12 > max)
    return;
  const char* names = reinterpret_cast<const char*>(fst + count * 12);
  const u32 names_size = max - count * 12;
  // The disc's own files by path (the extents built from sys/fst.bin)
  std::vector<std::pair<std::string, size_t>> by_path;
  for (size_t i = 0; i < s_extents.size(); ++i)
  {
    if (s_extents[i].data.empty())
      by_path.emplace_back(s_extents[i].path, i);
  }
  std::sort(by_path.begin(), by_path.end());
  const fs::path files = by_path.empty() ? fs::path() : [&] {
    // the files/ folder: the first file's path above its FST directories doesn't matter, any
    // file's prefix up to "files" does
    fs::path p = s_extents[by_path.front().second].path;
    while (!p.empty() && p.filename() != "files")
      p = p.parent_path();
    return p;
  }();
  if (files.empty())
    return;
  std::vector<Extent> adopted;
  std::vector<std::pair<u32, fs::path>> dirs{{count, files}};
  u32 moved = 0;
  for (u32 i = 1; i < count; ++i)
  {
    while (dirs.size() > 1 && i >= dirs.back().first)
      dirs.pop_back();
    const u8* entry = fst + i * 12;
    const u32 name_offset = BE32(entry) & 0x00FFFFFF;
    if (name_offset >= names_size || !std::memchr(names + name_offset, 0, names_size - name_offset))
      return;
    const fs::path path = dirs.back().second / (names + name_offset);
    if (entry[0] & 1)
    {
      dirs.push_back({BE32(entry + 8), path});
      continue;
    }
    const std::string key = path.string();
    const auto it = std::lower_bound(by_path.begin(), by_path.end(), std::make_pair(key, size_t{0}));
    if (it == by_path.end() || it->first != key)
      return;  // a file the disc doesn't have: not a table of this game
    Extent e;
    e.offset = BE32(entry + 4);
    e.size = BE32(entry + 8);
    e.path = key;
    if (e.offset != s_extents[it->second].offset)
      ++moved;
    adopted.push_back(std::move(e));
  }
  if (moved == 0)
    return;
  for (Extent& e : s_extents)
  {
    if (!e.data.empty() && e.path != "main.dol" && e.path != "fst.bin")
      adopted.push_back(std::move(e));
  }
  std::sort(adopted.begin(), adopted.end(), [](const Extent& a, const Extent& b) { return a.offset < b.offset; });
  s_extents = std::move(adopted);
  Log("[disc] the state's file table: %u of its files are at other offsets (another layout), served there\n",
      moved);
}

const u8* DiscHeader()
{
  return s_boot.data();
}

bool DiscRunsAsNtsc()
{
  return s_boot[3] != 'P' || std::memcmp(s_boot.data(), "GEAP8P", 6) == 0;
}

const std::vector<u8>& DiscDol()
{
  return s_dol;
}

std::string DiscPathAt(u64 offset)
{
  auto it = std::upper_bound(s_extents.begin(), s_extents.end(), offset,
                             [](u64 value, const Extent& e) { return value < e.offset; });
  if (it == s_extents.begin())
    return {};
  --it;
  if (offset >= it->offset + it->size)
    return {};
  return it->path.empty() ? std::string("(system)") : std::filesystem::path(it->path).filename().string();
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
#ifdef SKIES_STANDALONE_AURORA
    const std::string path = DreamcastRedirect(it->path, in_file);
#else
    const std::string& path = it->path;
#endif
    if (ReadFile(path, in_file, dst + (from - offset), n))
      continue;
    // A served copy that went away (removed with the game running, a cache cleared): the disc's
    // own file rather than a read error (the Dolphin build's review D8, D19)
    if (path == it->path)
      return false;
    static int s_warned = 0;
    if (s_warned++ < 8)
      Log("[disc] %s can't be read: the disc's own %s instead\n", path.c_str(), it->path.c_str());
    if (!ReadFile(it->path, in_file, dst + (from - offset), n))
      return false;
  }
  return true;
}
}  // namespace host
