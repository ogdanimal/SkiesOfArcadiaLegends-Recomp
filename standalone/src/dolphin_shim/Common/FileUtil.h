// Dolphin's Common/FileUtil.h for the Dreamcast modules (dolphin_shim/README.h): texture_pack.cpp's
// File:: helpers, and the user, cache and Sys folders from the host.
#pragma once

#include <string>
#include <string_view>

#include "Common/CommonTypes.h"
#include "dreamcast_host.h"
#include "texture_pack_file.h"

enum
{
  D_USER_IDX,
  D_CACHE_IDX,
};

namespace File
{
inline std::string GetUserPath(int index)
{
  return index == D_CACHE_IDX ? dchost::CacheDir() : dchost::UserDir();
}

inline std::string GetSysDirectory()
{
  return dchost::SysDir();
}

// The file's size, 0 when there is none (Dolphin's)
inline u64 GetSize(const std::string& path)
{
  std::error_code ec;
  const auto size = std::filesystem::file_size(StringToPath(path), ec);
  return ec ? 0 : static_cast<u64>(size);
}

inline bool WriteStringToFile(const std::string& path, std::string_view text)
{
  std::ofstream out(StringToPath(path), std::ios::binary | std::ios::trunc);
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  return static_cast<bool>(out);
}
}  // namespace File
