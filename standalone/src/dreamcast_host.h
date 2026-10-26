// What the Dolphin build's Dreamcast modules (standalone/src/dreamcast/: the Uncensored setting, the
// Dreamcast music and its import, the Dreamcast streams and the European 60 Hz audio, copied from
// dev as they are) need from the host, behind the Dolphin headers they include
// (standalone/src/dolphin_shim/). Implemented in dreamcast.cpp.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace dchost
{
// With a separator at the end: the --user folder, its Cache/, and the Sys folder (--sys)
std::string UserDir();
std::string CacheDir();
std::string SysDir();
void Log(const std::string& line);
// An integer key of the Dolphin build's Config/Dolphin.ini (the settings both builds share)
int MainSetting(const char* section, const char* key, int default_value);
// Guest memory (big-endian words, as Dolphin's MemoryManager reads them)
bool RamReady();
std::uint32_t Read32(std::uint32_t address);
std::uint8_t Read8(std::uint32_t address);
void Write32(std::uint32_t value, std::uint32_t address);
void CopyFromRam(void* out, std::uint32_t address, std::size_t size);
// A message over the picture (the in-game menu's)
void Message(const std::string& text, std::uint32_t milliseconds);
// The running disc's ID ("GEAE8P")
const std::string& GameId();
// Whether the game runs (its thread advanced lately): Dolphin's Core::State::Running
bool GameRunning();
}  // namespace dchost
