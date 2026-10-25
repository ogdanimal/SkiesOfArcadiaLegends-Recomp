// The memory card in slot A (EXI channel 0, device 0) over a GCI folder, as Dolphin's
// EXI_DeviceMemoryCard.cpp + GCMemcardDirectory.cpp: the same <user>/GC/<region>/Card A folder and
// file names the Dolphin build uses, so saves carry over both ways.
//
// Unlike GCMemcardDirectory, which maps card blocks onto per-save buffers, this keeps the whole
// card as one image (a 2043-block card is 16 MB): the current game's GCI files are laid out in it
// at startup, the SDK's CARD library reads and programs it as flash, and a flush walks the newest
// directory + block map and writes back every save that changed (a save that is gone becomes
// <file>.deleted, as Dolphin does). Flushes run one guest second after the last block write, and
// at exit.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"
#include "state.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace host
{
void ExiTransferComplete(u32 channel);  // hw_exi.cpp
void ExiUpdateInterrupts();

namespace
{
namespace fs = std::filesystem;

constexpr u32 BLOCK_SIZE = 0x2000;
constexpr u32 SYSTEM_BLOCKS = 5;  // header, directory x2, block map x2
constexpr u32 DENTRY_SIZE = 0x40;
constexpr u32 DIRLEN = 0x7F;
constexpr u16 SIZE_MBITS = 0x80;  // Memory Card 2043, Dolphin's default
constexpr u32 CARD_BYTES = SIZE_MBITS * 1024 * 1024 / 8;
constexpr u32 CARD_BLOCKS = CARD_BYTES / BLOCK_SIZE;
constexpr u16 CARD_ID = 0xC221;  // a Nintendo-brand card

// EXI_DeviceMemoryCard.cpp
constexpr u8 MC_STATUS_BUSY = 0x80, MC_STATUS_UNLOCKED = 0x40, MC_STATUS_ERASEERROR = 0x10,
             MC_STATUS_PROGRAMEERROR = 0x08, MC_STATUS_READY = 0x01;
constexpr u32 READ_CYCLES_PER_BYTE = CPU_HZ / (512 * 1024);
constexpr u32 WRITE_CYCLES_PER_BYTE = CPU_HZ / static_cast<u32>(96.125f * 1024.0f);
constexpr u64 FLUSH_DELAY = CPU_HZ;  // one guest second after the last write

enum Command : u8
{
  NintendoID = 0x00,
  ReadArray = 0x52,
  ArrayToBuffer = 0x53,
  SetInterrupt = 0x81,
  WriteBuffer = 0x82,
  ReadStatus = 0x83,
  ReadID = 0x85,
  ReadErrorBuffer = 0x86,
  WakeUp = 0x87,
  Sleep = 0x88,
  ClearStatus = 0x89,
  SectorErase = 0xF1,
  PageProgram = 0xF2,
  ExtraByteProgram = 0xF3,
  ChipErase = 0xF4,
};

bool s_present = false;
std::vector<u8> s_card;  // CARD_BYTES
fs::path s_folder;

u8 s_command = NintendoID;
u8 s_status = 0;
u32 s_position = 0;
u32 s_address = 0;
u8 s_interrupt_switch = 0;
bool s_interrupt_set = false;
u8 s_program[128];
u64 s_last_access = 0;  // guest cycles at the last chip select

// Saves known to be on disk: identity (game code, maker, file name) -> file and its contents.
struct SaveFile
{
  fs::path path;
  std::vector<u8> bytes;  // the GCI as last read or written
};
std::map<std::string, SaveFile> s_files;

void Put16(u8* p, u16 v)
{
  p[0] = static_cast<u8>(v >> 8);
  p[1] = static_cast<u8>(v);
}

// GCMemcard.cpp CalculateMemcardChecksums: big-endian 16-bit sums; 0xFFFF reads as 0.
void Checksums(const u8* data, u32 size, u8* out)
{
  u16 sum = 0, inv = 0;
  for (u32 i = 0; i < size; i += 2)
  {
    const u16 d = BE16(data + i);
    sum = static_cast<u16>(sum + d);
    inv = static_cast<u16>(inv + (d ^ 0xFFFF));
  }
  Put16(out, sum == 0xFFFF ? 0 : sum);
  Put16(out + 2, inv == 0xFFFF ? 0 : inv);
}

u8* Block(u32 index)
{
  return s_card.data() + index * BLOCK_SIZE;
}

// Header.m_checksum covers m_data + m_unused_1 (0x000..0x1FC); BAT covers 0x004..end;
// Directory covers 0x0000..0x1FFC.
void FixHeaderChecksums(u8* h)
{
  Checksums(h, 0x1FC, h + 0x1FC);
}
void FixDirChecksums(u8* d)
{
  Checksums(d, 0x1FFC, d + 0x1FFC);
}
void FixBatChecksums(u8* b)
{
  Checksums(b + 4, BLOCK_SIZE - 4, b);
}

std::string Identity(const u8* dentry)
{
  // game code + maker + file name (Memcard::HasSameIdentity)
  std::string id(reinterpret_cast<const char*>(dentry), 6);
  id.append(reinterpret_cast<const char*>(dentry + 8), 0x20);
  return id;
}

// Memcard::GenerateFilename + Common::EscapeFileName
std::string GciName(const u8* dentry)
{
  auto field = [](const u8* p, size_t n) {
    std::string s;
    for (size_t i = 0; i < n && p[i]; ++i)
      s += static_cast<char>(p[i]);
    return s;
  };
  const std::string raw = field(dentry + 4, 2) + '-' + field(dentry, 4) + '-' + field(dentry + 8, 0x20);
  std::string name;
  for (size_t i = 0; i < raw.size(); ++i)
  {
    const char c = raw[i];
    if (c == '_' && i + 1 < raw.size() && raw[i + 1] == '_')
    {
      name += "__5f____5f__";
      ++i;
    }
    else if (static_cast<unsigned char>(c) <= 0x1F || std::strchr("\"*/:<>?\\|\x7f", c))
    {
      char esc[8];
      std::snprintf(esc, sizeof(esc), "__%02x__", static_cast<unsigned char>(c));
      name += esc;
    }
    else
      name += c;
  }
  return name + ".gci";
}

bool ReadFile(const fs::path& path, std::vector<u8>& out)
{
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}

bool WriteFileAtomic(const fs::path& path, const std::vector<u8>& bytes)
{
  const fs::path staged = path.string() + ".tmp";
  {
    std::ofstream f(staged, std::ios::binary | std::ios::trunc);
    if (!f)
      return false;
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!f)
      return false;
  }
  std::error_code ec;
  fs::rename(staged, path, ec);
  return !ec;
}

// Memcard::InitializeHeaderData: the serial is the SRAM flash ID scrambled by the format time.
void FormatHeader(const u8* flash_id, bool shift_jis, u64 format_time)
{
  u8* h = Block(0);
  std::memset(h, 0xFF, BLOCK_SIZE);
  u64 rand = format_time;
  for (int i = 0; i < 12; ++i)
  {
    rand = ((rand * 0x41c64e6dull) + 0x3039ull) >> 16;
    h[i] = static_cast<u8>(flash_id[i] + static_cast<u32>(rand));
    rand = ((rand * 0x41c64e6dull) + 0x3039ull) >> 16;
    rand &= 0x7fffull;
  }
  for (int i = 0; i < 8; ++i)
    h[12 + i] = static_cast<u8>(format_time >> (56 - 8 * i));
  std::memset(h + 20, 0, 4);  // SRAM rtc bias (0)
  std::memset(h + 24, 0, 4);  // SRAM language (English)
  std::memset(h + 28, 0, 4);  // DTV status
  Put16(h + 32, 0);           // device ID
  Put16(h + 34, SIZE_MBITS);
  Put16(h + 36, shift_jis ? 1 : 0);
  FixHeaderChecksums(h);
}

// Sram.cpp SetCardFlashID: the SRAM keeps the unscrambled flash ID the CARD library checks the
// header's serial against.
void SetSramFlashId(u8* sram, const u8* header)
{
  u64 rand = 0;
  for (int i = 0; i < 8; ++i)
    rand = (rand << 8) | header[12 + i];
  u8 sum = 0;
  for (int i = 0; i < 12; ++i)
  {
    rand = ((rand * 0x41c64e6dull) + 0x3039ull) >> 16;
    sram[0x18 + i] = static_cast<u8>(header[i] - static_cast<u8>(rand));
    sum = static_cast<u8>(sum + sram[0x18 + i]);
    rand = ((rand * 0x41c64e6dull) + 0x3039ull) >> 16;
    rand &= 0x7fffull;
  }
  sram[0x3E] = sum ^ 0xFF;
}

// Lays out the folder's saves for this game, as GCMemcardDirectory's constructor with
// "current game only" on; saves of other games stay untouched in the folder.
void LoadFolder(const std::string& game_code)
{
  u8* dir = Block(1);
  u8* bat = Block(3);
  std::memset(dir, 0xFF, BLOCK_SIZE);
  Put16(dir + 0x1FFA, 0);  // update counter
  std::memset(bat, 0, BLOCK_SIZE);
  u16 free_blocks = static_cast<u16>(CARD_BLOCKS - SYSTEM_BLOCKS);
  u16 last = SYSTEM_BLOCKS - 1;

  std::vector<fs::path> names;
  std::error_code ec;
  for (const auto& entry : fs::directory_iterator(s_folder, ec))
    if (entry.is_regular_file() && entry.path().extension() == ".gci")
      names.push_back(entry.path());
  std::sort(names.begin(), names.end());  // Dolphin's DoFileSearch order

  u32 index = 0;
  for (const fs::path& path : names)
  {
    std::vector<u8> bytes;
    if (!ReadFile(path, bytes) || bytes.size() < DENTRY_SIZE)
      continue;
    const u8* dentry = bytes.data();
    if (std::memcmp(dentry, game_code.data(), 4) != 0)
      continue;
    const u16 count = BE16(dentry + 0x38);
    if (count == 0 || bytes.size() != DENTRY_SIZE + size_t(count) * BLOCK_SIZE)
    {
      Log("[card] %s: size doesn't match its %u blocks, not loaded\n", path.filename().string().c_str(),
          count);
      continue;
    }
    const std::string id = Identity(dentry);
    if (s_files.count(id))
    {
      Log("[card] %s: same save as %s, not loaded\n", path.filename().string().c_str(),
          s_files[id].path.filename().string().c_str());
      continue;
    }
    if (index >= DIRLEN || count > free_blocks)
    {
      Log("[card] %s: no room on the card, not loaded\n", path.filename().string().c_str());
      continue;
    }
    // BlockAlloc::AssignBlocksContiguous
    const u16 first = static_cast<u16>(last + 1);
    for (u16 b = first; b < first + count; ++b)
      Put16(bat + 0xA + (b - SYSTEM_BLOCKS) * 2, b + 1 == first + count ? 0xFFFF : b + 1);
    last = static_cast<u16>(first + count - 1);
    free_blocks = static_cast<u16>(free_blocks - count);
    u8* slot = dir + index * DENTRY_SIZE;
    std::memcpy(slot, dentry, DENTRY_SIZE);
    Put16(slot + 0x36, first);
    // the file as the card holds it now, so an unchanged save isn't written back
    Put16(bytes.data() + 0x36, first);
    std::memcpy(Block(first), bytes.data() + DENTRY_SIZE, size_t(count) * BLOCK_SIZE);
    s_files[id] = {path, std::move(bytes)};
    ++index;
    Log("[card] loaded %s (%u blocks at %u)\n", path.filename().string().c_str(), count, first);
  }
  Put16(bat + 6, free_blocks);
  Put16(bat + 8, last);
  FixDirChecksums(dir);
  FixBatChecksums(bat);
  std::memcpy(Block(2), dir, BLOCK_SIZE);
  std::memcpy(Block(4), bat, BLOCK_SIZE);
}

void Flush()
{
  if (!s_present)
    return;
  // The live copies are the ones with the higher update counter (GCMemcardDirectory::SyncSaves).
  const u8* dir = static_cast<s16>(BE16(Block(1) + 0x1FFA)) > static_cast<s16>(BE16(Block(2) + 0x1FFA))
                      ? Block(1)
                      : Block(2);
  const u8* bat = static_cast<s16>(BE16(Block(4) + 4)) > static_cast<s16>(BE16(Block(3) + 4))
                      ? Block(4)
                      : Block(3);
  std::map<std::string, bool> seen;
  for (u32 i = 0; i < DIRLEN; ++i)
  {
    const u8* dentry = dir + i * DENTRY_SIZE;
    if (BE32(dentry) == 0xFFFFFFFF)
      continue;
    const u16 count = BE16(dentry + 0x38);
    std::vector<u8> bytes(dentry, dentry + DENTRY_SIZE);
    bytes.reserve(DENTRY_SIZE + size_t(count) * BLOCK_SIZE);
    u16 block = BE16(dentry + 0x36);
    bool ok = true;
    for (u16 n = 0; n < count; ++n)
    {
      if (block < SYSTEM_BLOCKS || block >= CARD_BLOCKS)
      {
        ok = false;
        break;
      }
      bytes.insert(bytes.end(), Block(block), Block(block) + BLOCK_SIZE);
      block = BE16(bat + 0xA + (block - SYSTEM_BLOCKS) * 2);
    }
    const std::string id = Identity(dentry);
    seen[id] = true;
    if (!ok || block != 0xFFFF)
    {
      // a save mid-update (directory and block map out of step): keep the old file
      Log("[card] save %u: block chain doesn't match its %u blocks, not written\n", i, count);
      continue;
    }
    auto it = s_files.find(id);
    if (it != s_files.end() && it->second.bytes == bytes)
      continue;
    fs::path path;
    if (it != s_files.end())
      path = it->second.path;
    else
    {
      path = s_folder / GciName(dentry);
      for (int j = 0; fs::exists(path) && j < 10; ++j)
        path = path.parent_path() / (path.stem().string() + "0.gci");
    }
    if (WriteFileAtomic(path, bytes))
    {
      Log("[card] wrote %s (%u blocks)\n", path.filename().string().c_str(), count);
      s_files[id] = {path, std::move(bytes)};
    }
    else
      Log("[card] couldn't write %s\n", path.string().c_str());
  }
  for (auto it = s_files.begin(); it != s_files.end();)
  {
    if (seen.count(it->first))
    {
      ++it;
      continue;
    }
    std::error_code ec;
    const fs::path deleted = it->second.path.string() + ".deleted";
    fs::remove(deleted, ec);
    fs::rename(it->second.path, deleted, ec);
    Log("[card] save deleted: %s -> .deleted\n", it->second.path.filename().string().c_str());
    it = s_files.erase(it);
  }
}

void FlushEvent(u64)
{
  Flush();
}

void NoteWrite(u32 address, u32 length)
{
  // GCMemcardDirectory flushes after a write that ends a block
  if ((address + length) % BLOCK_SIZE == 0)
    Schedule(EV_CARD_FLUSH, FLUSH_DELAY, FlushEvent);
}

void CardRead(u32 address, u32 length, u8* out)
{
  for (u32 i = 0; i < length; ++i)
    out[i] = s_card[(address + i) % CARD_BYTES];
}

void CardWrite(u32 address, u32 length, const u8* in)
{
  for (u32 i = 0; i < length; ++i)
    s_card[(address + i) % CARD_BYTES] = in[i];
  NoteWrite(address, length);
}

void CmdDone(u64)
{
  s_status |= MC_STATUS_READY;
  s_status &= ~MC_STATUS_BUSY;
  s_interrupt_set = true;
  ExiUpdateInterrupts();
}

void CmdDoneLater(u64 cycles)
{
  Schedule(EV_CARD_CMD, cycles, CmdDone);
}

void TransferDone(u64)
{
  ExiTransferComplete(0);
}
}  // namespace

