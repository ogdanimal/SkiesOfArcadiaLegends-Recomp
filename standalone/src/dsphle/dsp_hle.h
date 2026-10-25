// DSP high-level emulation: the mailboxes and the microcodes a retail game boots (ROM, INIT, AX),
// ported from Dolphin's Core/HW/DSPHLE (GPL-2.0-or-later). Skies' MusyX uploads the AX ucode
// (Dolphin hash 0x4e8a8b21), so AX command lists produce its sound.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/types.h"

namespace host
{
class StateIO;
}

namespace host::dsphle
{
// ---- what the HLE calls on the host (hw_dsp.cpp implements these) ----------------------------
// Raise a DSP interrupt (DSP::INT_DSP = 0x80 and friends) now or after a delay in CPU cycles.
void HostGenerateInterrupt(u32 type, u64 cycles_into_future);
u8* HostRam();          // MEM1, 24 MB, big-endian guest data
u32 HostRamSize();
u8* HostAram();         // ARAM, 16 MB
u32 HostAramSize();
// Sample rate of the AI's DSP stream (32000 or 48000), for the AX resampler.
u32 HostAiDspSampleRate();

// ---- what the host calls on the HLE --------------------------------------------------------------
void Initialize();                   // power on: ROM ucode, halted and in init, as Dolphin's DSPHLE::Initialize
void Update();                       // every millisecond of guest time (DSP_UpdateRate)
u16 ReadMailboxHigh(bool cpu_mailbox);
u16 ReadMailboxLow(bool cpu_mailbox);
void WriteMailboxHigh(bool cpu_mailbox, u16 value);
void WriteMailboxLow(bool cpu_mailbox, u16 value);
u16 WriteControlRegister(u16 value);  // returns the value as DSPHLE::DSP_WriteControlRegister
u16 ReadControlRegister();
u32 CurrentUcodeCrc();                // for logs
void DoState(StateIO& p);             // save states (DSPHLE::DoState)
}  // namespace host::dsphle
