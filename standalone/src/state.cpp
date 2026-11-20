// Save states: the whole machine in one file. Every part writes its own state through StateIO
// (state.h) in a fixed order with a named marker before each; the result is zstd-compressed behind
// a header that names the game, the DOL and the recompiled module, since a state only makes sense
// with the very code it was taken from.
//
// The files are where and as the Dolphin build kept its states (menu.cpp: StateSaves/state-skies-
// slotN.sav, recovery-skies-*.sav), so the launchers list, Continue from, move and back them up as
// before; the header starts as Dolphin's does (the game ID first) and tells the two apart. A Dolphin
// build's state loads too: dolphin_state.cpp converts it.
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
#include "dolphin_state.h"
#include "host.h"
#include "state.h"

#include <zstd.h>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>

namespace host
{
double g_run_start_seconds = 0.0;

bool g_state_request = false;

namespace
{
namespace fs = std::filesystem;

constexpr u32 MAGIC = 0x54534B53;  // "SKST"
constexpr u32 VERSION = 2;
// Dolphin's version cookie (StateFile.h COOKIE_BASE + STATE_VERSION) of the states the Dolphin build
// writes
constexpr u32 DOLPHIN_COOKIE = 0xBAADBABE + 191;
// Dolphin's state time: seconds since 2000-01-01 (Common::Timer DOUBLE_TIME_OFFSET)
constexpr double DOLPHIN_TIME_OFFSET = 946684800.0;
constexpr u64 MAX_RAW_SIZE = 512ull << 20;
// A request waits for a display copy this long at most (a game that stops drawing, a load screen)
constexpr u64 FRAME_WAIT = CPU_HZ / 2;

struct Header
{
  // As a Dolphin state starts (StateFile.h StateHeaderLegacy, then the version cookie): the game ID,
  // which the launcher reads to move a version's states into its folder (MoveDiscStates), and a
  // cookie an older Dolphin build reads as a version it doesn't know, so it refuses the file cleanly
  char game_id[6];
  u8 reserved1[2];
  u32 lzo_size;  // 0
  u8 reserved2[4];
  double dolphin_time;  // when it was saved, as Dolphin counts
  u32 magic;
  u32 version_string_length;  // 0
  u32 version;
  u32 reserved3;
  u64 dol_hash;     // FNV-1a 64 of the DOL booted
  u64 module_hash;  // ModuleFingerprint
  double guest_seconds;
  s64 saved_at;  // time(), for the record
  u64 raw_size;  // the state before compression
};
static_assert(sizeof(Header) == 80);

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
std::string s_user_dir, s_state_dir;

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

// Compression and the file write run on a background thread: the game thread only serializes (its
// copy of the machine), so a save no longer holds the game for the whole 130-330 ms. Jobs are written
// in order; a load or the end of the run waits for them; results come back to the game thread
// (StateFrameEnd) for the menu's message.
struct WriteJob
{
  std::string path;
  Header header;
  std::vector<u8> raw;
  std::string message;
  double serialize_ms = 0;
  u64 frame = 0;
};
struct WriteResult
{
  bool ok = false;
  std::string error;
  std::string message;
};
std::mutex s_write_lock;
std::condition_variable s_write_cv;
std::deque<WriteJob> s_write_jobs;
std::vector<WriteResult> s_write_results;
bool s_write_busy = false;
bool s_write_quit = false;
std::thread s_writer;  // joined at the end of the run (a detached one waiting on the condition variable
                       // hung the exit when the statics were destroyed)

void WriteJobNow(WriteJob& job)
{
  const auto start = std::chrono::steady_clock::now();
  WriteResult result{false, {}, job.message};
  const size_t bound = ZSTD_compressBound(job.raw.size());
  std::vector<u8> out(sizeof(Header) + bound);
  std::memcpy(out.data(), &job.header, sizeof(Header));
  const size_t packed = ZSTD_compress(out.data() + sizeof(Header), bound, job.raw.data(), job.raw.size(), 1);
  if (ZSTD_isError(packed))
    result.error = std::string("compression failed: ") + ZSTD_getErrorName(packed);
  else
  {
    out.resize(sizeof(Header) + packed);
    result.ok = WriteAtomic(job.path, out, &result.error);
  }
  if (result.ok)
  {
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    Log("[state] saved %s at %.3f s guest, frame %llu (%.1f MB, %.1f MB packed; %.0f ms on the game thread, "
        "%.0f ms packing and writing)\n",
        job.path.c_str(), job.header.guest_seconds, static_cast<unsigned long long>(job.frame),
        job.raw.size() / 1048576.0, out.size() / 1048576.0, job.serialize_ms, ms);
  }
  std::lock_guard lk(s_write_lock);
  s_write_results.push_back(std::move(result));
}

void WriterThread()
{
  std::unique_lock lk(s_write_lock);
  for (;;)
  {
    s_write_cv.wait(lk, [] { return !s_write_jobs.empty() || s_write_quit; });
    if (s_write_jobs.empty())
      return;
    WriteJob job = std::move(s_write_jobs.front());
    s_write_jobs.pop_front();
    s_write_busy = true;
    lk.unlock();
    WriteJobNow(job);
    lk.lock();
    s_write_busy = false;
    s_write_cv.notify_all();
  }
}

// Until every queued save is on disk (before a load, which may read one, and at the end of the run)
void WaitForWrites()
{
  std::unique_lock lk(s_write_lock);
  s_write_cv.wait(lk, [] { return s_write_jobs.empty() && !s_write_busy; });
}

// Serializes now and queues the rest; false (and the error) only for what fails here.
bool Save(const std::string& path, const std::string& message)
{
  const auto start = std::chrono::steady_clock::now();
  WriteJob job;
  job.path = path;
  job.message = message;
  job.raw.reserve(72u << 20);
  StateIO p(&job.raw, false);
  DoAll(p);

  Header& header = job.header;
  header = {};
  std::memcpy(header.game_id, DiscHeader(), 6);
  header.saved_at = static_cast<s64>(std::time(nullptr));
  header.dolphin_time = static_cast<double>(header.saved_at) - DOLPHIN_TIME_OFFSET;
  header.magic = MAGIC;
  header.version = VERSION;
  header.dol_hash = DolHash();
  header.module_hash = ModuleFingerprint();
  header.guest_seconds = static_cast<double>(g_cycles) / CPU_HZ;
  header.raw_size = job.raw.size();
  job.frame = GpuFrameCount();
  job.serialize_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

  std::lock_guard lk(s_write_lock);
  if (!s_writer.joinable())
    s_writer = std::thread(WriterThread);
  s_write_jobs.push_back(std::move(job));
  s_write_cv.notify_all();
  return true;
}

// The finished saves' messages, on the game thread (the menu's list is its own)
void TakeWriteResults()
{
  std::vector<WriteResult> results;
  {
    std::lock_guard lk(s_write_lock);
    results.swap(s_write_results);
  }
  for (const WriteResult& r : results)
  {
    if (!r.ok)
      MenuMessage("Couldn't save the state: " + r.error, 5000);
    else if (!r.message.empty())
      MenuMessage(r.message, 2500);
  }
}

// After a load: Aurora gets the EFB copies and the GP registers, the Dreamcast music its song, the
// wall-clock pacing starts over
void Loaded(const std::string& path, double guest_seconds, std::chrono::steady_clock::time_point start,
            const char* what)
{
  static bool s_first = true;
  if (s_first)
    g_run_start_seconds = guest_seconds;
  s_first = false;
  CpuRestoreBootDol();
  DiscAdoptGuestFst();
  VideoStateLoaded();
#ifdef SKIES_STANDALONE_AURORA
  DreamcastStateLoaded();
#endif
  DffReloadAurora();
  ThrottleRestart();
  const double ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  Log("[state] loaded %s%s: %.3f s guest, frame %llu (%.0f ms)\n", what, path.c_str(), guest_seconds,
      static_cast<unsigned long long>(GpuFrameCount()), ms);
}

// The Dolphin build's state (dolphin_state.cpp), with this host's state from before kept to go back to
bool LoadDolphin(const std::string& path, const std::vector<u8>& bytes,
                 std::chrono::steady_clock::time_point start, std::string* error)
{
  // The video thread finishes the frames it has (they read guest RAM) before the RAM changes
  VideoSync([] {});
  std::vector<u8> backup;
  {
    StateIO w(&backup, false);
    w.with_gpu = false;
    DoAll(w);
  }
  double guest_seconds = 0;
  if (!DolphinStateApply(bytes, &guest_seconds, error))
  {
    StateIO b(&backup, true);
    DoAll(b);
    return false;
  }
  Loaded(path, guest_seconds, start, "the Dolphin build's state ");
  return true;
}

bool Load(const std::string& path, std::string* error)
{
  WaitForWrites();
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
  if (header.magic != MAGIC && header.magic != DOLPHIN_COOKIE)
  {
    *error = "not a save state";
    return false;
  }
  if (std::memcmp(header.game_id, DiscHeader(), 6) != 0)
  {
    *error = "a save state of another game (" + std::string(header.game_id, 6) + ")";
    return false;
  }
  if (header.magic == DOLPHIN_COOKIE)
    return LoadDolphin(path, bytes, start, error);
  if (header.version != VERSION)
  {
    *error = "a save state of another version (" + std::to_string(header.version) + ")";
    return false;
  }
  // As the Dolphin build loads its states: one from before an update (another game patch or module
  // build) loads too, since the game's data and the CPU's registers are the same kind of thing
  if (header.dol_hash != DolHash() || header.module_hash != ModuleFingerprint())
    Log("[state] %s was saved with another game patch or module build\n", path.c_str());
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

  // The video thread finishes the frames it has (they read guest RAM) before the RAM changes
  VideoSync([] {});
  // What to go back to if the state doesn't fit
  std::vector<u8> backup;
  backup.reserve(raw.size());
  {
    StateIO w(&backup, false);
    w.with_gpu = false;
    DoAll(w);
  }
  StateIO r(&raw, true);
  // A damaged or crafted state may also throw (a list the memory can't hold): back to the state before it
  try
  {
    DoAll(r);
  }
  catch (const std::exception& e)
  {
    r.Fail(std::string("a damaged save state (") + e.what() + ")");
  }
  if (!r.Ok())
  {
    *error = r.Error();
    StateIO b(&backup, true);
    DoAll(b);
    return false;
  }
  Loaded(path, header.guest_seconds, start, "");
  return true;
}

void Queue(Kind kind, const std::string& path, const std::string& message)
{
  s_request = {kind, path, message, g_cycles};
  s_frame_ended = false;
  g_state_request = true;
}
}  // namespace

void StateSetUserDir(const std::string& user_dir, const std::string& state_dir)
{
  s_user_dir = user_dir;
  s_state_dir = state_dir;
  s_save_at = ParseTestAt("SKIES_SAVE_AT");
  s_load_at = ParseTestAt("SKIES_LOAD_AT");
}

std::string StateDirectory()
{
  if (s_user_dir.empty())
    return {};
  return s_state_dir.empty() ? (fs::path(s_user_dir) / "StateSaves").string() : s_state_dir;
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
  TakeWriteResults();
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
    Save(request.path, request.message);
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
void StateFinishWrites()
{
  {
    std::lock_guard lk(s_write_lock);
    s_write_quit = true;
    s_write_cv.notify_all();
  }
  if (s_writer.joinable())
    s_writer.join();  // after the queued saves (the thread drains them first)
  TakeWriteResults();
}
}  // namespace host
