// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AudioCommon/DCMusic.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Core/Core.h"
#include "Core/System.h"

// Highly Theoretical's emulation headers define bare type macros (uint32, sint16, ...), so they
// come last and the macros are dropped again right after.
extern "C" {
#include "dcsound.h"
#include "arm.h"
#include "yam.h"
}
#undef uint8
#undef uint16
#undef uint32
#undef uint64
#undef sint8
#undef sint16
#undef sint32
#undef sint64

namespace DCMusic
{
namespace
{
constexpr u32 AICA_RATE = 44100;
// The Dreamcast driver at full master volume plays the same song about 1.4x (3 dB) quieter than
// the GameCube's MusyX mix without its effects (battle theme m0459, 50 s from its start, RMS 2355
// against 3412). With the reverb on (dcmusic-fx) that 1.4x made it louder than the GameCube music
// (issue #19; the whole mix, Dreamcast against GameCube music: a deck battle 1.35x, Pirate Isle
// 1.12x), so the gain is their geometric mean lower, 1.13x: within 1 dB of the GameCube music in
// both, and the reverb's peaks (25.7k) fit.
constexpr int DEFAULT_GAIN = 290;
// After a stop the chip is rendered this much longer, for the notes' release.
constexpr int TAIL_SAMPLES = 2 * AICA_RATE;

// Manatee host commands (the Dreamcast sd library's codes; dc-manatee's host_protocol.md).
enum : u8
{
  CMD_MIDI_PLAY = 0x01,
  CMD_MIDI_STOP = 0x02,
  CMD_STOP_ALL = 0x80,
  CMD_MASTER_VOL = 0x81,
  CMD_FX_OUT = 0x82,
  CMD_FX_PRG = 0x84,
  CMD_MIDI_SEND_MES = 0x86,
};

using Record = std::array<u8, 16>;

struct Request
{
  enum class Kind
  {
    Command,   // record goes to the driver
    Download,  // the MLT named mlt goes into sound RAM
    Fade,      // the level goes to level over fade_ms, then after
    Freeze,    // the chip stops (once a running fade has ended) until Thaw
    Thaw,
  };
  Kind kind = Kind::Command;
  Record record{};
  std::string mlt;
  std::shared_ptr<const std::string> data;  // Download: the MLT, read on the caller's thread
  float level = 1.0f;
  int fade_ms = 0;
  AfterFade after = AfterFade::None;
};

u32 Read32(const u8* p)
{
  return p[0] | p[1] << 8 | p[2] << 16 | static_cast<u32>(p[3]) << 24;
}

// One emulated AICA with the Dreamcast driver, playing one song on its MIDI port 0. There are two
// (PLAYERS): a paused song keeps its chip, frozen, while the next one plays on the other.
class Engine
{
public:
  bool Load(const std::string& dir, const std::string& common)
  {
    std::string drv;
    if (!File::ReadFileToString(dir + "BONITO.DRV", drv) || drv.size() < 0x120 ||
        std::memcmp(drv.data(), "SDRV", 4) != 0)
    {
      return false;
    }

    const u8* d = reinterpret_cast<const u8*>(drv.data());
    // The image's size from the header, at most the file's, and no more than the chip's 2 MB of RAM.
    // A size word below 0x20 wrapped to ~4 GB and the upload read that far past the file: a damaged
    // or crafted disc crashed every start (review D3).
    const u32 size = std::min<u32>(Read32(d + 8), static_cast<u32>(drv.size()));
    if (size < 0x120 || size - 0x20 > 0x200000)
      return false;
    const u32 image_size = size - 0x20;

    m_state.reset(new u8[dcsound_get_state_size()]);
    dcsound_clear_state(m_state.get());
    dcsound_upload_to_ram(m_state.get(), 0, const_cast<u8*>(d + 0x20), image_size);
    // The driver's work table (sd library's sdDrvGetInfo): port status, system block (the host
    // command flag), bank table, host command buffer.
    const u8* table = d + 0x20 + 0xC0;
    m_ports = Read32(table + 4);
    m_flag = Read32(table + 8);
    m_banks = Read32(table + 12);
    m_commands = Read32(table + 16);
    if (m_ports >= 0x200000 || m_flag >= 0x200000 || m_banks >= 0x200000 || m_commands >= 0x200000)
      return false;

    // Let the driver initialise (the SH-4 side waits a while after releasing the ARM too), then
    // download the common banks the Dreamcast game loads at boot.
    Render(nullptr, AICA_RATE / 4);
    if (!DownloadUnits(common))
      return false;
    // The reverb: sdSndSetFxPrg(0, 0) loads ARESNEW.MLT's DSP program (its only one, "ares2_1st")
    // and opens the DSP's outputs with its only output set. Without it the voices' effect sends
    // went into a DSP with no program and silent outputs (issue #8). The sd library sends 0x84,
    // then 0x82 in the same record; sdSndStopAll as sent here (bytes 6 and 7 zero) keeps both.
    Record fx{};
    fx[0] = CMD_FX_PRG;
    m_pending.push_back(fx);
    fx[0] = CMD_FX_OUT;
    m_pending.push_back(fx);
    Record master{};
    master[0] = CMD_MASTER_VOL;
    master[2] = 0xF0;
    m_pending.push_back(master);
    return true;
  }

