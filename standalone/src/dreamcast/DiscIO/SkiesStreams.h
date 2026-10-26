// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "Common/CommonTypes.h"

// The Skies of Arcadia Legends recomp's Dreamcast streamed music. Four of the game's songs aren't
// sequences but streams from the disc, sound/m01..m04_L.dsp / _R.dsp (DSP ADPCM, 22050 Hz): m01 the
// intro, m02 the title and the staff roll, m03 a story scene, m04 the ending. The Dreamcast disc
// carries its own recordings of them (SOUND/M0nS.P16, 16-bit PCM, or .P04, AICA ADPCM; 44100 Hz).
// m02 and m04 are the GameCube's recordings; m01 is a different cut and m03 another mix, which the
// music import copies (AudioCommon/DCMusicImport.cpp) and Core/SkiesMusic.cpp enables.
//
// When the Dreamcast music is on, each is converted once into a file laid out exactly as the
// GameCube one (same header, size and ADPCM coefficients, the recording resampled to its rate and
// matched to its loudness; cached in the music folder's STREAMS/), and the extracted disc serves
// that file's bytes for the GameCube one's (DirectoryBlob). The game streams it as before, so its
// start, fades, stops and savestates are untouched; switching mid-stream changes the data from the
// next read on (same coefficients: a click at worst).
//
// The European release's 50 Hz audio (issue #3), always on: the European game has no 50 Hz
// compensation in code (at 50 Hz it runs 5/6 speed, and the port runs it at 60 Hz), but its sound
// was made to match 50 Hz. 223 voice and effect streams (spells' and S-moves' KO*, battle B702*,
// field f71*, a few victory lines) are time-stretched to 1.2 times the USA length, and the sequence
// tempos in every enemy sound bank (e6*.info, and m5100000) are multiplied by 5/6, so at 60 Hz both
// lag their animations and spells hold the battle while their stream ends. Once per file, a
// background pass makes a 60 Hz copy in the user's Cache/SkiesPal60/: each stream shortened by its
// factor (WSOLA, pitch kept; the ones Europe re-edited longer cut at the USA length), re-encoded
// with its own coefficients at its original size (the header's sample count shorter: the game ends
// a stream there); each bank's tempo fields times 6/5. The extracted disc then serves those bytes.
// Which files, and where their tempo fields are, is in SkiesPal60Data.inc (names, sample counts,
// SHA-1 prefixes and positions, made by comparing the two discs): another disc's files never match.
namespace DiscIO::SkiesStreams
{
// Every file of an extracted game's folder, as the disc is built: notes the stream files (and the
// European sound files to convert).
void Register(const std::string& physical_path);
// Core::Shutdown: stops the background conversions between files and waits for them (up to 10 s).
void StopBackgroundWork();
// The file to read for physical_path: the converted stream when that track is enabled and ready,
// or the European file's 60 Hz copy once made (the first sound file read starts making them).
// The choice is made at offset 0 and kept for the rest of the file, so a copy finished while the
// game is part-way through the original isn't joined onto it.
std::string Redirect(const std::string& physical_path, u64 offset_in_file);
// Which tracks play the Dreamcast recording, bit n - 1 for mn; converts the missing ones on a
// background thread (until a track is ready, the GameCube one plays).
void SetEnabled(u32 mask);
}  // namespace DiscIO::SkiesStreams
