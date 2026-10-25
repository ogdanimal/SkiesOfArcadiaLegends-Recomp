// Ported from Dolphin Core/HW/DSPHLE/MailHandler.cpp
// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// (this port: GPL-3.0-or-later)

#include "dsphle/mail_handler.h"

#include "dsphle/dsp_hle.h"
#include "dsphle/hle_common.h"

namespace host::dsphle
{
// DSP::INT_DSP
static constexpr u32 INT_DSP = 0x80;

void CMailHandler::PushMail(u32 mail, bool interrupt, int cycles_into_future)
{
  if (interrupt)
  {
    if (m_pending_mails.empty())
    {
      HostGenerateInterrupt(INT_DSP, static_cast<u64>(cycles_into_future));
    }
    else
    {
      m_pending_mails.front().second = true;
    }
  }
  m_pending_mails.emplace_back(mail, false);
  DSPHLE_DEBUG("DSP writes %#010x", mail);
}

u16 CMailHandler::ReadDSPMailboxHigh()
{
  // check if we have a mail for the CPU core
  if (!m_halted && !m_pending_mails.empty())
  {
    m_last_mail = m_pending_mails.front().first;
  }
  return u16(m_last_mail >> 0x10);
}

u16 CMailHandler::ReadDSPMailboxLow()
{
  // check if we have a mail for the CPU core
  if (!m_halted && !m_pending_mails.empty())
  {
    m_last_mail = m_pending_mails.front().first;
    const bool generate_interrupt = m_pending_mails.front().second;

    m_pending_mails.pop_front();

    if (generate_interrupt)
    {
      HostGenerateInterrupt(INT_DSP, 0);
    }
  }
  // Clear the top bit of the high mail word after the mail has been read.
  // The remaining bits read back the same as the previous mail, until new mail sent.
  // (The CPU reads the high word first, and then the low word; since this function returns the low
  // word, this means that the next read of the high word will have the top bit cleared.)
  m_last_mail &= ~0x8000'0000;
  return u16(m_last_mail & 0xffff);
}

void CMailHandler::ClearPending()
{
  m_pending_mails.clear();
}

bool CMailHandler::HasPending() const
{
  return !m_pending_mails.empty();
}

void CMailHandler::SetHalted(bool halt)
{
  m_halted = halt;
}
}  // namespace host::dsphle
