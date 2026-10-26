// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

// Controller 1's bindings for the Skies of Arcadia Legends recomp, as the launchers and the in-game
// menu show them: every GameCube input has a controller binding and a keyboard binding, both kept
// in its expression in GCPadNew.ini ([GCPad1]). The controller's are unqualified (they follow
// controller 1's device, which Core/SkiesInput.cpp moves to the controller in use); the keyboard's
// name the keyboard device ("`DInput/0/Keyboard Mouse:SPACE`"). Anything else in an expression (a
// hand-written combination) is kept as it is. The two hotkeys (the settings menu, Invincible) aren't
// GameCube inputs and live in [SkiesHotkeys] of the same file. Plain strings only, so the desktop
// launcher (which has no input devices of Dolphin's) shares it with the runtime.
namespace SkiesBindings
{
enum class Slot
{
  Controller,
  Keyboard,
};

struct Input
{
  const char* section;  // "GCPad1" or HOTKEY_SECTION
  const char* key;      // "Buttons/A"
  const char* label;    // "A"
  // The analog half of a trigger: a controller binding goes here too (the keyboard's only to the
  // digital key, which presses the trigger all the way)
  const char* analog_key;
  const char* keyboard;    // the default key, KeyNames().id
  // The default controls on this platform's controllers (several: "|"-separated), and for a
  // trigger the analog half's
  const char* controller;
  const char* controller_analog;
};

inline constexpr const char* PAD_SECTION = "GCPad1";
inline constexpr const char* HOTKEY_SECTION = "SkiesHotkeys";
// [SkiesHotkeys] Version: the defaults a profile has had added (KeyboardDefaults below)
inline constexpr int DEFAULTS_VERSION = 1;

// In the order they're shown
std::span<const Input> Inputs();
const Input* FindInput(std::string_view key);

// A key: its id (SDL's scancode name, which the desktop launcher reads keys by), what the keyboard
// devices of Dolphin's call it on Windows (DirectInput), Linux (X11 keysyms, letters upper case) and
// Android (KeyEvent names), and how it's shown.
struct KeyName
{
  const char* id;
  const char* dinput;
  const char* x11;
  const char* android;
  const char* label;
  const char* quartz;  // macOS ("" for a key a Mac keyboard doesn't have)
};
std::span<const KeyName> KeyNames();
const KeyName* FindKey(std::string_view id);
// This platform's name for a key ("" if it has none)
std::string PlatformKeyName(const KeyName& key);

// The keyboard device the launcher writes before the game has seen one (the runtime moves the
// bindings to the keyboard it finds, Core/SkiesInput.cpp)
std::string DefaultKeyboardDevice();
// Sources of keyboard devices: DInput, XInput2, Quartz (and Android keyboards, found by their keys)
bool IsKeyboardSource(std::string_view source);
// How a controller's name is shown. Steam names its virtual gamepads after a translation key it
// leaves untranslated ("#SettingsController_SteamController"): that reads "Steam Controller".
std::string DeviceLabel(std::string_view name);

// One part of an expression, split at its top-level "|"
struct Term
{
  std::string text;     // as written
  bool simple = false;  // a single control: `name` or `device:name`
  std::string device;   // "" = controller 1's device
  std::string control;
};
std::vector<Term> SplitExpression(std::string_view expression);
std::string JoinTerms(const std::vector<Term>& terms);
// The term of a control, quoted as Dolphin writes it
std::string ControlTerm(std::string_view device, std::string_view control);

// Which slot a term belongs to: unqualified = the controller, a keyboard device = the keyboard,
// anything else = kept as written (returns false)
bool TermSlot(const Term& term, Slot* slot);
// The control of a slot in an expression ("" = unbound), the first if there are several
std::string SlotControl(std::string_view expression, Slot slot, std::string* device = nullptr);
// The expression with the slot's terms replaced by one (control "" = unbound)
std::string SetSlot(std::string_view expression, Slot slot, std::string_view device,
                    std::string_view control);

// How a control is shown: "Space", "Left Shift", "A", "RB", "Left stick up"
std::string ControlLabel(std::string_view control);
}  // namespace SkiesBindings
