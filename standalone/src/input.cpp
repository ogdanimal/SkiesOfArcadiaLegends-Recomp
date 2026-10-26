// Controllers and the keyboard through Aurora's PAD layer (its SDL pads, bindings and mapping
// store): what the in-game menu shows and changes about them. Kept apart from menu.cpp, whose
// GameCube button constants clash with Aurora's <dolphin/pad.h> macros.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

#include <array>
#include <cstdlib>
#include <cctype>
#include <chrono>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#ifdef SKIES_STANDALONE_AURORA
#include <SDL3/SDL.h>
#include <dolphin/pad.h>

#include "Core/SkiesBindings.h"

#include "gxruntime/aurora_backend.h"
#endif

namespace host
{
namespace
{
using Clock = std::chrono::steady_clock;

// The controller on port 1 as last seen ("" none)
std::string s_port1_name;
bool s_keyboard_applied = false;  // GCPadNew.ini's keyboard bindings are in Aurora's
// When the keyboard and controller 1 were last used (the menu's hints name what's in use)
Clock::time_point s_keyboard_used{};
Clock::time_point s_controller_used{};

#ifdef SKIES_STANDALONE_AURORA
// The mapping page's rows, in the Dolphin build's order (SkiesBindings::Inputs, without the
// hotkeys): a GameCube button (Aurora's button mapping), a stick direction (its axis mapping), or
// a trigger, which has both: the button for the click, the axis for the analog pull.
struct MapRow
{
  const char* label;
  u32 button;  // PAD_BUTTON_* / PAD_TRIGGER_*, 0 for a stick direction
  int axis;    // PADAxis, -1 for a button
};
constexpr std::array MAP_ROWS{
    MapRow{"A", PAD_BUTTON_A, -1},
    MapRow{"B", PAD_BUTTON_B, -1},
    MapRow{"X", PAD_BUTTON_X, -1},
    MapRow{"Y", PAD_BUTTON_Y, -1},
    MapRow{"Z", PAD_TRIGGER_Z, -1},
    MapRow{"Start", PAD_BUTTON_START, -1},
    MapRow{"L", PAD_TRIGGER_L, PAD_AXIS_TRIGGER_L},
    MapRow{"R", PAD_TRIGGER_R, PAD_AXIS_TRIGGER_R},
    MapRow{"Stick up", 0, PAD_AXIS_LEFT_Y_POS},
    MapRow{"Stick down", 0, PAD_AXIS_LEFT_Y_NEG},
    MapRow{"Stick left", 0, PAD_AXIS_LEFT_X_NEG},
    MapRow{"Stick right", 0, PAD_AXIS_LEFT_X_POS},
    MapRow{"C-stick up", 0, PAD_AXIS_RIGHT_Y_POS},
    MapRow{"C-stick down", 0, PAD_AXIS_RIGHT_Y_NEG},
    MapRow{"C-stick left", 0, PAD_AXIS_RIGHT_X_NEG},
    MapRow{"C-stick right", 0, PAD_AXIS_RIGHT_X_POS},
    MapRow{"D-pad up", PAD_BUTTON_UP, -1},
    MapRow{"D-pad down", PAD_BUTTON_DOWN, -1},
    MapRow{"D-pad left", PAD_BUTTON_LEFT, -1},
    MapRow{"D-pad right", PAD_BUTTON_RIGHT, -1},
};

// How long controller 1 must have been idle before a press on another controller takes it over
// (the Dolphin build's IDLE_BEFORE_SWITCH)
constexpr std::chrono::milliseconds IDLE_BEFORE_SWITCH{750};
// The controllers SDL has, as last logged
std::string s_pad_list;

// A capture gives up after this long without a press (the Dolphin build's CAPTURE_TIMEOUT)
constexpr std::chrono::milliseconds CAPTURE_TIMEOUT{6000};
// A stick or trigger counts as pulled past half way, and as let go under a quarter
constexpr int PULLED = 16384, LET_GO = 8000;

struct Capture
{
  int row = -1;  // -1: none running
  InputSlot slot = InputSlot::Controller;
  Clock::time_point until{};
  // An input counts only once it has been seen let go during the capture: the A that started it,
  // still held, doesn't bind itself
  std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> button_armed{};
  std::array<bool, SDL_GAMEPAD_AXIS_COUNT * 2> axis_armed{};
  std::vector<bool> key_armed;
};
Capture s_capture;

SDL_Gamepad* Port1Gamepad()
{
  const s32 index = PADGetIndexForPort(PAD_CHAN0);
  return index < 0 ? nullptr : PADGetSDLGamepadForIndex(static_cast<u32>(index));
}

bool PlayStation(SDL_Gamepad* pad)
{
  const SDL_GamepadType type = pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN;
  return type == SDL_GAMEPAD_TYPE_PS3 || type == SDL_GAMEPAD_TYPE_PS4 || type == SDL_GAMEPAD_TYPE_PS5;
}

// A controller button as printed on it where SDL knows (Cross, Circle on PlayStation pads; A, B on
// Xbox ones), else its place
std::string ButtonLabel(SDL_Gamepad* pad, u32 button)
{
  const bool ps = PlayStation(pad);
  const auto b = static_cast<SDL_GamepadButton>(button);
  switch (b)
  {
  case SDL_GAMEPAD_BUTTON_SOUTH:
  case SDL_GAMEPAD_BUTTON_EAST:
  case SDL_GAMEPAD_BUTTON_WEST:
  case SDL_GAMEPAD_BUTTON_NORTH:
    switch (pad ? SDL_GetGamepadButtonLabel(pad, b) : SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN)
    {
    case SDL_GAMEPAD_BUTTON_LABEL_A: return "A";
    case SDL_GAMEPAD_BUTTON_LABEL_B: return "B";
    case SDL_GAMEPAD_BUTTON_LABEL_X: return "X";
    case SDL_GAMEPAD_BUTTON_LABEL_Y: return "Y";
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS: return "Cross";
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE: return "Circle";
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE: return "Square";
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: return "Triangle";
    default:
      return b == SDL_GAMEPAD_BUTTON_SOUTH ? "Bottom face button" :
             b == SDL_GAMEPAD_BUTTON_EAST  ? "Right face button" :
             b == SDL_GAMEPAD_BUTTON_WEST  ? "Left face button" :
                                             "Top face button";
    }
  case SDL_GAMEPAD_BUTTON_BACK: return ps ? "Create / Share" : "Back";
  case SDL_GAMEPAD_BUTTON_GUIDE: return ps ? "PS" : "Guide";
  case SDL_GAMEPAD_BUTTON_START: return ps ? "Options" : "Start";
  case SDL_GAMEPAD_BUTTON_LEFT_STICK: return ps ? "L3" : "LS";
  case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return ps ? "R3" : "RS";
  case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return ps ? "L1" : "LB";
  case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return ps ? "R1" : "RB";
  case SDL_GAMEPAD_BUTTON_DPAD_UP: return "D-pad up";
  case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "D-pad down";
  case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "D-pad left";
  case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-pad right";
  case SDL_GAMEPAD_BUTTON_MISC1: return ps ? "Mute" : "Share";
  case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1: return "Right paddle 1";
  case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1: return "Left paddle 1";
  case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2: return "Right paddle 2";
  case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2: return "Left paddle 2";
  case SDL_GAMEPAD_BUTTON_TOUCHPAD: return "Touchpad";
  default:
  {
    const char* name = SDL_GetGamepadStringForButton(b);
    return name ? name : "Button " + std::to_string(button);
  }
  }
}

std::string AxisLabel(SDL_Gamepad* pad, PADSignedNativeAxis axis)
{
  const bool ps = PlayStation(pad);
  const bool pos = axis.sign == AXIS_SIGN_POSITIVE;
  switch (axis.nativeAxis)
  {
  case SDL_GAMEPAD_AXIS_LEFTX: return pos ? "Left stick right" : "Left stick left";
  case SDL_GAMEPAD_AXIS_LEFTY: return pos ? "Left stick down" : "Left stick up";
  case SDL_GAMEPAD_AXIS_RIGHTX: return pos ? "Right stick right" : "Right stick left";
  case SDL_GAMEPAD_AXIS_RIGHTY: return pos ? "Right stick down" : "Right stick up";
  case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: return ps ? "L2" : "LT";
  case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: return ps ? "R2" : "RT";
  default: return "Axis " + std::to_string(axis.nativeAxis) + (pos ? "+" : "-");
  }
}

std::string KeyLabel(s32 scancode)
{
  if (scancode == PAD_KEY_INVALID)
    return "-";
  if (scancode == SDL_SCANCODE_RETURN)
    return "Enter";
  const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(scancode));
  return name && *name ? std::string(name) : "Key " + std::to_string(scancode);
}

PADButtonMapping* FindButton(u32 button)
{
  u32 count = 0;
  PADButtonMapping* mappings = PADGetButtonMappings(PAD_CHAN0, &count);
  for (u32 i = 0; mappings && i < count; ++i)
  {
    if (mappings[i].padButton == button)
      return &mappings[i];
  }
  return nullptr;
}

PADAxisMapping* FindAxis(int axis)
{
  u32 count = 0;
  PADAxisMapping* mappings = PADGetAxisMappings(PAD_CHAN0, &count);
  for (u32 i = 0; mappings && i < count; ++i)
  {
    if (mappings[i].padAxis == axis)
      return &mappings[i];
  }
  return nullptr;
}

s32 KeyFor(const MapRow& row)
{
  u32 count = 0;
  if (row.button != 0)
  {
    const PADKeyButtonBinding* keys = PADGetKeyButtonBindings(PAD_CHAN0, &count);
    for (u32 i = 0; keys && i < count; ++i)
    {
      if (keys[i].padButton == row.button)
        return keys[i].scancode;
    }
    return PAD_KEY_INVALID;
  }
  const PADKeyAxisBinding* keys = PADGetKeyAxisBindings(PAD_CHAN0, &count);
  for (u32 i = 0; keys && i < count; ++i)
  {
    if (keys[i].padAxis == row.axis)
      return keys[i].scancode;
  }
  return PAD_KEY_INVALID;
}

// The keyboard's bindings are only read while active: turn them on (with the defaults) before
// the first change to a store that was never set
void EnsureKeyboardActive()
{
  u32 count = 0;
  if (PADGetKeyButtonBindings(PAD_CHAN0, &count) == nullptr)
    aurora_backend_pad_default_keys(PAD_CHAN0);
}

void SetController(const MapRow& row, PADSignedNativeAxis axis, u32 native_button)
{
  constexpr PADSignedNativeAxis NO_AXIS{-1, AXIS_SIGN_POSITIVE};
  const s32 axis_button = native_button == PAD_NATIVE_BUTTON_INVALID ? -1 : static_cast<s32>(native_button);
  if (row.axis < 0)
  {
    PADSetButtonMapping(PAD_CHAN0, {native_button, static_cast<PADButton>(row.button)});
    return;
  }
  if (row.button != 0)
  {
    // A trigger: a button clicks it and pulls it all the way; an axis pulls it, and the click comes
    // past Aurora's activation point (emulated triggers)
    PADSetButtonMapping(PAD_CHAN0, {axis.nativeAxis >= 0 ? PAD_NATIVE_BUTTON_INVALID : native_button,
                                    static_cast<PADButton>(row.button)});
  }
  PADSetAxisMapping(PAD_CHAN0, {axis.nativeAxis >= 0 ? axis : NO_AXIS, axis.nativeAxis >= 0 ? -1 : axis_button,
                                static_cast<PADAxis>(row.axis)});
}

void SetKey(const MapRow& row, s32 scancode)
{
  EnsureKeyboardActive();
  if (row.button != 0)
    PADSetKeyButtonBinding(PAD_CHAN0, {scancode, static_cast<PADButton>(row.button)});
  if (row.axis >= 0)
    PADSetKeyAxisBinding(PAD_CHAN0, {scancode, static_cast<PADAxis>(row.axis), 32767});
}

// What the capture saw: a new press, and whether to stop
enum class Seen
{
  Nothing,
  Bound,
  Cancelled,
};

Seen CaptureController(const MapRow& row)
{
  SDL_Gamepad* pad = Port1Gamepad();
  if (!pad)
    return Seen::Cancelled;
  for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b)
  {
    const bool down = SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(b));
    if (!down)
    {
      s_capture.button_armed[b] = true;
      continue;
    }
    if (!s_capture.button_armed[b])
      continue;
    // Back / Guide open the menu: they cancel instead
    if (b == SDL_GAMEPAD_BUTTON_BACK || b == SDL_GAMEPAD_BUTTON_GUIDE)
      return Seen::Cancelled;
    SetController(row, {-1, AXIS_SIGN_POSITIVE}, static_cast<u32>(b));
    Log("[input] %s (controller) bound to %s\n", row.label, ButtonLabel(pad, b).c_str());
    return Seen::Bound;
  }
  // A button's binding takes buttons only (Aurora maps GameCube buttons to buttons)
  if (row.axis < 0)
    return Seen::Nothing;
  for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; ++a)
  {
    const int value = SDL_GetGamepadAxis(pad, static_cast<SDL_GamepadAxis>(a));
    const bool trigger = a == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || a == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
    for (int side = 0; side < 2; ++side)
    {
      if (trigger && side == 1)
        continue;
      const int amount = side == 0 ? value : -value;
      bool& armed = s_capture.axis_armed[a * 2 + side];
      if (amount < LET_GO)
      {
        armed = true;
        continue;
      }
      if (amount < PULLED || !armed)
        continue;
      const PADSignedNativeAxis axis{a, side == 0 ? AXIS_SIGN_POSITIVE : AXIS_SIGN_NEGATIVE};
      SetController(row, axis, PAD_NATIVE_BUTTON_INVALID);
      Log("[input] %s (controller) bound to %s\n", row.label, AxisLabel(pad, axis).c_str());
      return Seen::Bound;
    }
  }
  return Seen::Nothing;
}

