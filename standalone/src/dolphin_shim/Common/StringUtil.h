// Dolphin's Common/StringUtil.h for the Dreamcast modules (dolphin_shim/README.h)
#pragma once

#include <algorithm>
#include <cctype>
#include <string>

#include "texture_pack_file.h"

namespace Common
{
inline void ToLower(std::string* s)
{
  std::transform(s->begin(), s->end(), s->begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
}

inline void ToUpper(std::string* s)
{
  std::transform(s->begin(), s->end(), s->begin(),
                 [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
}
}  // namespace Common
