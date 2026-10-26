// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AudioCommon/DCMusicImport.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/StringUtil.h"

#ifdef HAVE_LIBCHDR
#include <libchdr/chd.h>
#endif

namespace DCMusicImport
{
namespace
{
constexpr u32 HIGH_DENSITY_LBA = 45000;
constexpr u32 RAW_SECTOR = 2352;
constexpr u32 CHD_FRAME = 2448;  // a raw sector + 96 bytes of subcode
// The music files and their limits (the largest bank on the discs is 1.3 MB, the streams 5.3 MB).
constexpr u32 MAX_FILE = 4 << 20;
constexpr u32 MAX_STREAM = 16 << 20;
// All the copied files together: a real disc's are about 40 MB (review D16: many wanted entries
// pointing at the same sectors would otherwise fill the disk)
constexpr u64 MAX_TOTAL = 300 << 20;

// The 2048-byte user data of the high-density area's sectors (mode 1).
class SectorReader
{
public:
  virtual ~SectorReader() = default;
  virtual bool Read(u32 lba, u8* out) = 0;
};

struct Track
{
  u32 lba;     // first sector on the disc
  u32 frames;  // sectors
  u32 sector;  // bytes per sector in the image (2352 raw or 2048)
  u64 offset;  // GDI: the track file; CHD: the first frame
  std::string path;
};

const Track* FindTrack(const std::vector<Track>& tracks, u32 lba)
{
  for (const Track& t : tracks)
  {
    if (lba >= t.lba && lba - t.lba < t.frames)
      return &t;
  }
  return nullptr;
}

class GdiReader final : public SectorReader
{
public:
  bool Open(const std::string& gdi, std::string* error)
  {
    std::string text;
    if (!File::ReadFileToString(gdi, text))
    {
      *error = "Can't read the GDI file.";
      return false;
    }
    std::istringstream in(text);
    std::string line;
    std::getline(in, line);
    const std::string dir = gdi.substr(0, gdi.find_last_of("/\\") + 1);
    while (std::getline(in, line))
    {
      std::istringstream fields(line);
      u32 number, lba, type, size;
      std::string name;
      if (!(fields >> number >> lba >> type >> size))
        continue;
      std::getline(fields >> std::ws, name);
      if (!name.empty() && name.front() == '"')
        name = name.substr(1, name.find('"', 1) - 1);
      else
        name = name.substr(0, name.find(' '));
      if (type != 4 || lba < HIGH_DENSITY_LBA || (size != RAW_SECTOR && size != 2048))
        continue;
      Track t{lba, 0, size, 0, dir + name};
      t.frames = static_cast<u32>(File::GetSize(t.path) / size);
      m_tracks.push_back(t);
    }
    if (m_tracks.empty())
    {
      *error = "That GDI has no GD-ROM data track, or its track files aren't beside it.";
      return false;
    }
    return true;
  }

  bool Read(u32 lba, u8* out) override
  {
    const Track* t = FindTrack(m_tracks, lba);
    if (!t)
      return false;
    if (m_open != t)
    {
      if (!m_file.Open(t->path, "rb"))
        return false;
      m_open = t;
    }
    const u64 offset = u64(lba - t->lba) * t->sector + (t->sector == RAW_SECTOR ? 16 : 0);
    return m_file.Seek(offset, File::SeekOrigin::Begin) && m_file.ReadBytes(out, 2048);
  }

private:
  std::vector<Track> m_tracks;
  const Track* m_open = nullptr;
  File::IOFile m_file;
};

#ifdef HAVE_LIBCHDR
class ChdReader final : public SectorReader
{
public:
  ~ChdReader() override
  {
    if (m_chd)
      chd_close(m_chd);
  }

