// Ported from Dolphin Core/HW/DSPHLE/UCodes/ROM.h
// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// (this port: GPL-3.0-or-later)
#pragma once

#include "core/types.h"
#include "dsphle/ucodes.h"

namespace host::dsphle
{
class DSPHLE;

class ROMUCode final : public UCodeInterface
{
public:
  ROMUCode(DSPHLE* dsphle, u32 crc);

  void Initialize() override;
  void HandleMail(u32 mail) override;
  void Update() override;

private:
  struct UCodeBootInfo
  {
    u32 m_ram_address;
    u32 m_length;
    u32 m_imem_address;
    u32 m_dmem_length;
    u32 m_start_pc;
  };
  UCodeBootInfo m_current_ucode;
  int m_boot_task_num_steps;

  u32 m_next_parameter;

  void BootUCode();
};
}  // namespace host::dsphle
