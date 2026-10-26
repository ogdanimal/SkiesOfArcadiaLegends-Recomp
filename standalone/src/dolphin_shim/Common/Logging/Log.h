// Dolphin's Common/Logging/Log.h for the Dreamcast modules (dolphin_shim/README.h): the notices,
// infos and warnings go to the host's log; debug lines only with SKIES_DREAMCAST_DEBUG=1.
#pragma once

#include <cstdlib>
#include <fmt/format.h>

#include "dreamcast_host.h"

#define SKIES_SHIM_LOG(prefix, category, ...)                                                       \
  dchost::Log(std::string("[" #category "] ") + prefix + fmt::format(__VA_ARGS__))
#define NOTICE_LOG_FMT(category, ...) SKIES_SHIM_LOG("", category, __VA_ARGS__)
#define INFO_LOG_FMT(category, ...) SKIES_SHIM_LOG("", category, __VA_ARGS__)
#define WARN_LOG_FMT(category, ...) SKIES_SHIM_LOG("warning: ", category, __VA_ARGS__)
#define ERROR_LOG_FMT(category, ...) SKIES_SHIM_LOG("error: ", category, __VA_ARGS__)
#define DEBUG_LOG_FMT(category, ...)                                                                \
  do                                                                                                \
  {                                                                                                 \
    static const bool skies_shim_debug = std::getenv("SKIES_DREAMCAST_DEBUG") != nullptr;          \
    if (skies_shim_debug)                                                                           \
      SKIES_SHIM_LOG("", category, __VA_ARGS__);                                                    \
  } while (0)
