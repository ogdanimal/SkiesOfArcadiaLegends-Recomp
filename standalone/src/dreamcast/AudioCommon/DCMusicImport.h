// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <string>
#include <vector>

// Imports the Dreamcast music (AudioCommon/DCMusic.h) from the player's own Skies of Arcadia
// Dreamcast disc: a GDI (with its track files beside it) or a CHD. It reads the GD-ROM's
// high-density area (ISO 9660 from LBA 45000) and copies the sound driver (BONITO.DRV), the
// common banks (ARESNEW.MLT) and the music banks (SOUND/M*.MLT) into a folder: the user
// directory's DreamcastMusic/ (DCMusic::Directory() in the running game; the launchers pass it).
//
// The game shipped on two discs; most music is on both, a few banks only on one, so either disc
// gives most of it and importing both gives all of it. A file already imported from an NTSC disc
// (USA or Japan) is kept when a PAL disc is imported: the PAL discs slow the event music by 6/5 for
// 50 Hz, and the GameCube game runs at 60 Hz.
namespace DCMusicImport
{
// A file to copy too, from the disc whose number is `disc`, when its SHA-1 (lower-case hex) matches:
// the uncensored models and textures come from the Japanese discs (DiscIO/SkiesUncensor.h).
struct ExtraFile
{
  int disc;
  std::string path;  // on the disc, upper case, e.g. "FIELD/A002A.MLD"
  std::string sha1;
  std::string dest;  // where to write it
};

// Returns "" on success, else what went wrong (shown to the player). *summary gets what was
// imported, e.g. "Disc 1 (USA): 117 music banks".
// music_dir ends with a separator. *extra_copied (if given) counts the extra files copied.
std::string Import(const std::string& image, const std::string& music_dir, std::string* summary,
                   const std::function<void(const std::string&)>& status = {},
                   const std::vector<ExtraFile>& extra = {}, int* extra_copied = nullptr);
// What's imported so far, e.g. "Disc 1 (USA) and disc 2 (Europe): 120 music banks", or "" if none.
std::string Describe(const std::string& music_dir);
// Removes the imported files.
bool Remove(const std::string& music_dir);
}  // namespace DCMusicImport
