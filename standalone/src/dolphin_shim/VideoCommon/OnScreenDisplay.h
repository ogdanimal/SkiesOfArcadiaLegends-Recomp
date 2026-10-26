// Dolphin's VideoCommon/OnScreenDisplay.h for the Dreamcast modules (dolphin_shim/README.h): the
// in-game menu's messages
#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "dreamcast_host.h"

namespace OSD
{
inline void AddMessage(std::string message, u32 milliseconds = 2000)
{
  dchost::Message(message, milliseconds);
}
}  // namespace OSD
