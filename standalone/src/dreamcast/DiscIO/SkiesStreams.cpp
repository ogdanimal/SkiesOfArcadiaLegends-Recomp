// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/SkiesStreams.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>
#include <numbers>
#include <thread>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>

#include "Common/CommonPaths.h"
#include "Common/Crypto/SHA1.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

namespace DiscIO::SkiesStreams
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

constexpr int TRACKS = 4;
// Bumped when the conversion changes, so older cached files are made again.
constexpr u32 VERSION = 1;
constexpr u32 HEADER = 0x60;
constexpr u32 DC_RATE = 44100;
// The Dreamcast streams are stored in 0x2000-byte blocks, left and right in turn.
constexpr std::size_t DC_BLOCK = 0x2000;
// Where a Dreamcast recording runs past the GameCube one's end, it fades out over this much.
constexpr double FADE_SECONDS = 1.5;

struct Track
{
  std::string gc[2];       // the GameCube files, left and right ("" = not on this disc)
  std::string ready[2];    // the converted files, once made
  bool attempted = false;  // converted, or found impossible, for these GameCube files
};

struct State
{
  std::mutex lock;
  std::array<Track, TRACKS> tracks;
  u32 enabled = 0;
  bool building = false;
};

// Never destroyed: the conversion thread may still run while the process exits.
State& S()
{
  static State* state = new State;
  return *state;
}

std::string Lower(std::string s)
{
  Common::ToLower(&s);
  return s;
}

// "…/sound/m01_l.dsp" -> track 0, channel 0; -1 when it's no stream file.
int Classify(const std::string& path, int* channel)
{
  const std::string lower = Lower(path);
  const std::size_t slash = lower.find_last_of("/\\");
  const std::string name = lower.substr(slash == std::string::npos ? 0 : slash + 1);
  if (name.size() != 9 || name[0] != 'm' || name[1] != '0' || name[2] < '1' || name[2] > '4' ||
      name[3] != '_' || (name[4] != 'l' && name[4] != 'r') || name.substr(5) != ".dsp")
  {
    return -1;
  }
  if (slash == std::string::npos || slash < 5 || lower.compare(slash - 5, 5, "sound") != 0)
    return -1;
  *channel = name[4] == 'l' ? 0 : 1;
  return name[2] - '1';
}

u16 Be16(const u8* p)
{
  return static_cast<u16>(p[0] << 8 | p[1]);
}