bool CardInsert(const std::string& user_dir, const std::string& game_id, u8* sram)
{
  s_present = false;
  s_files.clear();
  if (user_dir.empty() || game_id.size() < 4)
    return false;
  const char region = game_id[3];
  const char* region_dir = region == 'J' ? "JAP" : region == 'P' ? "EUR" : "USA";
  s_folder = fs::path(user_dir) / "GC" / region_dir / "Card A";
  std::error_code ec;
  fs::create_directories(s_folder, ec);
  if (!fs::is_directory(s_folder))
  {
    Log("[card] %s is not a folder; no memory card\n", s_folder.string().c_str());
    return false;
  }
  s_card.assign(CARD_BYTES, 0xFF);
  // EXI.cpp: the header formatted from the SRAM's slot A flash ID at the emulated clock's time
  // (the wall clock, or SKIES_RTC for repeatable runs)
  const u64 format_time = RtcSeconds();
  FormatHeader(sram + 0x18, region == 'J', format_time);
  LoadFolder(game_id.substr(0, 4));
  SetSramFlashId(sram, Block(0));

  s_command = NintendoID;
  s_status = MC_STATUS_BUSY | MC_STATUS_UNLOCKED | MC_STATUS_READY;
  s_position = 0;
  s_interrupt_switch = 0;
  s_interrupt_set = false;
  std::memset(s_program, 0, sizeof(s_program));
  SchedRegister(EV_CARD_CMD, CmdDone);
  SchedRegister(EV_CARD_XFER, TransferDone);
  SchedRegister(EV_CARD_FLUSH, FlushEvent);
  s_present = true;
  Log("[card] slot A: %s, %zu save(s)\n", s_folder.string().c_str(), s_files.size());
  return true;
}

