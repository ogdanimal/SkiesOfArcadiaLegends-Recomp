// The Dolphin build's Dreamcast features on the standalone host (feature-gap item 7): its modules are
// in standalone/src/dreamcast/ as they are on dev (DiscIO/SkiesUncensor*, DiscIO/SkiesStreams with
// SkiesPal60Data.inc, AudioCommon/DCMusic*, Core/SkiesMusic), behind the Dolphin headers they include
// (standalone/src/dolphin_shim/, on dreamcast_host.h, implemented here). This file wires them in as
// the Dolphin build does:
//
// - Disc reads (disc.cpp): every file of the extracted game registered as the disc opens
//   (DirectoryBlob), each read served from the Dreamcast stream, the European file's 60 Hz copy or
//   the uncensored file when there is one (latched at offset 0, in the modules).
// - The Dreamcast music: SkiesMusic::Poll at each pad poll (the game thread, as Dolphin's CPU thread),
//   the AICA rendered in real time by an SDL audio stream of its own, mixed with the game's by SDL
//   (Dolphin mixes it in its audio thread: it plays at real speed whatever the game's speed, and
//   stops while the game doesn't run). SKIES_DC_MUSIC_SYNC=1 renders it into the game's audio in
//   guest time instead (deterministic runs and SKIES_WAV dumps); 2 renders it nowhere (a test: the
//   game's audio with its music muted for the Dreamcast's).
// - --import-dc IMAGE: the Dreamcast disc imported (the music, and with Eternal Arcadia the
//   uncensored files), then converted for --game (Android's SkiesImport.cpp order).
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

#include "dreamcast_host.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

#include "AudioCommon/DCMusic.h"
#include "AudioCommon/DCMusicImport.h"
#include "Core/HW/Memmap.h"
#include "Core/SkiesMusic.h"
#include "DiscIO/SkiesStreams.h"
#include "DiscIO/SkiesUncensor.h"
#include "texture_pack_file.h"

#ifdef SKIES_STANDALONE_AURORA
#include <SDL3/SDL_audio.h>
#endif

namespace
{
std::string s_sys_dir = SKIES_SYS_DEFAULT;
std::atomic<long long> s_last_field_ns{0};

long long NowNs()
{
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

std::string WithSeparator(std::string dir)
{
  if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
    dir += DIR_SEP;
  return dir;
}
}  // namespace

// ---- what the modules need (dreamcast_host.h) -----------------------------------------------------
namespace dchost
{
std::string UserDir()
{
  return WithSeparator(host::SettingsUserDir().empty() ? std::string(".") : host::SettingsUserDir());
}

std::string CacheDir()
{
  return UserDir() + "Cache" DIR_SEP;
}

std::string SysDir()
{
  return WithSeparator(s_sys_dir);
}

void Log(const std::string& line)
{
  host::Log("%s\n", line.c_str());
}

int MainSetting(const char* section, const char* key, int default_value)
{
  return host::SettingGet(host::SettingFile::Main, section, key, default_value);
}

bool RamReady()
{
  return host::g_ram != nullptr;
}

std::uint32_t Read32(std::uint32_t address)
{
  return host::Read32(address);
}

std::uint8_t Read8(std::uint32_t address)
{
  return host::Read8(address);
}

void Write32(std::uint32_t value, std::uint32_t address)
{
  host::Write32(address, value);
}

void CopyFromRam(void* out, std::uint32_t address, std::size_t size)
{
  if (const u8* p = host::RamPtr(address, static_cast<u32>(size)))
    std::memcpy(out, p, size);
  else
    std::memset(out, 0, size);
}

void Message(const std::string& text, std::uint32_t milliseconds)
{
  host::MenuMessage(text, static_cast<int>(milliseconds));
}

const std::string& GameId()
{
  static const std::string id(reinterpret_cast<const char*>(host::DiscHeader()), 6);
  return id;
}

// The game's fields come at least every 20 ms (50 Hz); a quarter second without one is a game
// that stopped (Android in the background, a load that blocks, the end)
bool GameRunning()
{
  const long long last = s_last_field_ns.load(std::memory_order_relaxed);
  return last != 0 && NowNs() - last < 250'000'000;
}
}  // namespace dchost

namespace host
{
namespace
{
// SKIES_DC_MUSIC_SYNC (read at first use: Android sets the environment in main, after static
// initialisation)
int SyncMode()
{
  static const int mode = [] {
    const char* v = std::getenv("SKIES_DC_MUSIC_SYNC");
    return v ? std::atoi(v) : 0;
  }();
  return mode;
}

bool SyncMusic()
{
  return SyncMode() != 0;
}

#ifdef SKIES_STANDALONE_AURORA
// The Dreamcast music's own stream on the default playback device, opened when the music first
// runs; SDL mixes it with the game's (Aurora's) stream
SDL_AudioStream* s_stream = nullptr;
constexpr int STREAM_RATE = 48000;

void SDLCALL MixCallback(void*, SDL_AudioStream* stream, int additional, int)
{
  static std::vector<s16> buffer;
  const int frames = additional / 4;
  if (frames <= 0)
    return;
  buffer.assign(static_cast<size_t>(frames) * 2, 0);
  DCMusic::Mix(buffer.data(), static_cast<size_t>(frames), STREAM_RATE);
  // The Volume setting, as the game's own audio (video.cpp AudioPushSamples)
  const int volume = MenuVolume();
  if (volume < 100)
  {
    for (s16& s : buffer)
      s = static_cast<s16>(s * volume / 100);
  }
  SDL_PutAudioStreamData(stream, buffer.data(), frames * 4);
  // SKIES_DREAMCAST_DEBUG=1: the stream's level every 5 s of audio
  static const bool s_debug = std::getenv("SKIES_DREAMCAST_DEBUG") != nullptr;
  if (s_debug)
  {
    static double s_sum = 0;
    static u64 s_frames = 0;
    for (const s16 s : buffer)
      s_sum += static_cast<double>(s) * s;
    s_frames += static_cast<u64>(frames);
    if (s_frames >= 5 * STREAM_RATE)
    {
      Log("[dcmusic] stream: %.1f s of audio, rms %.0f\n", static_cast<double>(s_frames) / STREAM_RATE,
          std::sqrt(s_sum / (2.0 * static_cast<double>(s_frames))));
      s_sum = 0;
      s_frames = 0;
    }
  }
}

void OpenStream()
{
  static bool s_tried = false;
  if (s_tried || SyncMusic() || std::getenv("SKIES_MUTE"))
    return;
  s_tried = true;
  const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, STREAM_RATE};
  s_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, MixCallback, nullptr);
  if (!s_stream)
  {
    Log("[dcmusic] no audio stream for the Dreamcast music: %s\n", SDL_GetError());
    return;
  }
  SDL_ResumeAudioStreamDevice(s_stream);
  Log("[dcmusic] the Dreamcast music plays on its own audio stream (%d Hz)\n", STREAM_RATE);
}
#endif
}  // namespace