Seen CaptureKeyboard(const MapRow& row)
{
  int count = 0;
  const bool* keys = SDL_GetKeyboardState(&count);
  if (!keys)
    return Seen::Nothing;
  if (static_cast<int>(s_capture.key_armed.size()) < count)
    s_capture.key_armed.resize(count, false);
  for (int k = SDL_SCANCODE_A; k < count; ++k)
  {
    if (!keys[k])
    {
      s_capture.key_armed[k] = true;
      continue;
    }
    if (!s_capture.key_armed[k])
      continue;
    // Esc opens the menu: it cancels instead
    if (k == SDL_SCANCODE_ESCAPE)
      return Seen::Cancelled;
    SetKey(row, k);
    Log("[input] %s (keyboard) bound to %s\n", row.label, KeyLabel(k).c_str());
    return Seen::Bound;
  }
  return Seen::Nothing;
}

// ---- GCPadNew.ini ----------------------------------------------------------------------------
// Controller 1's bindings are the Dolphin build's: its GCPadNew.ini ([GCPad1], SkiesBindings'
// expressions), which the launchers' Controls pages write and the mapping page here writes too, so
// an update from the Dolphin build keeps a player's bindings and both sides show the same. What the
// file says goes into Aurora's mapping: the keyboard's at the first read, a controller's each time
// one takes port 1 (Aurora keeps a mapping per controller). A file without [GCPad1] (a data folder
// the launcher never opened) leaves Aurora's own.

