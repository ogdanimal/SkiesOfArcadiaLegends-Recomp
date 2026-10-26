// Dolphin's Core/Config/SkiesSettings.h for the Dreamcast modules (dolphin_shim/README.h): the keys
// they read, on the Dolphin build's names and defaults (Config/Dolphin.ini, shared by both builds).
#pragma once

#include "dreamcast_host.h"

namespace Config
{
struct IntKey
{
  const char* section;
  const char* key;
  int default_value;
};

// The Dreamcast music: 1 = the Dreamcast driver plays the music
inline constexpr IntKey SKIES_DC_MUSIC{"Skies", "DreamcastMusic", 0};
// With it, the intro (m01) from the Dreamcast disc too
inline constexpr IntKey SKIES_DC_INTRO{"Skies", "DreamcastIntro", 1};
// Uncensored: 1 = the Japanese Dreamcast originals of the models and textures the West changed
inline constexpr IntKey SKIES_UNCENSORED{"Skies", "Uncensored", 0};

inline int Get(const IntKey& key)
{
  return dchost::MainSetting(key.section, key.key, key.default_value);
}
}  // namespace Config
