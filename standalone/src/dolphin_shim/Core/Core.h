// Dolphin's Core/Core.h for the Dreamcast modules (dolphin_shim/README.h): running = the game's
// thread advances (a window in the background on Android stops it)
#pragma once

#include "Core/System.h"
#include "dreamcast_host.h"

namespace Core
{
enum class State
{
  Uninitialized,
  Paused,
  Running,
  Stopping,
  Starting,
};

inline State GetState(System&)
{
  return dchost::GameRunning() ? State::Running : State::Paused;
}
}  // namespace Core