// Dolphin's SDL gamepad controls (InputCommon's SDLGamepad.h names; the face buttons also by their
// positional Xbox names, which its profiles use)
struct PadControl
{
  const char* name;
  int button;  // SDL_GamepadButton, -1 for an axis
  int axis;    // SDL_GamepadAxis
  PADAxisSign sign;
};
constexpr std::array PAD_CONTROLS{
    PadControl{"Button A", SDL_GAMEPAD_BUTTON_SOUTH, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Button B", SDL_GAMEPAD_BUTTON_EAST, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Button X", SDL_GAMEPAD_BUTTON_WEST, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Button Y", SDL_GAMEPAD_BUTTON_NORTH, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Button S", SDL_GAMEPAD_BUTTON_SOUTH, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Button E", SDL_GAMEPAD_BUTTON_EAST, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Button W", SDL_GAMEPAD_BUTTON_WEST, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Button N", SDL_GAMEPAD_BUTTON_NORTH, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Back", SDL_GAMEPAD_BUTTON_BACK, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Guide", SDL_GAMEPAD_BUTTON_GUIDE, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Start", SDL_GAMEPAD_BUTTON_START, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Thumb L", SDL_GAMEPAD_BUTTON_LEFT_STICK, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Thumb R", SDL_GAMEPAD_BUTTON_RIGHT_STICK, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Shoulder L", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Shoulder R", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Pad N", SDL_GAMEPAD_BUTTON_DPAD_UP, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Pad S", SDL_GAMEPAD_BUTTON_DPAD_DOWN, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Pad W", SDL_GAMEPAD_BUTTON_DPAD_LEFT, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Pad E", SDL_GAMEPAD_BUTTON_DPAD_RIGHT, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Misc 1", SDL_GAMEPAD_BUTTON_MISC1, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Paddle 1", SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Paddle 2", SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Paddle 3", SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Paddle 4", SDL_GAMEPAD_BUTTON_LEFT_PADDLE2, -1, AXIS_SIGN_POSITIVE},
    PadControl{"Touchpad", SDL_GAMEPAD_BUTTON_TOUCHPAD, -1, AXIS_SIGN_POSITIVE},
    // Dolphin's vertical axes are XInput's: Y+ is up, SDL's negative
    PadControl{"Left X+", -1, SDL_GAMEPAD_AXIS_LEFTX, AXIS_SIGN_POSITIVE},
    PadControl{"Left X-", -1, SDL_GAMEPAD_AXIS_LEFTX, AXIS_SIGN_NEGATIVE},
    PadControl{"Left Y+", -1, SDL_GAMEPAD_AXIS_LEFTY, AXIS_SIGN_NEGATIVE},
    PadControl{"Left Y-", -1, SDL_GAMEPAD_AXIS_LEFTY, AXIS_SIGN_POSITIVE},
    PadControl{"Right X+", -1, SDL_GAMEPAD_AXIS_RIGHTX, AXIS_SIGN_POSITIVE},
    PadControl{"Right X-", -1, SDL_GAMEPAD_AXIS_RIGHTX, AXIS_SIGN_NEGATIVE},
    PadControl{"Right Y+", -1, SDL_GAMEPAD_AXIS_RIGHTY, AXIS_SIGN_NEGATIVE},
    PadControl{"Right Y-", -1, SDL_GAMEPAD_AXIS_RIGHTY, AXIS_SIGN_POSITIVE},
    PadControl{"Trigger L", -1, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, AXIS_SIGN_POSITIVE},
    PadControl{"Trigger R", -1, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, AXIS_SIGN_POSITIVE},
};

const PadControl* FindPadControl(std::string_view name)
{
  for (const PadControl& control : PAD_CONTROLS)
  {
    if (name == control.name)
      return &control;
  }
  return nullptr;
}

// The name a binding is written with: the face buttons by their Xbox names, as the defaults
std::string PadControlName(PADSignedNativeAxis axis, u32 button)
{
  for (const PadControl& control : PAD_CONTROLS)
  {
    if (axis.nativeAxis >= 0 ? control.axis == axis.nativeAxis && control.sign == axis.sign :
                               control.button >= 0 && static_cast<u32>(control.button) == button)
      return control.name;
  }
  return {};
}

bool PadFilePresent()
{
  return !SettingGetString(SettingFile::Pad, SkiesBindings::PAD_SECTION, "Buttons/A").empty();
}

std::string PadExpression(const char* key)
{
  return SettingGetString(SettingFile::Pad, SkiesBindings::PAD_SECTION, key);
}

// A row's control in the file for the slot ("" unbound); a trigger's controller binding is its
// analog half when that has one, else the button
std::string PadFileControl(int row, InputSlot slot, std::string* device = nullptr)
{
  const SkiesBindings::Input& input = SkiesBindings::Inputs()[row];
  const SkiesBindings::Slot s =
      slot == InputSlot::Keyboard ? SkiesBindings::Slot::Keyboard : SkiesBindings::Slot::Controller;
  if (slot == InputSlot::Controller && input.analog_key)
  {
    const std::string analog = SkiesBindings::SlotControl(PadExpression(input.analog_key), s);
    if (!analog.empty())
      return analog;
  }
  return SkiesBindings::SlotControl(PadExpression(input.key), s, device);
}

// A key the file names as any of Dolphin's keyboard devices calls it (DirectInput, X11, Android,
// macOS), as SDL's scancode
s32 PadFileScancode(std::string_view control)
{
  const auto same = [control](const char* name) {
    if (!name || !*name || std::strlen(name) != control.size())
      return false;
    for (std::size_t i = 0; i < control.size(); ++i)
    {
      if (std::tolower(static_cast<unsigned char>(name[i])) !=
          std::tolower(static_cast<unsigned char>(control[i])))
        return false;
    }
    return true;
  };
  for (const SkiesBindings::KeyName& key : SkiesBindings::KeyNames())
  {
    if (same(key.dinput) || same(key.x11) || same(key.android) || same(key.quartz))
    {
      const SDL_Scancode scancode = SDL_GetScancodeFromName(key.id);
      return scancode == SDL_SCANCODE_UNKNOWN ? PAD_KEY_INVALID : static_cast<s32>(scancode);
    }
  }
  return PAD_KEY_INVALID;
}

void ApplyPadFileController()
{
  if (!PadFilePresent() || Port1Gamepad() == nullptr)
    return;
  // Aurora loads the controller's saved mapping at its first read: before ours, not over it
  u32 count = 0;
  PADGetButtonMappings(PAD_CHAN0, &count);
  PADGetAxisMappings(PAD_CHAN0, &count);
  for (int row = 0; row < static_cast<int>(MAP_ROWS.size()); ++row)
  {
    const PadControl* control = FindPadControl(PadFileControl(row, InputSlot::Controller));
    if (!control)
    {
      // Unbound, or a control Aurora has no name for: unbound here too (not A and B, as the page)
      if (row > 1)
        SetController(MAP_ROWS[row], {-1, AXIS_SIGN_POSITIVE}, PAD_NATIVE_BUTTON_INVALID);
      continue;
    }
    if (control->button >= 0)
      SetController(MAP_ROWS[row], {-1, AXIS_SIGN_POSITIVE}, static_cast<u32>(control->button));
    else if (MAP_ROWS[row].axis >= 0)
      SetController(MAP_ROWS[row], {control->axis, control->sign}, PAD_NATIVE_BUTTON_INVALID);
  }
  Log("[input] controller bindings from GCPadNew.ini\n");
}

void ApplyPadFileKeyboard()
{
  if (!PadFilePresent())
    return;
  std::string keys;
  for (int row = 0; row < static_cast<int>(MAP_ROWS.size()); ++row)
  {
    SetKey(MAP_ROWS[row], PadFileScancode(PadFileControl(row, InputSlot::Keyboard)));
    keys += std::string(keys.empty() ? "" : ", ") + MAP_ROWS[row].label + " " + KeyLabel(KeyFor(MAP_ROWS[row]));
  }
  Log("[input] keyboard bindings from GCPadNew.ini: %s\n", keys.c_str());
}

// The row's binding as Aurora now has it, back into the file
void WritePadFile(int row, InputSlot slot)
{
  if (SettingsConfigDir().empty())
    return;
  const SkiesBindings::Input& input = SkiesBindings::Inputs()[row];
  const MapRow& r = MAP_ROWS[row];
  const auto set = [](const char* key, const std::string& expression) {
    SettingSetString(SettingFile::Pad, SkiesBindings::PAD_SECTION, key, expression);
  };
  if (slot == InputSlot::Keyboard)
  {
    std::string device;
    SkiesBindings::SlotControl(PadExpression(input.key), SkiesBindings::Slot::Keyboard, &device);
    if (device.empty())
      device = SkiesBindings::DefaultKeyboardDevice();
    const s32 scancode = KeyFor(r);
    const SkiesBindings::KeyName* key =
        scancode == PAD_KEY_INVALID ? nullptr :
                                      SkiesBindings::FindKey(SDL_GetScancodeName(static_cast<SDL_Scancode>(scancode)));
    const std::string control = key ? SkiesBindings::PlatformKeyName(*key) : std::string();
    set(input.key, SkiesBindings::SetSlot(PadExpression(input.key), SkiesBindings::Slot::Keyboard,
                                          control.empty() ? "" : device, control));
  }
  else
  {
    PADSignedNativeAxis axis{-1, AXIS_SIGN_POSITIVE};
    u32 button = PAD_NATIVE_BUTTON_INVALID;
    if (r.button != 0)
      if (const PADButtonMapping* b = FindButton(r.button))
        button = b->nativeButton;
    if (r.axis >= 0)
      if (const PADAxisMapping* a = FindAxis(r.axis))
      {
        if (a->nativeAxis.nativeAxis >= 0)
          axis = a->nativeAxis;
        else if (a->nativeButton >= 0)
          button = static_cast<u32>(a->nativeButton);
      }
    const std::string control = PadControlName(axis, button);
    set(input.key,
        SkiesBindings::SetSlot(PadExpression(input.key), SkiesBindings::Slot::Controller, "", control));
    // A trigger: its analog half too when the control is an axis, else only the button
    if (input.analog_key)
      set(input.analog_key, SkiesBindings::SetSlot(PadExpression(input.analog_key),
                                                   SkiesBindings::Slot::Controller, "",
                                                   axis.nativeAxis >= 0 ? control : ""));
  }
  SettingsSave();
}

bool AnyButton(SDL_Gamepad* pad)
{
  for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b)
  {
    if (SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(b)))
      return true;
  }
  return false;
}

