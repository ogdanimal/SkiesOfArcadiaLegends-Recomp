// Expansion interface: the three EXI channels (EXI_Channel.cpp) with the IPL device on channel 0
// chip select 1 (EXI_DeviceIPL.cpp: the real-time clock and SRAM the OS reads at boot, the UART
// OSReport writes to), and the memory card in slot A (channel 0 chip select 0, hw_memcard.cpp).
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"
#include "state.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <string>

namespace host
{
bool CardInsert(const std::string& user_dir, const std::string& game_id, u8* sram);  // hw_memcard
void CardShutdown();
bool CardPresent();
bool CardInterruptSet();
void CardSetCs(bool selected);
void CardTransferByte(u8& byte);
void CardDmaRead(u8* ram, u32 length);
void CardDmaWrite(const u8* ram, u32 length);
void CardDoState(StateIO& p);

namespace
{
std::string s_user_dir;

enum : u32
{
  ST_EXIINTMASK = 1u << 0,
  ST_EXIINT = 1u << 1,
  ST_TCINTMASK = 1u << 2,
  ST_TCINT = 1u << 3,
  ST_CLK = 7u << 4,
  ST_CS = 7u << 7,
  ST_EXTINTMASK = 1u << 10,
  ST_EXTINT = 1u << 11,
  ST_EXT = 1u << 12,
  ST_ROMDIS = 1u << 13,
};

struct Channel
{
  u32 status = 0, dma_address = 0, dma_length = 0, control = 0, imm = 0;
};
Channel s_ch[3];

// ---- the IPL device: RTC + SRAM at 0x800000, UART at 0x800400 (command addresses >> 6) --------
u8 s_sram[0x44];
u32 s_ipl_command = 0;
u32 s_ipl_command_bytes = 0;
u32 s_ipl_cursor = 0;
std::string s_uart;

void FixSramChecksums()
{
  u16 sum = 0, inv = 0;
  for (u32 i = 0x10; i < 0x18; i += 2)  // rtc_bias .. flags
  {
    const u16 v = BE16(s_sram + i);
    sum = static_cast<u16>(sum + v);
    inv = static_cast<u16>(inv + static_cast<u16>(~v));
  }
  s_sram[4] = static_cast<u8>(sum >> 8);
  s_sram[5] = static_cast<u8>(sum);
  s_sram[6] = static_cast<u8>(inv >> 8);
  s_sram[7] = static_cast<u8>(inv);
}

void InitSram()
{
  std::memset(s_sram, 0, sizeof(s_sram));
  // Language: the European disc's from [Skies] Language (0 English, 1 German, 2 French, 3 Spanish, the
  // Dolphin build's key: GetLanguageAdjustedForRegion), read by the game once at boot; English
  // elsewhere
  s_sram[0x16] = DiscHeader()[3] == 'P' ?
                     static_cast<u8>(std::clamp(SettingGet(SettingFile::Main, "Skies", "Language", 0), 0, 3)) :
                     0;
  s_sram[0x17] = 0x2C;  // flags: stereo, setup done (Dolphin's template)
  const char slot_a[] = "DOLPHINSLOTA", slot_b[] = "DOLPHINSLOTB";
  std::memcpy(s_sram + 0x18, slot_a, 12);
  std::memcpy(s_sram + 0x24, slot_b, 12);
  s_sram[0x3E] = 0x6E;
  s_sram[0x3F] = 0x6D;
  FixSramChecksums();
}

void UpdateRtc()
{
  const u32 seconds = RtcSeconds();
  s_sram[0] = static_cast<u8>(seconds >> 24);
  s_sram[1] = static_cast<u8>(seconds >> 16);
  s_sram[2] = static_cast<u8>(seconds >> 8);
  s_sram[3] = static_cast<u8>(seconds);
}

void IplTransferByte(u8& data)
{
  if (s_ipl_command_bytes < 4)
  {
    s_ipl_command = (s_ipl_command << 8) | data;
    data = 0xFF;
    if (++s_ipl_command_bytes == 4)
      UpdateRtc();
    return;
  }
  const bool write = s_ipl_command >> 31;
  const u32 address = (s_ipl_command >> 6) & 0x1FFFFFF;
  if (address < 0x200000)
  {
    // the boot ROM (fonts): not present
    if (!write)
      data = 0;
    ++s_ipl_cursor;
  }
  else if (address >= 0x800000 && address < 0x800000 + sizeof(s_sram))
  {
    const u32 at = (address - 0x800000 + s_ipl_cursor++) % sizeof(s_sram);
    if (write)
      s_sram[at] = data;
    else
      data = s_sram[at];
  }
  else if (address >= 0x800400 && address < 0x800450)
  {
    if (address == 0x800400)
    {
      if (write)
      {
        if (data != '\0' && data != '\r')
          s_uart += static_cast<char>(data);
        if (data == '\r' || data == '\n')
        {
          if (!s_uart.empty() && s_uart != "\n")
            Log("[osreport] %s\n", s_uart.c_str());
          s_uart.clear();
        }
      }
      else
        data = 0;
    }
  }
  else if (address >= 0xC00000 && address < 0xC00008)
  {
    if (address == 0xC00004 && write && data != 0)
      s_uart += static_cast<char>(data);
  }
}

bool DevicePresent(u32 channel, u32 device)
{
  return channel == 0 && (device == 1 || (device == 0 && CardPresent()));
}

int CsToDevice(u32 status)
{
  const u32 cs = (status >> 7) & 7;
  if (cs & 1)
    return 0;
  if (cs & 2)
    return 1;
  if (cs & 4)
    return 2;
  return -1;
}

u8 TransferByte(u32 channel, int device, u8 data)
{
  if (channel == 0 && device == 1)
    IplTransferByte(data);
  else if (channel == 0 && device == 0 && CardPresent())
    CardTransferByte(data);
  else
    data = 0;
  return data;
}

void UpdateInterrupts()
{
  // EXI_Channel.cpp IsCausingInterrupt: the slot A card's interrupt latches channel 0's EXIINT
  if (CardInterruptSet())
    s_ch[0].status |= ST_EXIINT;
  bool set = false;
  for (const Channel& c : s_ch)
  {
    set |= (c.status & ST_EXIINT) && (c.status & ST_EXIINTMASK);
    set |= (c.status & ST_TCINT) && (c.status & ST_TCINTMASK);
    set |= (c.status & ST_EXTINT) && (c.status & ST_EXTINTMASK);
  }
  PiSetInterrupt(PI_INT_EXI, set);
}

void Transfer(u32 channel)
{
  Channel& c = s_ch[channel];
  const int device = CsToDevice(c.status);
  if (device < 0)
    return;
  const u32 rw = (c.control >> 2) & 3;
  // The card completes its DMAs later and raises no transfer-complete for immediate transfers
  // (UseDelayedTransferCompletion).
  const bool card = channel == 0 && device == 0 && CardPresent();
  if (card && (c.control & 2))
  {
    c.control &= ~1u;
    u8* ram = RamPtr(c.dma_address, c.dma_length);
    if (ram && rw == 0)
      CardDmaRead(ram, c.dma_length);
    else if (ram && rw == 1)
      CardDmaWrite(ram, c.dma_length);
    return;
  }
  if (!(c.control & 2))
  {
    // immediate: up to 4 bytes from the top of the data register
    const u32 length = ((c.control >> 4) & 3) + 1;
    u32 result = 0;
    for (u32 i = 0; i < length; ++i)
    {
      u8 byte = rw == 0 ? 0 : static_cast<u8>(c.imm >> (24 - 8 * i));
      byte = TransferByte(channel, device, byte);
      result |= u32(byte) << (24 - 8 * i);
    }
    if (rw != 1)
      c.imm = result;
  }
  else
  {
    u8* ram = RamPtr(c.dma_address, c.dma_length);
    if (ram)
    {
      for (u32 i = 0; i < c.dma_length; ++i)
      {
        const u8 byte = TransferByte(channel, device, rw == 1 ? ram[i] : 0);
        if (rw == 0)
          ram[i] = byte;
      }
    }
  }
  c.control &= ~1u;
  if (card)
    return;
  c.status |= ST_TCINT;
  UpdateInterrupts();
}
}  // namespace

void ExiTransferComplete(u32 channel)
{
  s_ch[channel].status |= ST_TCINT;
  UpdateInterrupts();
}

void ExiUpdateInterrupts()
{
  UpdateInterrupts();
}

void ExiSetUserDir(const std::string& user_dir)
{
  s_user_dir = user_dir;
}

void ExiInit()
{
  for (Channel& c : s_ch)
    c = {};
  InitSram();
  s_ipl_command = s_ipl_command_bytes = s_ipl_cursor = 0;
  CardInsert(s_user_dir, std::string(reinterpret_cast<const char*>(DiscHeader()), 6), s_sram);
}

void ExiDoState(StateIO& p)
{
  p.DoArray(s_ch, 3);
  p.DoArray(s_sram, sizeof(s_sram));
  p.Do(s_ipl_command);
  p.Do(s_ipl_command_bytes);
  p.Do(s_ipl_cursor);
  p.Do(s_uart);
  CardDoState(p);
}

void ExiShutdown()
{
  CardShutdown();
}

u32 ExiRead32(u32 offset)
{
  const u32 channel = offset / 0x14;
  if (channel > 2)
    return 0;
  Channel& c = s_ch[channel];
  switch (offset % 0x14)
  {
  case 0x00:
  {
    u32 status = c.status & ~ST_EXT;
    if (channel != 2 && DevicePresent(channel, 0))
      status |= ST_EXT;
    return status;
  }
  case 0x04:
    return c.dma_address;
  case 0x08:
    return c.dma_length;
  case 0x0C:
    return c.control;
  default:
    return c.imm;
  }
}

void ExiWrite32(u32 offset, u32 value)
{
  const u32 channel = offset / 0x14;
  if (channel > 2)
    return;
  Channel& c = s_ch[channel];
  switch (offset % 0x14)
  {
  case 0x00:
  {
    u32 next = c.status;
    next = (next & ~ST_EXIINTMASK) | (value & ST_EXIINTMASK);
    if (value & ST_EXIINT)
      next &= ~ST_EXIINT;
    next = (next & ~ST_TCINTMASK) | (value & ST_TCINTMASK);
    if (value & ST_TCINT)
      next &= ~ST_TCINT;
    next = (next & ~ST_CLK) | (value & ST_CLK);
    if (channel < 2)
    {
      next = (next & ~ST_EXTINTMASK) | (value & ST_EXTINTMASK);
      if (value & ST_EXTINT)
        next &= ~ST_EXTINT;
    }
    if (channel == 0)
      next = (next & ~ST_ROMDIS) | (value & ST_ROMDIS);
    const u32 old_cs = (c.status >> 7) & 7, new_cs = (value >> 7) & 7;
    next = (next & ~ST_CS) | (value & ST_CS);
    // selecting the IPL starts a new command
    if (channel == 0 && (new_cs & 2) && !(old_cs & 2))
    {
      s_ipl_command_bytes = 0;
      s_ipl_cursor = 0;
    }
    c.status = next;
    // EXI_Channel.cpp: the device whose chip select changed sees the new select lines
    if (channel == 0 && ((old_cs ^ new_cs) & 1))
      CardSetCs(new_cs & 1);
    UpdateInterrupts();
    return;
  }
  case 0x04:
    c.dma_address = value;
    return;
  case 0x08:
    c.dma_length = value;
    return;
  case 0x0C:
    c.control = value;
    if (value & 1)
      Transfer(channel);
    return;
  default:
    c.imm = value;
    return;
  }
}
}  // namespace host
