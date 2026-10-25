// The GPU's CPU-visible side: the write-gather pipe, the command processor's FIFO (linked mode,
// read/write pointers, watermarks), the pixel engine's token and draw-done interrupts, and the
// command stream handed to Aurora (GPFifo.cpp, CommandProcessor.cpp, PixelEngine.cpp,
// BPStructs.cpp, OpcodeDecoding.cpp, VertexLoaderBase.cpp).
//
// The GPU runs synchronously: every 32-byte burst into the FIFO is consumed at once. Commands are
// split here (vertex sizes from the VCD/VAT registers) so that side effects the CPU waits on
// happen in stream order; Aurora gets the bytes unchanged and resolves display lists, arrays
// and textures from guest RAM itself.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"
#include "state.h"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace host
{
namespace
{
// ---- gather pipe ---------------------------------------------------------------------------
constexpr u32 GP_SIZE = 32;
alignas(32) u8 s_gp[GP_SIZE * 16];
u8* s_gp_ptr = s_gp;

// ---- command processor -----------------------------------------------------------------------
struct Fifo
{
  u32 base = 0, end = 0, hi_wm = 0, lo_wm = 0, distance = 0, wp = 0, rp = 0, bp = 0;
} s_fifo;
u16 s_cp_ctrl = 0;
u16 s_cp_perf_select = 0, s_cp_unk_0a = 0;
bool s_cp_int = false;
enum : u16
{
  CTRL_READ_ENABLE = 1,
  CTRL_BP_ENABLE = 2,
  CTRL_OVF_INT = 4,
  CTRL_UNDF_INT = 8,
  CTRL_LINK = 0x10,
  CTRL_BP_INT = 0x20,
};

// ---- pixel engine ------------------------------------------------------------------------------
u16 s_pe_conf[5];  // ZCONF, ALPHACONF, DSTALPHACONF, ALPHAMODE, ALPHAREAD
u16 s_pe_ctrl = 0;  // bit 0 token interrupt enable, bit 1 finish interrupt enable
u16 s_pe_token = 0;
bool s_pe_token_int = false;
bool s_pe_finish_int = false;
u64 s_draw_dones = 0, s_tokens = 0, s_frames = 0;

void PeUpdateInterrupts()
{
  PiSetInterrupt(PI_INT_PE_TOKEN, s_pe_token_int && (s_pe_ctrl & 1));
  PiSetInterrupt(PI_INT_PE_FINISH, s_pe_finish_int && (s_pe_ctrl & 2));
}

// ---- command parsing ---------------------------------------------------------------------------
struct VertexState
{
  u32 vcd_lo = 0, vcd_hi = 0;
  u32 vat_a[8] = {}, vat_b[8] = {}, vat_c[8] = {};
} s_vtx;

u32 ComponentSize(u32 format)
{
  switch (format)
  {
  case 0:
  case 1:
    return 1;
  case 2:
  case 3:
    return 2;
  default:
    return 4;
  }
}

u32 IndexedSize(u32 type, u32 direct_size)
{
  switch (type)
  {
  case 1:
    return direct_size;
  case 2:
    return 1;
  case 3:
    return 2;
  default:
    return 0;
  }
}

u32 VertexSize(const VertexState& s, u32 vat)
{
  const u32 lo = s.vcd_lo, hi = s.vcd_hi;
  const u32 a = s.vat_a[vat], b = s.vat_b[vat], c = s.vat_c[vat];
  u32 size = 0;
  if (lo & 1)
    ++size;  // position/normal matrix index
  for (u32 i = 0; i < 8; ++i)
    size += (lo >> (1 + i)) & 1;  // texture matrix indices

  const u32 pos = (lo >> 9) & 3;
  size += IndexedSize(pos, ((a & 1) ? 3 : 2) * ComponentSize((a >> 1) & 7));

  const u32 nrm = (lo >> 11) & 3;
  if (nrm)
  {
    const bool nbt = (a >> 9) & 1;
    const u32 direct = (nbt ? 9 : 3) * ComponentSize((a >> 10) & 7);
    if (nrm == 1)
      size += direct;
    else
      size += (nbt && (a >> 31)) ? 3 * (nrm == 2 ? 1 : 2) : (nrm == 2 ? 1 : 2);
  }

  static constexpr u32 kColorSize[8] = {2, 3, 4, 2, 3, 4, 4, 4};
  size += IndexedSize((lo >> 13) & 3, kColorSize[(a >> 14) & 7]);
  size += IndexedSize((lo >> 15) & 3, kColorSize[(a >> 18) & 7]);

  // texture coordinates: (elements bit, format field) per VAT group
  const u32 tex_elems[8] = {(a >> 21) & 1, b & 1, (b >> 9) & 1, (b >> 18) & 1,
                            (b >> 27) & 1, (c >> 5) & 1, (c >> 14) & 1, (c >> 23) & 1};
  const u32 tex_fmt[8] = {(a >> 22) & 7, (b >> 1) & 7, (b >> 10) & 7, (b >> 19) & 7,
                          (b >> 28) & 7, (c >> 6) & 7, (c >> 15) & 7, (c >> 24) & 7};
  for (u32 i = 0; i < 8; ++i)
  {
    const u32 type = (hi >> (2 * i)) & 3;
    size += IndexedSize(type, (tex_elems[i] ? 2 : 1) * ComponentSize(tex_fmt[i]));
  }
  return size;
}

void LoadCpReg(VertexState& s, u8 reg, u32 value)
{
  switch (reg & 0xF0)
  {
  case 0x50:
    s.vcd_lo = value;
    break;
  case 0x60:
    s.vcd_hi = value;
    break;
  case 0x70:
    s.vat_a[reg & 7] = value;
    break;
  case 0x80:
    s.vat_b[reg & 7] = value;
    break;
  case 0x90:
    s.vat_c[reg & 7] = value;
    break;
  default:
    break;
  }
}

// Length of the command at p, 0 when it is not complete in avail bytes.
u32 CommandLength(const VertexState& s, const u8* p, u32 avail)
{
  const u8 cmd = p[0];
  u32 len;
  if (cmd & 0x80)
  {
    if (avail < 3)
      return 0;
    len = 3 + BE16(p + 1) * VertexSize(s, cmd & 7);
  }
  else
  {
    switch (cmd)
    {
    case 0x00:  // NOP
    case 0x44:  // metrics
    case 0x48:  // invalidate vertex cache
      len = 1;
      break;
    case 0x08:
      len = 6;
      break;
    case 0x10:
      if (avail < 5)
        return 0;
      len = 5 + 4 * (((BE32(p + 1) >> 16) & 0xF) + 1);
      break;
    case 0x20:
    case 0x28:
    case 0x30:
    case 0x38:
    case 0x61:
      len = 5;
      break;
    case 0x40:
      len = 9;
      break;
    default:
    {
      static int s_logged = 0;
      if (s_logged < 16)
      {
        ++s_logged;
        Log("[gpu] unknown opcode %02x, skipped\n", cmd);
      }
      len = 1;
      break;
    }
    }
  }
  return len <= avail ? len : 0;
}

enum class Effect
{
  None,
  XfbCopy,
};

// The EFB copy registers (source 0x49, size 0x4A, destination 0x4B, stride 0x4D).
u32 s_copy_size = 0, s_copy_dest = 0, s_copy_stride = 0;

// An EFB copy to a texture writes its pixels to RAM on the hardware. Aurora keeps the copy on the GPU
// and recognises it by the guest address, dropping it once the memory there changes; so the copy
// must leave a mark in RAM, or a buffer the game later reloads with the very bytes it held before
// the copy (the field's UI atlas at 0x80667540 after a battle, which the battle transition copies
// the screen into) still shows the old copy. The mark is Dolphin's for copies kept in VRAM
// (TextureCacheBase::UninitializeEFBMemory): the copied tiles zeroed, at the time the game sees
// the copy done.
void MarkEfbCopy(u32 control)
{
  // Dolphin's UPE_Copy::tp_realFormat and TexDecoder_GetEFBCopyBlock*: block size per format
  const u32 tp = (control >> 3) & 0xF;
  const u32 format = tp / 2 + (tp & 1) * 8;
  u32 bw = 4, bh = 4, block_bytes = 32;
  switch (format)
  {
  case 0x0:  // R4 / Z4
    bw = 8;
    bh = 8;
    break;
  case 0x1:  // R8 (0x1), RA4, A8, R8, G8, B8 (and their depth forms)
  case 0x2:
  case 0x7:
  case 0x8:
  case 0x9:
  case 0xA:
    bw = 8;
    break;
  case 0x6:  // RGBA8 / Z24X8
    block_bytes = 64;
    break;
  default:  // 16-bit formats
    break;
  }
  u32 width = (s_copy_size & 0x3FF) + 1, height = ((s_copy_size >> 10) & 0x3FF) + 1;
  if (control & (1u << 9))  // half scale
  {
    width = (width + 1) / 2;
    height = (height + 1) / 2;
  }
  const u32 pitch = (s_copy_stride & 0x3FF) * 32;
  const u32 row_bytes = (width + bw - 1) / bw * block_bytes;
  const u32 rows = (height + bh - 1) / bh;
  const u32 dest = (s_copy_dest & 0xFFFFFF) << 5;
  for (u32 r = 0; r < rows; ++r)
  {
    u8* p = RamPtr(dest + r * pitch, row_bytes);
    if (!p)
      break;
    std::memset(p, 0, row_bytes);
  }
}

// BP writes the CPU can observe.
Effect BpWrite(u8 reg, u32 value)
{
  switch (reg)
  {
  case 0x4A:
    s_copy_size = value;
    break;
  case 0x4B:
    s_copy_dest = value;
    break;
  case 0x4D:
    s_copy_stride = value;
    break;
  case 0x45:  // draw done
    if ((value & 0xFF) == 0x02)
    {
      s_pe_finish_int = true;
      ++s_draw_dones;
      PeUpdateInterrupts();
    }
    break;
  case 0x47:  // token
    s_pe_token = static_cast<u16>(value);
    ++s_tokens;
    break;
  case 0x48:  // token with interrupt
    s_pe_token = static_cast<u16>(value);
    s_pe_token_int = true;
    ++s_tokens;
    PeUpdateInterrupts();
    break;
  case 0x52:  // EFB copy; bit 14: to the XFB (the frame's display copy)
    if (value & 0x4000)
      return Effect::XfbCopy;
    MarkEfbCopy(value);
    break;
  default:
    break;
  }
  return Effect::None;
}

void ParseDisplayList(u32 address, u32 size, int depth);

// Debug: SKIES_GX_TRACE=N logs frame N's register loads and draws (top level and display lists).
u64 TraceFrame()
{
  static const u64 frame = [] {
    const char* v = std::getenv("SKIES_GX_TRACE");
    return v ? std::strtoull(v, nullptr, 0) : ~0ull;
  }();
  return frame;
}

void Trace(const u8* p, u32 len, int depth)
{
  if (s_frames != TraceFrame())
    return;
  if (p[0] == 0x61)
    Log("[gx] %d BP %02x %06x\n", depth, p[1], (u32(p[2]) << 16) | (u32(p[3]) << 8) | p[4]);
  else if (p[0] == 0x08)
    Log("[gx] %d CP %02x %08x\n", depth, p[1], BE32(p + 2));
  else if (p[0] == 0x10)
  {
    const u32 head = BE32(p + 1);
    std::string values;
    for (u32 i = 5; i + 4 <= len; i += 4)
    {
      char v[12];
      std::snprintf(v, sizeof(v), " %08x", BE32(p + i));
      values += v;
    }
    Log("[gx] %d XF %04x n%u%s\n", depth, head & 0xFFFF, ((head >> 16) & 0xF) + 1, values.c_str());
  }
  else if (p[0] & 0x80)
    Log("[gx] %d DRAW %02x n%u\n", depth, p[0], BE16(p + 1));
  else if (p[0] == 0x40)
    Log("[gx] %d CALL %08x %u\n", depth, BE32(p + 1), BE32(p + 5));
  else if (p[0] >= 0x20 && p[0] <= 0x38)
    Log("[gx] %d IDX %02x %08x\n", depth, p[0], BE32(p + 1));
}

// The side effects of one command; display lists are walked for theirs.
Effect Apply(const u8* p, u32 len, int depth)
{
  Trace(p, len, depth);
  DffCommand(p, len, depth == 0);
  switch (p[0])
  {
  case 0x08:
    LoadCpReg(s_vtx, p[1], BE32(p + 2));
    return Effect::None;
  case 0x61:
    return BpWrite(p[1], (u32(p[2]) << 16) | (u32(p[3]) << 8) | p[4]);
  case 0x40:
    ParseDisplayList(BE32(p + 1), BE32(p + 5), depth + 1);
    return Effect::None;
  default:
    return Effect::None;
  }
}

void ParseDisplayList(u32 address, u32 size, int depth)
{
  if (depth > 8)
    return;
  const u8* p = RamPtr(address, size);
  if (!p)
  {
    Log("[gpu] display list %08x (%u bytes) outside RAM\n", address, size);
    return;
  }
  u32 pos = 0;
  while (pos < size)
  {
    const u32 len = CommandLength(s_vtx, p + pos, size - pos);
    if (len == 0)
      break;
    Apply(p + pos, len, depth);
    pos += len;
  }
}

// FIFO bytes not yet forming a whole command
std::vector<u8> s_pending;

void ProcessCommands()
{
  u32 pos = 0, sent = 0;
  const u32 size = static_cast<u32>(s_pending.size());
  const u8* data = s_pending.data();
  while (pos < size)
  {
    const u32 len = CommandLength(s_vtx, data + pos, size - pos);
    if (len == 0)
      break;
    if (Apply(data + pos, len, 0) == Effect::XfbCopy)
    {
      // The picture is presented before the copy command goes in (Aurora applies the copy's
      // EFB clear), as the Dolphin-side backend does.
      if (pos > sent)
        VideoCommands(data + sent, pos - sent);
      sent = pos;
      ++s_frames;
      DffFrameEnd(s_frames);
      VideoFrameEnd();
      StateFrameEnd();
    }
    pos += len;
  }
  if (pos > sent)
    VideoCommands(data + sent, pos - sent);
  s_pending.erase(s_pending.begin(), s_pending.begin() + pos);
}

void RunGpu()
{
  bool consumed = false;
  while ((s_cp_ctrl & CTRL_READ_ENABLE) && s_fifo.distance > 0)
  {
    if ((s_cp_ctrl & CTRL_BP_ENABLE) && s_fifo.rp == s_fifo.bp)
      break;
    const u8* src = RamPtr(s_fifo.rp, 32);
    if (!src)
    {
      Log("[gpu] FIFO read pointer %08x outside RAM\n", s_fifo.rp);
      break;
    }
    s_pending.insert(s_pending.end(), src, src + 32);
    s_fifo.rp = s_fifo.rp == s_fifo.end ? s_fifo.base : s_fifo.rp + 32;
    s_fifo.distance -= 32;
    consumed = true;
  }
  if (consumed)
    ProcessCommands();
}

void UpdateCpInterrupt()
{
  const bool hi = s_fifo.distance > s_fifo.hi_wm;
  const bool lo = s_fifo.distance < s_fifo.lo_wm;
  const bool bp = (s_cp_ctrl & CTRL_BP_ENABLE) && s_fifo.rp == s_fifo.bp;
  const bool interrupt = ((bp && (s_cp_ctrl & CTRL_BP_INT)) || (hi && (s_cp_ctrl & CTRL_OVF_INT)) ||
                          (lo && (s_cp_ctrl & CTRL_UNDF_INT))) &&
                         (s_cp_ctrl & CTRL_READ_ENABLE);
  if (interrupt != s_cp_int)
  {
    s_cp_int = interrupt;
    PiSetInterrupt(PI_INT_CP, interrupt);
  }
}

void GatherPipeBursted()
{
  if (!(s_cp_ctrl & CTRL_LINK))
  {
    // Not linked (a display list being recorded): the bytes only go to RAM.
    RunGpu();
    return;
  }
  s_fifo.wp = s_fifo.wp == s_fifo.end ? s_fifo.base : s_fifo.wp + 32;
  if (s_cp_ctrl & CTRL_READ_ENABLE)
  {
    g_pi_fifo_wptr = s_fifo.wp;
    g_pi_fifo_base = s_fifo.base;
    g_pi_fifo_end = s_fifo.end;
  }
  s_fifo.distance += 32;
  RunGpu();
  UpdateCpInterrupt();
}

u16* FifoHalf(u32 offset, u16& mask)
{
  // CP registers 0x20..0x3E: 32-bit FIFO values as lo/hi 16-bit halves
  u32* field = nullptr;
  switch (offset & ~3u)
  {
  case 0x20:
    field = &s_fifo.base;
    break;
  case 0x24:
    field = &s_fifo.end;
    break;
  case 0x28:
    field = &s_fifo.hi_wm;
    break;
  case 0x2C:
    field = &s_fifo.lo_wm;
    break;
  case 0x30:
    field = &s_fifo.distance;
    break;
  case 0x34:
    field = &s_fifo.wp;
    break;
  case 0x38:
    field = &s_fifo.rp;
    break;
  case 0x3C:
    field = &s_fifo.bp;
    break;
  default:
    return nullptr;
  }
  const bool high = offset & 2;
  mask = high ? 0x1FFF : 0xFFE0;
  return reinterpret_cast<u16*>(field) + (high ? 1 : 0);  // little-endian host
}
}  // namespace

void GpuInit()
{
  s_fifo = {};
  s_cp_ctrl = 0;
  s_cp_int = false;
  std::memset(s_pe_conf, 0, sizeof(s_pe_conf));
  s_pe_ctrl = 0;
  s_pe_token = 0;
  s_pe_token_int = s_pe_finish_int = false;
  s_vtx = {};
  s_pending.clear();
  s_gp_ptr = s_gp;
}

// The gather pipe's queued bytes, the CP FIFO, the PE and the parser's own registers; what Aurora
// keeps of the GP is dff.cpp's.
void GpuDoState(StateIO& p)
{
  u32 queued = static_cast<u32>(s_gp_ptr - s_gp);
  p.Do(queued);
  if (p.Reading() && queued > sizeof(s_gp))
  {
    p.Fail("the gather pipe holds too much");
    queued = 0;
  }
  p.DoArray(s_gp, sizeof(s_gp));
  s_gp_ptr = s_gp + queued;
  p.Do(s_fifo);
  p.Do(s_cp_ctrl);
  p.Do(s_cp_perf_select);
  p.Do(s_cp_unk_0a);
  p.Do(s_cp_int);
  p.DoArray(s_pe_conf, 5);
  p.Do(s_pe_ctrl);
  p.Do(s_pe_token);
  p.Do(s_pe_token_int);
  p.Do(s_pe_finish_int);
  p.Do(s_draw_dones);
  p.Do(s_tokens);
  p.Do(s_frames);
  p.Do(s_vtx);
  p.Do(s_copy_size);
  p.Do(s_copy_dest);
  p.Do(s_copy_stride);
  p.Do(s_pending);
}

void GatherPipeInstall(CPUState* cpu)
{
  s_gp_ptr = s_gp;
  cpu->gather_pipe_ptr_ref = &s_gp_ptr;
  cpu->gather_pipe_base = s_gp;
  cpu->gather_pipe_flush = GatherPipeFlush;
}

void GatherPipeFlush(CPUState*)
{
  u32 count = static_cast<u32>(s_gp_ptr - s_gp);
  u32 processed = 0;
  while (count - processed >= GP_SIZE)
  {
    u8* dst = RamPtr(g_pi_fifo_wptr, GP_SIZE);
    if (dst)
      std::memcpy(dst, s_gp + processed, GP_SIZE);
    else
      Log("[gpu] gather pipe burst to %08x outside RAM\n", g_pi_fifo_wptr);
    g_pi_fifo_wptr = g_pi_fifo_wptr == g_pi_fifo_end ? g_pi_fifo_base : g_pi_fifo_wptr + GP_SIZE;
    processed += GP_SIZE;
    GatherPipeBursted();
  }
  std::memmove(s_gp, s_gp + processed, count - processed);
  s_gp_ptr = s_gp + (count - processed);
}

u16 CpRead16(u32 offset)
{
  switch (offset)
  {
  case 0x00:
  {
    // status: hi/lo watermark, read idle, command idle, breakpoint
    u16 status = 0;
    if (s_fifo.distance > s_fifo.hi_wm)
      status |= 1;
    if (s_fifo.distance < s_fifo.lo_wm)
      status |= 2;
    if (s_fifo.distance == 0 || !(s_cp_ctrl & CTRL_READ_ENABLE))
      status |= 4 | 8;
    if ((s_cp_ctrl & CTRL_BP_ENABLE) && s_fifo.rp == s_fifo.bp)
      status |= 0x10;
    return status;
  }
  case 0x02:
    return s_cp_ctrl;
  case 0x04:
    return 0;
  case 0x06:
    return s_cp_perf_select;
  case 0x0A:
    return s_cp_unk_0a;
  case 0x64:
    return 4;  // clocks per vertex out
  default:
  {
    u16 mask;
    if (u16* half = FifoHalf(offset, mask))
      return *half;
    return 0;  // metrics
  }
  }
}

void CpWrite16(u32 offset, u16 value)
{
  switch (offset)
  {
  case 0x02:
    s_cp_ctrl = value & 0x3F;
    RunGpu();
    UpdateCpInterrupt();
    return;
  case 0x04:
    // clear: watermark interrupts are recomputed from the distance
    UpdateCpInterrupt();
    return;
  case 0x06:
    s_cp_perf_select = value & 7;
    return;
  case 0x0A:
    s_cp_unk_0a = value & 0xFF;
    return;
  default:
  {
    u16 mask;
    if (u16* half = FifoHalf(offset, mask))
    {
      *half = value & mask;
      return;
    }
    Log("[cp] write %03x = %04x\n", offset, value);
    return;
  }
  }
}

u16 PeRead16(u32 offset)
{
  switch (offset)
  {
  case 0x00:
  case 0x02:
  case 0x04:
  case 0x06:
  case 0x08:
    return s_pe_conf[offset / 2];
  case 0x0A:
    return static_cast<u16>((s_pe_ctrl & 3) | (s_pe_token_int ? 4 : 0) | (s_pe_finish_int ? 8 : 0));
  case 0x0E:
    return s_pe_token;
  default:
    return 0;  // bounding box, performance counters
  }
}

void PeWrite16(u32 offset, u16 value)
{
  switch (offset)
  {
  case 0x00:
  case 0x02:
  case 0x04:
  case 0x06:
  case 0x08:
    s_pe_conf[offset / 2] = value;
    return;
  case 0x0A:
    if (value & 4)
      s_pe_token_int = false;
    if (value & 8)
      s_pe_finish_int = false;
    s_pe_ctrl = value & 3;
    PeUpdateInterrupts();
    return;
  default:
    return;
  }
}

u32 EfbRead(u32)
{
  // Skies never peeks or pokes the EFB: GXPeek*/GXPoke{ARGB,Z} aren't linked into the DOL and no
  // run has touched 0xC8000000. 0 is what the Dolphin build's Aurora backend (Null EFB) returns.
  return 0;
}

u64 GpuFrameCount()
{
  return s_frames;
}

void GpuStats(u64* frames, u64* draw_dones, u64* tokens)
{
  *frames = s_frames;
  *draw_dones = s_draw_dones;
  *tokens = s_tokens;
}
}  // namespace host
