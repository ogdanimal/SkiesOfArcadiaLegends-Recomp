// Save states: the whole machine in one file. Every part writes its own state through StateIO
// (state.h) in a fixed order with a named marker before each; the result is zstd-compressed behind
// a header that names the game, the DOL and the recompiled module, since a state only makes sense
// with the very code it was taken from.
//
// States are taken and put back by the run loop between two dispatches (cpu.cpp), once the next
// frame's display copy is done (hw_gpu.cpp calls StateFrameEnd): the picture is presented and the
// next frame not started, so after a load Aurora only needs the GP registers again
// (DffReloadAurora). A load that fails partway puts back the state from just before it.
//
// Debug / tests: SKIES_SAVE_AT=SECONDS:PATH saves a state at the first frame end after that guest
// time, SKIES_LOAD_AT=SECONDS:PATH loads one there (as the menu does); --load-state PATH starts
// from one (at the first frame end: Aurora's EFB then is a frame's, as at any later load).
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"
#include "state.h"

#include <zstd.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace host
{
bool g_state_request = false;

namespace
{
namespace fs = std::filesystem;

constexpr u32 MAGIC = 0x54534B53;  // "SKST"
constexpr u32 VERSION = 1;
constexpr u64 MAX_RAW_SIZE = 512ull << 20;
// A request waits for a display copy this long at most (a game that stops drawing, a load screen)
constexpr u64 FRAME_WAIT = CPU_HZ / 2;

struct Header
{
  u32 magic;
  u32 version;
  char game_id[8];
  u64 dol_hash;     // FNV-1a 64 of the DOL booted
  u64 module_hash;  // ModuleFingerprint
  double guest_seconds;
  s64 saved_at;  // time(), for the record
  u64 raw_size;  // the state before compression
};
static_assert(sizeof(Header) == 56);

enum class Kind
{
  None,
  Save,
  Load,
};
struct Request
{
  Kind kind = Kind::None;
  std::string path, message;
  u64 since = 0;
  bool fatal = false;  // --load-state: no game to go back to
};
Request s_request;
bool s_frame_ended = false;
std::string s_user_dir;

// SKIES_SAVE_AT, SKIES_LOAD_AT
struct TestAt
{
  u64 cycles = 0;
  std::string path;
  bool done = false;
};
TestAt s_save_at, s_load_at;

TestAt ParseTestAt(const char* name)
{
  TestAt at;
  if (const char* value = std::getenv(name))
  {
    char* end = nullptr;
    const double seconds = std::strtod(value, &end);
    if (end && *end == ':' && end[1])
    {
      at.cycles = static_cast<u64>(seconds * CPU_HZ);
      at.path = end + 1;
    }
    else
      Log("[state] %s=SECONDS:PATH, not '%s'\n", name, value);
  }
  return at;
}

u64 Fnv(const u8* data, size_t size)
{
  u64 hash = 0xCBF29CE484222325ull;
  for (size_t i = 0; i < size; ++i)
    hash = (hash ^ data[i]) * 0x100000001B3ull;
  return hash;
}

u64 DolHash()
{
  static const u64 hash = Fnv(DiscDol().data(), DiscDol().size());
  return hash;
}

void DoAll(StateIO& p)
{
  p.Marker("scheduler");
  SchedDoState(p);
  p.Marker("cpu");
  CpuDoState(p);
  p.Marker("ram");
  p.DoArray(g_ram, RAM_SIZE);
  p.Marker("pi");
  PiDoState(p);
  p.Marker("vi");
  ViDoState(p);
  p.Marker("gpu");
  GpuDoState(p);
  p.Marker("gp registers");
  DffDoState(p);
  p.Marker("dsp");
  DspDoState(p);
  p.Marker("di");
  DiDoState(p);
  p.Marker("si");
  SiDoState(p);
  p.Marker("exi");
  ExiDoState(p);
  p.Marker("ai");
  AiDoState(p);
  p.Marker("video");
  VideoDoState(p);
  p.Marker("end");
}

bool WriteAtomic(const fs::path& path, const std::vector<u8>& bytes, std::string* error)
{
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  const fs::path temp = path.string() + ".tmp";
  {
    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file)
    {
      *error = "couldn't write " + temp.string();
      return false;
    }
  }
  fs::rename(temp, path, ec);
  if (ec)
  {
    *error = "couldn't replace " + path.string() + ": " + ec.message();
    fs::remove(temp, ec);
    return false;
  }
  return true;
}

bool Save(const std::string& path, std::string* error)
{
  const auto start = std::chrono::steady_clock::now();
  std::vector<u8> raw;
  raw.reserve(72u << 20);
  StateIO p(&raw, false);
  DoAll(p);

  Header header{};
  header.magic = MAGIC;
  header.version = VERSION;
  std::memcpy(header.game_id, DiscHeader(), 6);
  header.dol_hash = DolHash();
  header.module_hash = ModuleFingerprint();
  header.guest_seconds = static_cast<double>(g_cycles) / CPU_HZ;
  header.saved_at = static_cast<s64>(std::time(nullptr));
  header.raw_size = raw.size();

  const size_t bound = ZSTD_compressBound(raw.size());
  std::vector<u8> out(sizeof(Header) + bound);
  std::memcpy(out.data(), &header, sizeof(Header));
  const size_t packed = ZSTD_compress(out.data() + sizeof(Header), bound, raw.data(), raw.size(), 1);
  if (ZSTD_isError(packed))
  {
    *error = std::string("compression failed: ") + ZSTD_getErrorName(packed);
    return false;
  }
  out.resize(sizeof(Header) + packed);
  if (!WriteAtomic(path, out, error))
    return false;
  const double ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  Log("[state] saved %s at %.3f s guest, frame %llu (%.1f MB, %.1f MB packed, %.0f ms)\n", path.c_str(),
      header.guest_seconds, static_cast<unsigned long long>(GpuFrameCount()), raw.size() / 1048576.0,
      out.size() / 1048576.0, ms);
  return true;
}

bool Load(const std::string& path, std::string* error)
{
  const auto start = std::chrono::steady_clock::now();
  std::ifstream file(path, std::ios::binary);
  if (!file)
  {
    *error = "couldn't open " + path;
    return false;
  }
  std::vector<u8> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  Header header{};
  if (bytes.size() < sizeof(Header))
  {
    *error = "not a save state";
    return false;
  }
  std::memcpy(&header, bytes.data(), sizeof(Header));
  if (header.magic != MAGIC)
  {
    *error = "not a save state of this host";
    return false;
  }
  if (header.version != VERSION)
  {
    *error = "a save state of another version (" + std::to_string(header.version) + ")";
    return false;
  }
  if (std::memcmp(header.game_id, DiscHeader(), 6) != 0)
  {
    *error = "a save state of another game (" + std::string(header.game_id, 6) + ")";
    return false;
  }
  if (header.dol_hash != DolHash() || header.module_hash != ModuleFingerprint())
  {
    *error = "a save state of another game patch or build";
    return false;
  }
  if (header.raw_size == 0 || header.raw_size > MAX_RAW_SIZE)
  {
    *error = "a damaged save state";
    return false;
  }
  std::vector<u8> raw(static_cast<size_t>(header.raw_size));
  const size_t unpacked = ZSTD_decompress(raw.data(), raw.size(), bytes.data() + sizeof(Header),
                                          bytes.size() - sizeof(Header));
  if (ZSTD_isError(unpacked) || unpacked != raw.size())
  {
    *error = "a damaged save state";
    return false;
  }
  bytes = {};

  // What to go back to if the state doesn't fit
  std::vector<u8> backup;
  backup.reserve(raw.size());
  {
    StateIO w(&backup, false);
    w.with_gpu = false;
    DoAll(w);
  }
  StateIO r(&raw, true);
  DoAll(r);
  if (!r.Ok())
  {
    *error = r.Error();
    StateIO b(&backup, true);
    DoAll(b);
    return false;
  }
  VideoStateLoaded();
  DffReloadAurora();
  ThrottleRestart();
  const double ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  Log("[state] loaded %s: %.3f s guest, frame %llu (%.0f ms)\n", path.c_str(), header.guest_seconds,
      static_cast<unsigned long long>(GpuFrameCount()), ms);
  return true;
}

void Queue(Kind kind, const std::string& path, const std::string& message)
{
  s_request = {kind, path, message, g_cycles};
  s_frame_ended = false;
  g_state_request = true;
}
}  // namespace

