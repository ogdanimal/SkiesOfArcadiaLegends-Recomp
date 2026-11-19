// Cheat codes (issue #5): the enabled Action Replay and Gecko codes of the data folder's Cheats/<disc ID>.ini (the
// Android launcher's import writes it) and GameSettings/<disc ID>.ini (where Dolphin keeps a game's codes, beside
// its per-game settings, which the launchers also write: never written here), in Dolphin's own format, so a
// player's Dolphin cheat file works as it is:
//   [ActionReplay]            [Gecko]
//   $Infinite gold            $Infinite gold [author]
//   0430BB40 05F5E0FF         0430BB40 05F5E0FF
//   [ActionReplay_Enabled]    [Gecko_Enabled]
//   $Infinite gold            $Infinite gold
// The recompiled game keeps the GameCube's memory layout, so codes that write memory work as written, applied
// at every frame end while [Skies] Cheats is on (the in-game menu's Gameplay tab). Supported: decrypted Action
// Replay RAM writes and fills (type 0 subtype 0: 8 / 16 / 32-bit) and Gecko 00 / 02 / 04 (and 01 / 03 / 05, the
// 0x81000000 half). A code with anything else (encrypted Action Replay lines, conditionals, Gecko C2 / 06 and
// the rest, which patch or branch the game's code: that is native here) is skipped as a whole and named in the
// log. The file is read at the first frame and again whenever the menu opens.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace host
{
namespace
{
enum class Op : u8
{
  Write8,
  Write16,
  Write32,
};

struct Write
{
  Op op;
  u32 address;
  u32 value;
  u32 count;  // consecutive units (fills)
};

struct Code
{
  std::string name;
  std::vector<Write> writes;
};

std::vector<Code> s_codes;  // enabled and supported
std::string s_file;         // the cheat file (Cheats/<ID>.ini)
int s_skipped = 0;          // enabled but unsupported
bool s_loaded = false;

std::string Trim(std::string s)
{
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.pop_back();
  size_t i = 0;
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
    ++i;
  return s.substr(i);
}

// "XXXXXXXX YYYYYYYY": the two words, or false
bool ParseLine(const std::string& line, u32* a, u32* b)
{
  if (line.size() < 17 || line[8] != ' ')
    return false;
  for (size_t i = 0; i < 17; ++i)
  {
    if (i != 8 && !std::isxdigit(static_cast<unsigned char>(line[i])))
      return false;
  }
  *a = static_cast<u32>(std::strtoul(line.substr(0, 8).c_str(), nullptr, 16));
  *b = static_cast<u32>(std::strtoul(line.substr(9, 8).c_str(), nullptr, 16));
  return true;
}

// One decrypted Action Replay line (Dolphin's ActionReplay.cpp naming): false if not a plain RAM write
bool ArLine(u32 address_word, u32 data, std::vector<Write>* out)
{
  if (address_word == 0)
    return false;  // a zero code (conditional blocks, the end of a code): not supported
  const u32 address = 0x80000000u | (address_word & 0x01FFFFFF);
  const u32 size = (address_word >> 25) & 3, type = (address_word >> 27) & 7, subtype = (address_word >> 30) & 3;
  if (type != 0 || subtype != 0)
    return false;
  switch (size)
  {
  case 0:
    out->push_back({Op::Write8, address, data & 0xFF, (data >> 8) + 1});
    return true;
  case 1:
    out->push_back({Op::Write16, address, data & 0xFFFF, (data >> 16) + 1});
    return true;
  case 2:
    out->push_back({Op::Write32, address, data, 1});
    return true;
  default:
    return false;
  }
}

// One Gecko line: 00 / 02 / 04 (01 / 03 / 05: the address in the 0x81000000 half)
bool GeckoLine(u32 a, u32 b, std::vector<Write>* out)
{
  const u32 type = a >> 24;
  const u32 address = 0x80000000u | ((type & 1) << 24) | (a & 0x00FFFFFF);
  switch (type & ~1u)
  {
  case 0x00:
    out->push_back({Op::Write8, address, b & 0xFF, (b >> 16) + 1});
    return true;
  case 0x02:
    out->push_back({Op::Write16, address, b & 0xFFFF, (b >> 16) + 1});
    return true;
  case 0x04:
    out->push_back({Op::Write32, address, b, 1});
    return true;
  default:
    return false;
  }
}

// <user folder>/<folder>/<disc ID>.ini
std::string UserIni(const char* folder)
{
  const std::string& config = SettingsConfigDir();
  if (config.empty())
    return {};
  const u8* header = DiscHeader();
  const std::string id(reinterpret_cast<const char*>(header), 6);
  return (std::filesystem::path(config).parent_path() / folder / (id + ".ini")).string();
}

// One file's enabled codes into s_codes (a name already there from an earlier file is left as it was)
void LoadFile(const std::string& path)
{
  std::ifstream in(path);
  if (!in)
    return;
  struct Raw
  {
    std::string name;
    bool gecko;
    std::vector<std::string> lines;
  };
  std::vector<Raw> raw;
  std::vector<std::string> enabled_ar, enabled_gecko;
  std::string section;
  for (std::string line; std::getline(in, line);)
  {
    line = Trim(line);
    if (line.empty() || line[0] == '*' || line[0] == '#')
      continue;
    if (line.front() == '[' && line.back() == ']')
    {
      section = line.substr(1, line.size() - 2);
      continue;
    }
    if (section == "ActionReplay_Enabled" && line[0] == '$')
      enabled_ar.push_back(Trim(line.substr(1)));
    else if (section == "Gecko_Enabled" && line[0] == '$')
      enabled_gecko.push_back(Trim(line.substr(1)));
    else if (section == "ActionReplay" || section == "Gecko")
    {
      if (line[0] == '$')
      {
        std::string name = Trim(line.substr(1));
        // Gecko names carry the creator in brackets; the enabled list has the name without it
        if (section == "Gecko")
        {
          const size_t bracket = name.find(" [");
          if (bracket != std::string::npos)
            name = Trim(name.substr(0, bracket));
        }
        raw.push_back({name, section == "Gecko", {}});
      }
      else if (!raw.empty())
      {
        raw.back().lines.push_back(line);
      }
    }
  }
  for (const Raw& r : raw)
  {
    const auto& list = r.gecko ? enabled_gecko : enabled_ar;
    bool on = false;
    for (const std::string& e : list)
      on |= e == r.name;
    if (!on)
      continue;
    bool seen = false;
    for (const Code& c : s_codes)
      seen |= c.name == r.name;
    if (seen)
      continue;
    Code code{r.name, {}};
    bool ok = !r.lines.empty();
    for (const std::string& line : r.lines)
    {
      u32 a = 0, b = 0;
      if (!ParseLine(line, &a, &b) || !(r.gecko ? GeckoLine(a, b, &code.writes) : ArLine(a, b, &code.writes)))
      {
        Log("[cheats] \"%s\" skipped: \"%s\" isn't a memory write this host can apply (encrypted, a "
            "conditional or a code patch)\n",
            r.name.c_str(), line.c_str());
        ok = false;
        break;
      }
    }
    if (ok)
      s_codes.push_back(std::move(code));
    else
      ++s_skipped;
  }
  Log("[cheats] %s: %zu code(s) enabled so far, %d skipped\n", path.c_str(), s_codes.size(), s_skipped);
}

void Load()
{
  s_loaded = true;
  s_codes.clear();
  s_skipped = 0;
  s_file = UserIni("Cheats");
  LoadFile(s_file);
  LoadFile(UserIni("GameSettings"));
}

bool InRam(u32 address, u32 bytes)
{
  return address >= 0x80000000u && address - 0x80000000u + bytes <= RAM_SIZE;
}
}  // namespace

void CheatsReload()
{
  Load();
}

int CheatsEnabledCount()
{
  if (!s_loaded)
    Load();
  return static_cast<int>(s_codes.size());
}

int CheatsSkippedCount()
{
  if (!s_loaded)
    Load();
  return s_skipped;
}

std::string CheatsFileName()
{
  if (!s_loaded)
    Load();
  return std::filesystem::path(s_file).filename().string();
}

void CheatsApply()
{
  if (!s_loaded)
    Load();
  if (s_codes.empty() || SettingGet(SettingFile::Main, "Skies", "Cheats", 0) == 0)
    return;
  for (const Code& code : s_codes)
  {
    for (const Write& w : code.writes)
    {
      const u32 unit = w.op == Op::Write8 ? 1 : w.op == Op::Write16 ? 2 : 4;
      if (!InRam(w.address, unit * w.count))
        continue;
      for (u32 i = 0; i < w.count; ++i)
      {
        const u32 at = w.address + i * unit;
        switch (w.op)
        {
        case Op::Write8:
          Write8(at, static_cast<u8>(w.value));
          break;
        case Op::Write16:
          Write16(at, static_cast<u16>(w.value));
          break;
        case Op::Write32:
          Write32(at, w.value);
          break;
        }
      }
    }
  }
}
}  // namespace host