  void Queue(Request&& request)
  {
    std::lock_guard lk(m_queue_lock);
    m_queue.push_back(std::move(request));
  }

  void SetGain(int gain) { m_gain = std::clamp(gain, 0, 1024); }

  // Adds the chip's output, if it has anything to play.
  void Mix(s16* out, std::size_t num_samples, u32 out_rate)
  {
    if (!out_rate)
      return;
    if (m_frozen)
    {
      // Only requests can wake it (Thaw, a new Play, a Stop).
      if (HasQueued())
        ApplyRequests();
      if (m_frozen)
        return;
    }
    if (!m_playing && m_tail <= 0 && m_pending.empty() && !HasQueued())
      return;
    const double step = static_cast<double>(AICA_RATE) / out_rate;
    for (std::size_t i = 0; i < num_samples; ++i)
    {
      const float gain = m_gain / 256.0f * m_level;
      // Hermite interpolation between m_hist[1] and m_hist[2].
      while (m_phase >= 1.0)
      {
        m_phase -= 1.0;
        if (m_buffer_pos >= m_buffer_len)
        {
          Refill();
          if (m_frozen)
            return;  // a fade to pause ended
        }
        for (int k = 0; k < 3; ++k)
        {
          m_hist[k][0] = m_hist[k + 1][0];
          m_hist[k][1] = m_hist[k + 1][1];
        }
        m_hist[3][0] = m_buffer[m_buffer_pos * 2];
        m_hist[3][1] = m_buffer[m_buffer_pos * 2 + 1];
        ++m_buffer_pos;
        StepFade();
      }
      const float t = static_cast<float>(m_phase);
      for (int c = 0; c < 2; ++c)
      {
        const float y0 = m_hist[0][c], y1 = m_hist[1][c], y2 = m_hist[2][c], y3 = m_hist[3][c];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        const float v = ((c3 * t + c2) * t + c1) * t + y1;
        out[i * 2 + c] = MathUtil::SaturatingCast<s16>(out[i * 2 + c] + v * gain);
      }
      m_phase += step;
    }
  }

private:
  static constexpr std::size_t CHUNK = 256;

  bool HasQueued()
  {
    std::lock_guard lk(m_queue_lock);
    return !m_queue.empty();
  }

  // Fades run per AICA sample, so their time is real time whatever the output rate.
  void StartFade(float level, int ms, AfterFade after)
  {
    m_level_target = std::max(level, 0.0f);
    m_fade_left = std::max(0, ms) * static_cast<int>(AICA_RATE) / 1000;
    m_level_step = m_fade_left > 0 ? (m_level_target - m_level) / m_fade_left : 0.0f;
    m_after = after;
    if (m_fade_left == 0)
      FinishFade();
  }

  void StepFade()
  {
    if (m_fade_left <= 0)
      return;
    m_level += m_level_step;
    if (--m_fade_left == 0)
      FinishFade();
  }

  void FinishFade()
  {
    m_level = m_level_target;
    if (m_after == AfterFade::Stop)
      Send(CMD_MIDI_STOP);
    else if (m_after == AfterFade::Pause)
      m_frozen = true;
    m_after = AfterFade::None;
  }