u32 Be32(const u8* p)
{
  return static_cast<u32>(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

u32 Fnv(u32 h, const void* data, std::size_t size)
{
  const u8* p = static_cast<const u8*>(data);
  for (std::size_t i = 0; i < size; ++i)
    h = (h ^ p[i]) * 16777619u;
  return h;
}

// A GameCube stream file: its header's sample count, rate and coefficient pairs.
struct GcStream
{
  std::vector<u8> data;
  u32 samples = 0;
  u32 rate = 0;
  std::array<s16, 16> coefs{};

  bool Load(const std::string& path)
  {
    std::string file;
    if (!File::ReadFileToString(path, file) || file.size() < HEADER + 8)
      return false;
    data.assign(file.begin(), file.end());
    samples = Be32(data.data());
    rate = Be32(data.data() + 8);
    for (int i = 0; i < 16; ++i)
      coefs[i] = static_cast<s16>(Be16(data.data() + 0x1C + i * 2));
    // The last frame may be cut short at the file's end (m01_L: 5 bytes).
    const u64 frames = (samples + 13) / 14;
    return rate >= 8000 && rate <= 48000 && samples > 0 && HEADER + frames * 8 < data.size() + 8;
  }

  std::vector<float> Decode() const
  {
    std::vector<float> out(samples);
    int h1 = 0, h2 = 0;
    for (u32 o = 0, p = HEADER; o < samples; p += 8)
    {
      const u8 ps = data[p];
      const int scale = 1 << (ps & 15);
      const int c1 = coefs[(ps >> 4 & 7) * 2], c2 = coefs[(ps >> 4 & 7) * 2 + 1];
      for (int k = 0; k < 14 && o < samples; ++k, ++o)
      {
        const u8 byte = p + 1 + k / 2 < data.size() ? data[p + 1 + k / 2] : 0;
        int nib = k % 2 == 0 ? byte >> 4 : byte & 15;
        if (nib >= 8)
          nib -= 16;
        const int s = std::clamp((((nib * scale) << 11) + c1 * h1 + c2 * h2 + 1024) >> 11, -32768, 32767);
        out[o] = static_cast<float>(s);
        h2 = h1;
        h1 = s;
      }
    }
    return out;
  }
};

// The Dreamcast recording, both channels at 44100 Hz. .P16: 16-bit little-endian PCM; .P04: the
// AICA's 4-bit ADPCM (Yamaha's), low nibble first.
bool LoadDreamcast(const std::string& path, bool adpcm, std::array<std::vector<float>, 2>* out)
{
  std::string file;
  if (!File::ReadFileToString(path, file) || file.size() < 2 * DC_BLOCK)
    return false;
  const u8* d = reinterpret_cast<const u8*>(file.data());
  const std::size_t pairs = file.size() / (2 * DC_BLOCK);
  for (int c = 0; c < 2; ++c)
  {
    std::vector<float>& ch = (*out)[c];
    ch.clear();
    ch.reserve(pairs * (adpcm ? DC_BLOCK * 2 : DC_BLOCK / 2));
    int signal = 0, step = 0x7F;
    for (std::size_t b = 0; b < pairs; ++b)
    {
      const u8* block = d + (b * 2 + c) * DC_BLOCK;
      if (!adpcm)
      {
        for (std::size_t i = 0; i < DC_BLOCK; i += 2)
          ch.push_back(static_cast<float>(static_cast<s16>(block[i] | block[i + 1] << 8)));
        continue;
      }
      static constexpr int STEP_SCALE[8] = {230, 230, 230, 230, 307, 409, 512, 614};
      for (std::size_t i = 0; i < DC_BLOCK * 2; ++i)
      {
        const int nib = i % 2 == 0 ? block[i / 2] & 15 : block[i / 2] >> 4;
        const int diff = ((2 * (nib & 7) + 1) * step) >> 3;
        signal = std::clamp(nib & 8 ? signal - diff : signal + diff, -32768, 32767);
        step = std::clamp((step * STEP_SCALE[nib & 7]) >> 8, 0x7F, 0x6000);
        ch.push_back(static_cast<float>(signal));
      }
    }
  }
  return true;
}

// Windowed-sinc resampling (Blackman, 16 zero crossings each side), with the cutoff below the
// lower of the two rates' Nyquist.
std::vector<float> Resample(const std::vector<float>& in, u32 in_rate, u32 out_rate)
{
  const double ratio = static_cast<double>(in_rate) / out_rate;
  const double cutoff = 0.5 * 0.92 / std::max(ratio, 1.0);  // in cycles per input sample
  constexpr int ZEROS = 16;
  const int half = static_cast<int>(std::ceil(ZEROS / (2.0 * cutoff)));
  const std::size_t n = static_cast<std::size_t>(in.size() / ratio);
  std::vector<float> out(n);
  // The kernel at 64 phases per input sample, interpolated linearly between them.
  constexpr int PHASES = 64;
  std::vector<float> kernel((2 * half + 1) * PHASES + 1);
  for (std::size_t i = 0; i < kernel.size(); ++i)
  {
    const double x = static_cast<double>(i) / PHASES - half;
    const double t = 2.0 * cutoff * x;
    const double sinc = x == 0 ? 1.0 : std::sin(std::numbers::pi * t) / (std::numbers::pi * t);
    const double w = (x + half) / (2.0 * half);
    const double window = w < 0 || w > 1 ? 0.0 :
                                           0.42 - 0.5 * std::cos(2 * std::numbers::pi * w) +
                                               0.08 * std::cos(4 * std::numbers::pi * w);
    kernel[i] = static_cast<float>(2.0 * cutoff * sinc * window);
  }
  for (std::size_t j = 0; j < n; ++j)
  {
    const double pos = j * ratio;
    const long center = static_cast<long>(std::floor(pos));
    const double frac = pos - center;
    double acc = 0;
    for (int k = -half; k <= half; ++k)
    {
      const long idx = center + k;
      if (idx < 0 || idx >= static_cast<long>(in.size()))
        continue;
      const double kp = (k - frac + half) * PHASES;
      const std::size_t ki = static_cast<std::size_t>(kp);
      if (ki + 1 >= kernel.size())
        continue;
      const double kf = kp - ki;
      acc += in[idx] * (kernel[ki] * (1 - kf) + kernel[ki + 1] * kf);
    }
    out[j] = static_cast<float>(acc);
  }
  return out;
}

double Rms(const std::vector<float>& x, std::size_t n)
{
  n = std::min(n, x.size());
  double sum = 0;
  for (std::size_t i = 0; i < n; ++i)
    sum += static_cast<double>(x[i]) * x[i];
  return n ? std::sqrt(sum / n) : 0.0;
}

// DSP ADPCM with the GameCube file's own coefficient pairs: each 14-sample frame takes the pair
// and scale that reproduce it best (the scales around the one its largest residual needs). The
// first `samples` samples (the file's own count by default).
void Encode(const std::vector<s16>& pcm, const GcStream& gc, std::vector<u8>* out, u32 samples = 0)
{
  int h1 = 0, h2 = 0;
  const u32 frames = ((samples ? samples : gc.samples) + 13) / 14;
  for (u32 f = 0; f < frames; ++f)
  {
    s16 x[14];
    for (int k = 0; k < 14; ++k)
    {
      const std::size_t i = f * 14 + k;
      x[k] = i < pcm.size() ? pcm[i] : 0;
    }
    u64 best_err = std::numeric_limits<u64>::max();
    u8 best[8] = {};
    int best_h1 = 0, best_h2 = 0;
    for (int pred = 0; pred < 8; ++pred)
    {
      const int c1 = gc.coefs[pred * 2], c2 = gc.coefs[pred * 2 + 1];
      // The scale the frame needs without quantisation feedback.
      int max_residual = 0;
      {
        int a = h1, b = h2;
        for (int k = 0; k < 14; ++k)
        {
          const int predicted = (c1 * a + c2 * b + 1024) >> 11;
          max_residual = std::max(max_residual, std::abs(x[k] - predicted));
          b = a;
          a = x[k];
        }
      }
      int need = 0;
      while (need < 12 && (7 << need) < max_residual)
        ++need;
      for (int shift = std::max(0, need - 1); shift <= std::min(12, need + 1); ++shift)
      {
        u8 frame[8] = {static_cast<u8>(pred << 4 | shift)};
        int a = h1, b = h2;
        u64 err = 0;
        for (int k = 0; k < 14 && err < best_err; ++k)
        {
          const int p = c1 * a + c2 * b;
          const double ideal = (x[k] * 2048.0 - p) / (2048.0 * (1 << shift));
          const int nib = std::clamp(static_cast<int>(std::lround(ideal)), -8, 7);
          const int s = std::clamp((((nib * (1 << shift)) << 11) + p + 1024) >> 11, -32768, 32767);
          const s64 e = static_cast<s64>(x[k]) - s;
          err += static_cast<u64>(e * e);
          frame[1 + k / 2] |= static_cast<u8>((nib & 15) << (k % 2 == 0 ? 4 : 0));
          b = a;
          a = s;
        }
        if (err < best_err)
        {
          best_err = err;
          std::memcpy(best, frame, 8);
          best_h1 = a;
          best_h2 = b;
        }
      }
    }
    const std::size_t at = HEADER + std::size_t{f} * 8;
    std::memcpy(out->data() + at, best, std::min<std::size_t>(8, out->size() - at));
    h1 = best_h1;
    h2 = best_h2;
  }
  // The header's initial predictor / scale and history are the first frame's.
  (*out)[0x3E] = 0;
  (*out)[0x3F] = (*out)[HEADER];
  std::fill(out->begin() + 0x40, out->begin() + 0x44, u8{0});
}

std::string MusicDir()
{
  return File::GetUserPath(D_USER_IDX) + "DreamcastMusic" DIR_SEP;
}

// Makes (or finds) the converted files of one track; "" when it can't.
bool Convert(int track, const std::string (&gc_paths)[2], std::string (&made)[2])
{
  const std::string sound = MusicDir() + "SOUND" DIR_SEP;
  const std::string base = fmt::format("M0{}S", track + 1);
  std::string source = sound + base + ".P16";
  bool adpcm = false;
  if (!File::Exists(source))
  {
    source = sound + base + ".P04";
    adpcm = true;
    if (!File::Exists(source))
      return false;
  }
  GcStream gc[2];
  if (!gc[0].Load(gc_paths[0]) || !gc[1].Load(gc_paths[1]))
    return false;

  u32 key = Fnv(2166136261u, &VERSION, sizeof(VERSION));
  for (const GcStream& g : gc)
  {
    key = Fnv(key, g.data.data(), HEADER);
    const u64 size = g.data.size();
    key = Fnv(key, &size, sizeof(size));
  }
  const u64 source_size = File::GetSize(source);
  key = Fnv(key, &source_size, sizeof(source_size));
  const std::string dir = MusicDir() + "STREAMS" DIR_SEP;
  for (int c = 0; c < 2; ++c)
    made[c] = dir + fmt::format("m0{}_{}-{:08x}.dsp", track + 1, c ? 'r' : 'l', key);
  if (File::GetSize(made[0]) == gc[0].data.size() && File::GetSize(made[1]) == gc[1].data.size())
    return true;

  std::array<std::vector<float>, 2> dc;
  if (!LoadDreamcast(source, adpcm, &dc))
    return false;
  File::CreateFullPath(dir);
  // Older conversions of this track.
  for (const File::FSTEntry& entry : File::ScanDirectoryTree(dir, false).children)
  {
    if (entry.virtualName.starts_with(fmt::format("m0{}_", track + 1)))
      File::Delete(entry.physicalName);
  }
  for (int c = 0; c < 2; ++c)
  {
    const GcStream& g = gc[c];
    std::vector<float> pcm = Resample(dc[c], DC_RATE, g.rate);
    // As loud as the GameCube recording (the Dreamcast's ADPCM streams are mastered hotter).
    const std::size_t common = std::min<std::size_t>(pcm.size(), g.samples);
    const double gc_rms = Rms(g.Decode(), common), dc_rms = Rms(pcm, common);
    const float gain = dc_rms > 1 ? static_cast<float>(std::clamp(gc_rms / dc_rms, 0.1, 2.0)) : 1.0f;
    // Past the GameCube length: faded out there.
    const std::size_t fade = static_cast<std::size_t>(FADE_SECONDS * g.rate);
    const bool longer = pcm.size() > g.samples;
    std::vector<s16> samples(std::min<std::size_t>(pcm.size(), g.samples));
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
      float v = pcm[i] * gain;
      if (longer && i + fade > g.samples)
        v *= static_cast<float>(g.samples - i) / fade;
      samples[i] = static_cast<s16>(std::clamp(std::lround(v), -32768l, 32767l));
    }
    std::vector<u8> file = g.data;
    Encode(samples, g, &file);
    const std::string temp = made[c] + ".part";
    if (!File::WriteStringToFile(temp, std::string_view(reinterpret_cast<const char*>(file.data()),
                                                        file.size())) ||
        !File::Rename(temp, made[c]))
    {
      return false;
    }
    INFO_LOG_FMT(DISCIO, "Dreamcast stream m0{} {}: {} ({:.1f} s; the GameCube's {:.1f} s), gain {:.2f}",
                 track + 1, c ? "right" : "left", source, dc[c].size() / double(DC_RATE),
                 g.samples / double(g.rate), gain);
  }
  return true;
}

// Converts the enabled tracks that aren't ready, the intro first.
void Build()
{
  const Worker worker;
  State& s = S();
  for (;;)
  {
    int track = -1;
    std::string gc[2];
    {
      std::lock_guard lk(s.lock);
      if (s_stop)
      {
        s.building = false;  // the tracks not attempted yet wait for the next SetEnabled
        return;
      }
      for (int t = 0; t < TRACKS && track < 0; ++t)
      {
        const Track& tr = s.tracks[t];
        if ((s.enabled >> t & 1) && !tr.attempted && !tr.gc[0].empty() && !tr.gc[1].empty())
          track = t;
      }
      if (track < 0)
      {
        s.building = false;
        return;
      }
      gc[0] = s.tracks[track].gc[0];
      gc[1] = s.tracks[track].gc[1];
      s.tracks[track].attempted = true;
    }
    std::string made[2];
    const bool ok = Convert(track, gc, made);
    std::lock_guard lk(s.lock);
    Track& tr = s.tracks[track];
    if (ok && tr.gc[0] == gc[0] && tr.gc[1] == gc[1])
    {
      tr.ready[0] = made[0];
      tr.ready[1] = made[1];
    }
    else if (!ok)
    {
      INFO_LOG_FMT(DISCIO, "Dreamcast stream m0{}: none (not imported, or unreadable)", track + 1);
    }
  }
}

// The European release's 50 Hz audio, played at 60 Hz (SkiesStreams.h). Bumped when the conversion
// changes, so older cached files are made again.
constexpr u32 PAL60_VERSION = 1;

struct StreamFix
{
  const char* name;  // without _l.dsp / _r.dsp
  u32 samples_left;  // the European files' sample counts
  u32 samples_right;
  float factor;  // how much shorter
  u32 cut;       // the length to cut at, in samples (0: none)
};

struct BankFix
{
  const char* name;
  u32 size;
  u64 sha1;   // the first 8 bytes of the European file's SHA-1
  u32 first;  // in TEMPO_FIELDS
  u32 count;
};

#include "DiscIO/SkiesPal60Data.inc"

struct Pal60State
{
  std::mutex lock;
  std::unordered_map<std::string, std::string> files;  // lower-case name -> the extracted file
  std::unordered_map<std::string, std::string> ready;  // the extracted file -> the converted one
  bool building = false;
  bool pending = false;  // files registered since the last conversion pass
};

// Never destroyed, like S().
Pal60State& P()
{
  static Pal60State* state = new Pal60State;
  return *state;
}

// Redirect's choice per file, latched at offset 0 (review D20, as SkiesUncensor's). Never
// destroyed, like S().
struct Latch
{
  std::mutex lock;
  std::unordered_map<std::string, std::string> serving;
};

Latch& L()
{
  static Latch* latch = new Latch;
  return *latch;
}

std::string BaseName(const std::string& path)
{
  const std::size_t slash = path.find_last_of("/\\");
  return Lower(path.substr(slash == std::string::npos ? 0 : slash + 1));
}

// Whether the extracted file at path is one the European fixes name (a sound/ file).
bool IsPal60File(const std::string& path, const std::string& name)
{
  const std::size_t slash = path.find_last_of("/\\");
  if (slash == std::string::npos || slash < 5 || Lower(path.substr(slash - 5, 5)) != "sound")
    return false;
  for (const BankFix& b : BANK_FIXES)
  {
    if (name == b.name)
      return true;
  }
  if (name.size() < 7 || name.compare(name.size() - 6, 6, "_l.dsp") != 0 &&
                             name.compare(name.size() - 6, 6, "_r.dsp") != 0)
  {
    return false;
  }
  const std::string stem = name.substr(0, name.size() - 6);
  for (const StreamFix& f : STREAM_FIXES)
  {
    if (stem == f.name)
      return true;
  }
  return false;
}

std::string Pal60Dir()
{
  return File::GetUserPath(D_CACHE_IDX) + "SkiesPal60" DIR_SEP;
}

bool WriteAtomically(const std::string& path, const std::vector<u8>& data)
{
  const std::string temp = path + ".part";
  return File::WriteStringToFile(
             temp, std::string_view(reinterpret_cast<const char*>(data.data()), data.size())) &&
         File::Rename(temp, path);
}

void Put32(u8* p, u32 v)
{
  p[0] = static_cast<u8>(v >> 24);
  p[1] = static_cast<u8>(v >> 16);
  p[2] = static_cast<u8>(v >> 8);
  p[3] = static_cast<u8>(v);
}

// A sound bank with each tempo field times 6/5, undoing the European 5/6: "" when the file isn't
// the European one.
std::string FixBank(const BankFix& fix, const std::string& path)
{
  std::string file;
  if (File::GetSize(path) != fix.size || !File::ReadFileToString(path, file) ||
      file.size() != fix.size)
    return "";
  const Common::SHA1::Digest digest = Common::SHA1::CalculateDigest(file);
  u64 sha1 = 0;
  for (int i = 0; i < 8; ++i)
    sha1 = sha1 << 8 | digest[i];
  if (sha1 != fix.sha1)
    return "";
  const std::string made =
      Pal60Dir() + fmt::format("{}-{:016x}-v{}", fix.name, fix.sha1, PAL60_VERSION);
  if (File::GetSize(made) == fix.size)
    return made;
  std::vector<u8> data(file.begin(), file.end());
  for (u32 i = 0; i < fix.count; ++i)
  {
    const u32 at = TEMPO_FIELDS[fix.first + i];
    if (at + 4 > data.size())
      return "";
    Put32(data.data() + at, static_cast<u32>(std::lround(Be32(data.data() + at) * 1.2)));
  }
  return WriteAtomically(made, data) ? made : "";
}

// WSOLA: both channels cut at the same places (picked on their sum, so the stereo picture holds),
// Hann windows at half overlap; each frame starts where it best continues the one before.
void Compress(const std::array<std::vector<float>, 2>& in, u32 rate, double factor,
              std::array<std::vector<float>, 2>* out)
{
  const std::size_t n = std::min(in[0].size(), in[1].size());
  const int frame = std::max(64, static_cast<int>(rate * 0.030) & ~1), hop = frame / 2;
  const int tolerance = frame / 4;
  const std::size_t out_n = static_cast<std::size_t>(n / factor);
  std::vector<float> guide(n), window(frame);
  for (std::size_t i = 0; i < n; ++i)
    guide[i] = in[0][i] + in[1][i];
  for (int i = 0; i < frame; ++i)
    window[i] = static_cast<float>(0.5 - 0.5 * std::cos(2 * std::numbers::pi * i / frame));
  for (std::vector<float>& ch : *out)
    ch.assign(out_n + frame, 0.0f);
  const auto at = [&](long i) { return i >= 0 && static_cast<std::size_t>(i) < n ? guide[i] : 0.0f; };
  long prev = 0;
  for (std::size_t k = 0; k * hop < out_n; ++k)
  {
    long pos = std::lround(k * hop * factor);
    if (k > 0)
    {
      // The continuation of the last frame's input; coarse (every 4th lag and sample), then the 7
      // lags around the best at every sample.
      const long natural = prev + hop;
      long best_pos = pos;
      for (int pass = 0; pass < 2; ++pass)
      {
        const int step = pass == 0 ? 4 : 1;
        const long lo = pass == 0 ? pos - tolerance : best_pos - 3;
        const long hi = pass == 0 ? pos + tolerance : best_pos + 3;
        double best = -std::numeric_limits<double>::infinity();
        for (long cand = lo; cand <= hi; cand += step)
        {
          double dot = 0, energy = 0;
          for (int i = 0; i < hop; i += step)
          {
            const float a = at(natural + i), b = at(cand + i);
            dot += a * b;
            energy += b * b;
          }
          const double score = energy > 0 ? dot / std::sqrt(energy) : 0;
          if (score > best)
          {
            best = score;
            best_pos = cand;
          }
        }
      }
      pos = best_pos;
    }
    for (int c = 0; c < 2; ++c)
    {
      for (int i = 0; i < frame; ++i)
      {
        const long src = pos + i;
        if (src >= 0 && static_cast<std::size_t>(src) < n)
          (*out)[c][k * hop + i] += in[c][src] * window[i];
      }
    }
    prev = pos;
  }
  // The first half frame had no partner window.
  for (std::vector<float>& ch : *out)
  {
    for (int i = 0; i < hop; ++i)
    {
      if (window[i] > 1e-3f)
        ch[i] /= window[i];
    }
    ch.resize(out_n);
  }
}

// A stream pair shortened to its 60 Hz length, each file at its original size (the header's
// sample count shorter, the rest zeros): false when the files aren't the European ones.
bool FixStream(const StreamFix& fix, const std::string (&paths)[2], std::string (&made)[2])
{
  // The headers first: every disc has files by these names, and reading them whole (over 100 MB)
  // at every boot just to compare sample counts was most of the work (review D18)
  std::array<u8, HEADER> head[2];
  u64 size[2];
  for (int c = 0; c < 2; ++c)
  {
    File::IOFile f(paths[c], "rb");
    if (!f || !f.ReadBytes(head[c].data(), HEADER))
      return false;
    size[c] = f.GetSize();
  }
  if (Be32(head[0].data()) != fix.samples_left || Be32(head[1].data()) != fix.samples_right ||
      Be32(head[0].data() + 8) != Be32(head[1].data() + 8))
  {
    return false;
  }
  u32 key = Fnv(2166136261u, &PAL60_VERSION, sizeof(PAL60_VERSION));
  for (int c = 0; c < 2; ++c)
  {
    key = Fnv(key, head[c].data(), HEADER);
    key = Fnv(key, &size[c], sizeof(size[c]));
  }
  for (int c = 0; c < 2; ++c)
    made[c] = Pal60Dir() + fmt::format("{}_{}-{:08x}.dsp", fix.name, c ? 'r' : 'l', key);
  if (File::GetSize(made[0]) == size[0] && File::GetSize(made[1]) == size[1])
    return true;

  GcStream gc[2];
  if (!gc[0].Load(paths[0]) || !gc[1].Load(paths[1]) || gc[0].samples != fix.samples_left ||
      gc[1].samples != fix.samples_right || gc[0].rate != gc[1].rate ||
      gc[0].data.size() != size[0] || gc[1].data.size() != size[1])
  {
    return false;
  }
  std::array<std::vector<float>, 2> pcm{gc[0].Decode(), gc[1].Decode()}, out;
  Compress(pcm, gc[0].rate, fix.factor, &out);
  for (int c = 0; c < 2; ++c)
  {
    const GcStream& g = gc[c];
    u32 samples = std::min<u32>(static_cast<u32>(out[c].size()), static_cast<u32>(g.samples / fix.factor));
    // Cut at the USA length (the European re-edits' longer tails), fading out over its last 0.25 s.
    const bool cut = fix.cut != 0 && samples > fix.cut;
    if (cut)
      samples = fix.cut;
    const u32 fade = cut ? std::min<u32>(samples, g.rate / 4) : 0;
    std::vector<s16> pcm16(samples);
    for (u32 i = 0; i < samples; ++i)
    {
      float v = out[c][i];
      if (fade && i + fade >= samples)
        v *= static_cast<float>(samples - i) / fade;
      pcm16[i] = static_cast<s16>(std::clamp(std::lround(v), -32768l, 32767l));
    }
    // The original header and size; the stream ends at the header's nibble count (the game's
    // PcmPrepare, fn_802159E0), so the zeros after it are never played.
    std::vector<u8> file(g.data.size(), 0);
    std::copy_n(g.data.begin(), HEADER, file.begin());
    const u32 rest = samples % 14, nibbles = samples / 14 * 16 + (rest ? rest + 2 : 0);
    Put32(file.data(), samples);
    Put32(file.data() + 4, nibbles);
    Put32(file.data() + 0x14, nibbles);  // the loop end, unused (no loop), as the originals have it
    Encode(pcm16, g, &file, samples);
    if (!WriteAtomically(made[c], file))
      return false;
  }
  return true;
}

// Converts every registered file that has a fix: the banks first (quick), then the streams, the
// spells' and S-moves' (KO) before the battle and field voices.
void BuildPal60()
{
  const Worker worker;
  Pal60State& p = P();
  const auto stopped = [&p] {
    if (!s_stop)
      return false;
    std::lock_guard lk(p.lock);
    p.building = false;
    p.pending = true;  // the next sound file read starts again where this left off
    return true;
  };
  for (;;)
  {
    std::unordered_map<std::string, std::string> files;
    {
      std::lock_guard lk(p.lock);
      if (!p.pending)
      {
        p.building = false;
        return;
      }
      p.pending = false;
      files = p.files;
    }
    File::CreateFullPath(Pal60Dir());
    const auto publish = [&](const std::string& from, const std::string& to) {
      std::lock_guard lk(p.lock);
      if (p.files.contains(BaseName(from)) && p.files[BaseName(from)] == from)
        p.ready[from] = to;
    };
    int banks = 0, streams = 0;
    for (const BankFix& fix : BANK_FIXES)
    {
      if (stopped())
        return;
      const auto it = files.find(fix.name);
      if (it == files.end())
        continue;
      const std::string made = FixBank(fix, it->second);
      if (!made.empty())
      {
        publish(it->second, made);
        ++banks;
      }
    }
    for (int pass = 0; pass < 2; ++pass)
    {
      for (const StreamFix& fix : STREAM_FIXES)
      {
        if ((std::string_view(fix.name).starts_with("ko")) != (pass == 0))
          continue;
        if (stopped())
          return;
        const auto l = files.find(fmt::format("{}_l.dsp", fix.name));
        const auto r = files.find(fmt::format("{}_r.dsp", fix.name));
        if (l == files.end() || r == files.end())
          continue;
        const std::string paths[2] = {l->second, r->second};
        std::string made[2];
        if (FixStream(fix, paths, made))
        {
          publish(paths[0], made[0]);
          publish(paths[1], made[1]);
          ++streams;
        }
      }
    }
    if (banks || streams)
    {
      INFO_LOG_FMT(DISCIO, "European 60 Hz audio: {} sound banks and {} streams ready", banks,
                   streams);
    }
  }
}
}  // namespace

