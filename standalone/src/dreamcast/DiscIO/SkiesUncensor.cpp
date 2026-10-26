// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/SkiesUncensor.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <system_error>
#include <thread>
#include <unordered_map>

#include <fmt/format.h>
#include <picojson.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/Config/SkiesSettings.h"
#include "DiscIO/SkiesUncensorConvert.h"

namespace DiscIO::SkiesUncensor
{
namespace
{
// The background conversions: Core::Shutdown stops them between files and waits for them, so
// none is left using the user paths or the tables while the program's statics are destroyed
// (review D21).
std::atomic<bool> s_stop{false};
std::atomic<int> s_workers{0};

struct Worker
{
  Worker() { s_workers.fetch_add(1); }
  ~Worker() { s_workers.fetch_sub(1); }
  Worker(const Worker&) = delete;
  Worker& operator=(const Worker&) = delete;
};

void WaitForWorkers()
{
  s_stop = true;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (s_workers.load() > 0 && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  s_stop = false;
}

std::string s_user_directory, s_sys_directory;  // SetDirectories(); "" = Dolphin's

std::string SysDirectory()
{
  return s_sys_directory.empty() ? File::GetSysDirectory() : s_sys_directory;
}

struct Manifest
{
  bool loaded = false;
  // game ID -> files/-relative path ("field/a002a.mld") -> the entry
  std::map<std::string, std::map<std::string, FileOps>> games;
};

// One manifest file entry (see SkiesUncensorConvert.h); nullopt when it isn't well formed.
std::optional<FileOps> ParseFileOps(const picojson::value& value)
{
  if (!value.is<picojson::object>())
    return std::nullopt;
  const picojson::object& o = value.get<picojson::object>();
  const auto str = [](const picojson::object& obj, const char* key) -> std::optional<std::string> {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->second.is<std::string>())
      return std::nullopt;
    return it->second.get<std::string>();
  };
  const auto num = [](const picojson::object& obj, const char* key) -> std::optional<u32> {
    const auto it = obj.find(key);
    if (it == obj.end() || !it->second.is<double>())
      return std::nullopt;
    return static_cast<u32>(it->second.get<double>());
  };
  FileOps e;
  const auto gc = str(o, "gc_sha1"), out = str(o, "out_sha1"), dc = str(o, "dc_sha1"),
             dec = str(o, "dec_sha1"), path = str(o, "dc_path");
  const auto size = num(o, "size"), disc = num(o, "dc_disc");
  const auto ops = o.find("ops");
  if (!gc || !out || !dc || !dec || !path || !size || !disc || ops == o.end() ||
      !ops->second.is<picojson::array>())
  {
    return std::nullopt;
  }
  e.gc_sha1 = *gc, e.out_sha1 = *out, e.dc_sha1 = *dc, e.dec_sha1 = *dec, e.dc_path = *path;
  e.size = *size;
  e.dc_disc = static_cast<int>(*disc);
  for (const picojson::value& v : ops->second.get<picojson::array>())
  {
    if (!v.is<picojson::object>())
      return std::nullopt;
    const picojson::object& p = v.get<picojson::object>();
    const auto kind = str(p, "op");
    Op op;
    if (kind == "model")
      op.kind = Op::Kind::Model;
    else if (kind == "tex")
      op.kind = Op::Kind::Tex;
    else if (kind == "tex_add")
      op.kind = Op::Kind::TexAdd;
    else if (kind == "tex_blank")
      op.kind = Op::Kind::TexBlank;
    else if (kind == "mlk_entry")
      op.kind = Op::Kind::MlkEntry;
    else if (kind == "std_records")
      op.kind = Op::Kind::StdRecords;
    else if (kind == "pad")
      op.kind = Op::Kind::Pad;
    else if (kind == "motion")
      op.kind = Op::Kind::Motion;
    else if (kind == "tex_swap")
      op.kind = Op::Kind::TexSwap;
    else
      return std::nullopt;
    op.name = str(p, "name").value_or("");
    op.after = str(p, "after").value_or("");
    op.new_name = str(p, "new_name").value_or("");
    op.gc_sha1 = str(p, "gc_sha1").value_or("");
    op.jp_sha1 = str(p, "jp_sha1").value_or("");
    op.gc_entry = num(p, "gc_entry").value_or(0);
    op.slot = num(p, "slot").value_or(0);
    op.jp_entry = num(p, "jp_entry").value_or(0);
    op.id = num(p, "id").value_or(0);
    op.tex_index = num(p, "tex_index").value_or(0);
    // std_records: "records" [["gc"|"jp", first, count]...], "layouts" {"type/size": "HWB..."}
    if (const auto it = p.find("records"); it != p.end() && it->second.is<picojson::array>())
    {
      for (const picojson::value& r : it->second.get<picojson::array>())
      {
        if (!r.is<picojson::array>() || r.get<picojson::array>().size() != 3)
          return std::nullopt;
        const picojson::array& a = r.get<picojson::array>();
        if (!a[0].is<std::string>() || !a[1].is<double>() || !a[2].is<double>())
          return std::nullopt;
        op.runs.push_back({a[0].get<std::string>() == "jp", static_cast<u32>(a[1].get<double>()),
                           static_cast<u32>(a[2].get<double>())});
      }
    }
    if (const auto it = p.find("layouts"); it != p.end() && it->second.is<picojson::object>())
    {
      for (const auto& [key, layout] : it->second.get<picojson::object>())
      {
        if (!layout.is<std::string>())
          return std::nullopt;
        op.layouts[key] = layout.get<std::string>();
      }
    }
    e.ops.push_back(std::move(op));
  }
  return e;
}

const Manifest& GetManifest()
{
  static std::once_flag once;
  static Manifest manifest;
  std::call_once(once, [] {
    std::string text;
    if (!File::ReadFileToString(SysDirectory() + "Skies/uncensor-manifest.json", text))
      return;
    picojson::value root;
    if (!picojson::parse(root, text).empty() || !root.is<picojson::object>())
      return;
    const picojson::value& games = root.get("games");
    if (!games.is<picojson::object>())
      return;
    for (const auto& [id, files] : games.get<picojson::object>())
    {
      if (!files.is<picojson::object>())
        continue;
      for (const auto& [path, entry] : files.get<picojson::object>())
      {
        if (auto ops = ParseFileOps(entry))
          manifest.games[id][path] = std::move(*ops);
      }
    }
    manifest.loaded = true;
  });
  return manifest;
}

std::string GameId(const std::string& game_root)
{
  File::IOFile boot(game_root + "/sys/boot.bin", "rb");
  char id[6] = {};
  if (!boot.IsOpen() || !boot.ReadBytes(id, sizeof(id)))
    return "";
  return std::string(id, sizeof(id));
}

std::string SourcePath(const FileOps& ops)
{
  return fmt::format("{}dc/disc{}/{}", Directory(), ops.dc_disc, ops.dc_path);
}

std::string ConvertedPath(const std::string& id, const std::string& rel)
{
  return Directory() + id + "/" + rel;
}

// The source file's size and modification time when a converted file was made, kept beside it
// (<converted>.src): a game re-extracted from another (e.g. romhacked) image doesn't use it.
std::string Stamp(const std::string& path)
{
  std::error_code ec;
  const auto time = std::filesystem::last_write_time(StringToPath(path), ec);
  if (ec)
    return "";
  return fmt::format("{} {}", File::GetSize(path),
                     static_cast<long long>(time.time_since_epoch().count()));
}

bool Current(const std::string& game_file, const std::string& converted)
{
  std::string stamp;
  return File::Exists(converted) && File::ReadFileToString(converted + ".src", stamp) &&
         !stamp.empty() && stamp == Stamp(game_file) &&
         File::GetSize(converted) == File::GetSize(game_file);
}

bool ReadAll(const std::string& path, std::vector<u8>* out)
{
  File::IOFile f(path, "rb");
  if (!f.IsOpen())
    return false;
  out->resize(f.GetSize());
  return f.ReadBytes(out->data(), out->size());
}

// Runtime state: physical game file -> converted file (and whether it's made), and which one each
// file reads now.
struct Target
{
  std::string converted;
  bool ready = false;
};
struct Redirects
{
  std::mutex lock;
  std::unordered_map<std::string, Target> files;
  std::unordered_map<std::string, bool> using_converted;
  std::set<std::string> preparing;  // game roots being converted on a thread
};

Redirects& R()
{
  static Redirects* r = new Redirects;  // never destroyed: the disc thread may read during exit
  return *r;
}
}  // namespace

std::string Directory()
{
  return (s_user_directory.empty() ? File::GetUserPath(D_USER_IDX) : s_user_directory) +
         "DreamcastUncensor" DIR_SEP;
}

void SetDirectories(const std::string& user_directory, const std::string& sys_directory)
{
  s_user_directory = user_directory;
  s_sys_directory = sys_directory;
}

std::vector<DreamcastFile> DreamcastFiles()
{
  std::map<std::pair<int, std::string>, std::string> unique;
  for (const auto& [id, files] : GetManifest().games)
  {
    for (const auto& [rel, ops] : files)
      unique[{ops.dc_disc, ops.dc_path}] = ops.dc_sha1;
  }
  std::vector<DreamcastFile> out;
  for (const auto& [key, sha1] : unique)
  {
    FileOps ops;
    ops.dc_disc = key.first;
    ops.dc_path = key.second;
    out.push_back({key.first, key.second, sha1, SourcePath(ops)});
  }
  return out;
}

Summary Describe(const std::string& game_root)
{
  Summary s;
  const auto& games = GetManifest().games;
  const auto game = games.find(GameId(game_root));
  if (game == games.end())
    return s;
  std::set<int> missing;
  for (const auto& [rel, ops] : game->second)
  {
    ++s.total;
    if (File::Exists(SourcePath(ops)))
      ++s.sources;
    else
      missing.insert(ops.dc_disc);
    if (Current(game_root + "/files/" + rel, ConvertedPath(game->first, rel)))
      ++s.ready;
  }
  s.discs_missing.assign(missing.begin(), missing.end());
  return s;
}

std::string Prepare(const std::string& game_root,
                    const std::function<void(const std::string&)>& status, int* made,
                    int* skipped)
{
  if (made)
    *made = 0;
  if (skipped)
    *skipped = 0;
  const Manifest& manifest = GetManifest();
  if (!manifest.loaded)
    return "The uncensor list (Sys/Skies/uncensor-manifest.json) is missing.";
  const std::string id = GameId(game_root);
  const auto game = manifest.games.find(id);
  if (game == manifest.games.end())
    return "";
  int n = 0;
  std::vector<u8> gc, dc, out;
  for (const auto& [rel, ops] : game->second)
  {
    if (s_stop)
      return "Stopped.";
    ++n;
    const std::string game_file = game_root + "/files/" + rel;
    const std::string converted = ConvertedPath(id, rel);
    if (Current(game_file, converted) || !File::Exists(SourcePath(ops)))
      continue;
    if (status)
      status(fmt::format("Converting {} ({} of {})", rel, n, game->second.size()));
    std::string error;
    if (!ReadAll(game_file, &gc) || !ReadAll(SourcePath(ops), &dc) ||
        !Convert(ops, gc, dc, &out, &error))
    {
      // Not the expected file (another release, a romhack): this one stays as it is.
      File::Delete(converted);
      if (skipped)
        ++*skipped;
      continue;
    }
    const std::string temp = converted + ".part";
    if (!File::CreateFullPath(converted) ||
        !File::WriteStringToFile(temp, std::string(out.begin(), out.end())) ||
        !File::Rename(temp, converted) ||
        !File::WriteStringToFile(converted + ".src", Stamp(game_file)))
    {
      return "Can't write " + converted + ".";
    }
    if (made)
      ++*made;
  }
  if (status)
    status("");
  return "";
}

void StopBackgroundWork()
{
  WaitForWorkers();
}

bool Remove()
{
  return !File::Exists(Directory()) || File::DeleteDirRecursively(Directory());
}

void Register(const std::string& physical_path)
{
  // …/<game>/files/<rel>: the game's ID from …/<game>/sys/boot.bin
  std::string path = physical_path;
  std::replace(path.begin(), path.end(), '\\', '/');
  const std::size_t files = path.rfind("/files/");
  if (files == std::string::npos)
    return;
  const std::string root = path.substr(0, files);
  std::string rel = path.substr(files + 7);
  Common::ToLower(&rel);
  const auto& games = GetManifest().games;
  static std::map<std::string, std::string> ids;  // game root -> ID (DirectoryBlob's thread only)
  auto id = ids.find(root);
  if (id == ids.end())
    id = ids.emplace(root, GameId(root)).first;
  const auto game = games.find(id->second);
  if (game == games.end())
    return;
  const auto entry = game->second.find(rel);
  if (entry == game->second.end())
    return;
  const std::string converted = ConvertedPath(id->second, rel);
  const bool ready = Current(physical_path, converted);
  Redirects& r = R();
  std::lock_guard lk(r.lock);
  r.files[physical_path] = {converted, ready};
  // Files not converted yet (a first run after the import, or a game extracted again) are made on a
  // thread while the game runs; each is used from the next time the game loads it.
  if (!ready && File::Exists(SourcePath(entry->second)) && !r.preparing.contains(root))
  {
    r.preparing.insert(root);
    std::thread([root] {
      const Worker worker;
      const bool stopped = Prepare(root, {}) == "Stopped.";
      Redirects& rr = R();
      std::lock_guard lock(rr.lock);
      if (stopped)
        rr.preparing.erase(root);  // the next disc build starts it again
      for (auto& [physical, redirect] : rr.files)
      {
        if (!redirect.ready)
          redirect.ready = Current(physical, redirect.converted);
      }
    }).detach();
  }
}

std::string Redirect(const std::string& physical_path, u64 offset_in_file)
{
  Redirects& r = R();
  std::lock_guard lk(r.lock);
  const auto it = r.files.find(physical_path);
  if (it == r.files.end() || !it->second.ready)
    return physical_path;
  bool& use = r.using_converted[physical_path];
  if (offset_in_file == 0)
  {
    use = Config::Get(Config::SKIES_UNCENSORED) != 0;
    // One line per load of a file, so a log shows which art the game actually got
    if (use)
      NOTICE_LOG_FMT(DISCIO, "Uncensored: {} from {}", physical_path, it->second.converted);
  }
  return use ? it->second.converted : physical_path;
}

bool Available()
{
  Redirects& r = R();
  std::lock_guard lk(r.lock);
  return std::any_of(r.files.begin(), r.files.end(), [](const auto& f) { return f.second.ready; });
}
}  // namespace DiscIO::SkiesUncensor
