// Dolphin's Core/ConfigManager.h for the Dreamcast modules (dolphin_shim/README.h): the game's ID
#pragma once

#include <string>

#include "dreamcast_host.h"

class SConfig
{
public:
  static SConfig& GetInstance()
  {
    static SConfig instance;
    return instance;
  }
  const std::string& GetGameID() const { return dchost::GameId(); }
};