void Register(const std::string& physical_path)
{
  {
    // A new disc build (Android keeps the process between boots) starts every choice afresh
    Latch& l = L();
    std::lock_guard lk(l.lock);
    l.serving.erase(physical_path);
  }
  const std::string name = BaseName(physical_path);
  if (IsPal60File(physical_path, name))
  {
    Pal60State& p = P();
    std::lock_guard lk(p.lock);
    std::string& known = p.files[name];
    if (known != physical_path)
    {
      p.ready.erase(known);
      known = physical_path;
      p.pending = true;
    }
  }
  int channel = 0;
  const int track = Classify(physical_path, &channel);
  if (track < 0)
    return;
  State& s = S();
  std::lock_guard lk(s.lock);
  Track& tr = s.tracks[track];
  if (tr.gc[channel] == physical_path)
    return;
  tr.gc[channel] = physical_path;
  tr.ready[0].clear();
  tr.ready[1].clear();
  tr.attempted = false;
}

namespace
{
std::string Choose(const std::string& physical_path, bool dsp)
{
  {
    // The European 50 Hz audio: the conversions start with the game's first read of a sound file
    // (the disc is built by then, every file registered).
    Pal60State& p = P();
    std::lock_guard lk(p.lock);
    if (p.pending && !p.building)
    {
      p.building = true;
      std::thread(BuildPal60).detach();
    }
    const auto it = p.ready.find(physical_path);
    if (it != p.ready.end())
      return it->second;
  }
  if (!dsp)
    return physical_path;
  int channel = 0;
  const int track = Classify(physical_path, &channel);
  if (track < 0)
    return physical_path;
  State& s = S();
  std::lock_guard lk(s.lock);
  const Track& tr = s.tracks[track];
  if (!(s.enabled >> track & 1) || tr.gc[channel] != physical_path || tr.ready[channel].empty())
    return physical_path;
  return tr.ready[channel];
}
}  // namespace

