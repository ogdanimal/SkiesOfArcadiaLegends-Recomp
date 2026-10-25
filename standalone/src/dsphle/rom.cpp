// Ported from Dolphin Core/HW/DSPHLE/UCodes/ROM.cpp
// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// (this port: GPL-3.0-or-later)

#include "dsphle/rom.h"

#include "dsphle/dsphle.h"
#include "dsphle/hle_common.h"
#include "dsphle/mail_handler.h"

namespace host::dsphle
{
ROMUCode::ROMUCode(DSPHLE* dsphle, u32 crc)
    : UCodeInterface(dsphle, crc), m_current_ucode(), m_boot_task_num_steps(0), m_next_parameter(0)
{
  DSPHLE_DEBUG("UCode_Rom - initialized");
}

void ROMUCode::Initialize()
{
  m_mail_handler.PushMail(0x8071FEED);
}

void ROMUCode::Update()
{
}

void ROMUCode::HandleMail(u32 mail)
{
  if (m_next_parameter == 0)
  {
    // wait for beginning of UCode
    if ((mail & 0xFFFF0000) != 0x80F30000)
    {
      u32 Message = 0xFEEE0000 | (mail & 0xFFFF);
      m_mail_handler.PushMail(Message);
    }
    else
    {
      m_next_parameter = mail;
    }
  }
  else
  {
    switch (m_next_parameter)
    {
    case 0x80F3A001:
      m_current_ucode.m_ram_address = mail;
      break;

    case 0x80F3A002:
      m_current_ucode.m_length = mail & 0xffff;
      break;

    case 0x80F3B002:
      m_current_ucode.m_dmem_length = mail & 0xffff;
      if (m_current_ucode.m_dmem_length)
      {
        DSPHLE_NOTICE("m_current_ucode.m_dmem_length = %#06x.", m_current_ucode.m_dmem_length);
      }
      break;

    case 0x80F3C002:
      m_current_ucode.m_imem_address = mail & 0xffff;
      break;

    case 0x80F3D001:
      m_current_ucode.m_start_pc = mail & 0xffff;
      BootUCode();
      return;  // Important! BootUCode indirectly does "delete this;". Must exit immediately.

    default:
      break;
    }

    // THE GODDAMN OVERWRITE WAS HERE. Without the return above, since BootUCode may delete "this",
    // well ...
    m_next_parameter = 0;
  }
}

void ROMUCode::BootUCode()
{
  const u32 ector_crc =
      HashGuestUCode(m_dsphle, m_current_ucode.m_ram_address, m_current_ucode.m_length);

  DSPHLE_DEBUG("CurrentUCode SOURCE Addr: %#010x", m_current_ucode.m_ram_address);
  DSPHLE_DEBUG("CurrentUCode Length:      %#010x", m_current_ucode.m_length);
  DSPHLE_DEBUG("CurrentUCode DEST Addr:   %#010x", m_current_ucode.m_imem_address);
  DSPHLE_DEBUG("CurrentUCode DMEM Length: %#010x", m_current_ucode.m_dmem_length);
  DSPHLE_DEBUG("CurrentUCode init_vector: %#010x", m_current_ucode.m_start_pc);
  DSPHLE_DEBUG("CurrentUCode CRC:         %#010x", ector_crc);
  DSPHLE_DEBUG("BootTask - done");

  m_dsphle->SetUCode(ector_crc);
}
}  // namespace host::dsphle