// Controller 1 follows the controller in use (the Dolphin build's SkiesInput::Follow,
// android-pad-follow): with no controller on port 1, the first there is (or the one pressed); a
// button on another controller while controller 1 has been idle IDLE_BEFORE_SWITCH, that one. Buttons
// only for the others: an axis may rest away from 0 on some Android pads. Aurora keeps the choice
// (its port preferences, USER/Config/Aurora/controller_ports.dat).
void Follow(Clock::time_point now)
{
  const u32 count = PADCount();
  std::string list;
  for (u32 i = 0; i < count; ++i)
  {
    const char* name = PADGetNameForControllerIndex(i);
    list += (list.empty() ? "" : ", ") + std::string(name ? name : "?");
  }
  if (list != s_pad_list)
  {
    s_pad_list = list;
    Log("[input] controllers: %s\n", list.empty() ? "none" : list.c_str());
  }
  SDL_Gamepad* current = Port1Gamepad();
  s32 pressed = -1, first = -1;
  for (u32 i = 0; i < count; ++i)
  {
    SDL_Gamepad* pad = PADGetSDLGamepadForIndex(i);
    if (!pad || pad == current)
      continue;
    if (first < 0)
      first = static_cast<s32>(i);
    if (pressed < 0 && AnyButton(pad))
      pressed = static_cast<s32>(i);
  }
  const bool take = current ? pressed >= 0 && now - s_controller_used > IDLE_BEFORE_SWITCH :
                              pressed >= 0 || first >= 0;
  if (!take)
    return;
  PADSetPortForIndex(static_cast<u32>(pressed >= 0 ? pressed : first), PAD_CHAN0);
  s_controller_used = now;
  // The "Controller: <name>" message follows from InputPoll's name check
}
#endif
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
  // Aurora's own stick dead zone (8000 of 32767 by default, a hard cut on each axis) off: the
  // Controls tab's Stick dead zone (menu.cpp, radial and rescaled as Dolphin's) is the only one, so
  // Off is off as in the Dolphin build. A controller's saved mapping can bring it back when it
  // loads (in PADRead, just before this), hence every read.
  for (u32 port = 0; port < 4; ++port)
  {
    if (PADDeadZones* zones = PADGetDeadZones(port))
      zones->useDeadzones = false;
  }
  // Which of the keyboard and controller 1 was used last
  const Clock::time_point now = Clock::now();
  int key_count = 0;
  if (const bool* keys = SDL_GetKeyboardState(&key_count))
  {
    for (int k = SDL_SCANCODE_A; k < key_count; ++k)
    {
      if (keys[k])
      {
        s_keyboard_used = now;
        break;
      }
    }
  }
  if (!s_keyboard_applied)
  {
    s_keyboard_applied = true;
    ApplyPadFileKeyboard();
  }
  if (s_capture.row < 0)
    Follow(now);
  if (SDL_Gamepad* pad = Port1Gamepad())
  {
    bool used = false;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT && !used; ++b)
      used = SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(b));
    for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT && !used; ++a)
      used = std::abs(static_cast<int>(SDL_GetGamepadAxis(pad, static_cast<SDL_GamepadAxis>(a)))) >= PULLED;
    if (used)
      s_controller_used = now;
  }
  // "Controller: <name>" when a controller takes port 1 (at start too, as the Dolphin build's)
  const char* name = PADGetName(PAD_CHAN0);
  const std::string current = name ? name : "";
  if (current != s_port1_name)
  {
    s_port1_name = current;
    if (!current.empty())
    {
      Log("[input] controller 1: %s\n", current.c_str());
      MenuMessage("Controller: " + InputDeviceLabel(current), 2500);
      ApplyPadFileController();
    }
    else
    {
      Log("[input] controller 1: none (keyboard)\n");
    }
  }
