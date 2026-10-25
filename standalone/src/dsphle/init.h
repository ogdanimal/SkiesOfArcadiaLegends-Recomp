// Ported from Dolphin Core/HW/DSPHLE/UCodes/INIT.h
// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// (this port: GPL-3.0-or-later)
#pragma once

#include "core/types.h"
#include "dsphle/ucodes.h"

namespace host::dsphle
{
class DSPHLE;

class INITUCode final : public UCodeInterface
{
public:
  INITUCode(DSPHLE* dsphle, u32 crc);

  void Initialize() override;
  void HandleMail(u32 mail) override;
  void Update() override;
};
}  // namespace host::dsphle
