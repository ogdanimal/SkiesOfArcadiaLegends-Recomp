// Disc interface: the DI registers and the drive commands a GameCube game sends
// (DVDInterface.cpp: RegisterMMIO, ExecuteCommand, UpdateInterrupts). Reads complete after a
// latency close to a fast drive; audio streaming (DTK) is off, as the disc header asks.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "dolphin_state.h"
#include "host.h"
#include "state.h"

#include <cstdlib>

namespace host
{
namespace
{
enum : u32
{
  DISR_BREAK = 1,
  DISR_DEINTMASK = 2,
  DISR_DEINT = 4,
  DISR_TCINTMASK = 8,
  DISR_TCINT = 0x10,
  DISR_BRKINTMASK = 0x20,
  DISR_BRKINT = 0x40,
  DICVR_CVR = 1,
  DICVR_CVRINTMASK = 2,
  DICVR_CVRINT = 4,
};
enum DriveError : u32
{
  ERR_NONE = 0x000000,
  ERR_INVALID_COMMAND = 0x052000,
  ERR_NO_AUDIO_BUF = 0x052001,
};

u32 s_disr = 0, s_dicvr = 0, s_cmd[3] = {}, s_dimar = 0, s_dilength = 0, s_dicr = 0, s_diimmbuf = 0;
u32 s_dicfg = 1;
u32 s_error = ERR_NONE;
bool s_drive_ready = false;  // a read has been made
bool s_pending_deint = false;
u64 s_reads = 0, s_bytes = 0;

void UpdateInterrupts()
{
  const bool set = ((s_disr & DISR_DEINT) && (s_disr & DISR_DEINTMASK)) ||
                   ((s_disr & DISR_TCINT) && (s_disr & DISR_TCINTMASK)) ||
                   ((s_disr & DISR_BRKINT) && (s_disr & DISR_BRKINTMASK)) ||
                   ((s_dicvr & DICVR_CVRINT) && (s_dicvr & DICVR_CVRINTMASK));
  PiSetInterrupt(PI_INT_DI, set);
}

void Complete(u64)
{
  s_dicr &= ~1u;  // TSTART
  if (!s_pending_deint)
  {
    // FinishExecutingCommand: a completed command consumes the whole DMA length
    s_dimar += s_dilength;
    s_dilength = 0;
  }
  s_disr |= s_pending_deint ? DISR_DEINT : DISR_TCINT;
  UpdateInterrupts();
}

bool DmaRead(u64 disc_offset, u32 source_length)
{
  const u32 length = s_dilength < source_length ? s_dilength : source_length;
  if (length)
  {
    u8* dst = RamPtr(s_dimar, length);
    if (!dst || !DiscRead(disc_offset, dst, length))
    {
      Log("[di] read %llx (%u bytes) to %08x failed\n", static_cast<unsigned long long>(disc_offset),
          length, s_dimar);
      return false;
    }
  }
  ++s_reads;
  s_bytes += length;
  return true;
}

void Execute()
{
  const u8 command = static_cast<u8>(s_cmd[0] >> 24);
  bool error = false;
  u64 latency = CPU_HZ / 10000;  // 100 us per command
  if (command != 0xE0)
    s_error = ERR_NONE;
  switch (command)
  {
  case 0x12:  // inquiry
    Write32(s_dimar, 0x00000002);
    Write32(s_dimar + 4, 0x20060526);
    Write32(s_dimar + 8, 0x41000000);
    break;
  case 0xA8:  // read
    if ((s_cmd[0] & 0xFF) == 0x40)
      error = !DmaRead(0, 0x20);  // disc ID
    else
    {
      const u32 length = s_dilength < s_cmd[2] ? s_dilength : s_cmd[2];
      latency += static_cast<u64>(length) * (CPU_HZ / 1000000) / 16;  // about 16 MB/s
      error = !DmaRead(static_cast<u64>(s_cmd[1]) << 2, s_cmd[2]);
      static const bool s_trace = std::getenv("SKIES_DI_TRACE") != nullptr;
      if (s_trace)
        Log("[di] read %llx +%x to %08x (%.3f s)\n", static_cast<unsigned long long>(s_cmd[1]) << 2, length,
            s_dimar, static_cast<double>(Now()) / CPU_HZ);
      s_drive_ready = true;
    }
    break;
  case 0xAB:  // seek
    break;
  case 0xE0:  // request error
    s_diimmbuf = s_error;
    s_error = ERR_NONE;
    break;
  case 0xE1:  // audio stream
  case 0xE2:  // audio status
    s_error = ERR_NO_AUDIO_BUF;
    error = true;
    break;
  case 0xE3:  // stop motor
    break;
  case 0xE4:  // audio buffer config (DTK off)
    break;
  default:
    Log("[di] unhandled command %08x %08x %08x\n", s_cmd[0], s_cmd[1], s_cmd[2]);
    s_error = ERR_INVALID_COMMAND;
    error = true;
    break;
  }
  s_pending_deint = error;
  Schedule(EV_DI, latency, Complete);
}
}  // namespace

void DiInit()
{
  s_disr = s_dicvr = s_dimar = s_dilength = s_dicr = s_diimmbuf = 0;
  s_cmd[0] = s_cmd[1] = s_cmd[2] = 0;
  s_dicfg = 1;
  s_error = ERR_NONE;
  s_drive_ready = false;
  SchedRegister(EV_DI, Complete);
}

void DiDoState(StateIO& p)
{
  p.Do(s_disr);
  p.Do(s_dicvr);
  p.DoArray(s_cmd, 3);
  p.Do(s_dimar);
  p.Do(s_dilength);
  p.Do(s_dicr);
  p.Do(s_diimmbuf);
  p.Do(s_dicfg);
  p.Do(s_error);
  p.Do(s_drive_ready);
  p.Do(s_pending_deint);
}

// A Dolphin state's DI. A command it had in flight completes here at the same time: a disc read
// (FinishReadDVDThread) with the bytes the state holds for it (the DVD thread's results), copied to
// RAM now, which the game sees only at the transfer interrupt anyway.
void DiFromDolphin(const DolphinState& d)
{
  s_disr = static_cast<u32>(d.U("hw/dvd/DISR"));
  s_dicvr = static_cast<u32>(d.U("hw/dvd/DICVR"));
  d.Array("hw/dvd/DICMDBUF", s_cmd, 3);
  s_dimar = static_cast<u32>(d.U("hw/dvd/DIMAR"));
  s_dilength = static_cast<u32>(d.U("hw/dvd/DILENGTH"));
  s_dicr = static_cast<u32>(d.U("hw/dvd/DICR"));
  s_diimmbuf = static_cast<u32>(d.U("hw/dvd/DIIMMBUF"));
  s_dicfg = static_cast<u32>(d.U("hw/dvd/DICFG"));
  s_error = static_cast<u32>(d.U("hw/dvd/error_code"));
  s_drive_ready = d.U("hw/dvd/drive_state") == 0;  // DriveState::Ready (not ReadyNoReadsMade)
  s_pending_deint = false;
  // FinishExecutingCommand (scheduled by SchedFromDolphin): its interrupt type, DEINT = 0
  for (const DolphinState::Event& e : d.Events())
  {
    if (e.name == "FinishExecutingCommand" && (e.userdata >> 32) != 3)
      s_pending_deint = (e.userdata & 0xFFFFFFFF) == 0;
  }
  const DolphinState::Event* read = d.FindEvent("FinishReadDVDThread");
  if (!read)
    return;
  const u64 results = d.U("hw/dvd/thread/result_count");
  for (u64 i = 0; i < results; ++i)
  {
    const std::string at = "hw/dvd/thread/result[" + std::to_string(i) + "]/";
    if (d.U(at + "id_key") != read->userdata)
      continue;
    // DVDThread::ReadRequest: copy_to_ram @0, output_address @4, dvd_offset @8, length @16
    const std::span<const u8> request = d.Bytes(at + "request");
    const std::span<const u8> data = d.Bytes(at + "data/data");
    u32 output = 0, length = 0;
    std::memcpy(&output, request.data() + 4, 4);
    std::memcpy(&length, request.data() + 16, 4);
    s_pending_deint = data.size() != length;
    if (!s_pending_deint && request[0] != 0)
    {
      u8* dst = RamPtr(output, length);
      if (!dst)
        throw dsr::ParseError("the state's disc read goes outside RAM");
      std::memcpy(dst, data.data(), length);
    }
    Schedule(EV_DI, read->time > Now() ? read->time - Now() : 0, Complete);
    return;
  }
  throw dsr::ParseError("the state's disc read has no data");
}

u32 DiRead32(u32 offset)
{
  switch (offset)
  {
  case 0x00:
    return s_disr;
  case 0x04:
    return s_dicvr;
  case 0x08:
  case 0x0C:
  case 0x10:
    return s_cmd[(offset - 8) / 4];
  case 0x14:
    return s_dimar;
  case 0x18:
    return s_dilength;
  case 0x1C:
    return s_dicr;
  case 0x20:
    return s_diimmbuf;
  case 0x24:
    return s_dicfg;
  default:
    return 0;
  }
}

void DiWrite32(u32 offset, u32 value)
{
  switch (offset)
  {
  case 0x00:
    s_disr = (s_disr & (DISR_DEINT | DISR_TCINT | DISR_BRKINT)) |
             (value & (DISR_DEINTMASK | DISR_TCINTMASK | DISR_BRKINTMASK | DISR_BREAK));
    if (value & DISR_DEINT)
      s_disr &= ~DISR_DEINT;
    if (value & DISR_TCINT)
      s_disr &= ~DISR_TCINT;
    if (value & DISR_BRKINT)
      s_disr &= ~DISR_BRKINT;
    UpdateInterrupts();
    return;
  case 0x04:
    s_dicvr = (s_dicvr & ~DICVR_CVRINTMASK) | (value & DICVR_CVRINTMASK);
    if (value & DICVR_CVRINT)
      s_dicvr &= ~DICVR_CVRINT;
    UpdateInterrupts();
    return;
  case 0x08:
  case 0x0C:
  case 0x10:
    s_cmd[(offset - 8) / 4] = value;
    return;
  case 0x14:
    s_dimar = value & ~0xFC00001Fu;
    return;
  case 0x18:
    s_dilength = value & ~0x1Fu;
    return;
  case 0x1C:
    s_dicr = value & 7;
    if (s_dicr & 1)
      Execute();
    return;
  case 0x20:
    s_diimmbuf = value;
    return;
  default:
    return;
  }
}

void DiStats(u64* reads, u64* bytes)
{
  *reads = s_reads;
  *bytes = s_bytes;
}
}  // namespace host