  void Send(u8 code)
  {
    Record r{};
    r[0] = code;
    if (code == CMD_STOP_ALL)
      r[2] = r[3] = r[4] = r[5] = 1;
    m_pending.push_back(r);
    if (code == CMD_MIDI_STOP || code == CMD_STOP_ALL)
    {
      m_playing = false;
      m_tail = TAIL_SAMPLES;
    }
  }

  u32 Get32(u32 a) const { return dcsound_getword(m_state.get(), a); }
  u8 Get8(u32 a) const { return static_cast<u8>(Get32(a & ~3u) >> ((a & 3) * 8)); }
  void Set8(u32 a, u8 v)
  {
    u32 w = Get32(a & ~3u);
    const int shift = (a & 3) * 8;
    w = (w & ~(0xFFu << shift)) | static_cast<u32>(v) << shift;
    dcsound_setword(m_state.get(), a & ~3u, w);
  }

  void Refill()
  {
    ApplyRequests();
    if (m_frozen)
      return;
    Render(m_buffer, CHUNK);
    m_buffer_len = CHUNK;
    m_buffer_pos = 0;
    if (!m_playing && m_tail > 0)
      m_tail -= static_cast<int>(CHUNK);
  }

  void Render(s16* buffer, std::size_t samples)
  {
    s16 scratch[CHUNK * 2];
    while (samples > 0)
    {
      u32 n = static_cast<u32>(std::min(samples, CHUNK));
      s16* dst = buffer ? buffer : scratch;
      u32 got = n;
      if (dcsound_execute(m_state.get(), 0x7FFFFFFF, dst, &got) < 0 || got == 0)
      {
        // The ARM crashed; keep the output silent rather than spin.
        std::fill(dst, dst + n * 2, s16{0});
        got = n;
      }
      samples -= got;
      if (buffer)
        buffer += got * 2;
    }
  }

  // Takes requests from the queue: commands go out to the driver in batches (it reads the batch
  // when the flag byte is set and clears the flag when done); a download waits until the driver
  // has taken every command before it, so a Stop reaches it before its bank is overwritten.
  void ApplyRequests()
  {
    if (Get8(m_flag) != 0)
      return;
    if (!m_pending.empty())
    {
      SendBatch();
      return;
    }
    std::lock_guard lk(m_queue_lock);
    while (!m_queue.empty())
    {
      Request& r = m_queue.front();
      switch (r.kind)
      {
      case Request::Kind::Fade:
        StartFade(r.level, r.fade_ms, r.after);
        break;
      case Request::Kind::Freeze:
        // A running fade (the music fading out before a pause) ends first.
        if (m_fade_left > 0)
          m_after = AfterFade::Pause;
        else
          m_frozen = true;
        break;
      case Request::Kind::Thaw:
        m_frozen = false;
        break;
      case Request::Kind::Download:
        if (!m_pending.empty())
          goto send;
        if (r.data && !r.data->empty())
          DownloadUnits(*r.data);
        break;
      case Request::Kind::Command:
        if (r.record[0] == CMD_MIDI_PLAY)
        {
          // A new song starts at its own level, on a running chip.
          StartFade(1.0f, 0, AfterFade::None);
          m_frozen = false;
          m_playing = true;
        }
        else if (r.record[0] == CMD_MIDI_STOP || r.record[0] == CMD_STOP_ALL)
        {
          m_frozen = false;
          m_after = AfterFade::None;
          m_playing = false;
          m_tail = TAIL_SAMPLES;
        }
        m_pending.push_back(r.record);
        if (m_pending.size() == 30)
        {
          m_queue.pop_front();
          goto send;
        }
        break;
      }
      m_queue.pop_front();
    }
  send:
    if (!m_pending.empty())
      SendBatch();
  }

  void SendBatch()
  {
    u8 batch[32 * 16] = {};
    std::size_t n = std::min<std::size_t>(m_pending.size(), 30);
    for (std::size_t i = 0; i < n; ++i)
      std::memcpy(batch + i * 16, m_pending[i].data(), 16);
    m_pending.erase(m_pending.begin(), m_pending.begin() + n);
    if (n & 1)
      ++n;
    dcsound_upload_to_ram(m_state.get(), m_commands, batch, static_cast<u32>(n * 16));
    Set8(m_flag, 1);
  }