// The card as a save state holds it (as Dolphin's GCMemcardDirectory::DoState): its image and the
// saves known on disk, so a loaded state's card is the one its game saw. Nothing is written back
// until the game writes again; files are named relative to this run's card folder.
void CardDoState(StateIO& p)
{
  bool present = s_present;
  p.Do(present);
  if (p.Reading() && present != s_present)
  {
    p.Fail(present ? "the state has a memory card and this run has none (start with --user)" :
                     "the state has no memory card and this run has one");
    return;
  }
  if (!present)
    return;
  p.Do(s_card);
  if (p.Reading() && s_card.size() != CARD_BYTES)
  {
    p.Fail("the state's memory card has another size");
    s_card.assign(CARD_BYTES, 0xFF);
  }
  p.Do(s_command);
  p.Do(s_status);
  p.Do(s_position);
  p.Do(s_address);
  p.Do(s_interrupt_switch);
  p.Do(s_interrupt_set);
  p.DoArray(s_program, sizeof(s_program));
  p.Do(s_last_access);
  u64 count = s_files.size();
  p.Do(count);
  if (!p.Reading())
  {
    for (auto& [id, file] : s_files)
    {
      std::string key = id, name = file.path.filename().string();
      p.Do(key);
      p.Do(name);
      p.Do(file.bytes);
    }
    return;
  }
  s_files.clear();
  for (u64 i = 0; i < count && p.Ok(); ++i)
  {
    std::string key, name;
    SaveFile file;
    p.Do(key);
    p.Do(name);
    p.Do(file.bytes);
    file.path = s_folder / name;
    s_files[key] = std::move(file);
  }
}

