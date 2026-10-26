// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

// The Skies of Arcadia Legends recomp's Uncensored setting. The Western releases (and the Japanese
// GameCube one) changed some models and textures: Dyne's and Gilder's cigars, Bellena's outfit, the
// tavern sign, red-faced drunks and their drinks. Only the Japanese Dreamcast discs (Eternal
// Arcadia) have the originals. The Dreamcast import (AudioCommon/DCMusicImport.h) copies the files
// that hold them from the player's own discs; Prepare() converts them into replacement GameCube
// files for the extracted game, and while the setting is on the extracted disc serves those in
// place of the originals (DirectoryBlob).
//
// What to change comes from a manifest shipped in Sys/Skies/uncensor-manifest.json (names,
// indices and SHA-1s, no game data): per game ID and file, the Dreamcast source and the edits
// (DiscIO/SkiesUncensorConvert.h). Every input and the result are checked against it, so a
// different disc release or a modified (romhacked) file is left alone. A converted file keeps its
// original's exact size: the disc layout doesn't change, so save states stay valid either way and
// the setting can switch while playing (from the next time the game loads a file).
namespace DiscIO::SkiesUncensor
{
// <user>/DreamcastUncensor/: dc/disc1|disc2/ (the Japanese discs' files) and <game ID>/ (the
// converted files, laid out as the game's files/ folder).
std::string Directory();
// The launchers, which don't set up Dolphin's paths: the user and Sys folders (with separators).
void SetDirectories(const std::string& user_directory, const std::string& sys_directory);

// A Dreamcast file the manifest needs (for DCMusicImport::ExtraFile).
struct DreamcastFile
{
  int disc;
  std::string path;
  std::string sha1;
  std::string dest;
};
std::vector<DreamcastFile> DreamcastFiles();

struct Summary
{
  int total = 0;    // the game's files the manifest changes (0: unknown game or no manifest)
  int sources = 0;  // of those, the ones whose Dreamcast file is imported
  int ready = 0;    // converted
  std::vector<int> discs_missing;  // the Japanese discs still to import (1, 2)
};
// For the game extracted at game_root (holding sys/ and files/).
Summary Describe(const std::string& game_root);
// Converts what's missing for that game. Returns "" when done, else an error; *made gets how many
// were converted, *skipped how many didn't match (a different or modified game file).
std::string Prepare(const std::string& game_root,
                    const std::function<void(const std::string&)>& status, int* made = nullptr,
                    int* skipped = nullptr);
// Removes the imported Dreamcast files and every converted file.
bool Remove();
// Core::Shutdown: stops the background conversion between files and waits for it (up to 10 s).
void StopBackgroundWork();

// DirectoryBlob: every file of an extracted game's folder, as the disc is built.
void Register(const std::string& physical_path);
// The file to read for physical_path at offset_in_file: the converted one while the setting is on.
// The choice is made when a file is read from its start, so a load in progress never mixes the two.
std::string Redirect(const std::string& physical_path, u64 offset_in_file);
// Whether the running game has converted files (the in-game menu offers the setting then).
bool Available();
}  // namespace DiscIO::SkiesUncensor