void StateSetUserDir(const std::string& user_dir)
{
  s_user_dir = user_dir;
  s_save_at = ParseTestAt("SKIES_SAVE_AT");
  s_load_at = ParseTestAt("SKIES_LOAD_AT");
}

std::string StateDirectory()
{
  if (s_user_dir.empty())
    return {};
  return (fs::path(s_user_dir) / "StateSaves" / "Standalone").string();
}

void StateRequestSave(const std::string& path, const std::string& message)
{
  Queue(Kind::Save, path, message);
}

void StateRequestLoad(const std::string& path, const std::string& message)
{
  Queue(Kind::Load, path, message);
}

void StateLoadAtStart(const std::string& path)
{
  Queue(Kind::Load, path, "");
  s_request.fatal = true;
}

void StateFrameEnd()
{
  if (g_state_request)
    s_frame_ended = true;
  for (TestAt* at : {&s_save_at, &s_load_at})
  {
    if (at->path.empty() || at->done || g_cycles < at->cycles || g_state_request)
      continue;
    at->done = true;
    Queue(at == &s_save_at ? Kind::Save : Kind::Load, at->path, "");
    s_frame_ended = true;
  }
}

void StateRun()
{
  if (!s_frame_ended && g_cycles - s_request.since < FRAME_WAIT)
    return;
  const Request request = s_request;
  s_request = {};
  g_state_request = false;
  s_frame_ended = false;
  std::string error;
  if (request.kind == Kind::Save)
  {
    if (Save(request.path, &error))
    {
      if (!request.message.empty())
        MenuMessage(request.message, 2500);
    }
    else
      MenuMessage("Couldn't save the state: " + error, 5000);
  }
  else if (request.kind == Kind::Load)
  {
    if (Load(request.path, &error))
    {
      if (!request.message.empty())
        MenuMessage(request.message, 2500);
    }
    else if (request.fatal)
      Fatal("can't load %s: %s\n", request.path.c_str(), error.c_str());
    else
      MenuMessage("Couldn't load the state: " + error, 5000);
  }
}
}  // namespace host
