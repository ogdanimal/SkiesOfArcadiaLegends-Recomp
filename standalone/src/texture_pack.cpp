// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// The Dolphin build's HD texture pack import (Source/Core/VideoCommon/TexturePackImport.cpp, with
// the review D fixes), kept as it is there so fixes carry over: only the includes differ, Dolphin's
// Common helpers coming from texture_pack_file.h. The LZMA SDK needs no renamed symbols here (no
// mGBA in this build).

#include "texture_pack.h"

#ifdef HAVE_LZMA_SDK
#include "Skies7zPrefix.h"

#include <7z.h>
#include <7zCrc.h>
#include <Lzma2Dec.h>
#include <LzmaDec.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include "texture_pack_file.h"

namespace TexturePackImport
{
namespace
{
// Where the pack goes in Load/Textures/: Dolphin's folder for both games (GEAE8P and GEAP8P).
constexpr char GAME_FOLDER[] = "GEA";
// The pack's name and counts, beside its textures (Dolphin only loads .png / .dds files).
constexpr char MARKER[] = "skies-texture-pack.txt";
constexpr size_t CHUNK = 1 << 20;

std::string Lower(std::string_view s)
{
  std::string out(s);
  std::ranges::transform(out, out.begin(), [](unsigned char c) { return std::tolower(c); });
  return out;
}

// A custom texture Dolphin loads: tex1_..., .png or .dds (HiresTextures.cpp, CustomTextureData).
bool IsTexture(std::string_view file_name)
{
  const std::string lower = Lower(file_name);
  return lower.starts_with("tex1_") && (lower.ends_with(".png") || lower.ends_with(".dds"));
}

// A smaller mipmap level of a texture, which some packs keep as its own file ("..._14_mip1.png"):
// not a texture of its own in the counts.
bool IsMipLevel(std::string_view file_name)
{
  const size_t dot = file_name.rfind('.');
  const std::string_view stem = file_name.substr(0, dot);
  const size_t mip = stem.rfind("_mip");
  return mip != std::string_view::npos && mip + 4 < stem.size() &&
         std::ranges::all_of(stem.substr(mip + 4), [](char c) { return c >= '0' && c <= '9'; });
}

std::string FileName(const std::string& path)
{
  const size_t slash = path.rfind('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

// An archive inside a pack: the pack's archives put into one .zip or folder by the player
bool IsArchive(std::string_view file_name)
{
  const std::string lower = Lower(file_name);
  return lower.ends_with(".7z") || lower.ends_with(".zip");
}

// A Mac's resource forks in a .zip ("__MACOSX/UI/._tex1_....dds")
bool IsMacResourceFork(const std::string& path)
{
  return path.starts_with("__MACOSX/") || path.find("/__MACOSX/") != std::string::npos;
}

// Whether a path from the pack (with '/' separators) stays inside the folder it's written under:
// no empty, "." or ".." part, no leading '/', no ':' (a drive or an NTFS stream), '\\' or control
// character. Folders are made from it as they are, so "GEA/../../Config/tex1_x.png" would write
// outside the import (review D2); the disc import refuses the same (tools/disc_extract.cpp).
bool StaysInside(std::string_view path)
{
  if (path.empty() || path.front() == '/')
    return false;
  size_t start = 0;
  while (true)
  {
    const size_t end = path.find('/', start);
    const std::string_view part =
        path.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
    if (part.empty() || part == "." || part == "..")
      return false;
    for (const char c : part)
    {
      if (static_cast<unsigned char>(c) < 0x20 || c == ':' || c == '\\')
        return false;
    }
    if (end == std::string_view::npos)
      return true;
    start = end + 1;
  }
}

// The part of a path in the pack that is inside its game folder:
// "SoA_HD_v0.06_DDS/GEA/UI/Main/tex1_....dds" -> "UI/Main/tex1_....dds".
std::optional<std::string> InGameFolder(std::string path)
{
  std::ranges::replace(path, '\\', '/');
  size_t start = 0;
  while (start < path.size())
  {
    const size_t end = path.find('/', start);
    if (end == std::string::npos)
      return std::nullopt;
    const std::string part = Lower(std::string_view(path).substr(start, end - start));
    if (part == "gea" || part == "geae8p" || part == "geap8p")
      return path.substr(end + 1);
    start = end + 1;
  }
  return std::nullopt;
}

// The pack's name for Describe: its file or folder name without the extension (percent-encoded in
// a document URI's last segment, "primary%3ADownload%2FSoA_HD.7z").
std::string PackName(std::string pack)
{
  while (!pack.empty() && (pack.back() == '/' || pack.back() == '\\'))
    pack.pop_back();
  std::string decoded;
  for (size_t i = 0; i < pack.size(); ++i)
  {
    if (pack[i] == '%' && i + 2 < pack.size() && std::isxdigit(static_cast<u8>(pack[i + 1])) &&
        std::isxdigit(static_cast<u8>(pack[i + 2])))
    {
      decoded += static_cast<char>(std::strtol(pack.substr(i + 1, 2).c_str(), nullptr, 16));
      i += 2;
    }
    else
    {
      decoded += pack[i];
    }
  }
  const size_t slash = decoded.find_last_of("/\\:");
  std::string name = slash == std::string::npos ? decoded : decoded.substr(slash + 1);
  const std::string lower = Lower(name);
  for (const char* ext : {".7z", ".zip"})
  {
    if (lower.ends_with(ext))
      name.resize(name.size() - std::strlen(ext));
  }
  return name.empty() ? "Texture pack" : name;
}

void ListFolder(const File::FSTEntry& entry, const std::string& prefix,
                std::vector<std::pair<std::string, std::string>>* files)
{
  for (const File::FSTEntry& child : entry.children)
  {
    const std::string relative = prefix + child.virtualName;
    if (child.isDirectory)
      ListFolder(child, relative + "/", files);
    else
      files->emplace_back(relative, child.physicalName);
  }
}

// Whether a DDS file's first 132 bytes are a DX10 header of DXGI_FORMAT_BC7_UNORM (98) or _SRGB (99)
bool IsBc7(const u8* header, size_t size)
{
  if (size < 132 || std::memcmp(header, "DDS ", 4) != 0 || std::memcmp(header + 84, "DX10", 4) != 0)
    return false;
  u32 format;
  std::memcpy(&format, header + 128, 4);
  return format == 98 || format == 99;
}

struct Marker
{
  std::vector<std::string> names;  // the packs imported into the folder, in order
  int textures = 0;
  int bc7 = 0;
};

std::optional<Marker> ReadMarker(const std::string& textures_dir)
{
  std::string text;
  if (!File::ReadFileToString(textures_dir + GAME_FOLDER + DIR_SEP + MARKER, text))
    return std::nullopt;
  Marker marker;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line))
  {
    const size_t eq = line.find('=');
    if (eq == std::string::npos)
      continue;
    const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
    if (key == "name")
      marker.names.push_back(value);
    else if (key == "textures")
      marker.textures = std::atoi(value.c_str());
    else if (key == "bc7")
      marker.bc7 = std::atoi(value.c_str());
  }
  return marker;
}

std::string Native(std::string path)
{
#ifdef _WIN32
  std::ranges::replace(path, '/', '\\');
#endif
  return path;
}

// Writes the pack's textures into Load/Textures/GEA.importing/, then adds them to GEA/. Or, with
// ExtractOnly, one archive inside the pack into its temp folder (GEA.unpacking/), to import that.
class Installer
{
public:
  Installer(const std::string& textures_dir, std::function<void(const std::string&)> status,
            const char* temp = ".importing")
      : m_textures_dir(textures_dir), m_temp(textures_dir + GAME_FOLDER + temp + DIR_SEP),
        m_status(std::move(status))
  {
  }

