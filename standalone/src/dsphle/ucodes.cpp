// Ported from Dolphin Core/HW/DSPHLE/UCodes/UCodes.cpp
// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// (this port: GPL-3.0-or-later). Only ROM, INIT and AX are built; the Zelda, CARD, GBA, ASnd,
// AESnd and AXWii ucodes of the original factory are dropped (unknown CRCs fall back to AX, as
// Dolphin does on GameCube).

#include "dsphle/ucodes.h"

#include <vector>

#include "dsphle/ax.h"
#include "dsphle/dsphle.h"
#include "dsphle/hle_common.h"
#include "dsphle/init.h"
#include "dsphle/rom.h"

namespace host::dsphle
{
UCodeInterface::UCodeInterface(DSPHLE* dsphle, u32 crc)
    : m_mail_handler(dsphle->AccessMailHandler()), m_dsphle(dsphle), m_crc(crc)
{
}

UCodeInterface::~UCodeInterface() = default;

bool UCodeInterface::NeedsResumeMail()
{
  if (m_needs_resume_mail)
  {
    m_needs_resume_mail = false;
    return true;
  }
  return false;
}

u32 HashGuestUCode(DSPHLE* dsphle, u32 address, u32 length)
{
  const u8* pointer = dsphle->GetMemory().GetPointerForRange(address, length);
  if (pointer == nullptr)
  {
    DSPHLE_WARN("ucode image at %08x (%x bytes) is outside RAM; hashing zeros", address, length);
    const std::vector<u8> zeros(length, 0);
    return HashEctor(zeros.data(), zeros.size());
  }
  return HashEctor(pointer, length);
}

void UCodeInterface::PrepareBootUCode(u32 mail)
{
  switch (m_next_ucode_steps)
  {
  case 0:
    m_next_ucode.mram_dest_addr = mail;
    break;
  case 1:
    m_next_ucode.mram_size = mail & 0xffff;
    break;
  case 2:
    m_next_ucode.mram_dram_addr = mail & 0xffff;
    break;
  case 3:
    m_next_ucode.iram_mram_addr = mail;
    break;
  case 4:
    m_next_ucode.iram_size = mail & 0xffff;
    break;
  case 5:
    m_next_ucode.iram_dest = mail & 0xffff;
    break;
  case 6:
    m_next_ucode.iram_startpc = mail & 0xffff;
    break;
  case 7:
    m_next_ucode.dram_mram_addr = mail;
    break;
  case 8:
    m_next_ucode.dram_size = mail & 0xffff;
    break;
  case 9:
    m_next_ucode.dram_dest = mail & 0xffff;
    break;
  }
  m_next_ucode_steps++;

  if (m_next_ucode_steps == 10)
  {
    m_next_ucode_steps = 0;
    m_needs_resume_mail = true;
    m_upload_setup_in_progress = false;

    const u32 ector_crc =
        HashGuestUCode(m_dsphle, m_next_ucode.iram_mram_addr, m_next_ucode.iram_size);

    DSPHLE_DEBUG("PrepareBootUCode %#010x", ector_crc);
    DSPHLE_DEBUG("DRAM -> MRAM: src %04x dst %08x size %04x", m_next_ucode.mram_dram_addr,
                 m_next_ucode.mram_dest_addr, m_next_ucode.mram_size);
    DSPHLE_DEBUG("MRAM -> IRAM: src %08x dst %04x size %04x startpc %04x",
                 m_next_ucode.iram_mram_addr, m_next_ucode.iram_dest, m_next_ucode.iram_size,
                 m_next_ucode.iram_startpc);
    DSPHLE_DEBUG("MRAM -> DRAM: src %08x dst %04x size %04x", m_next_ucode.dram_mram_addr,
                 m_next_ucode.dram_dest, m_next_ucode.dram_size);

    if (m_next_ucode.mram_size)
    {
      DSPHLE_WARN("Trying to boot new ucode with DRAM download - not implemented");
    }
    if (m_next_ucode.dram_size)
    {
      DSPHLE_WARN("Trying to boot new ucode with DRAM upload - not implemented");
    }

    m_dsphle->SwapUCode(ector_crc);
  }
}

std::unique_ptr<UCodeInterface> UCodeFactory(u32 crc, DSPHLE* dsphle)
{
  switch (crc)
  {
  case UCODE_ROM:
    DSPHLE_NOTICE("Switching to ROM ucode");
    return std::make_unique<ROMUCode>(dsphle, crc);

  case UCODE_INIT_AUDIO_SYSTEM:
    DSPHLE_NOTICE("Switching to INIT ucode");
    return std::make_unique<INITUCode>(dsphle, crc);

  case 0x3ad3b7ac:  // Naruto 3, Paper Mario - The Thousand Year Door
  case 0x3daf59b9:  // Alien Hominid
  case 0x4e8a8b21:  // spdemo, Crazy Taxi, 18 Wheeler, Disney, Monkeyball 1/2, Cubivore, Nintendo
                    // Puzzle Collection, Wario,
  // Capcom vs. SNK 2, Naruto 2, Lost Kingdoms, Star Fox, Mario Party 4, Mortal Kombat,
  // Smugglers Run Warzone, Smash Brothers, Sonic Mega Collection, ZooCube
  // nddemo, Star Fox
  case 0x07f88145:  // bustamove, Ikaruga, F-Zero GX, Robotech Battle Cry, Star Soldier, Soul
                    // Calibur 2,
                    // Zelda:OOT, Tony Hawk, Viewtiful Joe
  case 0xe2136399:  // Billy Hatcher, Dragon Ball Z, Mario Party 5, TMNT, 1080 Avalanche
    DSPHLE_NOTICE("CRC %08x: AX ucode chosen", crc);
    return std::make_unique<AXUCode>(dsphle, crc);

  default:
    DSPHLE_WARN("Unknown ucode (CRC = %08x) - forcing AX. (Dolphin: this title might be "
                "incompatible with DSP HLE emulation.)",
                crc);
    return std::make_unique<AXUCode>(dsphle, crc);

  case UCODE_NULL:
    break;
  }

  return nullptr;
}
}  // namespace host::dsphle
