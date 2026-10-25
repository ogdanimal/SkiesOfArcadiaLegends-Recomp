// SPDX-License-Identifier: GPL-3.0-or-later
//
// One-instruction Gekko interpreter, transliterated from DolRecomp's C emitter
// (DolRecomp/src/backend/emitter.c, emit_instruction_with_range and the helpers above it).
// Each case below executes what the emitted C does; `ctx` is the name the emitted code uses.
// Branches ignore the emitter's local-goto / cross-chunk machinery: they compute the same
// condition / CTR decrement / LR update and set ctx->pc.
#include "interp/interp.h"

#include <cstring>

// cpu.h first: its types.h and DolRecomp's common/types.h share one include guard, and the
// GXRuntime copy carries the helpers (convert_to_double ...) the runtime needs.
extern "C" {
#include "decoder.h"
}

namespace host::interp {
namespace {

// The static inline helpers emit_header_for_cpu() puts at the top of every generated file.
inline u32 dolrecomp_rotl32(u32 value, u32 sh)
{
  sh &= 31u;
  return sh ? ((value << sh) | (value >> (32u - sh))) : value;
}

inline f64 dolrecomp_f32_from_bits(u32 bits)
{
  u64 x = bits;
  u64 exp = (x >> 23) & 0xFFu;
  u64 frac = x & 0x007FFFFFu;
  u64 result;
  if (exp > 0 && exp < 255)
  {
    u64 y = !(exp >> 7);
    u64 z = (y << 61) | (y << 60) | (y << 59);
    result = ((x & 0xC0000000u) << 32) | z | ((x & 0x3FFFFFFFu) << 29);
  }
  else if (exp == 0 && frac != 0)
  {
    exp = 1023 - 126;
    do
    {
      frac <<= 1;
      exp -= 1;
    } while ((frac & 0x00800000u) == 0);
    result = ((x & 0x80000000u) << 32) | (exp << 52) | ((frac & 0x007FFFFFu) << 29);
  }
  else
  {
    u64 y = exp >> 7;
    u64 z = (y << 61) | (y << 60) | (y << 59);
    result = ((x & 0xC0000000u) << 32) | z | ((x & 0x3FFFFFFFu) << 29);
  }
  f64 value;
  memcpy(&value, &result, sizeof(value));
  return value;
}

inline u32 dolrecomp_f32_to_bits(f64 value)
{
  u64 bits;
  memcpy(&bits, &value, sizeof(bits));
  u32 exp = static_cast<u32>((bits >> 52) & 0x7FFu);
  if (exp > 896 || (bits & 0x7FFFFFFFFFFFFFFFull) == 0)
  {
    return static_cast<u32>(((bits >> 32) & 0xC0000000u) | ((bits >> 29) & 0x3FFFFFFFu));
  }
  if (exp >= 874)
  {
    u32 result = static_cast<u32>(0x80000000u | ((bits & 0x000FFFFFFFFFFFFFull) >> 21));
    result >>= 905 - exp;
    result |= static_cast<u32>((bits >> 32) & 0x80000000u);
    return result;
  }
  return static_cast<u32>(((bits >> 32) & 0xC0000000u) | ((bits >> 29) & 0x3FFFFFFFu));
}

inline f64 dolrecomp_f64_from_bits(u64 bits)
{
  f64 value;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

inline u64 dolrecomp_f64_to_bits(f64 value)
{
  u64 bits;
  memcpy(&bits, &value, sizeof(bits));
  return bits;
}

inline f64 dolrecomp_ps_from_bits(u32 bits)
{
  return dolrecomp_f32_from_bits(bits);
}

inline u32 dolrecomp_ps_to_bits(f64 value)
{
  return dolrecomp_f32_to_bits(value);
}

// emitter.c: cr_field_shift / ppc_mask32
inline u32 cr_field_shift(u8 crf)
{
  return 4u * (7u - static_cast<u32>(crf));
}

inline u32 ppc_mask32(u8 mb, u8 me)
{
  u32 mask = 0;
  u8 bit = mb;
  for (;;)
  {
    mask |= 0x80000000u >> bit;
    if (bit == me)
      break;
    bit = static_cast<u8>((bit + 1) & 31);
  }
  return mask;
}

// emit_set_cr0_from_gpr
inline void set_cr0_from_gpr(CPUState* ctx, u8 reg)
{
  u32 cr_bits = 0;
  s32 cr_value = static_cast<s32>(ctx->gpr[reg]);
  if (cr_value < 0)
    cr_bits |= 0x8u;
  if (cr_value > 0)
    cr_bits |= 0x4u;
  if (cr_value == 0)
    cr_bits |= 0x2u;
  cr_bits |= (ctx->xer >> 31) & 1u;
  ctx->cr = (ctx->cr & 0x0FFFFFFFu) | (cr_bits << 28);
}

// emit_set_cr1_from_fpscr
inline void set_cr1_from_fpscr(CPUState* ctx)
{
  ctx->cr = (ctx->cr & 0xF0FFFFFFu) | ((ctx->fpscr >> 4) & 0x0F000000u);
}

// emit_compare_s32 / emit_compare_u32
inline void compare_s32(CPUState* ctx, u8 crf, s32 val_a, s32 val_b)
{
  u32 shift = cr_field_shift(crf);
  u32 cr_bits = 0;
  if (val_a < val_b)
    cr_bits |= 0x8u;
  if (val_a > val_b)
    cr_bits |= 0x4u;
  if (val_a == val_b)
    cr_bits |= 0x2u;
  cr_bits |= (ctx->xer >> 31) & 1u;
  ctx->cr = (ctx->cr & ~(0xFu << shift)) | (cr_bits << shift);
}

inline void compare_u32(CPUState* ctx, u8 crf, u32 val_a, u32 val_b)
{
  u32 shift = cr_field_shift(crf);
  u32 cr_bits = 0;
  if (val_a < val_b)
    cr_bits |= 0x8u;
  if (val_a > val_b)
    cr_bits |= 0x4u;
  if (val_a == val_b)
    cr_bits |= 0x2u;
  cr_bits |= (ctx->xer >> 31) & 1u;
  ctx->cr = (ctx->cr & ~(0xFu << shift)) | (cr_bits << shift);
}

// emit_dform_ea / emit_xform_ea
inline u32 dform_ea(const CPUState* ctx, u8 ra, s16 simm, bool update)
{
  if (ra == 0 && !update)
    return static_cast<u32>(static_cast<s32>(simm));
  return ctx->gpr[ra] + static_cast<u32>(static_cast<s32>(simm));
}

inline u32 xform_ea(const CPUState* ctx, u8 ra, u8 rb, bool update)
{
  if (ra == 0 && !update)
    return ctx->gpr[rb];
  return ctx->gpr[ra] + ctx->gpr[rb];
}

// emit_branch_condition
struct BranchCond
{
  bool ctr_ok;
  bool cr_ok;
};

inline BranchCond branch_condition(CPUState* ctx, u8 bo, u8 bi)
{
  BranchCond c;
  bool ctr_ignored = (bo & 0x04) != 0;
  bool cond_ignored = (bo & 0x10) != 0;
  if (!ctr_ignored)
  {
    ctx->ctr--;
    c.ctr_ok = (((ctx->ctr != 0) ? 1u : 0u) ^ ((bo >> 1) & 1u)) != 0;
  }
  else
  {
    c.ctr_ok = true;
  }
  if (!cond_ignored)
  {
    u32 mask = 0x80000000u >> bi;
    c.cr_ok = (((ctx->cr & mask) != 0) == (((bo >> 3) & 1u) != 0));
  }
  else
  {
    c.cr_ok = true;
  }
  return c;
}

// emit_cr_logical: the emitted `value = (expr) & 1u` is evaluated on 32-bit a/b.
inline void cr_logical(CPUState* ctx, const PPCInst* inst, u32 (*expr)(u32 a, u32 b))
{
  u32 a = (ctx->cr >> (31u - inst->rA)) & 1u;
  u32 b = (ctx->cr >> (31u - inst->rB)) & 1u;
  u32 mask = 0x80000000u >> inst->rD;
  u32 value = expr(a, b) & 1u;
  ctx->cr = (ctx->cr & ~mask) | (value ? mask : 0u);
}

// emit_load / emit_loadx (read expression applied by the caller through a tag)
enum class Rd
{
  W,
  B,
  H,
  HA,
  WBR,
  HBR
};

inline u32 read_value(CPUState* ctx, Rd kind, u32 ea)
{
  switch (kind)
  {
  case Rd::W:
    return mem_read32(ctx, ea);
  case Rd::B:
    return mem_read8(ctx, ea);
  case Rd::H:
    return mem_read16(ctx, ea);
  case Rd::HA:
    return static_cast<u32>(static_cast<s32>(static_cast<s16>(mem_read16(ctx, ea))));
  case Rd::WBR:
    return bswap32(mem_read32(ctx, ea));
  case Rd::HBR:
    return bswap16(mem_read16(ctx, ea));
  }
  return 0;
}

inline void do_load(CPUState* ctx, const PPCInst* inst, Rd kind, bool update)
{
  u32 ea = dform_ea(ctx, inst->rA, inst->simm, update);
  ctx->gpr[inst->rD] = read_value(ctx, kind, ea);
  if (update)
    ctx->gpr[inst->rA] = ea;
}

inline void do_loadx(CPUState* ctx, const PPCInst* inst, Rd kind, bool update)
{
  u32 ea = xform_ea(ctx, inst->rA, inst->rB, update);
  ctx->gpr[inst->rD] = read_value(ctx, kind, ea);
  if (update)
    ctx->gpr[inst->rA] = ea;
}

enum class Wr
{
  W,
  B,
  H
};

inline void write_value(CPUState* ctx, Wr kind, u32 ea, u32 v)
{
  switch (kind)
  {
  case Wr::W:
    mem_write32(ctx, ea, static_cast<u32>(v));
    break;
  case Wr::B:
    mem_write8(ctx, ea, static_cast<u8>(v));
    break;
  case Wr::H:
    mem_write16(ctx, ea, static_cast<u16>(v));
    break;
  }
}

inline void do_store(CPUState* ctx, const PPCInst* inst, Wr kind, bool update)
{
  u32 ea = dform_ea(ctx, inst->rA, inst->simm, update);
  write_value(ctx, kind, ea, ctx->gpr[inst->rS]);
  if (update)
    ctx->gpr[inst->rA] = ea;
}

inline void do_storex(CPUState* ctx, const PPCInst* inst, Wr kind, bool update)
{
  u32 ea = xform_ea(ctx, inst->rA, inst->rB, update);
  write_value(ctx, kind, ea, ctx->gpr[inst->rS]);
  if (update)
    ctx->gpr[inst->rA] = ea;
}

// emit_fload / emit_floadx / emit_fstore / emit_fstorex
inline void do_fload_ea(CPUState* ctx, const PPCInst* inst, u32 ea, bool single, bool update)
{
  if (single)
  {
    f64 value = dolrecomp_f32_from_bits(mem_read32(ctx, ea));
    ctx->fpr[inst->rD] = value;
    ctx->ps1[inst->rD] = value;
  }
  else
  {
    ctx->fpr[inst->rD] = dolrecomp_f64_from_bits(mem_read64(ctx, ea));
  }
  if (update)
    ctx->gpr[inst->rA] = ea;
}

inline void do_fstore_ea(CPUState* ctx, const PPCInst* inst, u32 ea, bool single, bool update)
{
  if (single)
    mem_write32(ctx, ea, dolrecomp_f32_to_bits(ctx->fpr[inst->rS]));
  else
    mem_write64(ctx, ea, dolrecomp_f64_to_bits(ctx->fpr[inst->rS]));
  if (update)
    ctx->gpr[inst->rA] = ea;
}

// emit_psq_load / emit_psq_store. Returns false when the emitted `if (ctx->exception) return;`
// would return before the update-form write-back.
inline void do_psq_load(CPUState* ctx, const PPCInst* inst, bool indexed, bool update)
{
  u32 ea = indexed ? xform_ea(ctx, inst->rA, inst->rB, update) :
                     dform_ea(ctx, inst->rA, inst->simm, update);
  ppc_psq_load_inline(ctx, inst->rD, ea, inst->w != 0, inst->i, indexed, inst->address);
  if (ctx->exception)
    return;
  if (update)
    ctx->gpr[inst->rA] = ea;
}

inline void do_psq_store(CPUState* ctx, const PPCInst* inst, bool indexed, bool update)
{
  u32 ea = indexed ? xform_ea(ctx, inst->rA, inst->rB, update) :
                     dform_ea(ctx, inst->rA, inst->simm, update);
  ppc_psq_store_inline(ctx, inst->rS, ea, inst->w != 0, inst->i, indexed, inst->address);
  if (ctx->exception)
    return;
  if (update)
    ctx->gpr[inst->rA] = ea;
}

// emit_ps_merge
inline void ps_merge(CPUState* ctx, const PPCInst* inst, bool use_a_ps1, bool use_b_ps1)
{
  f64 ps0 = use_a_ps1 ? ctx->ps1[inst->rA] : ctx->fpr[inst->rA];
  f64 ps1 = use_b_ps1 ? ctx->ps1[inst->rB] : ctx->fpr[inst->rB];
  ctx->fpr[inst->rD] = ps0;
  ctx->ps1[inst->rD] = ps1;
}

// Arithmetic with the emitter's carry/overflow conventions.
inline void record(CPUState* ctx, const PPCInst* inst, u8 reg)
{
  if (inst->rc)
    set_cr0_from_gpr(ctx, reg);
}

inline void set_ca(CPUState* ctx, bool ca)
{
  ctx->xer = (ctx->xer & ~0x20000000u) | (ca ? 0x20000000u : 0u);
}

}  // namespace

bool ExecuteOne(CPUState* cpu, u32 raw, u32 cia)
{
  CPUState* ctx = cpu;
  PPCInst decoded = ppc_decode(raw, cia);
  const PPCInst* inst = &decoded;

  if (inst->embedded_data)
  {
    ppc_program_exception(ctx, PPC_PROGRAM_ILLEGAL, cia);
    return true;
  }
  if (inst->op == PPC_OP_UNKNOWN)
    return false;

  // Fall-through PC. Anything that raises an exception, or branches, overwrites it.
  ctx->pc = cia + 4u;

  if (ppc_op_uses_fpu(inst->op))
  {
    if (!ppc_fp_available_inline(ctx, inst->address))
      return true;
  }

  switch (inst->op)
  {
  case PPC_OP_MULLI:
    ctx->gpr[inst->rD] = static_cast<u32>(static_cast<s64>(static_cast<s32>(ctx->gpr[inst->rA])) *
                                          static_cast<s64>(static_cast<s32>(inst->simm)));
    break;

  case PPC_OP_SUBFIC:
  {
    u64 res = static_cast<u64>(static_cast<u32>(static_cast<s32>(inst->simm))) +
              static_cast<u64>(~ctx->gpr[inst->rA]) + 1u;
    ctx->gpr[inst->rD] = static_cast<u32>(res);
    ctx->xer = (ctx->xer & ~0x20000000u) | ((static_cast<u32>(res >> 32) & 1u) << 29);
    break;
  }

  case PPC_OP_ADDI:
    if (inst->rA == 0)
      ctx->gpr[inst->rD] = static_cast<u32>(static_cast<s32>(inst->simm));
    else
      ctx->gpr[inst->rD] = ctx->gpr[inst->rA] + static_cast<u32>(static_cast<s32>(inst->simm));
    break;

  case PPC_OP_ADDIC:
  case PPC_OP_ADDIC_DOT:
  {
    u64 a = ctx->gpr[inst->rA];
    u64 b = static_cast<u32>(static_cast<s32>(inst->simm));
    u64 res = a + b;
    ctx->gpr[inst->rD] = static_cast<u32>(res);
    ctx->xer = (ctx->xer & ~0x20000000u) | ((static_cast<u32>(res >> 32) & 1u) << 29);
    if (inst->op == PPC_OP_ADDIC_DOT)
      set_cr0_from_gpr(ctx, inst->rD);
    break;
  }

  case PPC_OP_ADDIS:
    if (inst->rA == 0)
      ctx->gpr[inst->rD] = (static_cast<u32>(static_cast<s32>(inst->simm)) << 16);
    else
      ctx->gpr[inst->rD] =
          ctx->gpr[inst->rA] + (static_cast<u32>(static_cast<s32>(inst->simm)) << 16);
    break;

  case PPC_OP_CMPI:
    compare_s32(ctx, inst->crfD, static_cast<s32>(ctx->gpr[inst->rA]),
                static_cast<s32>(inst->simm));
    break;

  case PPC_OP_CMPLI:
    compare_u32(ctx, inst->crfD, ctx->gpr[inst->rA], static_cast<u32>(inst->uimm));
    break;

  case PPC_OP_CMP:
    compare_s32(ctx, inst->crfD, static_cast<s32>(ctx->gpr[inst->rA]),
                static_cast<s32>(ctx->gpr[inst->rB]));
    break;

  case PPC_OP_CMPL:
    compare_u32(ctx, inst->crfD, ctx->gpr[inst->rA], ctx->gpr[inst->rB]);
    break;

  case PPC_OP_ORI:
    if (inst->rS == 0 && inst->rA == 0 && inst->uimm == 0)
    {
      // nop
    }
    else
    {
      ctx->gpr[inst->rA] = ctx->gpr[inst->rS] | inst->uimm;
    }
    break;

  case PPC_OP_ORIS:
    ctx->gpr[inst->rA] = ctx->gpr[inst->rS] | (static_cast<u32>(inst->uimm) << 16);
    break;

  case PPC_OP_XORI:
    ctx->gpr[inst->rA] = ctx->gpr[inst->rS] ^ inst->uimm;
    break;

  case PPC_OP_XORIS:
    ctx->gpr[inst->rA] = ctx->gpr[inst->rS] ^ (static_cast<u32>(inst->uimm) << 16);
    break;

  case PPC_OP_ANDI:
    ctx->gpr[inst->rA] = ctx->gpr[inst->rS] & inst->uimm;
    set_cr0_from_gpr(ctx, inst->rA);
    break;

  case PPC_OP_ANDIS:
    ctx->gpr[inst->rA] = ctx->gpr[inst->rS] & (static_cast<u32>(inst->uimm) << 16);
    set_cr0_from_gpr(ctx, inst->rA);
    break;

  case PPC_OP_ADD:
  case PPC_OP_ADDO:
  {
    u32 a = ctx->gpr[inst->rA];
    u32 b = ctx->gpr[inst->rB];
    u32 res = a + b;
    ctx->gpr[inst->rD] = res;
    if (inst->oe)
      ppc_set_xer_ov(ctx, ppc_add_overflowed(a, b, res));
    record(ctx, inst, inst->rD);
    break;
  }

  case PPC_OP_ADDC:
  case PPC_OP_ADDCO:
  {
    u32 a = ctx->gpr[inst->rA];
    u32 b = ctx->gpr[inst->rB];
    u64 wide = static_cast<u64>(a) + static_cast<u64>(b);
    u32 res = static_cast<u32>(wide);
    ctx->gpr[inst->rD] = res;
    ctx->xer = (ctx->xer & ~0x20000000u) | ((static_cast<u32>(wide >> 32) & 1u) << 29);
    if (inst->oe)
      ppc_set_xer_ov(ctx, ppc_add_overflowed(a, b, res));
    record(ctx, inst, inst->rD);
    break;
  }

  case PPC_OP_ADDE:
  case PPC_OP_ADDEO:
  {
    u32 carry = (ctx->xer >> 29) & 1u;
    u32 a = ctx->gpr[inst->rA];
    u32 b = ctx->gpr[inst->rB];
    u64 wide = static_cast<u64>(a) + static_cast<u64>(b) + carry;
    u32 res = static_cast<u32>(wide);
    ctx->gpr[inst->rD] = res;
    ctx->xer = (ctx->xer & ~0x20000000u) | ((static_cast<u32>(wide >> 32) & 1u) << 29);
    if (inst->oe)
      ppc_set_xer_ov(ctx, ppc_add_overflowed(a, b, res));
    record(ctx, inst, inst->rD);
    break;
  }

  case PPC_OP_ADDME:
  case PPC_OP_ADDMEO:
  {
    u32 input = ctx->gpr[inst->rA];
    u32 carry = (ctx->xer >> 29) & 1u;
    u64 res = static_cast<u64>(input) + 0xFFFFFFFFull + carry;
    ctx->gpr[inst->rD] = static_cast<u32>(res);
    ctx->xer = (ctx->xer & ~0x20000000u) | ((res >> 32) ? 0x20000000u : 0u);
    if (inst->oe)
      ppc_set_xer_ov(ctx, ppc_add_overflowed(input, 0xFFFFFFFFu, static_cast<u32>(res)));
    record(ctx, inst, inst->rD);
    break;
  }

  case PPC_OP_ADDZE:
  case PPC_OP_ADDZEO:
  {
    u32 a = ctx->gpr[inst->rA];
    u64 wide = static_cast<u64>(a) + ((ctx->xer >> 29) & 1u);
    u32 res = static_cast<u32>(wide);
    ctx->gpr[inst->rD] = res;
    ctx->xer = (ctx->xer & ~0x20000000u) | ((static_cast<u32>(wide >> 32) & 1u) << 29);
    if (inst->oe)
      ppc_set_xer_ov(ctx, ppc_add_overflowed(a, 0u, res));
    record(ctx, inst, inst->rD);
    break;
  }

  case PPC_OP_SUBF:
  case PPC_OP_SUBFO:
  {
    u32 a = ~ctx->gpr[inst->rA];
    u32 b = ctx->gpr[inst->rB];
    u32 res = a + b + 1u;
    ctx->gpr[inst->rD] = res;
    if (inst->oe)
      ppc_set_xer_ov(ctx, ppc_add_overflowed(a, b, res));
    record(ctx, inst, inst->rD);
    break;
  }

  case PPC_OP_SUBFC:
  case PPC_OP_SUBFCO:
  {
    u32 a = ~ctx->gpr[inst->rA];
    u32 b = ctx->gpr[inst->rB];
    u64 wide = static_cast<u64>(b) + static_cast<u64>(a) + 1u;
    u32 res = static_cast<u32>(wide);
    ctx->gpr[inst->rD] = res;
    ctx->xer = (ctx->xer & ~0x20000000u) | ((static_cast<u32>(wide >> 32) & 1u) << 29);
    if (inst->oe)
      ppc_set_xer_ov(ctx, ppc_add_overflowed(a, b, res));
    record(ctx, inst, inst->rD);
    break;
  }

  case PPC_OP_SUBFE:
  case PPC_OP_SUBFEO:
  {
    u32 a = ~ctx->gpr[inst->rA];
    u32 b = ctx->gpr[inst->rB];
    u32 carry = (ctx->xer >> 29) & 1u;
    u64 wide = static_cast<u64>(a) + static_cast<u64>(b) + carry;
    u32 res = static_cast<u32>(wide);
    ctx->gpr[inst->rD] = res;
    ctx->xer = (ctx->xer & ~0x20000000u) | ((static_cast<u32>(wide >> 32) & 1u) << 29);
    if (inst->oe)
      ppc_set_xer_ov(ctx, ppc_add_overflowed(a, b, res));
    record(ctx, inst, inst->rD);
    break;
  }

  case PPC_OP_SUBFME:
  case PPC_OP_SUBFMEO:
  {
    u32 input = ~ctx->gpr[inst->rA];
    u32 carry = (ctx->xer >> 29) & 1u;
    u64 res = static_cast<u64>(input) + 0xFFFFFFFFull + carry;
    ctx->gpr[inst->rD] = static_cast<u32>(res);
    ctx->xer = (ctx->xer & ~0x20000000u) | ((res >> 32) ? 0x20000000u : 0u);
    if (inst->oe)
      ppc_set_xer_ov(ctx, ppc_add_overflowed(input, 0xFFFFFFFFu, static_cast<u32>(res)));
    record(ctx, inst, inst->rD);
    break;
  }

  case PPC_OP_SUBFZE:
  case PPC_OP_SUBFZEO:
  {
    u32 a = ~ctx->gpr[inst->rA];
    u64 wide = static_cast<u64>(a) + ((ctx->xer >> 29) & 1u);
    u32 res = static_cast<u32>(wide);
    ctx->gpr[inst->rD] = res;
    ctx->xer = (ctx->xer & ~0x20000000u) | ((static_cast<u32>(wide >> 32) & 1u) << 29);
    if (inst->oe)
      ppc_set_xer_ov(ctx, ppc_add_overflowed(a, 0u, res));
    record(ctx, inst, inst->rD);
    break;
  }

  case PPC_OP_NEG:
  case PPC_OP_NEGO:
  {
    u32 a = ctx->gpr[inst->rA];
    ctx->gpr[inst->rD] = (~a) + 1u;
    if (inst->oe)
      ppc_set_xer_ov(ctx, a == 0x80000000u);
    record(ctx, inst, inst->rD);
    break;
  }

  case PPC_OP_MULLW:
  case PPC_OP_MULLWO:
  {
    s64 product = static_cast<s64>(static_cast<s32>(ctx->gpr[inst->rA])) *
                  static_cast<s64>(static_cast<s32>(ctx->gpr[inst->rB]));
    ctx->gpr[inst->rD] = static_cast<u32>(product);
    if (inst->oe)
      ppc_set_xer_ov(ctx, product < -0x80000000ll || product > 0x7fffffffll);
    record(ctx, inst, inst->rD);
    break;
  }

  case PPC_OP_MULHW:
  {
    s64 product = static_cast<s64>(static_cast<s32>(ctx->gpr[inst->rA])) *
                  static_cast<s64>(static_cast<s32>(ctx->gpr[inst->rB]));
    ctx->gpr[inst->rD] = static_cast<u32>(product >> 32);
    record(ctx, inst, inst->rD);
    break;
  }

  case PPC_OP_MULHWU:
  {
    u64 product = static_cast<u64>(ctx->gpr[inst->rA]) * static_cast<u64>(ctx->gpr[inst->rB]);
    ctx->gpr[inst->rD] = static_cast<u32>(product >> 32);
    record(ctx, inst, inst->rD);
    break;
  }

  case PPC_OP_DIVW:
  case PPC_OP_DIVWO:
  {
    s32 dividend = static_cast<s32>(ctx->gpr[inst->rA]);
    s32 divisor = static_cast<s32>(ctx->gpr[inst->rB]);
    bool ov = divisor == 0 || (static_cast<u32>(dividend) == 0x80000000u && divisor == -1);
    ctx->gpr[inst->rD] =
        ov ? ((dividend < 0) ? 0xFFFFFFFFu : 0u) : static_cast<u32>(dividend / divisor);
    if (inst->oe)
      ppc_set_xer_ov(ctx, ov);
    record(ctx, inst, inst->rD);
    break;
  }

  case PPC_OP_DIVWU:
  case PPC_OP_DIVWUO:
  {
    u32 divisor = ctx->gpr[inst->rB];
    ctx->gpr[inst->rD] = divisor == 0 ? 0u : ctx->gpr[inst->rA] / divisor;
    if (inst->oe)
      ppc_set_xer_ov(ctx, divisor == 0);
    record(ctx, inst, inst->rD);
    break;
  }

  case PPC_OP_AND:
    ctx->gpr[inst->rA] = ctx->gpr[inst->rS] & ctx->gpr[inst->rB];
    record(ctx, inst, inst->rA);
    break;
  case PPC_OP_ANDC:
    ctx->gpr[inst->rA] = ctx->gpr[inst->rS] & ~ctx->gpr[inst->rB];
    record(ctx, inst, inst->rA);
    break;
  case PPC_OP_OR:
    ctx->gpr[inst->rA] = ctx->gpr[inst->rS] | ctx->gpr[inst->rB];
    record(ctx, inst, inst->rA);
    break;
  case PPC_OP_ORC:
    ctx->gpr[inst->rA] = ctx->gpr[inst->rS] | ~ctx->gpr[inst->rB];
    record(ctx, inst, inst->rA);
    break;
  case PPC_OP_XOR:
    ctx->gpr[inst->rA] = ctx->gpr[inst->rS] ^ ctx->gpr[inst->rB];
    record(ctx, inst, inst->rA);
    break;
  case PPC_OP_NAND:
    ctx->gpr[inst->rA] = ~(ctx->gpr[inst->rS] & ctx->gpr[inst->rB]);
    record(ctx, inst, inst->rA);
    break;
  case PPC_OP_NOR:
    ctx->gpr[inst->rA] = ~(ctx->gpr[inst->rS] | ctx->gpr[inst->rB]);
    record(ctx, inst, inst->rA);
    break;
  case PPC_OP_EQV:
    ctx->gpr[inst->rA] = ~(ctx->gpr[inst->rS] ^ ctx->gpr[inst->rB]);
    record(ctx, inst, inst->rA);
    break;

  case PPC_OP_CNTLZW:
  {
    u32 v = ctx->gpr[inst->rS];
    u32 n = 0;
    while (n < 32 && ((v & (0x80000000u >> n)) == 0))
      n++;
    ctx->gpr[inst->rA] = n;
    record(ctx, inst, inst->rA);
    break;
  }

  case PPC_OP_EXTSB:
    ctx->gpr[inst->rA] = static_cast<u32>(static_cast<s32>(static_cast<s8>(ctx->gpr[inst->rS])));
    record(ctx, inst, inst->rA);
    break;

  case PPC_OP_EXTSH:
    ctx->gpr[inst->rA] = static_cast<u32>(static_cast<s32>(static_cast<s16>(ctx->gpr[inst->rS])));
    record(ctx, inst, inst->rA);
    break;

  case PPC_OP_SLW:
  {
    u32 sh = ctx->gpr[inst->rB] & 0x3Fu;
    ctx->gpr[inst->rA] = sh > 31 ? 0u : (ctx->gpr[inst->rS] << sh);
    record(ctx, inst, inst->rA);
    break;
  }

  case PPC_OP_SRW:
  {
    u32 sh = ctx->gpr[inst->rB] & 0x3Fu;
    ctx->gpr[inst->rA] = sh > 31 ? 0u : (ctx->gpr[inst->rS] >> sh);
    record(ctx, inst, inst->rA);
    break;
  }

  case PPC_OP_SRAW:
  case PPC_OP_SRAWI:
  {
    u32 sh;
    if (inst->op == PPC_OP_SRAWI)
      sh = inst->sh;
    else
      sh = ctx->gpr[inst->rB] & 0x3Fu;
    u32 value = ctx->gpr[inst->rS];
    bool ca = false;
    if (sh == 0)
    {
      ctx->gpr[inst->rA] = value;
    }
    else if (sh > 31)
    {
      ctx->gpr[inst->rA] = (value & 0x80000000u) ? 0xFFFFFFFFu : 0u;
      ca = (value & 0x80000000u) != 0;
    }
    else
    {
      ctx->gpr[inst->rA] = static_cast<u32>(static_cast<s32>(value) >> sh);
      ca = (value & 0x80000000u) && ((value << (32u - sh)) != 0);
    }
    ctx->xer = (ctx->xer & ~0x20000000u) | (ca ? 0x20000000u : 0u);
    record(ctx, inst, inst->rA);
    break;
  }

  case PPC_OP_RLWINM:
  {
    u32 mask = ppc_mask32(inst->mb, inst->me);
    ctx->gpr[inst->rA] = dolrecomp_rotl32(ctx->gpr[inst->rS], inst->sh) & mask;
    record(ctx, inst, inst->rA);
    break;
  }

  case PPC_OP_RLWNM:
  {
    u32 mask = ppc_mask32(inst->mb, inst->me);
    ctx->gpr[inst->rA] = dolrecomp_rotl32(ctx->gpr[inst->rS], ctx->gpr[inst->rB]) & mask;
    record(ctx, inst, inst->rA);
    break;
  }

  case PPC_OP_RLWIMI:
  {
    u32 mask = ppc_mask32(inst->mb, inst->me);
    u32 rot = dolrecomp_rotl32(ctx->gpr[inst->rS], inst->sh);
    ctx->gpr[inst->rA] = (ctx->gpr[inst->rA] & ~mask) | (rot & mask);
    record(ctx, inst, inst->rA);
    break;
  }

  case PPC_OP_FADDS:
    ppc_fadds(ctx, inst->rD, inst->rA, inst->rB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_FSUBS:
    ppc_fsubs(ctx, inst->rD, inst->rA, inst->rB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_FMULS:
    ppc_fmuls(ctx, inst->rD, inst->rA, inst->rC);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_FDIVS:
    ppc_fdivs(ctx, inst->rD, inst->rA, inst->rB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_FRES:
  {
    f64 result;
    if (ppc_fres(ctx, ctx->fpr[inst->rB], &result))
      ctx->fpr[inst->rD] = ctx->ps1[inst->rD] = result;
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;
  }

  case PPC_OP_FMADDS:
  case PPC_OP_FMSUBS:
  case PPC_OP_FNMADDS:
  case PPC_OP_FNMSUBS:
  {
    const bool sub = inst->op == PPC_OP_FMSUBS || inst->op == PPC_OP_FNMSUBS;
    const bool neg = inst->op == PPC_OP_FNMADDS || inst->op == PPC_OP_FNMSUBS;
    f64 result;
    if (ppc_fma(ctx, ctx->fpr[inst->rA], ctx->fpr[inst->rC], ctx->fpr[inst->rB], true, sub, neg,
                &result))
      ctx->fpr[inst->rD] = ctx->ps1[inst->rD] = result;
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;
  }

  case PPC_OP_FADD:
    ppc_fadd(ctx, inst->rD, inst->rA, inst->rB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_FSUB:
    ppc_fsub(ctx, inst->rD, inst->rA, inst->rB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_FMUL:
    ppc_fmul(ctx, inst->rD, inst->rA, inst->rC);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_FDIV:
    ppc_fdiv(ctx, inst->rD, inst->rA, inst->rB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_FRSQRTE:
  {
    f64 result;
    if (ppc_frsqrte(ctx, ctx->fpr[inst->rB], &result))
      ctx->fpr[inst->rD] = result;
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;
  }

  case PPC_OP_FMADD:
  case PPC_OP_FMSUB:
  case PPC_OP_FNMADD:
  case PPC_OP_FNMSUB:
  {
    const bool sub = inst->op == PPC_OP_FMSUB || inst->op == PPC_OP_FNMSUB;
    const bool neg = inst->op == PPC_OP_FNMADD || inst->op == PPC_OP_FNMSUB;
    f64 result;
    if (ppc_fma(ctx, ctx->fpr[inst->rA], ctx->fpr[inst->rC], ctx->fpr[inst->rB], false, sub, neg,
                &result))
      ctx->fpr[inst->rD] = result;
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;
  }

  case PPC_OP_FCTIW:
  case PPC_OP_FCTIWZ:
  {
    u64 result;
    if (ppc_fctiw(ctx, ctx->fpr[inst->rB], inst->op == PPC_OP_FCTIWZ, &result))
      ctx->fpr[inst->rD] = dolrecomp_f64_from_bits(result);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;
  }

  case PPC_OP_FMR:
    ctx->fpr[inst->rD] = ctx->fpr[inst->rB];
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_FNEG:
    ctx->fpr[inst->rD] =
        dolrecomp_f64_from_bits(dolrecomp_f64_to_bits(ctx->fpr[inst->rB]) ^ 0x8000000000000000ull);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_FABS:
    ctx->fpr[inst->rD] =
        dolrecomp_f64_from_bits(dolrecomp_f64_to_bits(ctx->fpr[inst->rB]) & 0x7FFFFFFFFFFFFFFFull);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_FNABS:
    ctx->fpr[inst->rD] =
        dolrecomp_f64_from_bits(dolrecomp_f64_to_bits(ctx->fpr[inst->rB]) | 0x8000000000000000ull);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_FRSP:
    ppc_frsp(ctx, inst->rD, inst->rB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_FSEL:
    ctx->fpr[inst->rD] = (ctx->fpr[inst->rA] >= 0.0) ? ctx->fpr[inst->rC] : ctx->fpr[inst->rB];
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_MTFSB0:
  case PPC_OP_MTFSB1:
  {
    u32 mask = 0x80000000u >> inst->rD;
    if (inst->op == PPC_OP_MTFSB0)
    {
      if (inst->rD != 1 && inst->rD != 2)
        ctx->fpscr &= ~mask;
    }
    else
    {
      if (inst->rD != 1 && inst->rD != 2)
        ctx->fpscr |= mask;
    }
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;
  }

  case PPC_OP_MFFS:
    ctx->fpr[inst->rD] = dolrecomp_f64_from_bits(0xFFF8000000000000ull | ctx->fpscr);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_MCRFS:
  {
    u32 shift = cr_field_shift(inst->crfS);
    u32 dst_shift = cr_field_shift(inst->crfD);
    u32 field = (ctx->fpscr >> shift) & 0xFu;
    ctx->fpscr &= ~((0xFu << shift) & 0x83F80700u);
    ppc_fpscr_updated(ctx);
    ctx->cr = (ctx->cr & ~(0xFu << dst_shift)) | (field << dst_shift);
    break;
  }

  case PPC_OP_MTFSFI:
  {
    u32 shift = cr_field_shift(inst->crfD);
    ctx->fpscr = (ctx->fpscr & ~(0xFu << shift)) | (static_cast<u32>(inst->imm) << shift);
    ppc_fpscr_updated(ctx);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;
  }

  case PPC_OP_MTFSF:
  {
    u32 mask = 0;
    for (u32 i = 0; i < 8; i++)
      if (static_cast<u32>(inst->fm) & (1u << i))
        mask |= 0xFu << (i * 4);
    u32 source = static_cast<u32>(dolrecomp_f64_to_bits(ctx->fpr[inst->rB]));
    ctx->fpscr = (ctx->fpscr & ~mask) | (source & mask);
    ppc_fpscr_updated(ctx);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;
  }

  case PPC_OP_PS_ADD:
    ppc_ps_add_op(ctx, inst->rD, inst->rA, inst->rB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_SUB:
    ppc_ps_sub_op(ctx, inst->rD, inst->rA, inst->rB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_MUL:
    ppc_ps_mul_op(ctx, inst->rD, inst->rA, inst->rC);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_DIV:
    ppc_ps_div_op(ctx, inst->rD, inst->rA, inst->rB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_RES:
    ppc_ps_res_op(ctx, inst->rD, inst->rB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_RSQRTE:
    ppc_ps_rsqrte_op(ctx, inst->rD, inst->rB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_MADD:
  case PPC_OP_PS_MSUB:
  case PPC_OP_PS_NMADD:
  case PPC_OP_PS_NMSUB:
    ppc_ps_madd_op(ctx, inst->rD, inst->rA, inst->rC, inst->rB,
                   inst->op == PPC_OP_PS_MSUB || inst->op == PPC_OP_PS_NMSUB,
                   inst->op == PPC_OP_PS_NMADD || inst->op == PPC_OP_PS_NMSUB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_NEG:
    ctx->fpr[inst->rD] =
        dolrecomp_ps_from_bits(dolrecomp_ps_to_bits(ctx->fpr[inst->rB]) ^ 0x80000000u);
    ctx->ps1[inst->rD] =
        dolrecomp_ps_from_bits(dolrecomp_ps_to_bits(ctx->ps1[inst->rB]) ^ 0x80000000u);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_ABS:
    ctx->fpr[inst->rD] =
        dolrecomp_ps_from_bits(dolrecomp_ps_to_bits(ctx->fpr[inst->rB]) & 0x7FFFFFFFu);
    ctx->ps1[inst->rD] =
        dolrecomp_ps_from_bits(dolrecomp_ps_to_bits(ctx->ps1[inst->rB]) & 0x7FFFFFFFu);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_NABS:
    ctx->fpr[inst->rD] =
        dolrecomp_ps_from_bits(dolrecomp_ps_to_bits(ctx->fpr[inst->rB]) | 0x80000000u);
    ctx->ps1[inst->rD] =
        dolrecomp_ps_from_bits(dolrecomp_ps_to_bits(ctx->ps1[inst->rB]) | 0x80000000u);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_MR:
    ctx->fpr[inst->rD] = ctx->fpr[inst->rB];
    ctx->ps1[inst->rD] = ctx->ps1[inst->rB];
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_SUM0:
    ppc_ps_sum0(ctx, inst->rD, inst->rA, inst->rC, inst->rB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_SUM1:
    ppc_ps_sum1(ctx, inst->rD, inst->rA, inst->rC, inst->rB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_MULS0:
    ppc_ps_muls0(ctx, inst->rD, inst->rA, inst->rC);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_MULS1:
    ppc_ps_muls1(ctx, inst->rD, inst->rA, inst->rC);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_MADDS0:
    ppc_ps_madds0(ctx, inst->rD, inst->rA, inst->rC, inst->rB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_MADDS1:
    ppc_ps_madds1(ctx, inst->rD, inst->rA, inst->rC, inst->rB);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_MERGE00:
    ps_merge(ctx, inst, false, false);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;
  case PPC_OP_PS_MERGE01:
    ps_merge(ctx, inst, false, true);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;
  case PPC_OP_PS_MERGE10:
    ps_merge(ctx, inst, true, false);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;
  case PPC_OP_PS_MERGE11:
    ps_merge(ctx, inst, true, true);
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_PS_CMPU0:
  case PPC_OP_PS_CMPO0:
  case PPC_OP_PS_CMPU1:
  case PPC_OP_PS_CMPO1:
  {
    bool lane1 = inst->op == PPC_OP_PS_CMPU1 || inst->op == PPC_OP_PS_CMPO1;
    bool ordered = inst->op == PPC_OP_PS_CMPO0 || inst->op == PPC_OP_PS_CMPO1;
    f64* bank = lane1 ? ctx->ps1 : ctx->fpr;
    ppc_fcmp(ctx, inst->crfD, bank[inst->rA], bank[inst->rB], ordered);
    break;
  }

  case PPC_OP_PS_SEL:
    ctx->fpr[inst->rD] = (static_cast<f32>(ctx->fpr[inst->rA]) >= 0.0f) ? ctx->fpr[inst->rC] :
                                                                         ctx->fpr[inst->rB];
    ctx->ps1[inst->rD] = (static_cast<f32>(ctx->ps1[inst->rA]) >= 0.0f) ? ctx->ps1[inst->rC] :
                                                                         ctx->ps1[inst->rB];
    if (inst->rc)
      set_cr1_from_fpscr(ctx);
    break;

  case PPC_OP_FCMPU:
  case PPC_OP_FCMPO:
    ppc_fcmp(ctx, inst->crfD, ctx->fpr[inst->rA], ctx->fpr[inst->rB], inst->op == PPC_OP_FCMPO);
    break;

  case PPC_OP_LWZ:
    do_load(ctx, inst, Rd::W, false);
    break;
  case PPC_OP_LWZU:
    do_load(ctx, inst, Rd::W, true);
    break;
  case PPC_OP_LBZ:
    do_load(ctx, inst, Rd::B, false);
    break;
  case PPC_OP_LBZU:
    do_load(ctx, inst, Rd::B, true);
    break;
  case PPC_OP_LHZ:
    do_load(ctx, inst, Rd::H, false);
    break;
  case PPC_OP_LHZU:
    do_load(ctx, inst, Rd::H, true);
    break;
  case PPC_OP_LHA:
    do_load(ctx, inst, Rd::HA, false);
    break;
  case PPC_OP_LHAU:
    do_load(ctx, inst, Rd::HA, true);
    break;

  case PPC_OP_LWZX:
    do_loadx(ctx, inst, Rd::W, false);
    break;
  case PPC_OP_LWZUX:
    do_loadx(ctx, inst, Rd::W, true);
    break;
  case PPC_OP_LBZX:
    do_loadx(ctx, inst, Rd::B, false);
    break;
  case PPC_OP_LBZUX:
    do_loadx(ctx, inst, Rd::B, true);
    break;
  case PPC_OP_LHZX:
    do_loadx(ctx, inst, Rd::H, false);
    break;
  case PPC_OP_LHZUX:
    do_loadx(ctx, inst, Rd::H, true);
    break;
  case PPC_OP_LHAX:
    do_loadx(ctx, inst, Rd::HA, false);
    break;
  case PPC_OP_LHAUX:
    do_loadx(ctx, inst, Rd::HA, true);
    break;
  case PPC_OP_LWBRX:
    do_loadx(ctx, inst, Rd::WBR, false);
    break;
  case PPC_OP_LHBRX:
    do_loadx(ctx, inst, Rd::HBR, false);
    break;

  case PPC_OP_LFS:
    do_fload_ea(ctx, inst, dform_ea(ctx, inst->rA, inst->simm, false), true, false);
    break;
  case PPC_OP_LFSU:
    do_fload_ea(ctx, inst, dform_ea(ctx, inst->rA, inst->simm, true), true, true);
    break;
  case PPC_OP_LFD:
    do_fload_ea(ctx, inst, dform_ea(ctx, inst->rA, inst->simm, false), false, false);
    break;
  case PPC_OP_LFDU:
    do_fload_ea(ctx, inst, dform_ea(ctx, inst->rA, inst->simm, true), false, true);
    break;

  case PPC_OP_LFSX:
    do_fload_ea(ctx, inst, xform_ea(ctx, inst->rA, inst->rB, false), true, false);
    break;
  case PPC_OP_LFSUX:
    do_fload_ea(ctx, inst, xform_ea(ctx, inst->rA, inst->rB, true), true, true);
    break;
  case PPC_OP_LFDX:
    do_fload_ea(ctx, inst, xform_ea(ctx, inst->rA, inst->rB, false), false, false);
    break;
  case PPC_OP_LFDUX:
    do_fload_ea(ctx, inst, xform_ea(ctx, inst->rA, inst->rB, true), false, true);
    break;

  case PPC_OP_PSQ_L:
    do_psq_load(ctx, inst, false, false);
    break;
  case PPC_OP_PSQ_LU:
    do_psq_load(ctx, inst, false, true);
    break;
  case PPC_OP_PSQ_LX:
    do_psq_load(ctx, inst, true, false);
    break;
  case PPC_OP_PSQ_LUX:
    do_psq_load(ctx, inst, true, true);
    break;

  case PPC_OP_STW:
    do_store(ctx, inst, Wr::W, false);
    break;
  case PPC_OP_STWU:
    do_store(ctx, inst, Wr::W, true);
    break;
  case PPC_OP_STB:
    do_store(ctx, inst, Wr::B, false);
    break;
  case PPC_OP_STBU:
    do_store(ctx, inst, Wr::B, true);
    break;
  case PPC_OP_STH:
    do_store(ctx, inst, Wr::H, false);
    break;
  case PPC_OP_STHU:
    do_store(ctx, inst, Wr::H, true);
    break;

  case PPC_OP_STWX:
    do_storex(ctx, inst, Wr::W, false);
    break;
  case PPC_OP_STWUX:
    do_storex(ctx, inst, Wr::W, true);
    break;
  case PPC_OP_STBX:
    do_storex(ctx, inst, Wr::B, false);
    break;
  case PPC_OP_STBUX:
    do_storex(ctx, inst, Wr::B, true);
    break;
  case PPC_OP_STHX:
    do_storex(ctx, inst, Wr::H, false);
    break;
  case PPC_OP_STHUX:
    do_storex(ctx, inst, Wr::H, true);
    break;

  case PPC_OP_STFS:
    do_fstore_ea(ctx, inst, dform_ea(ctx, inst->rA, inst->simm, false), true, false);
    break;
  case PPC_OP_STFSU:
    do_fstore_ea(ctx, inst, dform_ea(ctx, inst->rA, inst->simm, true), true, true);
    break;
  case PPC_OP_STFD:
    do_fstore_ea(ctx, inst, dform_ea(ctx, inst->rA, inst->simm, false), false, false);
    break;
  case PPC_OP_STFDU:
    do_fstore_ea(ctx, inst, dform_ea(ctx, inst->rA, inst->simm, true), false, true);
    break;

  case PPC_OP_STFSX:
    do_fstore_ea(ctx, inst, xform_ea(ctx, inst->rA, inst->rB, false), true, false);
    break;
  case PPC_OP_STFSUX:
    do_fstore_ea(ctx, inst, xform_ea(ctx, inst->rA, inst->rB, true), true, true);
    break;
  case PPC_OP_STFDX:
    do_fstore_ea(ctx, inst, xform_ea(ctx, inst->rA, inst->rB, false), false, false);
    break;
  case PPC_OP_STFDUX:
    do_fstore_ea(ctx, inst, xform_ea(ctx, inst->rA, inst->rB, true), false, true);
    break;

  case PPC_OP_PSQ_ST:
    do_psq_store(ctx, inst, false, false);
    break;
  case PPC_OP_PSQ_STU:
    do_psq_store(ctx, inst, false, true);
    break;
  case PPC_OP_PSQ_STX:
    do_psq_store(ctx, inst, true, false);
    break;
  case PPC_OP_PSQ_STUX:
    do_psq_store(ctx, inst, true, true);
    break;

  case PPC_OP_STWBRX:
  {
    u32 ea = xform_ea(ctx, inst->rA, inst->rB, false);
    mem_write32(ctx, ea, bswap32(ctx->gpr[inst->rS]));
    break;
  }

  case PPC_OP_STHBRX:
  {
    u32 ea = xform_ea(ctx, inst->rA, inst->rB, false);
    mem_write16(ctx, ea, bswap16(static_cast<u16>(ctx->gpr[inst->rS])));
    break;
  }

  case PPC_OP_LSWI:
  case PPC_OP_LSWX:
  {
    u32 count = inst->op == PPC_OP_LSWI ? (inst->nb ? inst->nb : 32u) : 0u;
    u32 ea;
    if (inst->op == PPC_OP_LSWX)
    {
      ea = ctx->gpr[inst->rB];
      if (inst->rA)
        ea += ctx->gpr[inst->rA];
      count = ctx->xer & 0x7Fu;
      u32 reg_count = (count + 3u) / 4u;
      for (u32 r = 0; r < reg_count; r++)
      {
        u32 reg = (static_cast<u32>(inst->rD) + r) & 31u;
        if (reg == static_cast<u32>(inst->rA) || reg == static_cast<u32>(inst->rB))
        {
          ppc_program_exception(ctx, PPC_PROGRAM_ILLEGAL, inst->address);
          return true;
        }
      }
    }
    else
    {
      ea = inst->rA ? ctx->gpr[inst->rA] : 0u;
    }
    for (u32 n = 0; n < count; n++)
    {
      u32 reg = (static_cast<u32>(inst->rD) + n / 4u) & 31u;
      if ((n & 3u) == 0)
        ctx->gpr[reg] = 0;
      ctx->gpr[reg] |= static_cast<u32>(mem_read8(ctx, ea + n)) << (24u - 8u * (n & 3u));
    }
    break;
  }

  case PPC_OP_STSWI:
  case PPC_OP_STSWX:
  {
    u32 count = inst->op == PPC_OP_STSWI ? (inst->nb ? inst->nb : 32u) : 0u;
    u32 ea;
    if (inst->op == PPC_OP_STSWX)
    {
      ea = ctx->gpr[inst->rB];
      if (inst->rA)
        ea += ctx->gpr[inst->rA];
      count = ctx->xer & 0x7Fu;
    }
    else
    {
      ea = inst->rA ? ctx->gpr[inst->rA] : 0u;
    }
    for (u32 n = 0; n < count; n++)
    {
      u32 reg = (static_cast<u32>(inst->rS) + n / 4u) & 31u;
      u8 value = static_cast<u8>(ctx->gpr[reg] >> (24u - 8u * (n & 3u)));
      mem_write8(ctx, ea + n, value);
    }
    break;
  }

  case PPC_OP_LWARX:
  {
    u32 ea = xform_ea(ctx, inst->rA, inst->rB, false);
    ctx->gpr[inst->rD] = mem_read32(ctx, ea);
    ctx->reserve_addr = ea;
    ctx->reserve_valid = true;
    break;
  }

  case PPC_OP_STWCX:
  {
    u32 ea = xform_ea(ctx, inst->rA, inst->rB, false);
    bool success = ctx->reserve_valid && ea == ctx->reserve_addr;
    if (success)
    {
      mem_write32(ctx, ea, ctx->gpr[inst->rS]);
      ctx->reserve_valid = false;
    }
    ctx->cr = (ctx->cr & 0x0FFFFFFFu) | ((success ? 2u : 0u) << 28) |
              ((ctx->xer >> 3) & 0x10000000u);
    break;
  }

  case PPC_OP_STFIWX:
  {
    u32 ea = xform_ea(ctx, inst->rA, inst->rB, false);
    mem_write32(ctx, ea, static_cast<u32>(dolrecomp_f64_to_bits(ctx->fpr[inst->rS])));
    break;
  }

  case PPC_OP_DCBZ:
  {
    u32 ea = xform_ea(ctx, inst->rA, inst->rB, false);
    ea &= ~31u;
    for (u32 i = 0; i < 32; i += 4)
      mem_write32(ctx, ea + i, 0);
    break;
  }

  case PPC_OP_DCBZ_L:
  {
    u32 ea = xform_ea(ctx, inst->rA, inst->rB, false);
    ppc_dcbz_l(ctx, ea, inst->address);
    if (ctx->exception)
      return true;
    break;
  }

  // The emitter hands these to ppc_fallback_instruction (= the host's instruction_fallback, i.e.
  // this very function); the work that fallback has to do is GXRuntime's ppc_cache_control.
  case PPC_OP_DCBST:
    ppc_cache_control(ctx, PPC_CACHE_DCBST, xform_ea(ctx, inst->rA, inst->rB, false), cia);
    break;
  case PPC_OP_DCBF:
    ppc_cache_control(ctx, PPC_CACHE_DCBF, xform_ea(ctx, inst->rA, inst->rB, false), cia);
    break;
  case PPC_OP_DCBI:
    ppc_cache_control(ctx, PPC_CACHE_DCBI, xform_ea(ctx, inst->rA, inst->rB, false), cia);
    break;
  case PPC_OP_ICBI:
    ppc_cache_control(ctx, PPC_CACHE_ICBI, xform_ea(ctx, inst->rA, inst->rB, false), cia);
    break;

  case PPC_OP_DCBTST:
  case PPC_OP_DCBT:
    break;

  case PPC_OP_LMW:
  {
    u32 ea = dform_ea(ctx, inst->rA, inst->simm, false);
    for (u32 r = inst->rD; r < 32; r++, ea += 4)
      ctx->gpr[r] = mem_read32(ctx, ea);
    break;
  }

  case PPC_OP_STMW:
  {
    u32 ea = dform_ea(ctx, inst->rA, inst->simm, false);
    for (u32 r = inst->rS; r < 32; r++, ea += 4)
      mem_write32(ctx, ea, ctx->gpr[r]);
    break;
  }

  case PPC_OP_B:
    if (inst->lk)
      ctx->lr = inst->address + 4;
    ctx->pc = inst->branch_target;
    break;

  case PPC_OP_BC:
  {
    BranchCond c = branch_condition(ctx, inst->bo, inst->bi);
    if (c.ctr_ok && c.cr_ok)
    {
      if (inst->lk)
        ctx->lr = inst->address + 4;
      ctx->pc = inst->branch_target;
    }
    break;
  }

  case PPC_OP_BCLR:
  case PPC_OP_BCCTR:
  {
    u32 target = inst->op == PPC_OP_BCLR ? (ctx->lr & ~3u) : (ctx->ctr & ~3u);
    BranchCond c = branch_condition(ctx, inst->bo, inst->bi);
    if (c.ctr_ok && c.cr_ok)
    {
      if (inst->lk)
        ctx->lr = inst->address + 4;
      ctx->pc = target;
    }
    break;
  }

  case PPC_OP_TWI:
    if (ppc_trap_condition(inst->to, ctx->gpr[inst->rA],
                           static_cast<u32>(static_cast<s32>(inst->simm))))
    {
      ppc_program_exception(ctx, PPC_PROGRAM_TRAP, inst->address);
      return true;
    }
    break;

  case PPC_OP_TW:
    if (ppc_trap_condition(inst->to, ctx->gpr[inst->rA], ctx->gpr[inst->rB]))
    {
      ppc_program_exception(ctx, PPC_PROGRAM_TRAP, inst->address);
      return true;
    }
    break;

  case PPC_OP_SC:
    ppc_system_call_exception(ctx, inst->address);
    return true;

  case PPC_OP_RFI:
    ppc_rfi(ctx, inst->address);
    return true;

  case PPC_OP_CRAND:
    cr_logical(ctx, inst, [](u32 a, u32 b) -> u32 { return a & b; });
    break;
  case PPC_OP_CRANDC:
    cr_logical(ctx, inst, [](u32 a, u32 b) -> u32 { return a & ~b; });
    break;
  case PPC_OP_CREQV:
    cr_logical(ctx, inst, [](u32 a, u32 b) -> u32 { return ~(a ^ b); });
    break;
  case PPC_OP_CRNAND:
    cr_logical(ctx, inst, [](u32 a, u32 b) -> u32 { return ~(a & b); });
    break;
  case PPC_OP_CRNOR:
    cr_logical(ctx, inst, [](u32 a, u32 b) -> u32 { return ~(a | b); });
    break;
  case PPC_OP_CROR:
    cr_logical(ctx, inst, [](u32 a, u32 b) -> u32 { return a | b; });
    break;
  case PPC_OP_CRORC:
    cr_logical(ctx, inst, [](u32 a, u32 b) -> u32 { return a | ~b; });
    break;
  case PPC_OP_CRXOR:
    cr_logical(ctx, inst, [](u32 a, u32 b) -> u32 { return a ^ b; });
    break;

  case PPC_OP_MCRF:
  {
    u32 dst_shift = cr_field_shift(inst->crfD);
    u32 src_shift = cr_field_shift(inst->crfS);
    u32 bits = (ctx->cr >> src_shift) & 0xFu;
    ctx->cr = (ctx->cr & ~(0xFu << dst_shift)) | (bits << dst_shift);
    break;
  }

  case PPC_OP_MCRXR:
  {
    u32 dst_shift = cr_field_shift(inst->crfD);
    u32 bits = (ctx->xer >> 28) & 0xFu;
    ctx->cr = (ctx->cr & ~(0xFu << dst_shift)) | (bits << dst_shift);
    ctx->xer &= ~0xE0000000u;
    break;
  }

  case PPC_OP_MFCR:
    ctx->gpr[inst->rD] = ctx->cr;
    break;

  case PPC_OP_MTCRF:
  {
    u32 mask = 0;
    for (u32 crf = 0; crf < 8; crf++)
    {
      if (inst->crm & (0x80u >> crf))
        mask |= 0xFu << cr_field_shift(static_cast<u8>(crf));
    }
    if (mask)
      ctx->cr = (ctx->cr & ~mask) | (ctx->gpr[inst->rS] & mask);
    break;
  }

  case PPC_OP_MFMSR:
    ctx->gpr[inst->rD] = ctx->msr;
    break;

  case PPC_OP_MTMSR:
    ctx->msr = ctx->gpr[inst->rS];
    break;

  case PPC_OP_MFSR:
    ctx->gpr[inst->rD] = ctx->sr[inst->sr];
    break;

  case PPC_OP_MFSRIN:
    ctx->gpr[inst->rD] = ctx->sr[(ctx->gpr[inst->rB] >> 28) & 0xFu];
    break;

  case PPC_OP_MTSR:
    ctx->sr[inst->sr] = ctx->gpr[inst->rS];
    break;

  case PPC_OP_MTSRIN:
    ctx->sr[(ctx->gpr[inst->rB] >> 28) & 0xFu] = ctx->gpr[inst->rS];
    break;

  case PPC_OP_MFTB:
    ctx->gpr[inst->rD] = ppc_mftb(ctx, inst->spr, inst->address);
    if (ctx->exception)
      return true;
    break;

  case PPC_OP_MFSPR:
    switch (inst->spr)
    {
    case 1:
      ctx->gpr[inst->rD] = ctx->xer;
      break;
    case 8:
      ctx->gpr[inst->rD] = ctx->lr;
      break;
    case 9:
      ctx->gpr[inst->rD] = ctx->ctr;
      break;
    case 26:
      ctx->gpr[inst->rD] = ctx->srr0;
      break;
    case 27:
      ctx->gpr[inst->rD] = ctx->srr1;
      break;
    case 268:
    case 269:
      ctx->gpr[inst->rD] = ppc_mftb(ctx, inst->spr, inst->address);
      if (ctx->exception)
        return true;
      break;
    case 912:
    case 913:
    case 914:
    case 915:
    case 916:
    case 917:
    case 918:
    case 919:
      ctx->gpr[inst->rD] = ctx->gqr[inst->spr - 912];
      break;
    case 282:
      ctx->gpr[inst->rD] = ctx->ear;
      break;
    case 920:
      ctx->gpr[inst->rD] = ctx->hid2;
      break;
    default:
      // emitter: ppc_fallback_instruction (host); the work is GXRuntime's ppc_mfspr
      ctx->gpr[inst->rD] = ppc_mfspr(ctx, inst->spr, cia);
      break;
    }
    break;

  case PPC_OP_MTSPR:
    switch (inst->spr)
    {
    case 1:
      ctx->xer = ctx->gpr[inst->rS];
      break;
    case 8:
      ctx->lr = ctx->gpr[inst->rS];
      break;
    case 9:
      ctx->ctr = ctx->gpr[inst->rS];
      break;
    case 26:
      ctx->srr0 = ctx->gpr[inst->rS];
      break;
    case 27:
      ctx->srr1 = ctx->gpr[inst->rS];
      break;
    case 282:
      ctx->ear = ctx->gpr[inst->rS];
      break;
    case 912:
    case 913:
    case 914:
    case 915:
    case 916:
    case 917:
    case 918:
    case 919:
      ctx->gqr[inst->spr - 912] = ctx->gpr[inst->rS];
      break;
    case 920:
      ctx->hid2 = ctx->gpr[inst->rS];
      break;
    default:
      // emitter: ppc_fallback_instruction (host); the work is GXRuntime's ppc_mtspr
      ppc_mtspr(ctx, inst->spr, ctx->gpr[inst->rS], cia);
      break;
    }
    break;

  case PPC_OP_TLBIE:
    ppc_tlbie(ctx, ctx->gpr[inst->rB], inst->address);
    if (ctx->exception)
      return true;
    break;

  case PPC_OP_SYNC:
  case PPC_OP_EIEIO:
  case PPC_OP_ISYNC:
  case PPC_OP_TLBSYNC:
    ppc_memory_fence();
    break;

  case PPC_OP_ECIWX:
  {
    u32 ea = xform_ea(ctx, inst->rA, inst->rB, false);
    u32 value = ppc_eciwx(ctx, ea, inst->address);
    if (ctx->exception)
      return true;
    ctx->gpr[inst->rD] = value;
    break;
  }

  case PPC_OP_ECOWX:
  {
    u32 ea = xform_ea(ctx, inst->rA, inst->rB, false);
    ppc_ecowx(ctx, ea, ctx->gpr[inst->rS], inst->address);
    if (ctx->exception)
      return true;
    break;
  }

  default:
    // The emitter has no case for this opcode either (it emits a host fallback call).
    return false;
  }

  return true;
}

}  // namespace host::interp