  // Unpacks only the pack's entry (an archive's path in it, as Look got it) into Temp()
  void ExtractOnly(std::string entry) { m_extract = std::move(entry); }

  // Starts the import of total textures
  bool Start(size_t total)
  {
    m_total = total;
    if (File::Exists(m_temp) && !File::DeleteDirRecursively(m_temp))
      return Fail("Can't clear " + m_temp);
    if (!File::CreateFullPath(m_temp))
      return Fail("Can't create " + m_temp);
    if (m_extract && m_status)
      m_status("Unpacking it from the pack");
    return true;
  }

  // Called with every entry of the pack before Target: a pack with a GEA folder gives only the
  // textures in it (others beside it are optional variants), one without (the textures in folders
  // of their own, "UI/Main/tex1_....dds") gives all of its textures. The archives in it are kept
  // for a pack without textures (physical: a folder's file on disk).
  void Look(const std::string& pack_path, const std::string& physical = {})
  {
    const std::optional<std::string> inside = InGameFolder(pack_path);
    m_game_folder |= inside && IsTexture(FileName(*inside));
    std::string path = pack_path;
    std::ranges::replace(path, '\\', '/');
    if (IsArchive(FileName(path)) && !IsMacResourceFork(path))
      m_archives.emplace_back(pack_path, physical);
  }

  // Where an entry of the pack goes in the game folder, or nullopt to skip it.
  std::optional<std::string> Target(const std::string& pack_path) const
  {
    if (m_extract)
    {
      std::string path = pack_path;
      std::ranges::replace(path, '\\', '/');
      if (pack_path != *m_extract || !StaysInside(FileName(path)))
        return std::nullopt;
      return std::optional(FileName(path));
    }
    std::optional<std::string> inside;
    if (m_game_folder)
    {
      inside = InGameFolder(pack_path);
    }
    else
    {
      inside = pack_path;
      std::ranges::replace(*inside, '\\', '/');
      if (IsMacResourceFork(*inside))
        return std::nullopt;
    }
    if (!inside || !IsTexture(FileName(*inside)) || !StaysInside(*inside))
      return std::nullopt;
    return inside;
  }

