// Disc interface: the DI registers and the drive commands a GameCube game sends
// (DVDInterface.cpp: RegisterMMIO, ExecuteCommand, UpdateInterrupts). Reads complete when Dolphin's
// drive model says (DVDMath.cpp, ScheduleReads: seeks, rotation, the CAV read speed and the 1 MiB read-ahead
// buffer), as the Dolphin build does by default; a faster drive left the USA intro waiting forever after its
// last model load. Audio streaming (DTK) is off, as the disc header asks.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "dolphin_state.h"
#include "host.h"
#include "state.h"

#include <algorithm>
#include <cmath>
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

// ---- Dolphin's drive timing (DVDMath.cpp and DVDInterface::ScheduleReads, GameCube discs only) ----
constexpr double WII_DISC_LAYER_SIZE = 0x118240000;
constexpr double DVD_INNER_RADIUS = 0.024;
constexpr double WII_DVD_OUTER_RADIUS = 0.058;
constexpr double GC_DVD_OUTER_RADIUS = 0.038;
constexpr double DVD_TRACK_PITCH = 0.00000074;
constexpr double GC_DISC_INNER_READ_SPEED = 1024 * 1024 * 2.1;    // bytes/s
constexpr double GC_DISC_OUTER_READ_SPEED = 1024 * 1024 * 3.325;  // bytes/s
constexpr double GC_ROTATIONS_PER_SECOND = 28.5;
constexpr double SHORT_SEEK_MAX_DISTANCE = 0.001;   // 1 mm
constexpr double SHORT_SEEK_CONSTANT = 0.035;       // seconds
constexpr double SHORT_SEEK_VELOCITY_INVERSE = 50;  // s/m
constexpr double LONG_SEEK_CONSTANT = 0.075;        // seconds
constexpr double LONG_SEEK_VELOCITY_INVERSE = 4.5;  // s/m
constexpr u64 MINIMUM_COMMAND_LATENCY_US = 300;
constexpr u64 READ_COMMAND_LATENCY_US = 600;
constexpr u64 STREAMING_BUFFER_SIZE = 1024 * 1024;
constexpr u64 DVD_ECC_BLOCK_SIZE = 16 * 0x800;
constexpr u64 BUFFER_TRANSFER_RATE = 32 * 1024 * 1024;  // bytes/s from the drive's buffer

// The read-ahead buffer (not in save states: after a load the next read starts unbuffered)
u64 s_buffer_start_offset = 0, s_buffer_end_offset = 0, s_buffer_start_time = 0, s_buffer_end_time = 0;

double PhysicalPosition(u64 offset)
{
  return std::sqrt(static_cast<double>(offset) / WII_DISC_LAYER_SIZE *
                       (WII_DVD_OUTER_RADIUS * WII_DVD_OUTER_RADIUS - DVD_INNER_RADIUS * DVD_INNER_RADIUS) +
                   DVD_INNER_RADIUS * DVD_INNER_RADIUS);
}

double SeekTime(u64 from, u64 to)
{
  const double distance = std::fabs(PhysicalPosition(from) - PhysicalPosition(to));
  if (distance < SHORT_SEEK_MAX_DISTANCE)
    return distance * SHORT_SEEK_VELOCITY_INVERSE + SHORT_SEEK_CONSTANT;
  return distance * LONG_SEEK_VELOCITY_INVERSE + LONG_SEEK_CONSTANT;
}

double RotationalLatency(u64 offset, double time)
{
  const double target = std::fmod(PhysicalPosition(offset) / DVD_TRACK_PITCH, 1.0);
  const double start = std::fmod(time * GC_ROTATIONS_PER_SECOND, 1.0);
  return std::fmod(target + 1.0 - start, 1.0) / GC_ROTATIONS_PER_SECOND;
}

double RawReadTime(u64 offset, u64 length)
{
  const double position = PhysicalPosition(offset + length / 2);
  const double speed = (position - DVD_INNER_RADIUS) / (GC_DVD_OUTER_RADIUS - DVD_INNER_RADIUS) *
                           (GC_DISC_OUTER_READ_SPEED - GC_DISC_INNER_READ_SPEED) +
                       GC_DISC_INNER_READ_SPEED;
  return length / speed;
}

// SKIES_DI_FAST=1: the timing before 2026-10-27 (100 us a command, about 16 MB/s, no seeks), for test
// routes (SKIES_PRESS_FILE) recorded with it; it leaves the USA intro waiting after its last model load
const bool s_fast = [] {
  const char* v = std::getenv("SKIES_DI_FAST");
  return v && v[0] != '0';
}();

