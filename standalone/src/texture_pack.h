// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <string>

// The Dolphin build's VideoCommon/TexturePackImport.h (texture_pack.cpp).
//
// Imports an HD texture pack the player downloaded (Dolphin custom textures: tex1_*.png / .dds)
// into the user directory's Load/Textures/GEA/, where Dolphin loads them for both the USA and the
// European game (VideoCommon/HiresTextures.cpp) while GFX.ini [Settings] HiresTextures is on.
//
// The pack is a .7z or .zip as downloaded, or its unpacked folder (not on Android, where the
// picker gives files). Only the game's folder in it (GEA, GEAE8P or GEAP8P) is copied, never the
// pack's optional extras beside it; a pack without one (its textures in folders of their own)
// gives all of its textures. An import adds to the packs imported before (a pack can come as
// several archives), replacing their textures of the same name; Remove takes them all out. A pack
// without textures but with .7z / .zip files in it (a pack's archives put into one .zip or folder)
// gives each of those archives' textures, as if they were imported one after another.
// Nothing of a pack is ever part of the recomp's builds: players bring their own.
namespace TexturePackImport
{
// Returns "" on success, else what went wrong (shown to the player). *summary gets what is
// imported now, e.g. "SoA_HD_v0.06_DDS: 745 textures" or "01_SoAL_HD_UI + 02_SoAL_HD_Characters:
// 3230 textures".
// textures_dir is the user directory's Load/Textures/ and ends with a separator. file_name names
// the pack when its path doesn't (an Android content URI: its display name).
std::string Import(const std::string& pack, const std::string& textures_dir, std::string* summary,
                   const std::function<void(const std::string&)>& status = {},
                   const std::string& file_name = {});
// What's imported (as for Import's summary), or "" if nothing.
std::string Describe(const std::string& textures_dir);
// How many of the imported textures are BC7 DDS files, which need a GPU with BC7 (BPTC) support:
// Dolphin skips them on others (most phones), the pack's PNG download works there.
int Bc7Count(const std::string& textures_dir);
// Removes the imported packs.
bool Remove(const std::string& textures_dir);
}  // namespace TexturePackImport
