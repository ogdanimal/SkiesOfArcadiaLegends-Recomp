// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace Memory
{
class MemoryManager;
}

// The Skies of Arcadia Legends recomp's Dreamcast music option ([Skies] DreamcastMusic): runs the
// Dreamcast sound driver (AudioCommon/DCMusic.h) while the game patch (patches/game/dcmusic.c)
// mutes the GameCube music it has Dreamcast data for, and turns the patch's events (a song starts,
// stops, pauses, continues, fades, a MIDI controller changes) into the Dreamcast driver's requests.
namespace SkiesMusic
{
// Once a frame or more, on the CPU thread (from the pad poll).
void Poll(Memory::MemoryManager& memory);
// After a savestate load: the loaded game's song starts again from its beginning.
void OnStateLoaded(Memory::MemoryManager& memory);
}  // namespace SkiesMusic