  // An MLT (SMLT) file: 0x20-byte unit entries from 0x20, each a type, a bank number, the bank's
  // sound RAM address and size, and the offset and size of the data the file carries for it. As
  // the sd library's sdMultiUnitDownload: the bank table entry is written, then the data copied.
  bool DownloadUnits(const std::string& file)
  {
    const u8* d = reinterpret_cast<const u8*>(file.data());
    if (file.size() < 0x20 || std::memcmp(d, "SMLT", 4) != 0)
      return false;
    const u32 count = Read32(d + 8);
    for (u32 i = 0; i < count && 0x40 + i * 0x20 <= file.size(); ++i)
    {
      const u8* e = d + 0x20 + i * 0x20;
      const u32 num = Read32(e + 4), addr = Read32(e + 8), size = Read32(e + 12);
      const u32 offset = Read32(e + 16), data_size = Read32(e + 20);
      int slot = -1;
      if (!std::memcmp(e, "SMSB", 4) && num < 16)
        slot = num * 8;
      else if (!std::memcmp(e, "SMPB", 4) && num < 16)
        slot = 0x80 + num * 8;
      else if (!std::memcmp(e, "SOSB", 4) && num < 16)
        slot = 0x100 + num * 8;
      else if (!std::memcmp(e, "SPSR", 4) && num < 16)
        slot = 0x180 + num * 8;
      else if (!std::memcmp(e, "SFPB", 4))
        slot = 0x200;
      else if (!std::memcmp(e, "SFOB", 4))
        slot = 0x280;
      else if (!std::memcmp(e, "SFPW", 4))
        slot = 0x288;
      if (slot >= 0 && static_cast<s32>(size) >= 0 && addr < 0x200000)
      {
        dcsound_setword(m_state.get(), m_banks + slot, addr);
        dcsound_setword(m_state.get(), m_banks + slot + 4, size);
      }
      if (offset != 0xFFFFFFFF && data_size > 0 && offset <= file.size() &&
          data_size <= file.size() - offset && addr < 0x200000 && data_size <= 0x200000 - addr)
      {
        dcsound_upload_to_ram(m_state.get(), addr, const_cast<u8*>(d + offset), data_size);
      }
    }
    return true;
  }

  std::unique_ptr<u8[]> m_state;
  u32 m_ports = 0, m_flag = 0, m_banks = 0, m_commands = 0;

  std::mutex m_queue_lock;
  std::deque<Request> m_queue;
  std::vector<Record> m_pending;