// Cycles until a read of LENGTH bytes at OFFSET completes, and the buffer's state after it
// (ScheduleReads; Dolphin completes each ECC block apart, but the game sees only the last one's interrupt)
u64 ReadLatency(u64 offset, u32 length)
{
  if (s_fast)
    return CPU_HZ / 10000 + static_cast<u64>(length) * (CPU_HZ / 1000000) / 16;
  const u64 now = Now();
  bool seek = false;
  u64 head, buffer_start, buffer_end;
  u64 dvd_offset = offset / DVD_ECC_BLOCK_SIZE * DVD_ECC_BLOCK_SIZE;
  const u64 first_block = dvd_offset;
  if (s_buffer_start_time == s_buffer_end_time)
    buffer_start = buffer_end = head = 0;
  else
  {
    buffer_start = s_buffer_end_offset > STREAMING_BUFFER_SIZE ? s_buffer_end_offset - STREAMING_BUFFER_SIZE : 0;
    if (now >= s_buffer_end_time)
      buffer_end = s_buffer_end_offset;
    else
      buffer_end = s_buffer_start_offset +
                   (now - s_buffer_start_time) * (s_buffer_end_offset - s_buffer_start_offset) /
                       (s_buffer_end_time - s_buffer_start_time) / DVD_ECC_BLOCK_SIZE * DVD_ECC_BLOCK_SIZE;
    head = buffer_end;
    if (dvd_offset < buffer_start)
      buffer_start = buffer_end = 0;
  }
  u64 ticks = READ_COMMAND_LATENCY_US * (CPU_HZ / 1000000);
  do
  {
    u32 chunk = static_cast<u32>((offset + 1 + DVD_ECC_BLOCK_SIZE - 1) / DVD_ECC_BLOCK_SIZE * DVD_ECC_BLOCK_SIZE - offset);
    chunk = std::min(chunk, length);
    if (dvd_offset >= buffer_start && dvd_offset < buffer_end)
      ticks += static_cast<u64>(chunk) * CPU_HZ / BUFFER_TRANSFER_RATE;
    else
    {
      if (dvd_offset != head)
      {
        seek = true;
        ticks += static_cast<u64>(CPU_HZ * SeekTime(head, dvd_offset));
        const double after_seek = static_cast<double>(now + ticks) / CPU_HZ;
        ticks += static_cast<u64>(CPU_HZ * RotationalLatency(dvd_offset, after_seek));
      }
      else
        ticks += static_cast<u64>(CPU_HZ * RawReadTime(dvd_offset, DVD_ECC_BLOCK_SIZE));
      head = dvd_offset + DVD_ECC_BLOCK_SIZE;
    }
    offset += chunk;
    length -= chunk;
    dvd_offset += DVD_ECC_BLOCK_SIZE;
  } while (length > 0);
  // The blocks kept: the last one read, the one before it and everything after
  const u64 last_block = dvd_offset;
  constexpr u64 BACKWARD_LIMIT = DVD_ECC_BLOCK_SIZE * 2;
  if (!(last_block - buffer_start <= BACKWARD_LIMIT && buffer_start != buffer_end))
  {
    s_buffer_start_offset = last_block >= buffer_end ? last_block : buffer_end;
    s_buffer_end_offset = last_block + STREAMING_BUFFER_SIZE - BACKWARD_LIMIT;
    if (seek)
      s_buffer_end_offset = std::max(s_buffer_end_offset, first_block + STREAMING_BUFFER_SIZE);
    s_buffer_start_time = now + ticks;
    s_buffer_end_time = s_buffer_start_time +
                        static_cast<u64>(CPU_HZ * RawReadTime(s_buffer_start_offset,
                                                              s_buffer_end_offset - s_buffer_start_offset));
  }
  return ticks;
}

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
  u64 latency = s_fast ? CPU_HZ / 10000 : MINIMUM_COMMAND_LATENCY_US * (CPU_HZ / 1000000);
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
      latency = ReadLatency(static_cast<u64>(s_cmd[1]) << 2, length);
      error = !DmaRead(static_cast<u64>(s_cmd[1]) << 2, s_cmd[2]);
      static const bool s_trace = std::getenv("SKIES_DI_TRACE") != nullptr;
      if (s_trace)
        Log("[di] read %llx +%x to %08x (%.3f s) %s\n", static_cast<unsigned long long>(s_cmd[1]) << 2,
            length, s_dimar, static_cast<double>(Now()) / CPU_HZ,
            DiscPathAt(static_cast<u64>(s_cmd[1]) << 2).c_str());
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
  s_buffer_start_offset = s_buffer_end_offset = s_buffer_start_time = s_buffer_end_time = 0;
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
  s_buffer_start_offset = s_buffer_end_offset = s_buffer_start_time = s_buffer_end_time = 0;
}

// A Dolphin state's DI. A command it had in flight completes here at the same time: a disc read
// (FinishReadDVDThread) with the bytes the state holds for it (the DVD thread's results), copied to
// RAM now, which the game sees only at the transfer interrupt anyway.
void DiFromDolphin(const DolphinState& d)
{
  s_buffer_start_offset = s_buffer_end_offset = s_buffer_start_time = s_buffer_end_time = 0;
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