#endif
}

int InputMappingRows()
{
#ifdef SKIES_STANDALONE_AURORA
  return static_cast<int>(MAP_ROWS.size());
#else
  return 0;
#endif
}

const char* InputMappingName(int row)
{
#ifdef SKIES_STANDALONE_AURORA
  if (row >= 0 && row < InputMappingRows())
    return MAP_ROWS[row].label;
#endif
  (void)row;
  return "";
}

std::string InputBindingLabel(int row, InputSlot slot)
{
#ifdef SKIES_STANDALONE_AURORA
  if (row < 0 || row >= InputMappingRows())
    return {};
  const MapRow& r = MAP_ROWS[row];
  if (slot == InputSlot::Keyboard)
    return KeyLabel(KeyFor(r));
  SDL_Gamepad* pad = Port1Gamepad();
  if (!pad)
    return "-";
  if (r.button != 0)
  {
    const PADButtonMapping* b = FindButton(r.button);
    if (b && b->nativeButton != PAD_NATIVE_BUTTON_INVALID)
      return ButtonLabel(pad, b->nativeButton);
  }
  if (r.axis >= 0)
  {
    const PADAxisMapping* a = FindAxis(r.axis);
    if (a && a->nativeAxis.nativeAxis >= 0)
      return AxisLabel(pad, a->nativeAxis);
    if (a && a->nativeButton >= 0)
      return ButtonLabel(pad, static_cast<u32>(a->nativeButton));
  }
  return "-";
#else
  (void)row;
  (void)slot;
  return "-";
#endif
}

