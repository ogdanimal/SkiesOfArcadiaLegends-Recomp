// Ported from Dolphin Core/HW/DSPHLE/DSPHLE.cpp
// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// (this port: GPL-3.0-or-later). Also implements the free-function interface of dsp_hle.h on top
// of one global DSPHLE instance; the DSPManager control-register masking stays in the host.

#include "dsphle/dsp_hle.h"

#include <utility>

#include "dsphle/dsphle.h"
#include "dsphle/ucodes.h"

namespace host::dsphle
{
DSPHLE::DSPHLE() = default;

DSPHLE::~DSPHLE() = default;

bool DSPHLE::Initialize()
{
  m_ucode = nullptr;
  m_last_ucode = nullptr;

  SetUCode(UCODE_ROM);

  m_dsp_control.Hex = 0;
  m_dsp_control.DSPHalt = 1;
  m_dsp_control.DSPInit = 1;
  m_mail_handler.SetHalted(m_dsp_control.DSPHalt);
  m_init_code_reads_left = 0;

  m_dsp_state.Reset();

  return true;
}

void DSPHLE::Shutdown()
{
  m_ucode = nullptr;
  m_last_ucode = nullptr;
}

// Called every millisecond of guest time (DSP_UpdateRate: AX HLE uses a 3ms (Wii) or 5ms (GC)
// timing period, but Dolphin updates the HLE every ms to be sure).
void DSPHLE::DSP_Update()
{
  if (m_init_code_reads_left != 0)
  {
    m_init_code_reads_left = 0;
    m_dsp_control.DSPInitCode = 0;
  }
  if (m_ucode != nullptr)
    m_ucode->Update();
}

void DSPHLE::SendMailToDSP(u32 mail)
{
  if (m_ucode != nullptr)
  {
    DSPHLE_DEBUG("CPU writes %#010x", mail);
    m_ucode->HandleMail(mail);
  }
}

void DSPHLE::SetUCode(u32 crc)
{
  m_mail_handler.ClearPending();
  m_ucode = UCodeFactory(crc, this);
  m_ucode->Initialize();
}

// TODO do it better?
// Assumes that every odd call to this func is by the persistent ucode.
// Even callers are deleted.
void DSPHLE::SwapUCode(u32 crc)
{
  m_mail_handler.ClearPending();

  if (m_last_ucode && UCodeInterface::GetCRC(m_last_ucode.get()) == crc)
  {
    m_ucode = std::move(m_last_ucode);
  }
  else
  {
    if (!m_last_ucode)
      m_last_ucode = std::move(m_ucode);
    m_ucode = UCodeFactory(crc, this);
    m_ucode->Initialize();
  }
}

u32 DSPHLE::CurrentUcodeCrc() const
{
  return UCodeInterface::GetCRC(m_ucode.get());
}

// Mailbox functions
u16 DSPHLE::DSP_ReadMailBoxHigh(bool cpu_mailbox)
{
  if (cpu_mailbox)
  {
    return (m_dsp_state.cpu_mailbox >> 16) & 0xFFFF;
  }
  else
  {
    return AccessMailHandler().ReadDSPMailboxHigh();
  }
}

u16 DSPHLE::DSP_ReadMailBoxLow(bool cpu_mailbox)
{
  if (cpu_mailbox)
  {
    return m_dsp_state.cpu_mailbox & 0xFFFF;
  }
  else
  {
    return AccessMailHandler().ReadDSPMailboxLow();
  }
}

void DSPHLE::DSP_WriteMailBoxHigh(bool cpu_mailbox, u16 value)
{
  if (cpu_mailbox)
  {
    m_dsp_state.cpu_mailbox = (m_dsp_state.cpu_mailbox & 0xFFFF) | (u32(value) << 16);
  }
  else
  {
    DSPHLE_ERROR("CPU can't write %08x to DSP mailbox", value);
  }
}

void DSPHLE::DSP_WriteMailBoxLow(bool cpu_mailbox, u16 value)
{
  if (cpu_mailbox)
  {
    m_dsp_state.cpu_mailbox = (m_dsp_state.cpu_mailbox & 0xFFFF0000) | value;
    SendMailToDSP(m_dsp_state.cpu_mailbox);
    // Mail sent so clear MSB to show that it is progressed
    m_dsp_state.cpu_mailbox &= 0x7FFFFFFF;
  }
  else
  {
    DSPHLE_ERROR("CPU can't write %08x to DSP mailbox", value);
  }
}

// Other DSP functions
u16 DSPHLE::DSP_WriteControlRegister(u16 value)
{
  UDSPControl temp(value);

  if (m_dsp_control.DSPHalt != temp.DSPHalt)
  {
    DSPHLE_DEBUG("DSP_CONTROL halt bit changed: %04x -> %04x", m_dsp_control.Hex, value);
    m_mail_handler.SetHalted(temp.DSPHalt);
  }

  if (temp.DSPReset)
  {
    SetUCode(UCODE_ROM);
    temp.DSPReset = 0;
  }

  // init - unclear if writing DSPInitCode does something. Clearing DSPInit immediately sets
  // DSPInitCode, which gets unset a bit later...
  if ((m_dsp_control.DSPInit != 0) && (temp.DSPInit == 0))
  {
    // Copy 1024(?) bytes of uCode from main memory 0x81000000 (or is it ARAM 00000000?)
    // to IMEM 0000 and jump to that code
    // TODO: Determine exactly how this initialization works
    // We could hash the input data, but this is only used for initialization purposes on licensed
    // games and by devkitpro, so there's no real point in doing so.
    // Datel has similar logic to retail games, but they clear bit 0x80 (DSP) instead of bit 0x800
    // (DSPInit) so they end up not using the init uCode.
    SetUCode(UCODE_INIT_AUDIO_SYSTEM);
    temp.DSPInitCode = 1;
    // Number obtained from real hardware on a Wii, but it's not perfectly consistent
    // (Dolphin: fake time base + 130; see m_init_code_reads_left for the replacement.)
    m_init_code_reads_left = 2;
  }

  m_dsp_control.Hex = temp.Hex;
  return m_dsp_control.Hex;
}

u16 DSPHLE::DSP_ReadControlRegister()
{
  if (m_dsp_control.DSPInitCode != 0)
  {
    if (m_init_code_reads_left > 0)
      --m_init_code_reads_left;
    if (m_init_code_reads_left == 0)
    {
      // Dolphin clears the flag *before* returning the value once the time has passed.
      m_dsp_control.DSPInitCode = 0;
    }
  }
  return m_dsp_control.Hex;
}

// ---- dsp_hle.h interface -------------------------------------------------------------------------
namespace
{
DSPHLE& Instance()
{
  static DSPHLE instance;
  return instance;
}
}  // namespace

void Initialize()
{
  Instance().Initialize();
}

void Update()
{
  Instance().DSP_Update();
}

u16 ReadMailboxHigh(bool cpu_mailbox)
{
  return Instance().DSP_ReadMailBoxHigh(cpu_mailbox);
}

u16 ReadMailboxLow(bool cpu_mailbox)
{
  return Instance().DSP_ReadMailBoxLow(cpu_mailbox);
}

void WriteMailboxHigh(bool cpu_mailbox, u16 value)
{
  Instance().DSP_WriteMailBoxHigh(cpu_mailbox, value);
}

void WriteMailboxLow(bool cpu_mailbox, u16 value)
{
  Instance().DSP_WriteMailBoxLow(cpu_mailbox, value);
}

u16 WriteControlRegister(u16 value)
{
  return Instance().DSP_WriteControlRegister(value);
}

u16 ReadControlRegister()
{
  return Instance().DSP_ReadControlRegister();
}

u32 CurrentUcodeCrc()
{
  return Instance().CurrentUcodeCrc();
}
}  // namespace host::dsphle