void DreamcastSetSysDir(const std::string& dir)
{
  s_sys_dir = dir;
}

void DreamcastDiscFile(const std::string& path)
{
  DiscIO::SkiesStreams::Register(path);
  DiscIO::SkiesUncensor::Register(path);
}

std::string DreamcastRedirect(const std::string& path, u64 offset_in_file)
{
  return DiscIO::SkiesUncensor::Redirect(DiscIO::SkiesStreams::Redirect(path, offset_in_file),
                                         offset_in_file);
}

void DreamcastPoll()
{
  static Memory::MemoryManager s_memory;
  // SKIES_DREAMCAST_DEBUG=1: the poll's time every 600 polls
  static const bool s_debug = std::getenv("SKIES_DREAMCAST_DEBUG") != nullptr;
  const auto start = std::chrono::steady_clock::now();
  SkiesMusic::Poll(s_memory);
  if (s_debug)
  {
    static double s_total = 0, s_max = 0;
    static int s_polls = 0;
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    s_total += ms;
    s_max = std::max(s_max, ms);
    if (++s_polls == 600)
    {
      Log("[dcmusic] poll: %.3f ms on average, %.2f at most\n", s_total / s_polls, s_max);
      s_total = s_max = 0;
      s_polls = 0;
    }
  }
#ifdef SKIES_STANDALONE_AURORA
  if (DCMusic::Running())
    OpenStream();
#endif
}

void DreamcastStateLoaded()
{
  static Memory::MemoryManager s_memory;
  SkiesMusic::OnStateLoaded(s_memory);
}

void DreamcastFieldTick()
{
  s_last_field_ns.store(NowNs(), std::memory_order_relaxed);
}

void DreamcastShutdown()
{
  DiscIO::SkiesUncensor::StopBackgroundWork();
  DiscIO::SkiesStreams::StopBackgroundWork();
#ifdef SKIES_STANDALONE_AURORA
  if (s_stream)
  {
    SDL_DestroyAudioStream(s_stream);
    s_stream = nullptr;
  }
#endif
  DCMusic::Shutdown();
}

bool DreamcastMusicSync()
{
  return SyncMusic();
}

void DreamcastAudio(s16* samples, u32 frames)
{
  if (SyncMode() == 1)
    DCMusic::Mix(samples, frames, 32000);
}

bool DreamcastMusicInstalled()
{
  return DCMusic::Installed();
}

bool DreamcastUncensorAvailable()
{
  return DiscIO::SkiesUncensor::Available();
}

int DreamcastImportCommand(const std::string& image, const std::string& game_dir)
{
  const auto status = [](const std::string& text) {
    if (!text.empty())
      Log("%s\n", text.c_str());
  };
  // The Japanese discs (Eternal Arcadia) also give the Uncensored setting's files
  std::vector<DCMusicImport::ExtraFile> extra;
  for (const auto& f : DiscIO::SkiesUncensor::DreamcastFiles())
    extra.push_back({f.disc, f.path, f.sha1, f.dest});
  std::string summary;
  int copied = 0;
  std::string error = DCMusicImport::Import(image, DCMusic::Directory(), &summary, status, extra, &copied);
  if (!error.empty())
  {
    Log("Import failed: %s\n", error.c_str());
    return 1;
  }
  Log("Imported. %s%s\n", summary.c_str(),
      copied > 0 ? (" (and " + std::to_string(copied) + " files for Uncensored)").c_str() : "");
  if (!game_dir.empty())
  {
    int made = 0, skipped = 0;
    error = DiscIO::SkiesUncensor::Prepare(game_dir, status, &made, &skipped);
    if (!error.empty())
    {
      Log("Uncensored: %s\n", error.c_str());
      return 1;
    }
    const auto s = DiscIO::SkiesUncensor::Describe(game_dir);
    Log("Uncensored: %d made now, %d skipped (another release); %d of %d files ready\n", made, skipped,
        s.ready, s.total);
  }
  return 0;
}
}  // namespace host