  s16 m_buffer[CHUNK * 2] = {};
  std::size_t m_buffer_len = 0;
  std::size_t m_buffer_pos = 0;
  float m_hist[4][2] = {};
  double m_phase = 0.0;
  int m_gain = DEFAULT_GAIN;
  float m_level = 1.0f, m_level_target = 1.0f, m_level_step = 0.0f;
  int m_fade_left = 0;
  AfterFade m_after = AfterFade::None;
  bool m_frozen = false;   // paused: not rendered at all
  bool m_playing = false;  // a song was started and not stopped
  int m_tail = 0;          // samples still to render after a stop
};

std::mutex s_lock;  // guards s_engines against Start / Shutdown while the audio thread mixes
std::array<std::unique_ptr<Engine>, PLAYERS> s_engines;
// The MLTs read so far, by name: read by Download on its own thread (the CPU thread), not in the
// audio callback under the queue lock, which a 4 MB read stalled (review D17). Emptied by Start
// and Shutdown (Android keeps the process, and an import may have changed the files).
std::mutex s_files_lock;
std::map<std::string, std::shared_ptr<const std::string>, std::less<>> s_files;
int s_gain = DEFAULT_GAIN;
bool s_initialized = false;

void Send(int player, Request&& request)
{
  std::lock_guard lk(s_lock);
  if (player >= 0 && player < PLAYERS && s_engines[player])
    s_engines[player]->Queue(std::move(request));
}

Request Command(u8 code)
{
  Request r;
  r.record[0] = code;  // [2] = port 0
  return r;
}

Request Kind(Request::Kind kind)
{
  Request r;
  r.kind = kind;
  return r;
}
}  // namespace

std::string Directory()
{
  return File::GetUserPath(D_USER_IDX) + "DreamcastMusic" DIR_SEP;
}

bool Installed()
{
  const std::string dir = Directory();
  return File::Exists(dir + "BONITO.DRV") && File::Exists(dir + "ARESNEW.MLT") &&
         File::IsDirectory(dir + "SOUND");
}

bool Start()
{
  std::lock_guard lk(s_lock);
  if (s_engines[0])
    return true;
  if (!s_initialized)
  {
    arm_init();
    yam_init();
    dcsound_init();
    s_initialized = true;
  }
  const std::string dir = Directory();
  std::string common;
  std::array<std::unique_ptr<Engine>, PLAYERS> engines;
  bool ok = File::ReadFileToString(dir + "ARESNEW.MLT", common);
  for (auto& engine : engines)
  {
    engine = std::make_unique<Engine>();
    ok = ok && engine->Load(dir, common);
    engine->SetGain(s_gain);
  }
  if (!ok)
  {
    WARN_LOG_FMT(AUDIO, "Dreamcast music: the files in {} couldn't be loaded", dir);
    return false;
  }
  s_engines = std::move(engines);
  {
    std::lock_guard files_lk(s_files_lock);
    s_files.clear();
  }
  INFO_LOG_FMT(AUDIO, "Dreamcast music started");
  return true;
}

void Shutdown()
{
  {
    std::lock_guard lk(s_lock);
    for (auto& engine : s_engines)
      engine.reset();
  }
  std::lock_guard files_lk(s_files_lock);
  s_files.clear();
}

bool Running()
{
  std::lock_guard lk(s_lock);
  return s_engines[0] != nullptr;
}

void Download(int player, std::string_view mlt)
{
  if (!Running())
    return;
  Request r = Kind(Request::Kind::Download);
  r.mlt = std::string(mlt);
  {
    std::lock_guard files_lk(s_files_lock);
    auto it = s_files.find(mlt);
    if (it == s_files.end())
    {
      auto data = std::make_shared<std::string>();
      if (!File::ReadFileToString(Directory() + "SOUND/" + r.mlt + ".MLT", *data))
      {
        WARN_LOG_FMT(AUDIO, "Dreamcast music: no {}.MLT", r.mlt);
        data->clear();
      }
      it = s_files.emplace(r.mlt, std::move(data)).first;
    }
    r.data = it->second;
  }
  Send(player, std::move(r));
}

void Play(int player, int bank, int sequence)
{
  Request r = Command(CMD_MIDI_PLAY);
  r.record[3] = static_cast<u8>(bank);
  r.record[4] = static_cast<u8>(sequence);
  Send(player, std::move(r));
}

void Stop(int player)
{
  Send(player, Command(CMD_MIDI_STOP));
}

void Pause(int player)
{
  Send(player, Kind(Request::Kind::Freeze));
}

void Continue(int player)
{
  Send(player, Kind(Request::Kind::Thaw));
}

void MidiControl(int player, int channel, int controller, int value)
{
  // sdMidiSendMes: the message bytes are data2, data1, port << 4 | channel, status >> 4 & 7.
  Request r = Command(CMD_MIDI_SEND_MES);
  r.record[4] = static_cast<u8>(value & 0x7F);
  r.record[5] = static_cast<u8>(controller & 0x7F);
  r.record[6] = static_cast<u8>(channel & 0xF);
  r.record[7] = 0xB0 >> 4 & 7;
  Send(player, std::move(r));
}

void Fade(int player, float level, int fade_ms, AfterFade after)
{
  Request r = Kind(Request::Kind::Fade);
  r.level = level;
  r.fade_ms = fade_ms;
  r.after = after;
  Send(player, std::move(r));
}

void StopAll()
{
  for (int player = 0; player < PLAYERS; ++player)
    Send(player, Command(CMD_STOP_ALL));
}

void SetGain(int gain)
{
  std::lock_guard lk(s_lock);
  s_gain = gain;
  for (auto& engine : s_engines)
  {
    if (engine)
      engine->SetGain(gain);
  }
}

void Mix(s16* samples, std::size_t num_samples, u32 out_rate)
{
  std::unique_lock lk(s_lock, std::try_to_lock);
  if (!lk.owns_lock() || !s_engines[0])
    return;
  if (Core::GetState(Core::System::GetInstance()) != Core::State::Running)
    return;
  for (auto& engine : s_engines)
    engine->Mix(samples, num_samples, out_rate);
}

}  // namespace DCMusic
