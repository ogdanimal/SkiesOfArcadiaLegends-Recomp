// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/cpu.h"
namespace host::interp {
// Execute the instruction raw at cia on cpu. Sets cpu->pc to the next PC (or the exception
// vector when the instruction raised one). Returns false only for an opcode the decoder doesn't
// know (the caller then reports it).
//
// A transliteration of DolRecomp's C emitter (emit_instruction_with_range): every case runs what
// the emitted C would run, calling the same GXRuntime helpers, so the semantics match the
// recompiled code exactly.
bool ExecuteOne(CPUState* cpu, u32 raw, u32 cia);
}  // namespace host::interp
