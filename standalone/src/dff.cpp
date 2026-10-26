// The GP's registers as the command stream left them, for save states (DffDoState, DffReloadAurora)
// and, as a debug tool, frames recorded as a Dolphin FIFO log (.dff, Core/FifoPlayer/FifoDataFile.cpp format 6)
// so stock Dolphin can render exactly the command stream this host sends Aurora; any difference to
// the standalone's own picture is then Aurora's (or this host's command handling).
//
//   SKIES_DFF=FIRST:COUNT:PATH[;FIRST:COUNT:PATH...]  records the frames ending with XFB copies
//                               FIRST..FIRST+COUNT-1 (the numbers the frame dumps use); captures
//                               in frame order, not overlapping
//
// The registers are shadowed from boot (BP with its mask register, CP, XF memory and registers, TMEM
// for TLUT loads); the frames hold the top-level commands, and RAM as it is at each frame's end
// (the first frame: all 24 MB; then the 4 KB pages that changed) as memory updates at its start.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"
#include "state.h"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace host
{
namespace
{
u32 s_bp[256];
u32 s_bp_mask = 0xFFFFFF;
u32 s_cp[256];
u32 s_xf_mem[4096];
u32 s_xf_regs[88];
std::vector<u8> s_tmem(1024 * 1024);
u32 s_preloads = 0;
// The last TLUT load to each TMEM offset (BP 0x65's low 10 bits): its source (BP 0x64) and the load.
// Aurora keeps a palette as a pointer into guest RAM, so a load replayed after a save state points
// it at the same palette.
struct TlutLoad
{
  u32 src = 0, load = 0;
  bool loaded = false;
};
std::vector<TlutLoad> s_tlut_loads(1024);

struct Update
{
  u32 address;
  std::vector<u8> data;
};
struct Frame
{
  std::vector<u8> fifo;
  std::vector<Update> updates;
};

struct Capture
{
  u64 first, count;
  std::string path;
};
std::vector<Capture> s_captures;  // pending, in order
bool s_enabled = false;
u64 s_first = 0, s_count = 0;
std::string s_path;

bool NextCapture()
{
  if (s_captures.empty())
    return false;
  s_first = s_captures.front().first;
  s_count = s_captures.front().count;
  s_path = s_captures.front().path;
  s_captures.erase(s_captures.begin());
  return true;
}
bool s_recording = false;
std::vector<Frame> s_frames_rec;
std::vector<u8> s_image;  // RAM as last recorded
u32 s_start_bp[256], s_start_cp[256], s_start_xf_mem[4096], s_start_xf_regs[88];
std::vector<u8> s_start_tmem;

void XfWrite(u32 address, u32 value)
{
  if (address < 0x1000)
    s_xf_mem[address] = value;
  else if (address - 0x1000 < 88)
    s_xf_regs[address - 0x1000] = value;
}

void BpWrite(u8 reg, u32 value)
{
  if (reg == 0xFE)
  {
    s_bp_mask = value & 0xFFFFFF;
    s_bp[reg] = value & 0xFFFFFF;
    return;
  }
  s_bp[reg] = (s_bp[reg] & ~s_bp_mask) | (value & s_bp_mask);
  s_bp_mask = 0xFFFFFF;
  if (reg == 0x65)
  {
    s_tlut_loads[s_bp[0x65] & 0x3FF] = {s_bp[0x64], s_bp[0x65], true};
    // BPMEM_LOADTLUT1: tlut_src (0x64) << 5 to TMEM tmem_addr << 9, line_count x 32 bytes
    const u32 tmem_addr = (s_bp[0x65] & 0x3FF) << 9;
    const u32 count = ((s_bp[0x65] >> 10) & 0x7FF) * 32;
    const u32 src = (s_bp[0x64] << 5) & 0x01FFFFFF;
    const u8* p = RamPtr(src, count);
    if (p && tmem_addr + count <= s_tmem.size())
      std::memcpy(s_tmem.data() + tmem_addr, p, count);
  }
  else if (reg == 0x52 && !(s_bp[0x52] & 0x4000) && std::getenv("SKIES_COPY_LOG"))
  {
    // EFB copy to a texture: source 0x49 (x 0-9, y 10-19), size 0x4A (w-1, h-1), dest 0x4B << 5,
    // dest stride 0x4D (in 32-byte lines), format bits 4-6 + bit 3 (and intensity bit 15)
    const u32 v = s_bp[0x52];
    Log("[copy] frame %llu: %08x <- %u,%u %ux%u fmt %u%s stride %u half %u clear %u\n", static_cast<unsigned long long>(GpuFrameCount()), (s_bp[0x4B] & 0xFFFFFF) << 5,
        s_bp[0x49] & 0x3FF, (s_bp[0x49] >> 10) & 0x3FF, (s_bp[0x4A] & 0x3FF) + 1,
        ((s_bp[0x4A] >> 10) & 0x3FF) + 1, ((v >> 4) & 7) | ((v & 8) ? 8 : 0), (v & 0x8000) ? "i" : "",
        (s_bp[0x4D] & 0x3FF), (v >> 9) & 1, (v >> 11) & 1);
  }
  else if (reg == 0x63 && s_bp[0x63] != 0 && s_preloads++ == 0)
    Log("[dff] the game preloads TMEM (BP 63 = %06x): not shadowed\n", s_bp[0x63]);
}

void Begin()
{
  std::memcpy(s_start_bp, s_bp, sizeof(s_bp));
  std::memcpy(s_start_cp, s_cp, sizeof(s_cp));
  std::memcpy(s_start_xf_mem, s_xf_mem, sizeof(s_xf_mem));
  std::memcpy(s_start_xf_regs, s_xf_regs, sizeof(s_xf_regs));
  s_start_tmem = s_tmem;
  s_frames_rec.clear();
  s_frames_rec.emplace_back();
  s_image.clear();
  s_recording = true;
}

// RAM at the end of the frame, as updates at its start: everything the first time, then the pages
// that changed since.
void CloseFrame()
{
  Frame& f = s_frames_rec.back();
  if (s_image.empty())
  {
    f.updates.push_back({0, std::vector<u8>(g_ram, g_ram + RAM_SIZE)});
    s_image.assign(g_ram, g_ram + RAM_SIZE);
    return;
  }
  constexpr u32 PAGE = 4096;
  u32 run_start = 0;
  bool in_run = false;
  for (u32 page = 0; page <= RAM_SIZE; page += PAGE)
  {
    const bool changed =
        page < RAM_SIZE && std::memcmp(g_ram + page, s_image.data() + page, PAGE) != 0;
    if (changed && !in_run)
    {
      run_start = page;
      in_run = true;
    }
    else if (!changed && in_run)
    {
      f.updates.push_back({run_start, std::vector<u8>(g_ram + run_start, g_ram + page)});
      std::memcpy(s_image.data() + run_start, g_ram + run_start, page - run_start);
      in_run = false;
    }
  }
}

void PutAt(std::vector<u8>& out, size_t at, const void* data, size_t size)
{
  std::memcpy(out.data() + at, data, size);
}

void Write()
{
  // FileHeader (128) + FileFrameInfo (64) per frame, then the register blocks, then each frame's
  // FIFO data, its FileMemoryUpdate list (24 each) and their data. Little-endian, as Dolphin
  // writes it on x86.
  std::vector<u8> out(128 + 64 * s_frames_rec.size(), 0);
  const u64 bp_off = out.size();
  out.insert(out.end(), reinterpret_cast<u8*>(s_start_bp), reinterpret_cast<u8*>(s_start_bp) + sizeof(s_start_bp));
  const u64 cp_off = out.size();
  out.insert(out.end(), reinterpret_cast<u8*>(s_start_cp), reinterpret_cast<u8*>(s_start_cp) + sizeof(s_start_cp));
  const u64 xf_off = out.size();
  out.insert(out.end(), reinterpret_cast<u8*>(s_start_xf_mem),
             reinterpret_cast<u8*>(s_start_xf_mem) + sizeof(s_start_xf_mem));
  const u64 xfr_off = out.size();
  out.insert(out.end(), reinterpret_cast<u8*>(s_start_xf_regs),
             reinterpret_cast<u8*>(s_start_xf_regs) + sizeof(s_start_xf_regs));
  const u64 tex_off = out.size();
  out.insert(out.end(), s_start_tmem.begin(), s_start_tmem.end());

  for (size_t i = 0; i < s_frames_rec.size(); ++i)
  {
    const Frame& f = s_frames_rec[i];
    const u64 fifo_off = out.size();
    out.insert(out.end(), f.fifo.begin(), f.fifo.end());
    const u64 list_off = out.size();
    out.resize(out.size() + 24 * f.updates.size(), 0);
    for (size_t u = 0; u < f.updates.size(); ++u)
    {
      const u64 data_off = out.size();
      out.insert(out.end(), f.updates[u].data.begin(), f.updates[u].data.end());
      u8 entry[24] = {};
      const u32 position = 0, address = f.updates[u].address;
      const u32 size = static_cast<u32>(f.updates[u].data.size());
      std::memcpy(entry + 0, &position, 4);
      std::memcpy(entry + 4, &address, 4);
      std::memcpy(entry + 8, &data_off, 8);
      std::memcpy(entry + 16, &size, 4);
      entry[20] = 0x01;  // TextureMap (FifoPlayer writes every kind to RAM alike)
      PutAt(out, list_off + 24 * u, entry, 24);
    }
    u8 info[64] = {};
    const u32 fifo_size = static_cast<u32>(f.fifo.size());
    const u32 updates = static_cast<u32>(f.updates.size());
    std::memcpy(info + 0, &fifo_off, 8);
    std::memcpy(info + 8, &fifo_size, 4);
    std::memcpy(info + 16, &fifo_size, 4);  // fifoStart 0, fifoEnd: the CP FIFO range, unused
    std::memcpy(info + 20, &list_off, 8);
    std::memcpy(info + 28, &updates, 4);
    PutAt(out, 128 + 64 * i, info, 64);
  }

  u8 header[128] = {};
  auto put32 = [&](size_t at, u32 v) { std::memcpy(header + at, &v, 4); };
  auto put64 = [&](size_t at, u64 v) { std::memcpy(header + at, &v, 8); };
  put32(0, 0x0d01f1f0);  // FILE_ID
  put32(4, 6);           // version
  put32(8, 1);           // min loader version
  put64(12, bp_off);
  put32(20, 256);
  put64(24, cp_off);
  put32(32, 256);
  put64(36, xf_off);
  put32(44, 4096);
  put64(48, xfr_off);
  put32(56, 88);
  put64(60, 128);  // frame list
  put32(68, static_cast<u32>(s_frames_rec.size()));
  put32(72, 0);  // flags: GameCube
  put64(76, tex_off);
  put32(84, static_cast<u32>(s_start_tmem.size()));
  put32(88, RAM_SIZE);
  put32(92, 0);
  std::memcpy(header + 96, DiscHeader(), 6);
  PutAt(out, 0, header, 128);

  std::ofstream file(s_path, std::ios::binary | std::ios::trunc);
  file.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
  Log("[dff] wrote %s: %zu frames, %.1f MB%s\n", s_path.c_str(), s_frames_rec.size(),
      out.size() / 1048576.0, file ? "" : " (write failed)");
}
}  // namespace

void DffDoState(StateIO& p)
{
  p.DoArray(s_bp, 256);
  p.Do(s_bp_mask);
  p.DoArray(s_cp, 256);
  p.DoArray(s_xf_mem, 4096);
  p.DoArray(s_xf_regs, 88);
  p.Do(s_tmem);
  p.Do(s_tlut_loads);
  if (p.Reading() && (s_tmem.size() != 1024 * 1024 || s_tlut_loads.size() != 1024))
  {
    p.Fail("the state's GP registers have another layout");
    s_tmem.assign(1024 * 1024, 0);
    s_tlut_loads.assign(1024, {});
  }
}

void DffSnapshot(GpRegisters& out)
{
  std::memcpy(out.bp, s_bp, sizeof(s_bp));
  std::memcpy(out.cp, s_cp, sizeof(s_cp));
  std::memcpy(out.xf_mem, s_xf_mem, sizeof(s_xf_mem));
  std::memcpy(out.xf_regs, s_xf_regs, sizeof(s_xf_regs));
  out.tluts.clear();
  for (const TlutLoad& t : s_tlut_loads)
  {
    if (t.loaded)
      out.tluts.emplace_back(t.src, t.load);
  }
}

const u32* DffBp()
{
  return s_bp;
}

const u32* DffCp()
{
  return s_cp;
}

// Aurora only ever sees the command stream, so registers go to it as commands, as the Dolphin
// build's Aurora backend does after a savestate (VideoBackends/Aurora ReloadRegisterState,
// FifoPlayer::LoadRegisters): the TLUT loads, every BP register but those with side effects (draw
// done, tokens, the EFB copy, TLUT and TMEM loads, perf counters, the mask), the CP's vertex
// descriptors, formats and arrays, then XF memory and registers.
void DffRegisterCommands(const GpRegisters& regs, std::vector<u8>& fifo)
{
  fifo.reserve(fifo.size() + 32 * 1024);
  const auto put32 = [&](u32 value) {
    for (int shift = 24; shift >= 0; shift -= 8)
      fifo.push_back(static_cast<u8>(value >> shift));
  };
  const auto bp = [&](u32 reg, u32 value) {
    fifo.push_back(0x61);
    put32((reg << 24) | (value & 0xFFFFFF));
  };
  for (const auto& [src, load] : regs.tluts)
  {
    bp(0x64, src);
    bp(0x65, load);
  }
  for (u32 reg = 0; reg < 0xFE; ++reg)
  {
    switch (reg)
    {
    case 0x45:
    case 0x47:
    case 0x48:
    case 0x52:
    case 0x63:
    case 0x65:
    case 0x67:
      continue;
    default:
      bp(reg, regs.bp[reg]);
    }
  }
  const auto cp = [&](u8 reg) {
    fifo.push_back(0x08);
    fifo.push_back(reg);
    put32(regs.cp[reg]);
  };
  cp(0x30);
  cp(0x40);
  cp(0x50);
  cp(0x60);
  for (u8 i = 0; i < 8; ++i)
  {
    cp(0x70 + i);
    cp(0x80 + i);
    cp(0x90 + i);
  }
  for (u8 i = 0; i < 16; ++i)
  {
    cp(0xA0 + i);
    cp(0xB0 + i);
  }
  for (u32 address = 0; address < 4096; address += 16)
  {
    fifo.push_back(0x10);
    put32(0x000F0000u | address);
    for (u32 i = 0; i < 16; ++i)
      put32(regs.xf_mem[address + i]);
  }
  for (u32 reg = 0; reg < 88; ++reg)
  {
    // FifoPlayer::ShouldLoadXF: not the unknown and perf ranges
    if (reg == 0x07 || (reg >= 0x13 && reg <= 0x17) || (reg >= 0x27 && reg <= 0x3E) ||
        (reg >= 0x48 && reg <= 0x4F))
      continue;
    fifo.push_back(0x10);
    put32(0x1000u | reg);
    put32(regs.xf_regs[reg]);
  }
}

// After a state load: the registers, then the display copy's clear. A state is taken just after a
// display copy, which cleared the EFB (Aurora applies only that clear): the same copy again leaves
// the EFB as the next frame expects it (the clear colour and depth: Skies' save point and minimap
// copy the EFB's depth).
void DffReloadAurora()
{
  static GpRegisters regs;
  DffSnapshot(regs);
  std::vector<u8> fifo;
  DffRegisterCommands(regs, fifo);
  if (s_bp[0x52] & 0x4000)
  {
    fifo.push_back(0x61);
    const u32 value = (0x52u << 24) | (s_bp[0x52] & 0xFFFFFF);
    for (int shift = 24; shift >= 0; shift -= 8)
      fifo.push_back(static_cast<u8>(value >> shift));
  }
  VideoCommands(fifo.data(), static_cast<u32>(fifo.size()), false);
}

void DffInit()
{
  const char* spec = std::getenv("SKIES_DFF");
  if (!spec)
    return;
  std::string rest = spec;
  while (!rest.empty())
  {
    const size_t semi = rest.find(';');
    const std::string one = rest.substr(0, semi);
    rest = semi == std::string::npos ? "" : rest.substr(semi + 1);
    char* end = nullptr;
    Capture c{std::strtoull(one.c_str(), &end, 0), 0, ""};
    if (*end == ':')
      c.count = std::strtoull(end + 1, &end, 0);
    if (*end == ':')
      c.path = end + 1;
    if (c.first > 1 && c.count > 0 && !c.path.empty())
      s_captures.push_back(c);
    else
      Log("[dff] SKIES_DFF=FIRST:COUNT:PATH[;...] (FIRST > 1): skipped '%s'\n", one.c_str());
  }
  s_enabled = NextCapture();
}

// Every command's register effects, top level and inside display lists; top-level commands are
// recorded while a capture runs.
void DffCommand(const u8* p, u32 len, bool top_level)
{
  switch (p[0])
  {
  case 0x61:
    BpWrite(p[1], (u32(p[2]) << 16) | (u32(p[3]) << 8) | p[4]);
    break;
  case 0x08:
    s_cp[p[1]] = BE32(p + 2);
    break;
  case 0x10:
  {
    const u32 head = BE32(p + 1);
    const u32 address = head & 0xFFFF;
    const u32 count = ((head >> 16) & 0xF) + 1;
    for (u32 i = 0; i < count && 5 + 4 * i + 4 <= len; ++i)
      XfWrite(address + i, BE32(p + 5 + 4 * i));
    break;
  }
  case 0x20:
  case 0x28:
  case 0x30:
  case 0x38:
  {
    // indexed XF load from array 12..15: index << 16 | (count - 1) << 12 | XF address
    const u32 value = BE32(p + 1);
    const u32 array = 12 + ((p[0] - 0x20) >> 3);
    const u32 index = value >> 16, address = value & 0xFFF, count = ((value >> 12) & 0xF) + 1;
    const u32 base = s_cp[0xA0 + array] + index * (s_cp[0xB0 + array] & 0xFF);
    if (const u8* src = RamPtr(base, count * 4))
      for (u32 i = 0; i < count; ++i)
        XfWrite(address + i, BE32(src + 4 * i));
    break;
  }
  default:
    break;
  }
  if (top_level && s_recording)
    s_frames_rec.back().fifo.insert(s_frames_rec.back().fifo.end(), p, p + len);
}

// After XFB copy number `frame` (its command already recorded).
void DffFrameEnd(u64 frame)
{
  if (!s_enabled)
    return;
  if (!s_recording && frame + 1 == s_first)
  {
    Begin();
    return;
  }
  if (!s_recording)
    return;
  CloseFrame();
  if (frame + 1 >= s_first + s_count)
  {
    Write();
    s_recording = false;
    s_enabled = NextCapture();
    return;
  }
  s_frames_rec.emplace_back();
}
}  // namespace host