  bool Open(const std::string& path, std::string* error)
  {
    if (!m_file.Open(path, "rb"))
    {
      *error = "Can't open the CHD file.";
      return false;
    }
    if (chd_open_file(m_file.GetHandle(), CHD_OPEN_READ, nullptr, &m_chd) != CHDERR_NONE)
    {
      m_chd = nullptr;
      *error = "That CHD file can't be read.";
      return false;
    }
    const chd_header* header = chd_get_header(m_chd);
    if (header->hunkbytes == 0 || header->hunkbytes % CHD_FRAME != 0)
    {
      *error = "That CHD isn't a CD image.";
      return false;
    }
    m_frames_per_hunk = header->hunkbytes / CHD_FRAME;
    m_hunk.resize(header->hunkbytes);
    // The tracks are stored one after another, each padded to a multiple of 4 frames (as MAME
    // writes them). GD-ROM tracks 1 and 2 are the low-density area, which ends at LBA 45000.
    u32 frame = 0, lba = 0;
    for (u32 i = 0;; ++i)
    {
      char meta[256];
      u32 length = 0;
      if (chd_get_metadata(m_chd, GDROM_TRACK_METADATA_TAG, i, meta, sizeof(meta) - 1, &length,
                           nullptr, nullptr) != CHDERR_NONE)
      {
        break;
      }
      meta[std::min<u32>(length, sizeof(meta) - 1)] = 0;
      char type[32] = {};
      int number = 0, frames = 0;
      const char* f = std::strstr(meta, "FRAMES:");
      if (std::sscanf(meta, "TRACK:%d TYPE:%31s", &number, type) < 2 || !f ||
          std::sscanf(f, "FRAMES:%d", &frames) != 1 || frames <= 0)
      {
        break;
      }
      if (number == 3 && lba < HIGH_DENSITY_LBA)
        lba = HIGH_DENSITY_LBA;
      const std::string_view kind = type;
      if (lba >= HIGH_DENSITY_LBA && (kind == "MODE1_RAW" || kind == "MODE1"))
        m_tracks.push_back({lba, static_cast<u32>(frames), kind == "MODE1" ? 2048u : RAW_SECTOR,
                            frame, ""});
      lba += frames;
      frame += (frames + 3) & ~3;
    }
    if (m_tracks.empty())
    {
      *error = "That CHD isn't a Dreamcast GD-ROM image.";
      return false;
    }
    return true;
  }

