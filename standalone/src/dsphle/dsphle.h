// Ported from Dolphin Core/HW/DSPHLE/DSPHLE.h
// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// (this port: GPL-3.0-or-later; the DSPEmulator base class, Core::System and savestates are gone)
#pragma once

#include <memory>

#include "core/types.h"
#include "dsphle/hle_common.h"
#include "dsphle/mail_handler.h"

namespace host
{
class StateIO;
}

namespace host::dsphle
{
class UCodeInterface;

// DSP::UDSPControl (Core/HW/DSP.h)
union UDSPControl
{
  u16 Hex;
  struct
  {
    // DSP Control
    u16 DSPReset : 1;  // Write 1 to reset and waits for 0
    u16 DSPAssertInt : 1;
    u16 DSPHalt : 1;
    // Interrupt for DMA to the AI/speakers
    u16 AID : 1;
    u16 AID_mask : 1;
    // ARAM DMA interrupt
    u16 ARAM : 1;
    u16 ARAM_mask : 1;
    // DSP DMA interrupt
    u16 DSP : 1;
    u16 DSP_mask : 1;
    // Other ???
    u16 DMAState : 1;     // DSPGetDMAStatus() uses this flag. __ARWaitForDMA() uses it too...maybe
                          // it's just general DMA flag
    u16 DSPInitCode : 1;  // Indicator that the DSP was initialized?
    u16 DSPInit : 1;      // DSPInit() writes to this flag
    u16 pad : 4;
  };
  UDSPControl(u16 hex = 0) : Hex(hex) {}
};

class DSPHLE
{
public:
  DSPHLE();
  DSPHLE(const DSPHLE& other) = delete;
  DSPHLE(DSPHLE&& other) = delete;
  DSPHLE& operator=(const DSPHLE& other) = delete;
  DSPHLE& operator=(DSPHLE&& other) = delete;
  ~DSPHLE();

  bool Initialize();
  void Shutdown();

  void DSP_WriteMailBoxHigh(bool cpu_mailbox, u16 value);
  void DSP_WriteMailBoxLow(bool cpu_mailbox, u16 value);
  u16 DSP_ReadMailBoxHigh(bool cpu_mailbox);
  u16 DSP_ReadMailBoxLow(bool cpu_mailbox);
  u16 DSP_ReadControlRegister();
  u16 DSP_WriteControlRegister(u16 value);
  void DSP_Update();

  CMailHandler& AccessMailHandler() { return m_mail_handler; }
  void SetUCode(u32 crc);
  void SwapUCode(u32 crc);

  MemoryManager GetMemory() const { return MemoryManager(); }
  u32 CurrentUcodeCrc() const;
  void DoState(StateIO& p);

private:
  void SendMailToDSP(u32 mail);

  // Fake mailbox utility
  struct DSPState
  {
    u32 cpu_mailbox;
    u32 dsp_mailbox;

    void Reset()
    {
      cpu_mailbox = 0x00000000;
      dsp_mailbox = 0x00000000;
    }

    DSPState() { Reset(); }
  };
  DSPState m_dsp_state;

  std::unique_ptr<UCodeInterface> m_ucode;
  std::unique_ptr<UCodeInterface> m_last_ucode;

  UDSPControl m_dsp_control;
  // Dolphin clears DSPInitCode once the fake time base passes (write time + 130 ticks, about 3
  // microseconds of guest time). The host has no time base here, so the flag is cleared by the
  // second DSP_ReadControlRegister after the init write (the first read still sees it set, as a
  // CPU read within those 130 ticks would), or by the next DSP_Update() tick, whichever is first.
  // 0 = not pending.
  int m_init_code_reads_left = 0;

  CMailHandler m_mail_handler;
};
}  // namespace host::dsphle
