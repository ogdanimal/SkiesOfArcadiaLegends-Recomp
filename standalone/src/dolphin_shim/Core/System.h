// Dolphin's Core/System.h for the Dreamcast modules (dolphin_shim/README.h)
#pragma once

namespace Core
{
class System
{
public:
  static System& GetInstance()
  {
    static System instance;
    return instance;
  }
};
}  // namespace Core