double CardSecondsSinceLastAccess()
{
  return static_cast<double>(Now() - s_last_access) / CPU_HZ;
}

void CardShutdown()
{
  Deschedule(EV_CARD_FLUSH);
  Flush();
}

bool CardPresent()
{
  return s_present;
}

bool CardInterruptSet()
{
  return s_present && s_interrupt_switch && s_interrupt_set;
}

// EXI_DeviceMemoryCard.cpp SetCS
void CardSetCs(bool selected)
{
  if (!s_present)
    return;
  s_last_access = Now();
  if (selected)
  {
    s_position = 0;
    return;
  }
  switch (s_command)
  {
  case SectorErase:
    if (s_position > 2)
    {
      const u32 block = (s_address & (CARD_BYTES - 1)) / BLOCK_SIZE;
      std::memset(Block(block), 0xFF, BLOCK_SIZE);
      s_status |= MC_STATUS_BUSY;
      s_status &= ~MC_STATUS_READY;
      CmdDoneLater(5000);
    }
    break;
  case ChipErase:
    // GCMemcardDirectory::ClearAll does nothing (a format rewrites the system blocks anyway)
    if (s_position > 2)
      s_status &= ~MC_STATUS_BUSY;
    break;
  case PageProgram:
    if (s_position >= 5)
    {
      int count = static_cast<int>(s_position) - 5;
      int i = 0;
      s_status &= ~MC_STATUS_BUSY;
      while (count--)
      {
        CardWrite(s_address, 1, &s_program[i++]);
        i &= 127;
        s_address = (s_address & ~0x1FFu) | ((s_address + 1) & 0x1FF);
      }
      CmdDoneLater(5000);
    }
    break;
  default:
    break;
  }
}

