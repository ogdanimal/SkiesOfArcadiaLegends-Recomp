// Ported from Dolphin Core/HW/DSPHLE/UCodes/INIT.cpp
// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// (this port: GPL-3.0-or-later)

#include "dsphle/init.h"

#include "dsphle/dsphle.h"
#include "dsphle/hle_common.h"
#include "dsphle/mail_handler.h"

namespace host::dsphle
{
INITUCode::INITUCode(DSPHLE* dsphle, u32 crc) : UCodeInterface(dsphle, crc)
{
  DSPHLE_DEBUG("INITUCode - initialized");
}

void INITUCode::Initialize()
{
  m_mail_handler.PushMail(0x80544348);
}

void INITUCode::Update()
{
}

void INITUCode::HandleMail(u32 /*mail*/)
{
}
}  // namespace host::dsphle
