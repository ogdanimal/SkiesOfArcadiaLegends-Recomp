// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include "Common/CommonTypes.h"

// The Dreamcast music for the Skies of Arcadia Legends recomp: the original Dreamcast game's sound
// driver (BONITO.DRV, Sega's Manatee driver for the AICA's ARM7) and its music banks (MLT files),
// imported from the player's own Dreamcast disc (AudioCommon/DCMusicImport.h), run on an emulated
// AICA sound chip (Highly Theoretical's dcsound, GPL-3). The game keeps playing its GameCube music
// engine; Core/SkiesMusic.cpp mutes its music and mirrors each music request here as the Manatee
// host command the Dreamcast game sent (sdMidiPlay / Stop / Pause / Continue / SetVol).
//
// The chip runs in the audio thread's Mix (Mixer::Mix calls DCMusic::Mix), so it plays in real time
// whatever the emulation speed (the battle speed option) and is silent while emulation is paused.
// There are two chips (players): a paused song (the field music during a battle) stays frozen on
// its own while the next plays on the other, and each has its own fades. A player that plays
// nothing isn't rendered. None of it is part of savestates: after a state load Core/SkiesMusic.cpp
// starts the music the loaded game is playing again.
namespace DCMusic
{
// Where the import puts the files: <user dir>/DreamcastMusic/ with BONITO.DRV, ARESNEW.MLT and
// SOUND/*.MLT.
std::string Directory();
// The driver, the common bank and at least one music bank are there.
bool Installed();

// Loads the driver and the common banks and starts the emulated chips (no-op when already running
// or not installed). Returns whether it runs.
bool Start();
void Shutdown();
bool Running();

constexpr int PLAYERS = 2;

// Music requests to one player (0 or 1), from any thread; the audio thread applies them, in order,
// before it renders. Download copies an MLT file's units (by name, e.g. "M0231") into the sound RAM
// banks they name.
void Download(int player, std::string_view mlt);
void Play(int player, int bank, int sequence);
void Stop(int player);
// Freezes the player (once a fade it runs has ended), Continue thaws it: the song resumes exactly.
void Pause(int player);
void Continue(int player);
void StopAll();
// A MIDI controller change on the player's sequence (sdMidiSendMes), e.g. the towns' per-channel
// volume. Channels 0..15.
void MidiControl(int player, int channel, int controller, int value);
// The player's level (1 = as the sequence plays it), reached in fade_ms of real time; then, if
// asked, it stops or pauses (the GameCube's sndSeqVolume modes 1 and 2). Play resets it to 1.
enum class AfterFade
{
  None,
  Stop,
  Pause,
};
void Fade(int player, float level, int fade_ms, AfterFade after);
// Overall level of the Dreamcast music against the GameCube sound effects, 0..1024 (256 = the
// driver's own output level; the default matches the GameCube music's loudness).
void SetGain(int gain);

// Called from Mixer::Mix: adds num_samples stereo frames at out_rate to samples.
void Mix(s16* samples, std::size_t num_samples, u32 out_rate);
}  // namespace DCMusic