  bool Read(u32 lba, u8* out) override
  {
    const Track* t = FindTrack(m_tracks, lba);
    if (!t)
      return false;
    const u64 frame = t->offset + (lba - t->lba);
    const u32 hunk = static_cast<u32>(frame / m_frames_per_hunk);
    if (hunk != m_cached)
    {
      if (chd_read(m_chd, hunk, m_hunk.data()) != CHDERR_NONE)
        return false;
      m_cached = hunk;
    }
    std::memcpy(out,
                m_hunk.data() + (frame % m_frames_per_hunk) * CHD_FRAME +
                    (t->sector == RAW_SECTOR ? 16 : 0),
                2048);
    return true;
  }

private:
  File::IOFile m_file;
  chd_file* m_chd = nullptr;
  std::vector<u8> m_hunk;
  std::vector<Track> m_tracks;
  u32 m_cached = ~0u;
  u32 m_frames_per_hunk = 1;
};
#endif

u32 Le32(const u8* p)
{
  return p[0] | p[1] << 8 | p[2] << 16 | static_cast<u32>(p[3]) << 24;
}

struct Entry
{
  u32 lba;
  u32 size;
};

// The ISO 9660 files under the root and SOUND/ (all the music needs) and the root folders in
// `more` (FIELD/ for the uncensored models), upper case, no ";1".
bool ListFiles(SectorReader& reader, std::map<std::string, Entry>* files,
               const std::set<std::string>& more = {})
{
  std::array<u8, 2048> sector;
  if (!reader.Read(HIGH_DENSITY_LBA + 16, sector.data()) ||
      std::memcmp(sector.data() + 1, "CD001", 5) != 0)
  {
    return false;
  }
  struct Dir
  {
    std::string prefix;
    u32 lba, size;
  };
  std::vector<Dir> dirs{{"", Le32(sector.data() + 156 + 2), Le32(sector.data() + 156 + 10)}};
  for (std::size_t d = 0; d < dirs.size(); ++d)
  {
    const Dir dir = dirs[d];
    std::vector<u8> data;
    // FIELD/ holds a few thousand files
    for (u32 i = 0; i < std::min<u32>((dir.size + 2047) / 2048, 1024); ++i)
    {
      if (!reader.Read(dir.lba + i, sector.data()))
        return false;
      data.insert(data.end(), sector.begin(), sector.end());
    }
    for (std::size_t pos = 0; pos < data.size();)
    {
      const u8 length = data[pos];
      if (length == 0)
      {
        pos = (pos / 2048 + 1) * 2048;
        continue;
      }
      if (length < 34 || pos + length > data.size())
        break;
      const u8* rec = data.data() + pos;
      pos += length;
      const u8 name_length = rec[32];
      if (33 + name_length > length || (name_length == 1 && rec[33] <= 1))
        continue;
      std::string name(reinterpret_cast<const char*>(rec + 33), name_length);
      name = name.substr(0, name.find(';'));
      Common::ToUpper(&name);
      if (rec[25] & 2)
      {
        if (dir.prefix.empty() && (name == "SOUND" || more.contains(name)))
          dirs.push_back({name + "/", Le32(rec + 2), Le32(rec + 10)});
      }
      else
      {
        (*files)[dir.prefix + name] = {Le32(rec + 2), Le32(rec + 10)};
      }
    }
  }
  return true;
}

// The streamed songs whose Dreamcast recordings differ from the GameCube's, SOUND/M01S.P04 (the
// intro, disc 1) and M03S.P04 (a story scene, disc 2) (DiscIO/SkiesStreams.h): the track number,
// else 0. M02S and M04S are the GameCube's recordings, so they aren't copied.
int StreamTrack(const std::string& name)
{
  if (name.size() != 14 || !name.starts_with("SOUND/M0") || (name[8] != '1' && name[8] != '3') ||
      name[9] != 'S' || (!name.ends_with(".P04") && !name.ends_with(".P16")))
  {
    return 0;
  }
  return name[8] - '0';
}

bool Wanted(const std::string& name)
{
  if (name == "BONITO.DRV" || name == "ARESNEW.MLT" || StreamTrack(name))
    return true;
  // A plain bank name only: the disc's names go into the output path (review D16, "M\..\X.MLT")
  if (!name.starts_with("SOUND/M") || !name.ends_with(".MLT") || name.size() > 6 + 31)
    return false;
  const std::string_view base(name.data() + 6, name.size() - 6 - 4);
  return base.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") == std::string_view::npos;
}

// sources.txt in the music folder: one "NAME REGION" line per imported file.
std::map<std::string, std::string> ReadSources(const std::string& dir)
{
  std::map<std::string, std::string> sources;
  std::string text;
  File::ReadFileToString(dir + "sources.txt", text);
  std::istringstream in(text);
  std::string name, region;
  while (in >> name >> region)
    sources[name] = region;
  return sources;
}

bool Ntsc(const std::string& region)
{
  return region == "USA" || region == "Japan";
}
}  // namespace

std::string Import(const std::string& image, const std::string& music_dir, std::string* summary,
                   const std::function<void(const std::string&)>& status,
                   const std::vector<ExtraFile>& extra, int* extra_copied)
{
  if (extra_copied)
    *extra_copied = 0;
  auto report = [&](const std::string& s) {
    if (status)
      status(s);
  };
  report("Reading the disc image");
  std::string error;
  std::unique_ptr<SectorReader> reader;
  {
    File::IOFile probe(image, "rb");
    char magic[8] = {};
    if (!probe.IsOpen() || !probe.ReadBytes(magic, sizeof(magic)))
      return "Can't open that file.";
    if (std::memcmp(magic, "MComprHD", 8) == 0)
    {
#ifdef HAVE_LIBCHDR
      auto chd = std::make_unique<ChdReader>();
      if (!chd->Open(image, &error))
        return error;
      reader = std::move(chd);
#else
      return "This build can't read CHD images; use a GDI.";
#endif
    }
    else if (magic[0] >= '0' && magic[0] <= '9')
    {
      auto gdi = std::make_unique<GdiReader>();
      if (!gdi->Open(image, &error))
        return error;
      reader = std::move(gdi);
    }
    else
    {
      return "That isn't a Dreamcast GDI or CHD image.";
    }
  }

  // IP.BIN, the high-density area's first sector: "SEGA SEGAKATANA", "GD-ROMn/m" at 0x25 (the disc
  // number n at 0x2B), the areas ('J', 'U', 'E') at 0x30 and the title at 0x80.
  std::array<u8, 2048> ip;
  if (!reader->Read(HIGH_DENSITY_LBA, ip.data()) ||
      std::memcmp(ip.data(), "SEGA SEGAKATANA", 15) != 0)
  {
    return "That isn't a Dreamcast disc.";
  }
  std::string title(reinterpret_cast<const char*>(ip.data() + 0x80), 0x80);
  Common::ToUpper(&title);
  if (title.find("ARCADIA") == std::string::npos)
    return "That disc isn't Skies of Arcadia.";
  const char* areas = reinterpret_cast<const char*>(ip.data() + 0x30);
  const std::string region = areas[1] == 'U' ? "USA" : areas[0] == 'J' ? "Japan" : "Europe";
  const int disc =
      std::memcmp(ip.data() + 0x25, "GD-ROM", 6) == 0 && ip[0x2B] >= '1' && ip[0x2B] <= '9' ?
          ip[0x2B] - '0' :
          1;

  report("Reading the file system");
  std::map<std::string, Entry> files;
  std::set<std::string> more;
  for (const ExtraFile& e : extra)
  {
    if (e.disc == disc && e.path.find('/') != std::string::npos)
      more.insert(e.path.substr(0, e.path.find('/')));
  }
  if (!ListFiles(*reader, &files, more))
    return "The disc's file system can't be read.";
  if (!files.contains("BONITO.DRV") || !files.contains("ARESNEW.MLT"))
    return "The disc has no sound driver (BONITO.DRV); is it really Skies of Arcadia?";

  const std::string& dir = music_dir;
  if (!File::CreateFullPath(dir + "SOUND" DIR_SEP))
    return "Can't create " + dir + ".";
  std::map<std::string, std::string> sources = ReadSources(dir);

  int banks = 0, streams = 0, kept = 0;
  u64 total = 0;
  std::vector<u8> data;
  std::array<u8, 2048> sector;
  for (const auto& [name, entry] : files)
  {
    if (!Wanted(name))
      continue;
    // An NTSC disc's file wins over a PAL one's (the PAL event music is slowed for 50 Hz).
    if (auto it = sources.find(name); it != sources.end() && Ntsc(it->second) && !Ntsc(region))
    {
      ++kept;
      continue;
    }
    if (entry.size > (StreamTrack(name) ? MAX_STREAM : MAX_FILE))
      return "Unexpected file size for " + name + ".";
    total += entry.size;
    if (total > MAX_TOTAL)
      return "The disc's music files are far larger than expected; is it really Skies of Arcadia?";
    report("Copying " + name);
    data.resize(entry.size);
    for (u32 i = 0; i * 2048 < entry.size; ++i)
    {
      if (!reader->Read(entry.lba + i, sector.data()))
        return "Reading " + name + " failed.";
      std::memcpy(data.data() + i * 2048, sector.data(), std::min<u32>(2048, entry.size - i * 2048));
    }
    // The driver and the common banks as DCMusic's Load reads them, so a damaged disc is refused
    // here rather than failing (or, before review D3, crashing) at every start
    if (name == "BONITO.DRV" &&
        (data.size() < 0x120 || std::memcmp(data.data(), "SDRV", 4) != 0 ||
         (u32(data[8]) | u32(data[9]) << 8 | u32(data[10]) << 16 | u32(data[11]) << 24) < 0x120))
    {
      return "The disc's sound driver (BONITO.DRV) is damaged.";
    }
    if (name == "ARESNEW.MLT" && (data.size() < 0x20 || std::memcmp(data.data(), "SMLT", 4) != 0))
      return "The disc's sound banks (ARESNEW.MLT) are damaged.";
    const std::string path = dir + name;
    const std::string temp = path + ".part";
    if (!File::WriteStringToFile(temp, std::string(data.begin(), data.end())) ||
        !File::Rename(temp, path))
    {
      return "Can't write " + path + ".";
    }
    sources[name] = region;
    if (StreamTrack(name))
      ++streams;
    else if (name.starts_with("SOUND/"))
      ++banks;
  }
  // The extra files (the uncensored models' sources), each only when its contents are the
  // expected ones: another release of the disc may differ.
  for (const ExtraFile& e : extra)
  {
    const auto it = files.find(e.path);
    if (e.disc != disc || it == files.end() || it->second.size > 16 * 1024 * 1024)
      continue;
    report("Copying " + e.path);
    data.resize(it->second.size);
    for (u32 i = 0; i * 2048 < it->second.size; ++i)
    {
      if (!reader->Read(it->second.lba + i, sector.data()))
        return "Reading " + e.path + " failed.";
      std::memcpy(data.data() + i * 2048, sector.data(),
                  std::min<u32>(2048, it->second.size - i * 2048));
    }
    if (fmt::format("{:02x}", fmt::join(Common::SHA1::CalculateDigest(data), "")) != e.sha1)
      continue;
    const std::string temp = e.dest + ".part";
    if (!File::CreateFullPath(e.dest) ||
        !File::WriteStringToFile(temp, std::string(data.begin(), data.end())) ||
        !File::Rename(temp, e.dest))
    {
      return "Can't write " + e.dest + ".";
    }
    if (extra_copied)
      ++*extra_copied;
  }

  std::string list;
  for (const auto& [name, r] : sources)
    list += name + ' ' + r + '\n';
  if (!File::WriteStringToFile(dir + "sources.txt", list))
    return "Can't write the music folder's list.";
  std::string discs;
  File::ReadFileToString(dir + "discs.txt", discs);
  const std::string line = fmt::format("{} {}\n", disc, region);
  if (discs.find(line) == std::string::npos)
    File::WriteStringToFile(dir + "discs.txt", discs + line);

  if (summary)
  {
    *summary = fmt::format("{} disc {} ({}): {} music banks, {} streamed song{}",
                           region == "Japan" ? "Eternal Arcadia" : "Skies of Arcadia", disc, region,
                           banks, streams, streams == 1 ? "" : "s");
    if (kept)
      *summary += fmt::format(" ({} kept from an NTSC disc)", kept);
  }
  report("");
  return "";
}

std::string Describe(const std::string& music_dir)
{
  if (!File::Exists(music_dir + "BONITO.DRV") || !File::Exists(music_dir + "ARESNEW.MLT"))
    return "";
  std::string discs;
  File::ReadFileToString(music_dir + "discs.txt", discs);
  std::istringstream in(discs);
  // Which discs, by release: "Eternal Arcadia discs 1 and 2 (Japan), Skies of Arcadia disc 1 (USA)"
  std::map<std::string, std::set<int>> by_region;
  int disc;
  std::string region;
  while (in >> disc >> region)
    by_region[region].insert(disc);
  std::vector<std::string> parts;
  for (const char* r : {"USA", "Europe", "Japan"})
  {
    const auto it = by_region.find(r);
    if (it == by_region.end())
      continue;
    parts.push_back(fmt::format("{} disc{} {} ({})",
                                it->first == "Japan" ? "Eternal Arcadia" : "Skies of Arcadia",
                                it->second.size() > 1 ? "s" : "", fmt::join(it->second, " and "),
                                it->first));
  }
  int banks = 0;
  std::array<bool, 5> tracks{};
  for (const auto& [name, r] : ReadSources(music_dir))
  {
    if (const int track = StreamTrack(name))
      tracks[track] = true;
    else if (name.starts_with("SOUND/"))
      ++banks;
  }
  const auto streams = std::count(tracks.begin(), tracks.end(), true);
  const std::string text = parts.empty() ? "Imported" : fmt::format("{}", fmt::join(parts, ", "));
  // An import from before the streamed songs were copied: importing the disc again adds them.
  if (streams == 0)
    return fmt::format("{}: {} music banks (import again for the streamed songs)", text, banks);
  return fmt::format("{}: {} music banks, {}", text, banks,
                     streams == 2 ? "both streamed songs" :
                     tracks[1]    ? "the intro's streamed song (disc 2 has the other)" :
                                    "one streamed song (disc 1 has the intro's)");
}

bool Remove(const std::string& music_dir)
{
  return !File::Exists(music_dir) || File::DeleteDirRecursively(music_dir);
}
}  // namespace DCMusicImport