bool InputClearable(int row, InputSlot slot)
{
  if (row < 0 || row >= InputMappingRows())
    return false;
  // Not the controller's A and B, without which a controller (on Android often the only input)
  // could no longer work the mapping page
  return slot == InputSlot::Keyboard || row > 1;
}

bool InputStartCapture(int row, InputSlot slot)
{
#ifdef SKIES_STANDALONE_AURORA
  if (row < 0 || row >= InputMappingRows())
    return false;
  if (slot == InputSlot::Keyboard ? !InputHasKeyboard() : Port1Gamepad() == nullptr)
  {
    MenuMessage(slot == InputSlot::Keyboard ? "No keyboard connected" : "No controller connected", 2500);
    return false;
  }
  s_capture = {};
  s_capture.row = row;
  s_capture.slot = slot;
  s_capture.until = Clock::now() + CAPTURE_TIMEOUT;
  return true;
#else
  (void)row;
  (void)slot;
  return false;
#endif
}

bool InputPollCapture()
{
#ifdef SKIES_STANDALONE_AURORA
  if (s_capture.row < 0)
    return false;
  const MapRow& row = MAP_ROWS[s_capture.row];
  const Seen seen = s_capture.slot == InputSlot::Keyboard ? CaptureKeyboard(row) : CaptureController(row);
  if (seen == Seen::Nothing && Clock::now() < s_capture.until)
    return true;
  // Bound, cancelled or timed out (nothing changes)
  if (seen == Seen::Bound)
  {
    WritePadFile(s_capture.row, s_capture.slot);
    PADSerializeMappings();
  }
  s_capture.row = -1;
  return false;
#else
  return false;
#endif
}