std::string Redirect(const std::string& physical_path, u64 offset_in_file)
{
  // Cheap for every other file the game reads.
  const std::string ext = physical_path.size() < 9 ? "" : Lower(physical_path.substr(physical_path.size() - 5));
  const bool dsp = ext.ends_with(".dsp");
  if (!dsp && ext != ".info")
    return physical_path;
  std::string chosen = Choose(physical_path, dsp);
  Latch& l = L();
  std::lock_guard lk(l.lock);
  std::string& serving = l.serving[physical_path];
  // A first read past the start (a state loaded mid-song) takes the current choice too
  if (offset_in_file == 0 || serving.empty())
    serving = std::move(chosen);
  return serving;
}

void StopBackgroundWork()
{
  WaitForWorkers();
}

void SetEnabled(u32 mask)
{
  State& s = S();
  std::lock_guard lk(s.lock);
  if (mask == s.enabled)
    return;
  s.enabled = mask;
  if (s.building)
    return;
  for (int t = 0; t < TRACKS; ++t)
  {
    const Track& tr = s.tracks[t];
    if ((mask >> t & 1) && !tr.attempted && !tr.gc[0].empty() && !tr.gc[1].empty())
    {
      s.building = true;
      std::thread(Build).detach();
      return;
    }
  }
}
}  // namespace DiscIO::SkiesStreams
