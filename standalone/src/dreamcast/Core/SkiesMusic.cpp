// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Core/SkiesMusic.h"

#ifdef HAVE_DCMUSIC

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>

#include "AudioCommon/DCMusic.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/Config/SkiesSettings.h"
#include "Core/Config/ConfigManager.h"
#include "Core/HW/Memmap.h"
#include "DiscIO/SkiesStreams.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace SkiesMusic
{
namespace
{
// patches/game/dcmusic.c's block in MetroTRK's buffers, and the game's sound layer state
// (../SkiesOfArcadiaLegends-Decomp/docs/sound.md), USA / Europe / Japan.
struct Addresses
{
  u32 box;          // struct dcm_box
  u32 banks;        // the bank table: 11 slots of 0x3C bytes
  u32 bgm;          // the music's MusyX sequence id, -1 = none
  u32 bgm_slot;     // its bank slot
  u32 list;         // the current music list: next pair's index, count, (slot, num) pairs
  u32 paused;       // the paused music's sequence id, -1 = none
  u32 paused_slot;  // its bank slot
  u32 paused_list;  // its list, saved by MusicPause
  u32 ready;        // 1 once SndInit ran (before that the sound layer's state is zeros)
};
constexpr Addresses USA{0x8031A890, 0x80315120, 0x803477E0, 0x803477DC, 0x80315484,
                        0x803477D8, 0x803477D4, 0x80315434, 0x803474E0};
constexpr Addresses EUROPE{0x8031DE50, 0x803187C0, 0x8034AF08, 0x8034AF04, 0x80318B24,
                           0x8034AF00, 0x8034AEFC, 0x80318AD4, 0x8034ABF8};
constexpr Addresses JAPAN{0x8031A310, 0x80314BA0, 0x80347260, 0x8034725C, 0x80314F04,
                          0x80347258, 0x80347254, 0x80314EB4, 0x80346F60};

constexpr u32 DCM_MAGIC = 0x44434D55;  // 'DCMU'
constexpr int BANK_SLOTS = 11;
constexpr u32 BANK_SIZE = 0x3C;
constexpr u32 EVENTS = 12;
// struct dcm_box's fields
constexpr u32 BOX_MAGIC = 0x00, BOX_ENABLED = 0x04, BOX_HEAD = 0x08, BOX_SEQ = 0x0C,
              BOX_SLOT_NUM = 0x10, BOX_MUTED = 0x14, BOX_PAUSED_SEQ = 0x18,
              BOX_PAUSED_SLOT_NUM = 0x1C, BOX_PAUSED_MUTED = 0x20, BOX_ALLOW = 0x24,
              BOX_ALLOW_NAME = BOX_ALLOW + BANK_SLOTS * 8, BOX_VOL_SET = BOX_ALLOW_NAME + BANK_SLOTS * 4,
              BOX_VOL = BOX_VOL_SET + 4, BOX_EVENTS = BOX_VOL + 16;

enum : u32
{
  EV_PLAY = 1,
  EV_STOP = 2,
  EV_PAUSE = 3,
  EV_CONTINUE = 4,
  EV_VOLUME = 5,
  EV_UNMUTE = 6,
  EV_MIDICTRL = 7,
};

// The adaptive music's channel volumes are sent again this often (polls), as the game sets them
// every frame over whatever the sequence itself does.
constexpr int VOL_REFRESH = 30;

// A GameCube bank and the Dreamcast data for it: the MLT to download, the bank (SMSB number) it
// fills on the Dreamcast, and how many sequences that bank has (0 = no Dreamcast data).
struct DcBank
{
  std::string mlt;  // "" = the common banks (ARESNEW.MLT, loaded at start)
  int bank = 0;
  u32 sequences = 0;
};

// A song of the game's (current or paused) and the Dreamcast player it's on.
struct Song
{
  s32 seq = -1;          // its MusyX sequence id, -1 = none
  int player = 0;
  bool on_dc = false;    // the player plays it (else the GameCube does, or nobody)
  float full_volume = 100;  // its bank's default MusyX volume: the Dreamcast sequence's own level
};

const Addresses* s_addr = nullptr;
bool s_running = false;
// The driver files' sizes and dates when DCMusic::Start last failed: not tried again until they
// change (each try reads both files and runs a quarter second of the driver, so a try every poll
// halved the game's speed with no word; review D4).
std::string s_failed_files;

std::string FilesStamp()
{
  std::string stamp;
  for (const char* name : {"BONITO.DRV", "ARESNEW.MLT"})
  {
    std::error_code ec;
    const std::filesystem::path path = DCMusic::Directory() + name;
    const auto size = std::filesystem::file_size(path, ec);
    const auto time = std::filesystem::last_write_time(path, ec);
    // libc++'s file_clock counts in __int128 (Android, macOS): no to_string overload takes it
    stamp += std::to_string(size) + ":" +
             std::to_string(static_cast<long long>(time.time_since_epoch().count())) + ";";
  }
  return stamp;
}
u32 s_read = 0;  // events taken from the ring so far
Song s_current, s_paused;
std::array<std::array<std::string, 16>, DCMusic::PLAYERS> s_resident;  // per player and bank: MLT
std::map<std::string, DcBank> s_banks;  // by GameCube file name ("/sound/m0231")
std::array<int, 16> s_vol_sent;  // the channel volumes sent for the current song, -1 = none
int s_vol_refresh = 0;

u32 Fnv(const std::string& name)
{
  u32 h = 2166136261u;
  for (unsigned char c : name)
    h = (h ^ c) * 16777619u;
  return h;
}

u32 Read32(const std::string& d, std::size_t o)
{
  if (o + 4 > d.size())
    return 0;
  const auto* p = reinterpret_cast<const u8*>(d.data() + o);
  return p[0] | p[1] << 8 | p[2] << 16 | static_cast<u32>(p[3]) << 24;
}

// The SMSB unit an MLT carries data for: its bank and sequence count (the SMSB header's +0xC).
// want_bank >= 0 picks that bank (the common file carries several).
bool ReadSequenceBank(const std::string& file, int want_bank, int* bank, u32* sequences)
{
  if (file.size() < 0x20 || std::memcmp(file.data(), "SMLT", 4) != 0)
    return false;
  const u32 count = Read32(file, 8);
  for (u32 i = 0; i < count; ++i)
  {
    const std::size_t e = 0x20 + i * 0x20;
    if (e + 0x20 > file.size())
      break;
    const u32 num = Read32(file, e + 4), offset = Read32(file, e + 16), size = Read32(file, e + 20);
    if (std::memcmp(file.data() + e, "SMSB", 4) != 0 || offset == 0xFFFFFFFF || size < 0x10 ||
        offset + 0x10 > file.size() || (want_bank >= 0 && static_cast<int>(num) != want_bank))
    {
      continue;
    }
    *bank = static_cast<int>(num);
    *sequences = Read32(file, offset + 0xC);
    return true;
  }
  return false;
}

const DcBank& Lookup(const std::string& gc_name)
{
  auto it = s_banks.find(gc_name);
  if (it != s_banks.end())
    return it->second;
  DcBank result;
  std::string base = gc_name.substr(gc_name.find_last_of('/') + 1);
  Common::ToUpper(&base);
  const std::string dir = DCMusic::Directory();
  std::string data;
  // The common banks the GameCube splits into three files are one Dreamcast file.
  int common_bank = -1;
  if (base == "TONE")
    common_bank = 0;
  else if (base == "S1000000")
    common_bank = 1;
  else if (base == "B7000000")
    common_bank = 7;
  if (common_bank >= 0)
  {
    if (File::ReadFileToString(dir + "ARESNEW.MLT", data))
      ReadSequenceBank(data, common_bank, &result.bank, &result.sequences);
  }
  else if (File::ReadFileToString(dir + "SOUND/" + base + ".MLT", data) &&
           ReadSequenceBank(data, -1, &result.bank, &result.sequences))
  {
    result.mlt = base;
  }
  if (result.bank < 0 || result.bank >= 16)
    result.sequences = 0;
  return s_banks.emplace(gc_name, result).first->second;
}

std::string BankName(Memory::MemoryManager& memory, int slot)
{
  char name[0x21] = {};
  memory.CopyFromEmu(name, s_addr->banks + slot * BANK_SIZE + 0x14, 0x20);
  return std::string(name, strnlen(name, 0x20));
}

// The data numbers the Dreamcast has for each loaded bank slot, for the patch's mute decision.
void WriteAllowed(Memory::MemoryManager& memory)
{
  for (int slot = 0; slot < BANK_SLOTS; ++slot)
  {
    const u32 entry = s_addr->banks + slot * BANK_SIZE;
    const std::string name = BankName(memory, slot);
    const u32 hash = Fnv(name);
    if (memory.Read_U32(s_addr->box + BOX_ALLOW_NAME + slot * 4) == hash &&
        static_cast<s32>(memory.Read_U32(entry + 0x10)) >= 0)
    {
      continue;
    }
    u32 bits[2] = {};
    if (!name.empty())
    {
      const DcBank& bank = Lookup(name);
      for (u32 n = 0; n < std::min<u32>(bank.sequences, 64); ++n)
        bits[n >> 5] |= 1u << (n & 31);
    }
    memory.Write_U32(bits[0], s_addr->box + BOX_ALLOW + slot * 8);
    memory.Write_U32(bits[1], s_addr->box + BOX_ALLOW + slot * 8 + 4);
    memory.Write_U32(hash, s_addr->box + BOX_ALLOW_NAME + slot * 4);
  }
}

void StopSong(Song& song)
{
  if (song.on_dc)
    DCMusic::Stop(song.player);
  song = Song{-1, song.player};
}

// Starts the song on the Dreamcast: on the player the paused song doesn't hold.
void Play(Memory::MemoryManager& memory, Song& song, s32 seq, u32 slot_num)
{
  const int slot = static_cast<int>(slot_num >> 8);
  const int num = static_cast<int>(slot_num & 0xFF);
  const int player = s_paused.seq >= 0 && s_paused.on_dc ? 1 - s_paused.player : song.player;
  if (song.on_dc && song.player != player)
    DCMusic::Stop(song.player);
  song = Song{seq, player};
  if (slot >= BANK_SLOTS)
    return;
  const DcBank& bank = Lookup(BankName(memory, slot));
  if (static_cast<u32>(num) >= bank.sequences)
  {
    DCMusic::Stop(player);
    return;
  }
  if (!bank.mlt.empty() && s_resident[player][bank.bank] != bank.mlt)
  {
    DCMusic::Download(player, bank.mlt);
    s_resident[player][bank.bank] = bank.mlt;
  }
  DCMusic::Play(player, bank.bank, num);
  song.on_dc = true;
  // The game plays every song at its bank's default volume (slot +0x38) and scales any volume
  // change by it; read here rather than taken from the start's volume call, which a song taken
  // over mid-play (an old savestate, the option switched on) never had.
  song.full_volume = std::max<float>(memory.Read_U8(s_addr->banks + slot * BANK_SIZE + 0x38), 1.0f);
  if (&song == &s_current)
    s_vol_sent.fill(-1);
  DEBUG_LOG_FMT(AUDIO, "Dreamcast music: player {} bank {} ({}) sequence {}", player, bank.bank,
                bank.mlt, num);
}

Song* Find(s32 seq)
{
  if (seq < 0)
    return nullptr;
  if (seq == s_current.seq)
    return &s_current;
  if (seq == s_paused.seq)
    return &s_paused;
  return nullptr;
}

void Handle(Memory::MemoryManager& memory, const u32 (&e)[4])
{
  const s32 seq = static_cast<s32>(e[1]);
  switch (e[0])
  {
  case EV_PLAY:
    if (e[3])
    {
      Play(memory, s_current, seq, e[2]);
    }
    else
    {
      StopSong(s_current);
      s_current.seq = seq;
    }
    break;
  case EV_STOP:
  case EV_UNMUTE:
    if (Song* song = Find(seq))
    {
      StopSong(*song);
      if (e[0] == EV_UNMUTE)
        song->seq = seq;  // still the current song, the GameCube's now
    }
    break;
  case EV_PAUSE:
    if (seq != s_current.seq)
      break;
    if (s_paused.on_dc)
      DCMusic::Stop(s_paused.player);
    if (s_current.on_dc)
      DCMusic::Pause(s_current.player);
    s_paused = s_current;
    s_current = Song{-1, s_current.player};
    break;
  case EV_CONTINUE:
  {
    const bool known = seq == s_paused.seq;
    const Song resumed = known ? s_paused : Song{seq, s_current.player};
    s_paused = Song{-1, resumed.player};
    s_current = resumed;
    s_vol_sent.fill(-1);
    if (resumed.on_dc)
      DCMusic::Continue(resumed.player);
    else if (e[3])
      Play(memory, s_current, seq, e[2]);  // a pause from before a state load: from its start
    break;
  }
  case EV_VOLUME:
  {
    Song* song = Find(seq);
    if (!song || !song->on_dc)
      break;
    // MusyX volume 0..127; the bank's default is the Dreamcast sequence's own level.
    const float volume = static_cast<float>(e[2] & 0xFF);
    const u32 mode = (e[2] >> 8) & 0xF;
    const DCMusic::AfterFade after = mode == 1 ? DCMusic::AfterFade::Stop :
                                     mode == 2 ? DCMusic::AfterFade::Pause :
                                                 DCMusic::AfterFade::None;
    DCMusic::Fade(song->player, volume / song->full_volume, static_cast<int>(e[3]), after);
    if (mode == 1)
      song->on_dc = false;  // the player stops it after the fade
    break;
  }
  case EV_MIDICTRL:
    if (seq == s_current.seq && s_current.on_dc)
      DCMusic::MidiControl(s_current.player, (e[2] >> 16) & 0xF, (e[2] >> 8) & 0x7F, e[2] & 0x7F);
    break;
  }
}

// The adaptive music's channel volumes (controller 7), a table the patch keeps for the current song.
void SendChannelVolumes(Memory::MemoryManager& memory)
{
  if (!s_current.on_dc || static_cast<s32>(memory.Read_U32(s_addr->box + BOX_SEQ)) != s_current.seq)
    return;
  const u32 set = memory.Read_U32(s_addr->box + BOX_VOL_SET);
  const bool refresh = ++s_vol_refresh >= VOL_REFRESH;
  if (refresh)
    s_vol_refresh = 0;
  for (int channel = 0; channel < 16; ++channel)
  {
    if (!(set >> channel & 1))
      continue;
    const int value = memory.Read_U8(s_addr->box + BOX_VOL + channel) & 0x7F;
    if (value != s_vol_sent[channel] || refresh)
    {
      if (value != s_vol_sent[channel])
      DCMusic::MidiControl(s_current.player, channel, 7, value);
      s_vol_sent[channel] = value;
    }
  }
}

const Addresses* ForGame()
{
  const std::string& id = SConfig::GetInstance().GetGameID();
  return id == "GEAE8P" ? &USA : id == "GEAP8P" ? &EUROPE : id == "GEAJ8P" ? &JAPAN : nullptr;
}

// A song the patch didn't see start, from a savestate made before it: which pair of its music list
// it is. SndUpdate starts pair idx and then advances idx, to 0 after the last pair of a one-pair
// list, to 1 (pair 0 is the intro) after the last pair of a longer one. Right after a wrap, idx 1
// can't tell the intro from the last pair; the intro is taken (the next pair puts both versions
// back in step). Returns slot << 8 | num, or -1.
s32 PairFromList(Memory::MemoryManager& memory, u32 list, u32 slot_addr)
{
  const u32 idx = memory.Read_U32(list);
  const u32 count = memory.Read_U32(list + 4);
  if (count == 0 || count > 16 || idx >= count)
    return -1;
  const u32 pair = idx == 0 ? 0 : idx - 1;
  const u32 slot = memory.Read_U8(list + 8 + pair * 2);
  const u32 num = memory.Read_U8(list + 9 + pair * 2);
  if (slot != memory.Read_U32(slot_addr))
    return -1;
  return static_cast<s32>(slot << 8 | num);
}

// Takes up what the game plays now (at the start, after a state load or missed events). The block
// describes a song the patch didn't see start as playing unmuted: dcm_frame then mutes it and posts
// its start, or its continue posts it.
void Restart(Memory::MemoryManager& memory)
{
  s_read = memory.Read_U32(s_addr->box + BOX_HEAD);
  s_current = Song{-1, 0};
  s_paused = Song{-1, 1};
  if (memory.Read_U32(s_addr->ready) != 1)
    return;
  const s32 bgm = static_cast<s32>(memory.Read_U32(s_addr->bgm));
  if (bgm >= 0)
  {
    s_current.seq = bgm;
    if (static_cast<s32>(memory.Read_U32(s_addr->box + BOX_SEQ)) == bgm)
    {
      if (memory.Read_U32(s_addr->box + BOX_MUTED))
        Play(memory, s_current, bgm, memory.Read_U32(s_addr->box + BOX_SLOT_NUM));
    }
    else if (const s32 pair = PairFromList(memory, s_addr->list, s_addr->bgm_slot); pair >= 0)
    {
      memory.Write_U32(static_cast<u32>(bgm), s_addr->box + BOX_SEQ);
      memory.Write_U32(static_cast<u32>(pair), s_addr->box + BOX_SLOT_NUM);
      memory.Write_U32(0, s_addr->box + BOX_MUTED);
    }
  }
  const s32 paused = static_cast<s32>(memory.Read_U32(s_addr->paused));
  if (paused >= 0)
  {
    // Not resumed on the Dreamcast (its player's state is gone): its continue starts it again.
    s_paused.seq = paused;
    if (static_cast<s32>(memory.Read_U32(s_addr->box + BOX_PAUSED_SEQ)) != paused)
    {
      const s32 pair = PairFromList(memory, s_addr->paused_list, s_addr->paused_slot);
      memory.Write_U32(static_cast<u32>(paused), s_addr->box + BOX_PAUSED_SEQ);
      memory.Write_U32(static_cast<u32>(pair < 0 ? 0xFFFF : pair), s_addr->box + BOX_PAUSED_SLOT_NUM);
      memory.Write_U32(0, s_addr->box + BOX_PAUSED_MUTED);
    }
  }
}

// A block without the magic (a savestate made before the patch, or the first poll): zeros would
// say sequence 0, a valid MusyX id.
void InitBlock(Memory::MemoryManager& memory)
{
  if (memory.Read_U32(s_addr->box + BOX_MAGIC) == DCM_MAGIC)
    return;
  memory.Write_U32(0xFFFFFFFF, s_addr->box + BOX_SEQ);
  memory.Write_U32(0xFFFFFFFF, s_addr->box + BOX_PAUSED_SEQ);
  memory.Write_U32(0, s_addr->box + BOX_VOL_SET);
  memory.Write_U32(DCM_MAGIC, s_addr->box + BOX_MAGIC);
}
}  // namespace

void Poll(Memory::MemoryManager& memory)
{
  s_addr = ForGame();
  if (!s_addr || !memory.IsInitialized())
    return;
  // The magic is there whatever the option: the patch then keeps track of the music (its sequence
  // and pair) even while the GameCube's plays, so turning the option on mid-song can take it over.
  InitBlock(memory);
  const bool want = Config::Get(Config::SKIES_DC_MUSIC) != 0 && DCMusic::Installed();
  if (want && !s_running && FilesStamp() != s_failed_files)
  {
    s_running = DCMusic::Start();
    s_failed_files = s_running ? std::string() : FilesStamp();
    if (!s_running)
    {
      OSD::AddMessage("The Dreamcast music couldn't start: its files are damaged. Import the "
                      "Dreamcast disc again.", 10000);
    }
    for (auto& resident : s_resident)
      resident.fill({});
    s_banks.clear();
    if (s_running)
    {
      memory.Write_U32(1, s_addr->box + BOX_ENABLED);
      WriteAllowed(memory);
      Restart(memory);
    }
  }
  else if (!want && s_running)
  {
    s_running = false;
    s_current = s_paused = Song{};
    DCMusic::Shutdown();
  }
  // The streamed songs that differ on the Dreamcast (m03, and the intro m01 only if asked; m02 and
  // m04 are the same recordings): their Dreamcast recordings in place of the GameCube's files.
  DiscIO::SkiesStreams::SetEnabled(
      s_running ? 0b0100u | (Config::Get(Config::SKIES_DC_INTRO) != 0 ? 1u : 0u) : 0u);
  if (!s_running)
  {
    // The GameCube music: the patch mutes nothing (a running song is unmuted by dcm_frame).
    memory.Write_U32(0, s_addr->box + BOX_ENABLED);
    return;
  }
  memory.Write_U32(1, s_addr->box + BOX_ENABLED);
  WriteAllowed(memory);

  const u32 head = memory.Read_U32(s_addr->box + BOX_HEAD);
  if (head - s_read > EVENTS)
  {
    // More than the ring holds since the last poll (or a state from elsewhere): start over from
    // what plays now.
    WARN_LOG_FMT(AUDIO, "Dreamcast music: {} events missed", head - s_read - EVENTS);
    DCMusic::StopAll();
    Restart(memory);
    return;
  }
  for (; s_read != head; ++s_read)
  {
    u32 e[4];
    const u32 at = s_addr->box + BOX_EVENTS + (s_read % EVENTS) * 16;
    for (int i = 0; i < 4; ++i)
      e[i] = memory.Read_U32(at + i * 4);
    Handle(memory, e);
  }
  SendChannelVolumes(memory);
}

void OnStateLoaded(Memory::MemoryManager& memory)
{
  s_addr = ForGame();
  if (!s_addr || !s_running || !memory.IsInitialized())
    return;
  DCMusic::StopAll();
  InitBlock(memory);
  Restart(memory);
}
}  // namespace SkiesMusic
#endif  // HAVE_DCMUSIC