  bool Begin(const std::string& relative)
  {
    const size_t slash = relative.rfind('/');
    const std::string file_name = slash == std::string::npos ? relative : relative.substr(slash + 1);
    // The same texture twice (a pack's copies in two folders): Dolphin would load one of them
    // anyway and log the other as an error.
    if (!m_names.insert(Lower(file_name)).second)
    {
      m_skip = true;
      return true;
    }
    m_skip = false;
    m_written.push_back(relative);
    const std::string path = Native(m_temp + relative);
    File::CreateFullPath(path);
    m_file = File::IOFile(path, "wb");
    if (!m_file.IsOpen())
      return Fail("Can't write " + path);
    m_header.clear();
    m_dds = Lower(file_name).ends_with(".dds");
    m_mip = IsMipLevel(file_name);
    return true;
  }

  bool Write(const u8* data, size_t size)
  {
    if (m_skip)
      return true;
    if (m_dds && m_header.size() < 132)
      m_header.insert(m_header.end(), data, data + std::min(size, 132 - m_header.size()));
    // Well above any real pack (the 4x pack, all six archives, is 8.7 GB unpacked; a nested archive
    // adds its own size): a decompression bomb stops here instead of filling the disk (review D22)
    constexpr u64 MAX_BYTES = u64{24} << 30;
    if (m_bytes + size > MAX_BYTES)
      return Fail("The pack unpacks to more than 24 GB; is it really a texture pack?");
    if (!m_file.WriteBytes(data, size))
      return Fail("Can't write the textures. Is the disk full?");
    // An archive in the pack can be gigabytes
    constexpr u64 MB = 1 << 20;
    if (m_extract && m_status && (m_bytes + size) / (64 * MB) != m_bytes / (64 * MB))
      m_status(fmt::format("Unpacking it from the pack: {} MB", (m_bytes + size) / MB));
    m_bytes += size;
    return true;
  }

  bool End()
  {
    if (m_skip)
      return true;
    if (!m_file.Close())
      return Fail("Can't write the textures. Is the disk full?");
    ++m_files;
    m_textures += m_mip ? 0 : 1;
    m_bc7 += !m_mip && IsBc7(m_header.data(), m_header.size()) ? 1 : 0;
    if (m_status && !m_extract && (m_files % 20 == 0 || m_files == m_total))
      m_status(fmt::format("Unpacking textures: {} of {} files", m_files, m_total));
    return true;
  }

  // Adds this pack to the imported ones (a pack can come as several archives, one per kind of
  // texture): its textures go into GEA/, replacing the ones there of the same name.
  bool Commit(const std::string& name)
  {
    if (m_textures == 0)
      return Fail("No textures were unpacked.");
    const std::string target = m_textures_dir + GAME_FOLDER;
    std::string temp = m_temp;
    temp.pop_back();
    Marker marker{{name}, m_textures, m_bc7};
    if (!File::Exists(target))
    {
      if (!File::Rename(temp, target))
        return Fail("Can't move the textures into " + target);
    }
    else
    {
      std::vector<std::pair<std::string, std::string>> files;
      ListFolder(File::ScanDirectoryTree(target, true), "", &files);
      std::map<std::string, std::string> imported;  // by file name: Dolphin's key
      for (const auto& [relative, physical] : files)
        imported.emplace(Lower(FileName(relative)), physical);
      // Each new texture is in before the old one of its name goes (review D22: an error part-way
      // used to leave textures deleted and not replaced); a rename over the same path replaces it.
      for (const std::string& relative : m_written)
      {
        const std::string path = Native(target + "/" + relative);
        File::CreateFullPath(path);
        if (!File::Rename(Native(m_temp + relative), path))
          return Fail("Can't move the textures into " + target);
        if (const auto it = imported.find(Lower(FileName(relative))); it != imported.end())
        {
          std::error_code error;
          if (!std::filesystem::equivalent(StringToPath(it->second), StringToPath(path), error))
            File::Delete(it->second);
        }
      }
      File::DeleteDirRecursively(temp);

      // The counts of everything there now (the earlier packs', a folder put there by hand)
      const std::optional<Marker> before = ReadMarker(m_textures_dir);
      marker.names = before ? before->names : std::vector<std::string>{};
      if (std::ranges::find(marker.names, name) == marker.names.end())
        marker.names.push_back(name);
      marker.textures = marker.bc7 = 0;
      files.clear();
      ListFolder(File::ScanDirectoryTree(target, true), "", &files);
      for (const auto& [relative, physical] : files)
      {
        const std::string file_name = FileName(relative);
        if (!IsTexture(file_name) || IsMipLevel(file_name))
          continue;
        ++marker.textures;
        std::array<u8, 132> header;
        File::IOFile file(physical, "rb");
        if (Lower(file_name).ends_with(".dds") && file.ReadArray(&header) &&
            IsBc7(header.data(), header.size()))
        {
          ++marker.bc7;
        }
      }
    }
    std::string text;
    for (const std::string& pack : marker.names)
      text += fmt::format("name={}\n", pack);
    text += fmt::format("textures={}\nbc7={}\n", marker.textures, marker.bc7);
    const std::string marker_path = Native(target + "/" + MARKER);
    if (!File::WriteStringToFile(marker_path, text))
      return Fail("Can't write " + marker_path);
    return true;
  }

