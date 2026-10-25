// DSP interface: the CPU-side registers around the DSP (mailboxes and control into the HLE in
// dsphle/), ARAM and its DMA, and the audio DMA that streams the DSP's mix out to the AI
// (DSP.cpp: RegisterMMIO, UpdateInterrupts, Do_ARAM_DMA, UpdateAudioDMA).
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"
#include "state.h"

#include "dsphle/dsp_hle.h"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace host
{
void AudioPushSamples(const s16* samples, u32 frames);  // video.cpp

namespace
{
constexpr u16 INT_AID = 0x08, INT_ARAM = 0x20, INT_DSP = 0x80;
constexpr u16 DSP_CONTROL_MASK = 0x0C07;  // the bits the DSP emulator owns
constexpr u64 kUpdatePeriod = CPU_HZ / 1000;
constexpr u64 kAudioDmaPeriod = CPU_HZ / 4000;  // 32 bytes (8 stereo frames at 32 kHz) per tick

u8* s_aram = nullptr;
u16 s_control = 0;
u16 s_aram_info = 0, s_aram_mode = 1, s_aram_refresh = 0;
u32 s_aram_mm = 0, s_aram_ar = 0, s_aram_cnt = 0;
u32 s_audio_source = 0;
u16 s_audio_control = 0;
u32 s_audio_current = 0;
u32 s_audio_remaining = 0;

struct PendingInt
{
  u64 when;
  u16 type;
};
std::vector<PendingInt> s_pending;

void UpdateInterrupts()
{
  const bool set = ((s_control >> 1) & s_control & (INT_DSP | INT_ARAM | INT_AID)) != 0;
  PiSetInterrupt(PI_INT_DSP, set);
}

void GenerateInterrupt(u16 type)
{
  s_control |= type & (INT_DSP | INT_ARAM | INT_AID);
  UpdateInterrupts();
}

void SchedulePending();
void PendingDue(u64)
{
  const u64 now = Now();
  for (size_t i = 0; i < s_pending.size();)
  {
    if (s_pending[i].when <= now)
    {
      GenerateInterrupt(s_pending[i].type);
      s_pending.erase(s_pending.begin() + static_cast<long>(i));
    }
    else
      ++i;
  }
  SchedulePending();
}

void SchedulePending()
{
  if (s_pending.empty())
    return;
  u64 first = s_pending[0].when;
  for (const PendingInt& p : s_pending)
    first = p.when < first ? p.when : first;
  Schedule(EV_DSP_INT, first > Now() ? first - Now() : 0, PendingDue);
}

void UpdateTick(u64 late)
{
  dsphle::Update();
  Schedule(EV_DSP, kUpdatePeriod > late ? kUpdatePeriod - late : 1, UpdateTick);
}

void AudioDmaTick(u64 late)
{
  static const s16 zero[16] = {};
  if (s_audio_control & 0x8000)
  {
    const u8* src = RamPtr(s_audio_current, 32);
    if (src)
    {
      // Big-endian, right channel first (Dolphin's Mixer::PushSamples): out as left, right
      s16 frames[16];
      for (int i = 0; i < 16; i += 2)
      {
        frames[i] = static_cast<s16>(BE16(src + (i + 1) * 2));
        frames[i + 1] = static_cast<s16>(BE16(src + i * 2));
      }
      AudioPushSamples(frames, 8);
    }
    if (s_audio_remaining != 0)
    {
      --s_audio_remaining;
      s_audio_current += 32;
    }
    if (s_audio_remaining == 0)
    {
      s_audio_current = s_audio_source;
      s_audio_remaining = s_audio_control & 0x7FFF;
      GenerateInterrupt(INT_AID);
    }
  }
  else
  {
    AudioPushSamples(zero, 8);
  }
  Schedule(EV_AUDIO_DMA, kAudioDmaPeriod > late ? kAudioDmaPeriod - late : 1, AudioDmaTick);
}

void AramDmaDone(u64)
{
  s_control &= ~0x200;
  GenerateInterrupt(INT_ARAM);
}

void AramDma()
{
  s_control |= 0x200;  // DMAState
  u32 count = s_aram_cnt & 0x7FFFFFFF;
  const bool to_ram = s_aram_cnt & 0x80000000u;
  u32 ar = s_aram_ar & 0x3FFFFFF, mm = s_aram_mm & 0x3FFFFFF;
  const u64 ticks = static_cast<u64>(count / 32) * 246;
  while (count >= 8)
  {
    u8* ram = RamPtr(mm, 8);
    if (ram && ar < ARAM_SIZE)
    {
      if (to_ram)
        std::memcpy(ram, s_aram + (ar & (ARAM_SIZE - 1)), 8);
      else
        std::memcpy(s_aram + (ar & (ARAM_SIZE - 1)), ram, 8);
    }
    mm += 8;
    ar += 8;
    count -= 8;
  }
  s_aram_mm = mm;
  s_aram_ar = ar;
  s_aram_cnt &= 0x80000000u;
  Schedule(EV_ARAM, ticks ? ticks : 1, AramDmaDone);
}

u16* Half(u32* value, bool high)
{
  return reinterpret_cast<u16*>(value) + (high ? 1 : 0);
}
}  // namespace

// ---- the HLE's view of the host ------------------------------------------------------------------
namespace dsphle
{
void HostGenerateInterrupt(u32 type, u64 cycles_into_future)
{
  s_pending.push_back({Now() + cycles_into_future, static_cast<u16>(type)});
  SchedulePending();
}
u8* HostRam()
{
  return g_ram;
}
u32 HostRamSize()
{
  return RAM_SIZE;
}
u8* HostAram()
{
  return s_aram;
}
u32 HostAramSize()
{
  return ARAM_SIZE;
}
u32 HostAiDspSampleRate()
{
  return AiDspSampleRate();
}
}  // namespace dsphle

u8* Aram()
{
  return s_aram;
}

void DspInit()
{
  if (!s_aram)
    s_aram = static_cast<u8*>(std::calloc(ARAM_SIZE, 1));
  s_control = 0;
  s_aram_info = 0;
  s_aram_mode = 1;  // ARAM ready
  s_pending.clear();
  dsphle::Initialize();
  s_control = (s_control & ~DSP_CONTROL_MASK) | (dsphle::ReadControlRegister() & DSP_CONTROL_MASK);
  Schedule(EV_DSP, kUpdatePeriod, UpdateTick);
  Schedule(EV_AUDIO_DMA, kAudioDmaPeriod, AudioDmaTick);
  SchedRegister(EV_DSP_INT, PendingDue);
  SchedRegister(EV_ARAM, AramDmaDone);
}

void DspDoState(StateIO& p)
{
  p.Do(s_control);
  p.Do(s_aram_info);
  p.Do(s_aram_mode);
  p.Do(s_aram_refresh);
  p.Do(s_aram_mm);
  p.Do(s_aram_ar);
  p.Do(s_aram_cnt);
  p.Do(s_audio_source);
  p.Do(s_audio_control);
  p.Do(s_audio_current);
  p.Do(s_audio_remaining);
  p.Do(s_pending);
  p.DoArray(s_aram, ARAM_SIZE);
  dsphle::DoState(p);
}

u16 DspRead16(u32 offset)
{
  switch (offset)
  {
  case 0x000:
    return dsphle::ReadMailboxHigh(true);
  case 0x002:
    return dsphle::ReadMailboxLow(true);
  case 0x004:
    return dsphle::ReadMailboxHigh(false);
  case 0x006:
    return dsphle::ReadMailboxLow(false);
  case 0x00A:
    return static_cast<u16>((s_control & ~DSP_CONTROL_MASK) |
                            (dsphle::ReadControlRegister() & DSP_CONTROL_MASK));
  case 0x012:
    return s_aram_info;
  case 0x016:
    return s_aram_mode;
  case 0x01A:
    return s_aram_refresh;
  case 0x020:
    return *Half(&s_aram_mm, true);
  case 0x022:
    return *Half(&s_aram_mm, false);
  case 0x024:
    return *Half(&s_aram_ar, true);
  case 0x026:
    return *Half(&s_aram_ar, false);
  case 0x028:
    return *Half(&s_aram_cnt, true);
  case 0x02A:
    return *Half(&s_aram_cnt, false);
  case 0x030:
    return *Half(&s_audio_source, true);
  case 0x032:
    return *Half(&s_audio_source, false);
  case 0x036:
    return s_audio_control;
  case 0x03A:
    return static_cast<u16>(s_audio_remaining > 0 ? s_audio_remaining - 1 : 0);
  default:
    Log("[dsp] read %03x\n", offset);
    return 0;
  }
}

void DspWrite16(u32 offset, u16 value)
{
  switch (offset)
  {
  case 0x000:
    dsphle::WriteMailboxHigh(true, value);
    return;
  case 0x002:
    dsphle::WriteMailboxLow(true, value);
    return;
  case 0x00A:
  {
    const u16 emulated = dsphle::WriteControlRegister(value);
    u16 next = static_cast<u16>((value & ~DSP_CONTROL_MASK) | (emulated & DSP_CONTROL_MASK));
    if (value & 1)  // DSP reset also stops the audio DMA
      s_audio_control = 0;
    // interrupt status bits are cleared by writing 1; masks and the rest are taken as written
    u16 status = s_control & (INT_DSP | INT_ARAM | INT_AID);
    status &= ~(value & (INT_DSP | INT_ARAM | INT_AID));
    next = static_cast<u16>((next & ~(INT_DSP | INT_ARAM | INT_AID)) | status);
    next = static_cast<u16>((next & ~0x200) | (s_control & 0x200));  // DMAState is read-only
    s_control = next;
    UpdateInterrupts();
    return;
  }
  case 0x012:
    s_aram_info = value & 0x7F;
    return;
  case 0x01A:
    s_aram_refresh = value & 0x7FF;
    return;
  case 0x020:
    *Half(&s_aram_mm, true) = value & 0x03FF;
    return;
  case 0x022:
    *Half(&s_aram_mm, false) = value & 0xFFE0;
    return;
  case 0x024:
    *Half(&s_aram_ar, true) = value & 0x03FF;
    return;
  case 0x026:
    *Half(&s_aram_ar, false) = value & 0xFFE0;
    return;
  case 0x028:
    *Half(&s_aram_cnt, true) = value & (0x03FF | 0x8000);
    return;
  case 0x02A:
    *Half(&s_aram_cnt, false) = value & 0xFFE0;
    AramDma();
    return;
  case 0x030:
    *Half(&s_audio_source, true) = value & 0x03FF;
    return;
  case 0x032:
    *Half(&s_audio_source, false) = value & 0xFFE0;
    return;
  case 0x036:
  {
    const bool was_enabled = s_audio_control & 0x8000;
    s_audio_control = value;
    if (!was_enabled && (value & 0x8000))
    {
      s_audio_current = s_audio_source;
      s_audio_remaining = value & 0x7FFF;
      dsphle::HostGenerateInterrupt(INT_AID, 200);
    }
    return;
  }
  default:
    Log("[dsp] write %03x = %04x\n", offset, value);
    return;
  }
}
}  // namespace host
