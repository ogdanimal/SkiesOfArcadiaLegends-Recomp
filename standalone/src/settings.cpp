// Player settings in the Dolphin build's files, so both builds share one set of choices: the user
// folder's Config/Dolphin.ini ([Skies] as Core/Config/SkiesSettings.cpp names them, [DSP] Volume)
// and Config/GFX.ini ([Settings] InternalResolution / ShowFPS, [Hardware] VSync). Only the lines
// that change are rewritten; everything else in the files stays as it was. Without a user folder
// the settings live for the run only.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace host
{
namespace
{
struct IniFile
{
  std::filesystem::path path;
  std::vector<std::string> lines;
  bool dirty = false;

  void Load()
  {
    lines.clear();
    std::ifstream in(path, std::ios::binary);
    for (std::string line; std::getline(in, line);)
    {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      lines.push_back(line);
    }
  }

  static std::string Trim(const std::string& s)
  {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a])))
      ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
      --b;
    return s.substr(a, b - a);
  }

  static bool IsSection(const std::string& line, std::string* name)
  {
    const std::string t = Trim(line);
    if (t.size() < 2 || t.front() != '[' || t.back() != ']')
      return false;
    if (name)
      *name = t.substr(1, t.size() - 2);
    return true;
  }

  // The line index of section/key, or -1; *insert_at: where a new key of that section goes (-1:
  // the section isn't there).
  int Find(const std::string& section, const std::string& key, int* insert_at) const
  {
    bool in_section = false;
    if (insert_at)
      *insert_at = -1;
    for (int i = 0; i < static_cast<int>(lines.size()); ++i)
    {
      std::string name;
      if (IsSection(lines[i], &name))
      {
        in_section = name == section;
        if (in_section && insert_at)
          *insert_at = i + 1;
        continue;
      }
      if (!in_section)
        continue;
      const size_t eq = lines[i].find('=');
      if (eq != std::string::npos && Trim(lines[i].substr(0, eq)) == key)
        return i;
      if (insert_at && !Trim(lines[i]).empty())
        *insert_at = i + 1;
    }
    return -1;
  }

  bool Get(const std::string& section, const std::string& key, std::string* value) const
  {
    const int i = Find(section, key, nullptr);
    if (i < 0)
      return false;
    *value = Trim(lines[i].substr(lines[i].find('=') + 1));
    return true;
  }

  void Set(const std::string& section, const std::string& key, const std::string& value)
  {
    int insert_at;
    const int i = Find(section, key, &insert_at);
    const std::string line = key + " = " + value;
    if (i >= 0)
    {
      if (lines[i] == line)
        return;
      lines[i] = line;
    }
    else if (insert_at >= 0)
    {
      lines.insert(lines.begin() + insert_at, line);
    }
    else
    {
      if (!lines.empty() && !Trim(lines.back()).empty())
        lines.push_back("");
      lines.push_back("[" + section + "]");
      lines.push_back(line);
    }
    dirty = true;
  }

  void Save()
  {
    if (!dirty || path.empty())
      return;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::filesystem::path tmp = path;
    tmp += ".tmp";
    {
      std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
      for (const std::string& line : lines)
        out << line << '\n';
      if (!out)
      {
        Log("[settings] cannot write %s\n", tmp.string().c_str());
        return;
      }
    }
    std::filesystem::rename(tmp, path, ec);
    if (ec)
    {
      Log("[settings] cannot replace %s: %s\n", path.string().c_str(), ec.message().c_str());
      return;
    }
    dirty = false;
  }
};

IniFile s_files[2];  // SettingFile::Main, SettingFile::Gfx
std::string s_config_dir;
}  // namespace

void SettingsInit(const std::string& user_dir)
{
  for (IniFile& f : s_files)
    f = {};
  s_config_dir.clear();
  if (user_dir.empty())
    return;
  const std::filesystem::path config = std::filesystem::path(user_dir) / "Config";
  s_config_dir = config.string();
  s_files[0].path = config / "Dolphin.ini";
  s_files[1].path = config / "GFX.ini";
  for (IniFile& f : s_files)
    f.Load();
}

const std::string& SettingsConfigDir()
{
  return s_config_dir;
}

int SettingGet(SettingFile file, const char* section, const char* key, int fallback)
{
  std::string value;
  if (!s_files[static_cast<int>(file)].Get(section, key, &value) || value.empty())
    return fallback;
  if (value == "True" || value == "true")
    return 1;
  if (value == "False" || value == "false")
    return 0;
  char* end = nullptr;
  // Base 0 as Dolphin's TryParse: it writes u32 settings (MSAA) as 0x hex
  const long v = std::strtol(value.c_str(), &end, 0);
  return end && end != value.c_str() ? static_cast<int>(v) : fallback;
}

void SettingSet(SettingFile file, const char* section, const char* key, int value, bool boolean)
{
  s_files[static_cast<int>(file)].Set(section, key,
                                      boolean ? (value ? "True" : "False") : std::to_string(value));
}

void SettingsSave()
{
  for (IniFile& f : s_files)
    f.Save();
}
}  // namespace host