void InputClear(int row, InputSlot slot)
{
#ifdef SKIES_STANDALONE_AURORA
  if (!InputClearable(row, slot))
    return;
  const MapRow& r = MAP_ROWS[row];
  if (slot == InputSlot::Keyboard)
  {
    SetKey(r, PAD_KEY_INVALID);
  }
  else
  {
    if (Port1Gamepad() == nullptr)
      return;
    SetController(r, {-1, AXIS_SIGN_POSITIVE}, PAD_NATIVE_BUTTON_INVALID);
  }
  WritePadFile(row, slot);
  PADSerializeMappings();
  Log("[input] %s (%s) cleared\n", r.label, slot == InputSlot::Keyboard ? "keyboard" : "controller");
#else
  (void)row;
  (void)slot;
#endif
}

bool InputResetDefaults(InputSlot slot)
{
#ifdef SKIES_STANDALONE_AURORA
  if (slot == InputSlot::Keyboard)
  {
    // Nothing changes without a keyboard (review D23)
    if (!InputHasKeyboard())
    {
      MenuMessage("No keyboard connected", 2500);
      return false;
    }
    aurora_backend_pad_default_keys(PAD_CHAN0);
  }
  else
  {
    if (Port1Gamepad() == nullptr)
    {
      MenuMessage("No controller connected", 2500);
      return false;
    }
    PADRestoreDefaultMapping(PAD_CHAN0);
  }
  for (int row = 0; row < InputMappingRows(); ++row)
    WritePadFile(row, slot);
  PADSerializeMappings();
  Log("[input] %s defaults restored\n", slot == InputSlot::Keyboard ? "keyboard" : "controller");
  return true;
#else
  (void)slot;
  return false;
#endif
}

bool InputKeyboardInUse()
{
  return s_keyboard_used > s_controller_used;
}

bool InputHasKeyboard()
{
#ifdef SKIES_STANDALONE_AURORA
  return SDL_HasKeyboard();
#else
  return false;
#endif
}
}  // namespace host
