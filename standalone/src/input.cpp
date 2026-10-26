// Controllers and the keyboard through Aurora's PAD layer (its SDL pads, bindings and mapping
// store): what the in-game menu shows and changes about them. Kept apart from menu.cpp, whose
// GameCube button constants clash with Aurora's <dolphin/pad.h> macros.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

#include <cctype>
#include <string>
#include <string_view>

#ifdef SKIES_STANDALONE_AURORA
#include <dolphin/pad.h>
#endif

namespace host
{
namespace
{
// The controller on port 1 as last seen ("" none)
std::string s_port1_name;
}  // namespace

// How a controller's name is shown (the Dolphin build's SkiesBindings::DeviceLabel). Steam names
// its virtual gamepads after a translation key it leaves untranslated
// ("#SettingsController_SteamController"): that reads "Steam Controller".
std::string InputDeviceLabel(std::string_view name)
{
  if (!name.starts_with('#'))
    return std::string(name);
  // The key's last part, split into words where a capital follows a lower-case letter or digit
  std::string_view key = name.substr(1);
  if (const std::size_t underscore = key.rfind('_'); underscore != std::string_view::npos)
    key = key.substr(underscore + 1);
  std::string label;
  for (std::size_t i = 0; i < key.size(); ++i)
  {
    const unsigned char c = static_cast<unsigned char>(key[i]);
    const unsigned char before = i > 0 ? static_cast<unsigned char>(key[i - 1]) : 0;
    if (std::isupper(c) && (std::islower(before) || std::isdigit(before)))
      label += ' ';
    label += static_cast<char>(c);
  }
  return label.empty() ? std::string(name) : label;
}

void InputPoll()
{
#ifdef SKIES_STANDALONE_AURORA
  // "Controller: <name>" when a controller takes port 1 (at start too, as the Dolphin build's)
  const char* name = PADGetName(PAD_CHAN0);
  const std::string now = name ? name : "";
  if (now != s_port1_name)
  {
    s_port1_name = now;
    if (!now.empty())
    {
      Log("[input] controller 1: %s\n", now.c_str());
      MenuMessage("Controller: " + InputDeviceLabel(now), 2500);
    }
    else
    {
      Log("[input] controller 1: none (keyboard)\n");
    }
  }
#endif
}
}  // namespace host