// EXI_DeviceMemoryCard.cpp TransferByte
void CardTransferByte(u8& byte)
{
  if (s_position == 0)
  {
    s_command = byte;
    byte = 0xFF;
    if (s_command == ClearStatus)
    {
      s_status &= ~(MC_STATUS_PROGRAMEERROR | MC_STATUS_ERASEERROR);
      s_status |= MC_STATUS_READY;
      s_interrupt_set = false;
      s_position = 0;
    }
  }
  else
  {
    switch (s_command)
    {
    case NintendoID:
      if (s_position == 1)
        byte = 0x80;
      else
        byte = static_cast<u8>(u32(SIZE_MBITS) >> (24 - (((s_position - 2) & 3) * 8)));
      break;
    case ReadArray:
      switch (s_position)
      {
      case 1:
        s_address = u32(byte) << 17;
        byte = 0xFF;
        break;
      case 2:
        s_address |= u32(byte) << 9;
        break;
      case 3:
        s_address |= u32(byte & 3) << 7;
        break;
      case 4:
        s_address |= byte & 0x7F;
        break;
      }
      if (s_position > 1)
      {
        CardRead(s_address & (CARD_BYTES - 1), 1, &byte);
        if (s_position >= 9)
          s_address = (s_address & ~0x1FFu) | ((s_address + 1) & 0x1FF);
      }
      break;
    case ReadStatus:
      byte = s_status;
      break;
    case ReadID:
      if (s_position == 1)
        byte = static_cast<u8>(CARD_ID >> 8);
      else
        byte = static_cast<u8>((s_position & 1) ? CARD_ID : (CARD_ID >> 8));
      break;
    case SectorErase:
      if (s_position == 1)
        s_address = u32(byte) << 17;
      else if (s_position == 2)
        s_address |= u32(byte) << 9;
      byte = 0xFF;
      break;
    case SetInterrupt:
      if (s_position == 1)
        s_interrupt_switch = byte;
      byte = 0xFF;
      break;
    case ChipErase:
      byte = 0xFF;
      break;
    case PageProgram:
      switch (s_position)
      {
      case 1:
        s_address = u32(byte) << 17;
        break;
      case 2:
        s_address |= u32(byte) << 9;
        break;
      case 3:
        s_address |= u32(byte & 3) << 7;
        break;
      case 4:
        s_address |= byte & 0x7F;
        break;
      }
      if (s_position >= 5)
        s_program[(s_position - 5) & 0x7F] = byte;
      byte = 0xFF;
      break;
    default:
      Log("[card] unknown command %02x\n", s_command);
      byte = 0xFF;
      break;
    }
  }
  ++s_position;
}

// DMA reads and writes move the whole length at s_address (set up by the immediate command
// bytes); the transfer-complete interrupt comes later at the card's read / write speed.
void CardDmaRead(u8* ram, u32 length)
{
  CardRead(s_address, length, ram);
  Schedule(EV_CARD_XFER, u64(length) * READ_CYCLES_PER_BYTE, TransferDone);
}

void CardDmaWrite(const u8* ram, u32 length)
{
  CardWrite(s_address, length, ram);
  Schedule(EV_CARD_XFER, u64(length) * WRITE_CYCLES_PER_BYTE, TransferDone);
}
}  // namespace host