  void Abort()
  {
    m_file.Close();
    File::DeleteDirRecursively(m_temp);
  }

  bool Fail(std::string error)
  {
    if (m_error.empty())
      m_error = std::move(error);
    return false;
  }

  const std::string& Error() const { return m_error; }
  int Textures() const { return m_textures; }
  int Bc7() const { return m_bc7; }
  const std::string& Temp() const { return m_temp; }
  // The archives in the pack: its path in the pack, and its file for a folder's
  const std::vector<std::pair<std::string, std::string>>& Archives() const { return m_archives; }

private:
  std::string m_textures_dir;
  std::string m_temp;
  std::function<void(const std::string&)> m_status;
  bool m_game_folder = false;
  std::vector<std::pair<std::string, std::string>> m_archives;
  std::optional<std::string> m_extract;
  u64 m_bytes = 0;
  size_t m_total = 0;
  std::set<std::string> m_names;
  File::IOFile m_file;
  bool m_skip = false;
  bool m_dds = false;
  bool m_mip = false;
  size_t m_files = 0;
  std::vector<u8> m_header;
  std::vector<std::string> m_written;  // the textures in m_temp, relative
  int m_textures = 0;
  int m_bc7 = 0;
  std::string m_error;
};

// ---- An unpacked folder ----


bool ImportFolder(const std::string& folder, Installer& installer, size_t* count)
{
  std::vector<std::pair<std::string, std::string>> files;
  // The folder's own name counts too: the player may pick the GEA folder itself
  std::string base = folder;
  while (!base.empty() && (base.back() == '/' || base.back() == '\\'))
    base.pop_back();
  const size_t slash = base.find_last_of("/\\");
  ListFolder(File::ScanDirectoryTree(folder, true),
             (slash == std::string::npos ? base : base.substr(slash + 1)) + "/", &files);
  for (const auto& [relative, physical] : files)
    installer.Look(relative, physical);
  std::vector<std::pair<std::string, std::string>> wanted;
  for (const auto& [relative, physical] : files)
  {
    if (std::optional<std::string> target = installer.Target(relative))
      wanted.emplace_back(*target, physical);
  }
  *count = wanted.size();
  if (wanted.empty() || !installer.Start(wanted.size()))
    return false;
  std::vector<u8> buffer(CHUNK);
  for (const auto& [target, physical] : wanted)
  {
    File::IOFile in(physical, "rb");
    if (!in.IsOpen())
      return installer.Fail("Can't read " + physical);
    if (!installer.Begin(target))
      return false;
    u64 left = in.GetSize();
    while (left > 0)
    {
      const size_t n = static_cast<size_t>(std::min<u64>(left, buffer.size()));
      if (!in.ReadBytes(buffer.data(), n))
        return installer.Fail("Can't read " + physical);
      if (!installer.Write(buffer.data(), n))
        return false;
      left -= n;
    }
    if (!installer.End())
      return false;
  }
  return true;
}

// ---- .zip ----

bool ImportZip(const std::string& path, Installer& installer, size_t* count)
{
  void* reader = mz_zip_reader_create();
  if (!reader)
    return installer.Fail("Out of memory.");
  Common::ScopeGuard guard{[&] { mz_zip_reader_delete(&reader); }};
  if (mz_zip_reader_open_file(reader, path.c_str()) != MZ_OK)
    return installer.Fail("Can't read that .zip file. Is it damaged?");
  // First the textures in it, then unpack them (the reader walks the entries in order)
  std::vector<std::string> names;
  for (int32_t err = mz_zip_reader_goto_first_entry(reader); err == MZ_OK;
       err = mz_zip_reader_goto_next_entry(reader))
  {
    mz_zip_file* info;
    if (mz_zip_reader_entry_get_info(reader, &info) == MZ_OK && info->filename &&
        mz_zip_reader_entry_is_dir(reader) != MZ_OK)
    {
      names.emplace_back(info->filename);
      installer.Look(names.back());
    }
  }
  *count = static_cast<size_t>(std::ranges::count_if(names, [&](const std::string& name) {
    return installer.Target(name).has_value();
  }));
  if (*count == 0 || !installer.Start(*count))
    return false;
  std::vector<u8> buffer(CHUNK);
  for (int32_t err = mz_zip_reader_goto_first_entry(reader); err == MZ_OK;
       err = mz_zip_reader_goto_next_entry(reader))
  {
    mz_zip_file* info;
    if (mz_zip_reader_entry_get_info(reader, &info) != MZ_OK || !info->filename ||
        mz_zip_reader_entry_is_dir(reader) == MZ_OK)
    {
      continue;
    }
    const std::optional<std::string> target = installer.Target(info->filename);
    if (!target)
      continue;
    if (mz_zip_reader_entry_open(reader) != MZ_OK)
      return installer.Fail(fmt::format("Can't read {} in the .zip file.", info->filename));
    Common::ScopeGuard entry_guard{[&] { mz_zip_reader_entry_close(reader); }};
    if (!installer.Begin(*target))
      return false;
    for (;;)
    {
      const int32_t n =
          mz_zip_reader_entry_read(reader, buffer.data(), static_cast<int32_t>(buffer.size()));
      if (n < 0)
        return installer.Fail(fmt::format("Can't read {} in the .zip file.", info->filename));
      if (n == 0)
        break;
      if (!installer.Write(buffer.data(), static_cast<size_t>(n)))
        return false;
    }
    if (!installer.End())
      return false;
  }
  return true;
}

// ---- .7z (the LZMA SDK's archive reader; LZMA / LZMA2 folders decoded as a stream) ----

#ifdef HAVE_LZMA_SDK
void* SzMalloc(ISzAllocPtr, size_t size)
{
  return size == 0 ? nullptr : std::malloc(size);
}
void SzRelease(ISzAllocPtr, void* address)
{
  std::free(address);
}
const ISzAlloc s_alloc = {SzMalloc, SzRelease};

struct FileSeekStream
{
  ISeekInStream vt;  // first: the SDK's pointer is this struct's
  File::IOFile* file;
};

SRes FileRead(ISeekInStreamPtr p, void* buf, size_t* size)
{
  File::IOFile* file = reinterpret_cast<const FileSeekStream*>(p)->file;
  size_t read = 0;
  if (*size != 0 && !file->ReadArray(static_cast<u8*>(buf), *size, &read))
    file->ClearError();  // a short read at the end; IOFile would fail every later call
  *size = read;
  return SZ_OK;
}

SRes FileSeek(ISeekInStreamPtr p, Int64* pos, ESzSeek origin)
{
  File::IOFile* file = reinterpret_cast<const FileSeekStream*>(p)->file;
  const File::SeekOrigin from = origin == SZ_SEEK_SET ? File::SeekOrigin::Begin :
                                origin == SZ_SEEK_CUR ? File::SeekOrigin::Current :
                                                        File::SeekOrigin::End;
  if (!file->Seek(*pos, from))
  {
    file->ClearError();
    return SZ_ERROR_READ;
  }
  *pos = static_cast<Int64>(file->Tell());
  return SZ_OK;
}

constexpr UInt32 METHOD_COPY = 0;
constexpr UInt32 METHOD_LZMA2 = 0x21;
constexpr UInt32 METHOD_LZMA = 0x30101;

// Decodes folder f (one compressed stream holding files back to back) and hands its bytes to out
// in order. LZMA2, LZMA and stored folders are decoded as a stream, with a dictionary no larger
// than the folder; other methods (filters, PPMd) through the SDK, all in memory.
std::string DecodeFolder(File::IOFile& file, const CSzArEx& db, ILookInStreamPtr look, UInt32 f,
                         const std::function<bool(const u8*, size_t)>& out)
{
  constexpr char DAMAGED[] = "The .7z file is damaged (or not completely downloaded).";
  constexpr char MEMORY[] = "Not enough memory to unpack this .7z file.";
  const CSzAr& ar = db.db;
  const Byte* data = ar.CodersData + ar.FoCodersOffsets[f];
  CSzData sd{data, ar.FoCodersOffsets[f + 1] - ar.FoCodersOffsets[f]};
  CSzFolder folder;
  if (SzGetNextFolderItem(&folder, &sd) != SZ_OK)
    return DAMAGED;
  const UInt64 unpack_size = SzAr_GetFolderUnpackSize(&ar, f);
  const UInt32 method = folder.Coders[0].MethodID;
  if (folder.NumCoders != 1 || folder.NumPackStreams != 1 ||
      (method != METHOD_COPY && method != METHOD_LZMA2 && method != METHOD_LZMA))
  {
    if (unpack_size > (u64{1} << 30))
      return "This .7z file uses a compression the import can't unpack. Unpack it, then import "
             "the folder.";
    std::unique_ptr<u8[]> buffer(new (std::nothrow) u8[std::max<size_t>(unpack_size, 1)]);
    if (!buffer)
      return MEMORY;
    if (SzAr_DecodeFolder(&ar, f, look, db.dataPos, buffer.get(), unpack_size, &s_alloc) != SZ_OK)
      return DAMAGED;
    return out(buffer.get(), unpack_size) ? "" : "-";
  }

  const UInt32 pack_index = ar.FoStartPackStreamIndex[f];
  UInt64 in_left = ar.PackPositions[pack_index + 1] - ar.PackPositions[pack_index];
  if (!file.Seek(db.dataPos + ar.PackPositions[pack_index], File::SeekOrigin::Begin))
    return DAMAGED;
  std::vector<u8> in(CHUNK), outbuf(CHUNK);
  size_t in_pos = 0, in_len = 0;
  const auto refill = [&] {
    if (in_pos < in_len || in_left == 0)
      return true;
    const size_t n = static_cast<size_t>(std::min<UInt64>(in.size(), in_left));
    if (!file.ReadBytes(in.data(), n))
      return false;
    in_pos = 0;
    in_len = n;
    in_left -= n;
    return true;
  };

  if (method == METHOD_COPY)
  {
    while (in_left > 0 || in_pos < in_len)
    {
      if (!refill())
        return DAMAGED;
      if (!out(in.data() + in_pos, in_len - in_pos))
        return "-";
      in_pos = in_len;
    }
    return "";
  }

  const Byte* props = data + folder.Coders[0].PropsOffset;
  const unsigned props_size = folder.Coders[0].PropsSize;
  const bool lzma2 = method == METHOD_LZMA2;
  UInt64 dictionary;
  if (lzma2)
  {
    if (props_size != 1 || props[0] > 40)
      return DAMAGED;
    dictionary = props[0] == 40 ? 0xFFFFFFFF : u64{2u | (props[0] & 1u)} << (props[0] / 2 + 11);
  }
  else
  {
    if (props_size != LZMA_PROPS_SIZE)
      return DAMAGED;
    dictionary = props[1] | (props[2] << 8) | (props[3] << 16) | (UInt64{props[4]} << 24);
  }
  // The window never needs to be larger than everything decoded
  const size_t window = static_cast<size_t>(std::max<UInt64>(std::min(dictionary, unpack_size), 1));
  std::unique_ptr<u8[]> dic(new (std::nothrow) u8[window]);
  if (!dic)
    return MEMORY;

  CLzma2Dec dec2;
  CLzmaDec dec;
  Lzma2Dec_CONSTRUCT(&dec2);
  LzmaDec_CONSTRUCT(&dec);
  if (lzma2 ? Lzma2Dec_AllocateProbs(&dec2, props[0], &s_alloc) != SZ_OK :
              LzmaDec_AllocateProbs(&dec, props, props_size, &s_alloc) != SZ_OK)
  {
    return MEMORY;
  }
  Common::ScopeGuard probs_guard{[&] {
    if (lzma2)
      Lzma2Dec_FreeProbs(&dec2, &s_alloc);
    else
      LzmaDec_FreeProbs(&dec, &s_alloc);
  }};
  CLzmaDec& core = lzma2 ? dec2.decoder : dec;
  core.dic = dic.get();
  core.dicBufSize = window;
  if (lzma2)
    Lzma2Dec_Init(&dec2);
  else
    LzmaDec_Init(&dec);

  UInt64 done = 0;
  while (done < unpack_size)
  {
    if (!refill())
      return DAMAGED;
    SizeT out_len = static_cast<SizeT>(std::min<UInt64>(outbuf.size(), unpack_size - done));
    SizeT in_avail = in_len - in_pos;
    ELzmaStatus status;
    const SRes res =
        lzma2 ? Lzma2Dec_DecodeToBuf(&dec2, outbuf.data(), &out_len, in.data() + in_pos, &in_avail,
                                     LZMA_FINISH_ANY, &status) :
                LzmaDec_DecodeToBuf(&dec, outbuf.data(), &out_len, in.data() + in_pos, &in_avail,
                                    LZMA_FINISH_ANY, &status);
    in_pos += in_avail;
    if (res != SZ_OK)
      return DAMAGED;
    if (out_len == 0 && in_avail == 0 && (in_pos < in_len || in_left == 0))
      return DAMAGED;  // stuck, or the stream ended before its size
    done += out_len;
    if (out_len != 0 && !out(outbuf.data(), out_len))
      return "-";
  }
  return "";
}

bool Import7z(const std::string& path, Installer& installer, size_t* count)
{
  File::IOFile file(path, "rb");
  if (!file.IsOpen())
    return installer.Fail("Can't open that file.");
  FileSeekStream stream{{FileRead, FileSeek}, &file};
  CLookToRead2 look;
  LookToRead2_CreateVTable(&look, False);
  std::vector<Byte> look_buffer(1 << 18);
  look.buf = look_buffer.data();
  look.bufSize = look_buffer.size();
  look.realStream = &stream.vt;
  LookToRead2_INIT(&look);

  static const bool crc_table = [] {
    CrcGenerateTable();
    return true;
  }();
  (void)crc_table;

  CSzArEx db;
  SzArEx_Init(&db);
  Common::ScopeGuard db_guard{[&] { SzArEx_Free(&db, &s_alloc); }};
  if (SzArEx_Open(&db, &look.vt, &s_alloc, &s_alloc) != SZ_OK)
    return installer.Fail("Can't read that .7z file. Is it completely downloaded? Download it again.");

  // Each file's place in the pack, and where it goes (nullopt: skipped)
  std::vector<std::optional<std::string>> targets(db.NumFiles);
  std::vector<std::string> names(db.NumFiles);
  std::vector<UInt16> name16;
  for (UInt32 i = 0; i < db.NumFiles; ++i)
  {
    if (SzArEx_IsDir(&db, i))
      continue;
    name16.resize(SzArEx_GetFileNameUtf16(&db, i, nullptr));
    SzArEx_GetFileNameUtf16(&db, i, name16.data());
    names[i] = UTF16ToUTF8(std::u16string(name16.begin(), name16.end() - 1));
    installer.Look(names[i]);
  }
  *count = 0;
  for (UInt32 i = 0; i < db.NumFiles; ++i)
  {
    if (SzArEx_IsDir(&db, i))
      continue;
    targets[i] = installer.Target(names[i]);
    *count += targets[i] ? 1 : 0;
  }
  if (*count == 0 || !installer.Start(*count))
    return false;

  for (UInt32 f = 0; f < db.db.NumFolders; ++f)
  {
    // The folder's files in stream order, and whether any of them is wanted
    std::vector<UInt32> files;
    bool wanted = false;
    for (UInt32 i = db.FolderToFile[f]; i < db.NumFiles; ++i)
    {
      if (db.FileToFolder[i] == f)
      {
        files.push_back(i);
        wanted |= targets[i].has_value();
      }
      else if (db.FileToFolder[i] != static_cast<UInt32>(-1))
      {
        break;
      }
    }
    if (!wanted)
      continue;
    size_t current = 0;  // index in files
    UInt64 left = files.empty() ? 0 : SzArEx_GetFileSize(&db, files[0]);
    UInt32 crc = CRC_INIT_VAL;
    bool open = false;
    const auto finish_files = [&]() -> bool {
      // Ends every file that is complete (and empty ones), starting the next
      while (current < files.size())
      {
        const UInt32 i = files[current];
        if (!open && targets[i])
        {
          if (!installer.Begin(*targets[i]))
            return false;
          open = true;
        }
        if (left != 0)
          return true;
        if (SzBitWithVals_Check(&db.CRCs, i) && CRC_GET_DIGEST(crc) != db.CRCs.Vals[i])
          return installer.Fail("The .7z file is damaged (a file's checksum doesn't match).");
        if (open && !installer.End())
          return false;
        open = false;
        crc = CRC_INIT_VAL;
        if (++current < files.size())
          left = SzArEx_GetFileSize(&db, files[current]);
      }
      return true;
    };
    if (!finish_files())
      return false;
    const std::string error = DecodeFolder(
        file, db, &look.vt, f, [&](const u8* data, size_t size) {
          while (size > 0 && current < files.size())
          {
            const size_t n = static_cast<size_t>(std::min<UInt64>(left, size));
            crc = CrcUpdate(crc, data, n);
            if (open && !installer.Write(data, n))
              return false;
            data += n;
            size -= n;
            left -= n;
            if (!finish_files())
              return false;
          }
          return true;
        });
    if (error == "-")
      return false;  // the installer has the error
    if (!error.empty())
      return installer.Fail(error);
    if (current < files.size())
      return installer.Fail("The .7z file is damaged (it ends too early).");
  }
  return true;
}
#endif

enum class Kind
{
  Folder,
  Zip,
  SevenZip
};

// Unpacks the pack's entries the installer wants
bool Run(Kind kind, const std::string& pack, Installer& installer, size_t* count)
{
  switch (kind)
  {
  case Kind::Folder:
    return ImportFolder(pack, installer, count);
  case Kind::Zip:
#ifdef ANDROID
  {
    // minizip opens files by path: a content URI's file through the process's descriptor
    File::IOFile file(pack, "rb");
    if (!file.IsOpen())
      return installer.Fail("Can't open that file.");
    return ImportZip(fmt::format("/proc/self/fd/{}", fileno(file.GetHandle())), installer, count);
  }
#else
    return ImportZip(pack, installer, count);
#endif
  case Kind::SevenZip:
#ifdef HAVE_LZMA_SDK
    return Import7z(pack, installer, count);
#else
    return installer.Fail("This build can't read .7z files: unpack the pack, then import its folder.");
#endif
  }
  return false;
}

std::string ImportPack(const std::string& pack, const std::string& textures_dir,
                       const std::function<void(const std::string&)>& status,
                       const std::string& file_name, bool inner, bool* empty);

// A pack without textures of its own but with archives in it (the player put a pack's archives
// into one .zip or folder): each archive is imported as if picked on its own, in name order
// ("01_SoAL_HD_UI.7z" first), unpacked from the pack into GEA.unpacking/ first when it is one.
// Archives without textures are skipped.
std::string ImportArchives(Kind kind, const std::string& pack, const std::string& textures_dir,
                           const std::function<void(const std::string&)>& status,
                           std::vector<std::pair<std::string, std::string>> archives, bool* empty)
{
  std::ranges::sort(archives, {}, [](const auto& archive) { return Lower(archive.first); });
  int imported = 0;
  for (size_t i = 0; i < archives.size(); ++i)
  {
    const auto& [entry, physical] = archives[i];
    std::string path = entry;
    std::ranges::replace(path, '\\', '/');
    const std::string name = FileName(path);
    const auto step = [&, i](const std::string& text) {
      if (status)
        status(fmt::format("{} ({} of {}): {}", name, i + 1, archives.size(), text));
    };
    Installer extractor(textures_dir, step, ".unpacking");
    Common::ScopeGuard extracted{[&] { extractor.Abort(); }};
    path = physical;
    if (path.empty())
    {
      extractor.ExtractOnly(entry);
      size_t found = 0;
      if (!Run(kind, pack, extractor, &found))
      {
        return extractor.Error().empty() ? fmt::format("Can't unpack {} from the pack.", name) :
                                           extractor.Error();
      }
      path = Native(extractor.Temp() + name);
    }
    bool none = false;
    const std::string error = ImportPack(path, textures_dir, step, name, true, &none);
    if (none)
      continue;
    if (!error.empty())
    {
      return fmt::format("{} in the pack: {}{}", name, error,
                         imported == 0 ? "" : " The archives before it are imported.");
    }
    ++imported;
  }
  if (imported != 0)
    return "";
  *empty = true;
  return "No Skies of Arcadia textures in that pack or the archives in it: they need tex1_ .png or "
         ".dds files.";
}

// Imports one pack (inner: an archive in a pack, which doesn't look for archives in it). "" or
// what went wrong; *empty when it has no textures.
std::string ImportPack(const std::string& pack, const std::string& textures_dir,
                       const std::function<void(const std::string&)>& status,
                       const std::string& file_name, bool inner, bool* empty)
{
  if (status)
    status("Reading the texture pack");
  Kind kind;
  if (File::IsDirectory(pack))
  {
    kind = Kind::Folder;
  }
  else
  {
    File::IOFile probe(pack, "rb");
    std::array<u8, 6> magic{};
    if (!probe.IsOpen() || !probe.ReadArray(&magic))
      return "Can't open that file.";
    if (magic[0] == 'P' && magic[1] == 'K')
      kind = Kind::Zip;
    else if (std::memcmp(magic.data(), "7z\xBC\xAF\x27\x1C", 6) == 0)
      kind = Kind::SevenZip;
    else
      return "That isn't a texture pack: pick the .7z or .zip file you downloaded.";
  }

  size_t count = 0;  // textures found in the pack
  Installer installer(textures_dir, status);
  if (!Run(kind, pack, installer, &count))
  {
    installer.Abort();
    if (count == 0 && installer.Error().empty())
    {
      if (!inner && !installer.Archives().empty())
        return ImportArchives(kind, pack, textures_dir, status, installer.Archives(), empty);
      *empty = true;
      return "No Skies of Arcadia textures in that pack: it needs tex1_ .png or .dds files.";
    }
    return installer.Error().empty() ? "The import stopped." : installer.Error();
  }
  const std::string base = PackName(file_name.empty() ? pack : file_name);
  const std::string name = kind == Kind::Folder && Lower(base) == "gea" ? "Texture pack" : base;
  if (!installer.Commit(name))
  {
    installer.Abort();
    return installer.Error();
  }
  return "";
}
}  // namespace

std::string Import(const std::string& pack, const std::string& textures_dir, std::string* summary,
                   const std::function<void(const std::string&)>& status,
                   const std::string& file_name)
{
  bool empty = false;
  const std::string error = ImportPack(pack, textures_dir, status, file_name, false, &empty);
  if (!error.empty())
    return error;
  if (summary)
    *summary = Describe(textures_dir);
  return "";
}

std::string Describe(const std::string& textures_dir)
{
  if (const std::optional<Marker> marker = ReadMarker(textures_dir))
    return fmt::format("{}: {} textures", fmt::join(marker->names, " + "), marker->textures);
  // A folder the player put there by hand
  const std::string folder = textures_dir + GAME_FOLDER;
  if (!File::IsDirectory(folder))
    return "";
  std::vector<std::pair<std::string, std::string>> files;
  ListFolder(File::ScanDirectoryTree(folder, true), "", &files);
  const auto textures = std::ranges::count_if(files, [](const auto& file) {
    const size_t slash = file.first.rfind('/');
    const std::string name = slash == std::string::npos ? file.first : file.first.substr(slash + 1);
    return IsTexture(name) && !IsMipLevel(name);
  });
  return textures == 0 ? "" : fmt::format("{} textures", textures);
}

int Bc7Count(const std::string& textures_dir)
{
  const std::optional<Marker> marker = ReadMarker(textures_dir);
  return marker ? marker->bc7 : 0;
}

bool Remove(const std::string& textures_dir)
{
  const std::string folder = textures_dir + GAME_FOLDER;
  return !File::Exists(folder) || File::DeleteDirRecursively(folder);
}
}  // namespace TexturePackImport
