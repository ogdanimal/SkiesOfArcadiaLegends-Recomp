// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/SkiesBindings.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace SkiesBindings
{
namespace
{
// This platform's controller names: SDL's gamepad controls on desktop (InputCommon's SDL backend,
// what the launcher's generated profile uses), Android's keys and axes (jni/SkiesImport.cpp's
// ensurePadProfile)
#ifdef __ANDROID__
#define PAD(sdl, android) android
#else
#define PAD(sdl, android) sdl
#endif

// The keyboard layout: the left hand moves (W A S D) and holds Z (Tab) for the camera, the right
// hand has the C-stick (I J K L) and the arrow keys (D-pad, menus); Space / Left Shift are A / B.
constexpr std::array INPUTS{
    Input{PAD_SECTION, "Buttons/A", "A", nullptr, "Space", PAD("Button A", "Button A"), nullptr},
    Input{PAD_SECTION, "Buttons/B", "B", nullptr, "Left Shift", PAD("Button B", "Button B"), nullptr},
    Input{PAD_SECTION, "Buttons/X", "X", nullptr, "F", PAD("Button X", "Button X"), nullptr},
    Input{PAD_SECTION, "Buttons/Y", "Y", nullptr, "R", PAD("Button Y", "Button Y"), nullptr},
    Input{PAD_SECTION, "Buttons/Z", "Z", nullptr, "Tab", PAD("Shoulder R", "Button R1"), nullptr},
    Input{PAD_SECTION, "Buttons/Start", "Start", nullptr, "Return", PAD("Start", "Start"), nullptr},
    Input{PAD_SECTION, "Triggers/L", "L", "Triggers/L-Analog", "Q", PAD("Trigger L", "Button L2"),
          PAD("Trigger L", "Axis 23+")},
    Input{PAD_SECTION, "Triggers/R", "R", "Triggers/R-Analog", "E", PAD("Trigger R", "Button R2"),
          PAD("Trigger R", "Axis 22+")},
    Input{PAD_SECTION, "Main Stick/Up", "Stick up", nullptr, "W", PAD("Left Y+", "Axis 1-"), nullptr},
    Input{PAD_SECTION, "Main Stick/Down", "Stick down", nullptr, "S", PAD("Left Y-", "Axis 1+"),
          nullptr},
    Input{PAD_SECTION, "Main Stick/Left", "Stick left", nullptr, "A", PAD("Left X-", "Axis 0-"),
          nullptr},
    Input{PAD_SECTION, "Main Stick/Right", "Stick right", nullptr, "D", PAD("Left X+", "Axis 0+"),
          nullptr},
    Input{PAD_SECTION, "C-Stick/Up", "C-stick up", nullptr, "I", PAD("Right Y+", "Axis 14-"),
          nullptr},
    Input{PAD_SECTION, "C-Stick/Down", "C-stick down", nullptr, "K", PAD("Right Y-", "Axis 14+"),
          nullptr},
    Input{PAD_SECTION, "C-Stick/Left", "C-stick left", nullptr, "J", PAD("Right X-", "Axis 11-"),
          nullptr},
    Input{PAD_SECTION, "C-Stick/Right", "C-stick right", nullptr, "L", PAD("Right X+", "Axis 11+"),
          nullptr},
    Input{PAD_SECTION, "D-Pad/Up", "D-pad up", nullptr, "Up", PAD("Pad N", "Axis 16-"), nullptr},
    Input{PAD_SECTION, "D-Pad/Down", "D-pad down", nullptr, "Down", PAD("Pad S", "Axis 16+"), nullptr},
    Input{PAD_SECTION, "D-Pad/Left", "D-pad left", nullptr, "Left", PAD("Pad W", "Axis 15-"), nullptr},
    Input{PAD_SECTION, "D-Pad/Right", "D-pad right", nullptr, "Right", PAD("Pad E", "Axis 15+"),
          nullptr},
    Input{HOTKEY_SECTION, "Menu", "Settings menu", nullptr, "Escape", PAD("Back", "Select"), nullptr},
    Input{HOTKEY_SECTION, "Invincible", "Invincible on / off", nullptr, "",
          PAD("Thumb L|Thumb R", "Button L3|Button R3"), nullptr},
};
#undef PAD

constexpr std::array KEYS{
    KeyName{"A", "A", "A", "A", "A", "A"},
    KeyName{"B", "B", "B", "B", "B", "B"},
    KeyName{"C", "C", "C", "C", "C", "C"},
    KeyName{"D", "D", "D", "D", "D", "D"},
    KeyName{"E", "E", "E", "E", "E", "E"},
    KeyName{"F", "F", "F", "F", "F", "F"},
    KeyName{"G", "G", "G", "G", "G", "G"},
    KeyName{"H", "H", "H", "H", "H", "H"},
    KeyName{"I", "I", "I", "I", "I", "I"},
    KeyName{"J", "J", "J", "J", "J", "J"},
    KeyName{"K", "K", "K", "K", "K", "K"},
    KeyName{"L", "L", "L", "L", "L", "L"},
    KeyName{"M", "M", "M", "M", "M", "M"},
    KeyName{"N", "N", "N", "N", "N", "N"},
    KeyName{"O", "O", "O", "O", "O", "O"},
    KeyName{"P", "P", "P", "P", "P", "P"},
    KeyName{"Q", "Q", "Q", "Q", "Q", "Q"},
    KeyName{"R", "R", "R", "R", "R", "R"},
    KeyName{"S", "S", "S", "S", "S", "S"},
    KeyName{"T", "T", "T", "T", "T", "T"},
    KeyName{"U", "U", "U", "U", "U", "U"},
    KeyName{"V", "V", "V", "V", "V", "V"},
    KeyName{"W", "W", "W", "W", "W", "W"},
    KeyName{"X", "X", "X", "X", "X", "X"},
    KeyName{"Y", "Y", "Y", "Y", "Y", "Y"},
    KeyName{"Z", "Z", "Z", "Z", "Z", "Z"},
    KeyName{"1", "1", "1", "1", "1", "1"},
    KeyName{"2", "2", "2", "2", "2", "2"},
    KeyName{"3", "3", "3", "3", "3", "3"},
    KeyName{"4", "4", "4", "4", "4", "4"},
    KeyName{"5", "5", "5", "5", "5", "5"},
    KeyName{"6", "6", "6", "6", "6", "6"},
    KeyName{"7", "7", "7", "7", "7", "7"},
    KeyName{"8", "8", "8", "8", "8", "8"},
    KeyName{"9", "9", "9", "9", "9", "9"},
    KeyName{"0", "0", "0", "0", "0", "0"},
    KeyName{"Return", "RETURN", "Return", "Enter", "Enter", "Return"},
    KeyName{"Escape", "ESCAPE", "Escape", "Escape", "Esc", "Escape"},
    KeyName{"Backspace", "BACK", "BackSpace", "Backspace", "Backspace", "Backspace"},
    KeyName{"Tab", "TAB", "Tab", "Tab", "Tab", "Tab"},
    KeyName{"Space", "SPACE", "space", "Space", "Space", "Space"},
    KeyName{"-", "MINUS", "minus", "Minus", "-", "-"},
    KeyName{"=", "EQUALS", "equal", "Equals", "=", "="},
    KeyName{"[", "LBRACKET", "bracketleft", "Left Bracket", "[", "["},
    KeyName{"]", "RBRACKET", "bracketright", "Right Bracket", "]", "]"},
    KeyName{"\\", "BACKSLASH", "backslash", "Backslash", "\\", "\\"},
    KeyName{";", "SEMICOLON", "semicolon", "Semicolon", ";", ";"},
    KeyName{"'", "APOSTROPHE", "apostrophe", "Apostrophe", "'", "'"},
    KeyName{"`", "GRAVE", "grave", "Grave", "`", "Tilde"},
    KeyName{",", "COMMA", "comma", "Comma", ",", ","},
    KeyName{".", "PERIOD", "period", "Period", ".", "."},
    KeyName{"/", "SLASH", "slash", "Slash", "/", "/"},
    KeyName{"CapsLock", "CAPITAL", "Caps_Lock", "Caps Lock", "Caps Lock", "Caps Lock"},
    KeyName{"F1", "F1", "F1", "F1", "F1", "F1"},
    KeyName{"F2", "F2", "F2", "F2", "F2", "F2"},
    KeyName{"F3", "F3", "F3", "F3", "F3", "F3"},
    KeyName{"F4", "F4", "F4", "F4", "F4", "F4"},
    KeyName{"F5", "F5", "F5", "F5", "F5", "F5"},
    KeyName{"F6", "F6", "F6", "F6", "F6", "F6"},
    KeyName{"F7", "F7", "F7", "F7", "F7", "F7"},
    KeyName{"F8", "F8", "F8", "F8", "F8", "F8"},
    KeyName{"F9", "F9", "F9", "F9", "F9", "F9"},
    KeyName{"F10", "F10", "F10", "F10", "F10", "F10"},
    KeyName{"F11", "F11", "F11", "F11", "F11", "F11"},
    KeyName{"F12", "F12", "F12", "F12", "F12", "F12"},
    KeyName{"Insert", "INSERT", "Insert", "Insert", "Insert", ""},
    KeyName{"Home", "HOME", "Home", "Move Home", "Home", "Home"},
    KeyName{"PageUp", "PRIOR", "Prior", "Page Up", "Page Up", "Page Up"},
    KeyName{"Delete", "DELETE", "Delete", "Delete", "Delete", "Delete"},
    KeyName{"End", "END", "End", "Move End", "End", "End"},
    KeyName{"PageDown", "NEXT", "Next", "Page Down", "Page Down", "Page Down"},
    KeyName{"Right", "RIGHT", "Right", "Right", "Right", "Right Arrow"},
    KeyName{"Left", "LEFT", "Left", "Left", "Left", "Left Arrow"},
    KeyName{"Down", "DOWN", "Down", "Down", "Down", "Down Arrow"},
    KeyName{"Up", "UP", "Up", "Up", "Up", "Up Arrow"},
    KeyName{"Keypad /", "DIVIDE", "KP_Divide", "Numpad Divide", "Num /", "Keypad /"},
    KeyName{"Keypad *", "MULTIPLY", "KP_Multiply", "Numpad Multiply", "Num *", "Keypad *"},
    KeyName{"Keypad -", "SUBTRACT", "KP_Subtract", "Numpad Subtract", "Num -", "Keypad -"},
    KeyName{"Keypad +", "ADD", "KP_Add", "Numpad Add", "Num +", "Keypad +"},
    KeyName{"Keypad Enter", "NUMPADENTER", "KP_Enter", "Numpad Enter", "Num Enter", "Keypad Enter"},
    KeyName{"Keypad 1", "NUMPAD1", "KP_End", "Numpad 1", "Num 1", "Keypad 1"},
    KeyName{"Keypad 2", "NUMPAD2", "KP_Down", "Numpad 2", "Num 2", "Keypad 2"},
    KeyName{"Keypad 3", "NUMPAD3", "KP_Next", "Numpad 3", "Num 3", "Keypad 3"},
    KeyName{"Keypad 4", "NUMPAD4", "KP_Left", "Numpad 4", "Num 4", "Keypad 4"},
    KeyName{"Keypad 5", "NUMPAD5", "KP_Begin", "Numpad 5", "Num 5", "Keypad 5"},
    KeyName{"Keypad 6", "NUMPAD6", "KP_Right", "Numpad 6", "Num 6", "Keypad 6"},
    KeyName{"Keypad 7", "NUMPAD7", "KP_Home", "Numpad 7", "Num 7", "Keypad 7"},
    KeyName{"Keypad 8", "NUMPAD8", "KP_Up", "Numpad 8", "Num 8", "Keypad 8"},
    KeyName{"Keypad 9", "NUMPAD9", "KP_Prior", "Numpad 9", "Num 9", "Keypad 9"},
    KeyName{"Keypad 0", "NUMPAD0", "KP_Insert", "Numpad 0", "Num 0", "Keypad 0"},
    KeyName{"Keypad .", "DECIMAL", "KP_Delete", "Numpad Dot", "Num .", "Keypad ."},
    KeyName{"Left Ctrl", "LCONTROL", "Control_L", "Left Ctrl", "Left Ctrl", "Left Control"},
    KeyName{"Left Shift", "LSHIFT", "Shift_L", "Left Shift", "Left Shift", "Left Shift"},
    KeyName{"Left Alt", "LMENU", "Alt_L", "Left Alt", "Left Alt", "Left Alt"},
    KeyName{"Right Ctrl", "RCONTROL", "Control_R", "Right Ctrl", "Right Ctrl", "Right Control"},
    KeyName{"Right Shift", "RSHIFT", "Shift_R", "Right Shift", "Right Shift", "Right Shift"},
    KeyName{"Right Alt", "RMENU", "Alt_R", "Right Alt", "Right Alt", "Right Alt"},
};

// Controller controls as shown: Xbox names for SDL's (as the README's table), the buttons' own for
// Android's
constexpr std::array<std::pair<const char*, const char*>, 52> CONTROL_LABELS{{
    {"Button A", "A"},
    {"Button S", "A"},
    {"Button B", "B"},
    {"Button E", "B"},
    {"Button X", "X"},
    {"Button W", "X"},
    {"Button Y", "Y"},
    {"Button N", "Y"},
    {"Shoulder L", "LB"},
    {"Shoulder R", "RB"},
    {"Trigger L", "LT"},
    {"Trigger R", "RT"},
    {"Thumb L", "L3"},
    {"Thumb R", "R3"},
    {"Back", "Back"},
    {"Start", "Start"},
    {"Guide", "Guide"},
    {"Pad N", "D-pad up"},
    {"Pad S", "D-pad down"},
    {"Pad W", "D-pad left"},
    {"Pad E", "D-pad right"},
    {"Left Y+", "Left stick up"},
    {"Left Y-", "Left stick down"},
    {"Left X-", "Left stick left"},
    {"Left X+", "Left stick right"},
    {"Right Y+", "Right stick up"},
    {"Right Y-", "Right stick down"},
    {"Right X-", "Right stick left"},
    {"Right X+", "Right stick right"},
    {"Button L1", "L1"},
    {"Button R1", "R1"},
    {"Button L2", "L2"},
    {"Button R2", "R2"},
    {"Button L3", "L3"},
    {"Button R3", "R3"},
    {"Select", "Select"},
    {"Axis 0-", "Left stick left"},
    {"Axis 0+", "Left stick right"},
    {"Axis 1-", "Left stick up"},
    {"Axis 1+", "Left stick down"},
    {"Axis 11-", "Right stick left"},
    {"Axis 11+", "Right stick right"},
    {"Axis 14-", "Right stick up"},
    {"Axis 14+", "Right stick down"},
    {"Axis 15-", "D-pad left"},
    {"Axis 15+", "D-pad right"},
    {"Axis 16-", "D-pad up"},
    {"Axis 16+", "D-pad down"},
    {"Axis 22+", "R2"},
    {"Axis 23+", "L2"},
    {"Misc 1", "Share"},
    {"Touchpad", "Touchpad click"},
}};

std::string_view Trim(std::string_view text)
{
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
    text.remove_prefix(1);
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
    text.remove_suffix(1);
  return text;
}

// The source of a device string ("DInput/0/Keyboard Mouse" -> "DInput")
std::string_view Source(std::string_view device)
{
  return device.substr(0, device.find('/'));
}

// A control Dolphin's expressions take without backticks: a word (a letter, then letters, digits or
// '_') that isn't one of its functions
bool IsBareName(std::string_view text)
{
  static constexpr std::array<std::string_view, 25> FUNCTIONS = {
      "not",   "if",    "abs",   "sin",   "cos",   "tan",      "asin",     "acos",   "atan",
      "atan2", "sqrt",  "pow",   "min",   "max",   "clamp",    "timer",    "toggle", "minus",
      "plus",  "deadzone", "smooth", "hold", "tap", "relative", "pulse"};
  if (text.empty() || std::isalpha(static_cast<unsigned char>(text.front())) == 0)
    return false;
  if (!std::ranges::all_of(text, [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
      }))
  {
    return false;
  }
  return std::ranges::find(FUNCTIONS, text) == FUNCTIONS.end();
}

Term ParseTerm(std::string_view text)
{
  Term term;
  term.text = std::string(text);
  std::string_view inner;
  if (text.size() >= 2 && text.front() == '`' && text.back() == '`' &&
      text.substr(1, text.size() - 2).find('`') == std::string_view::npos)
  {
    inner = text.substr(1, text.size() - 2);
  }
  else if (IsBareName(text))
  {
    inner = text;
  }
  else
  {
    return term;
  }
  term.simple = !inner.empty();
  // As ControlQualifier::FromString: the device is everything before the last ':'
  const std::size_t colon = inner.find_last_of(':');
  if (colon != std::string_view::npos)
  {
    term.device = std::string(inner.substr(0, colon));
    term.control = std::string(inner.substr(colon + 1));
  }
  else
  {
    term.control = std::string(inner);
  }
  return term;
}
}  // namespace

std::span<const Input> Inputs()
{
  return INPUTS;
}

const Input* FindInput(std::string_view key)
{
  const auto found = std::ranges::find_if(INPUTS, [&](const Input& input) { return input.key == key; });
  return found == INPUTS.end() ? nullptr : &*found;
}

std::span<const KeyName> KeyNames()
{
  return KEYS;
}

const KeyName* FindKey(std::string_view id)
{
  const auto found = std::ranges::find_if(KEYS, [&](const KeyName& key) { return key.id == id; });
  return found == KEYS.end() ? nullptr : &*found;
}

std::string PlatformKeyName(const KeyName& key)
{
#if defined(_WIN32)
  return key.dinput;
#elif defined(__ANDROID__)
  return key.android;
#elif defined(__APPLE__)
  return key.quartz;
#else
  return key.x11;
#endif
}

std::string DefaultKeyboardDevice()
{
#if defined(_WIN32)
  return "DInput/0/Keyboard Mouse";
#elif defined(__ANDROID__)
  return "";
#elif defined(__APPLE__)
  return "Quartz/0/Keyboard & Mouse";
#else
  return "XInput2/0/Virtual core pointer";
#endif
}

bool IsKeyboardSource(std::string_view source)
{
  return source == "DInput" || source == "XInput2" || source == "Quartz";
}

std::string DeviceLabel(std::string_view name)
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

std::vector<Term> SplitExpression(std::string_view expression)
{
  std::vector<Term> terms;
  int depth = 0;
  bool quoted = false;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= expression.size(); ++i)
  {
    const char c = i < expression.size() ? expression[i] : '|';
    if (c == '`')
      quoted = !quoted;
    else if (!quoted && c == '(')
      ++depth;
    else if (!quoted && c == ')')
      --depth;
    else if (!quoted && depth <= 0 && c == '|' &&
             (i + 1 >= expression.size() || expression[i + 1] != '|'))
    {
      const std::string_view text = Trim(expression.substr(start, i - start));
      if (!text.empty())
        terms.push_back(ParseTerm(text));
      start = i + 1;
    }
  }
  return terms;
}

std::string JoinTerms(const std::vector<Term>& terms)
{
  std::string expression;
  for (const Term& term : terms)
  {
    if (!expression.empty())
      expression += " | ";
    expression += term.text;
  }
  return expression;
}

std::string ControlTerm(std::string_view device, std::string_view control)
{
  std::string text = device.empty() ? std::string(control) :
                                      std::string(device) + ":" + std::string(control);
  return IsBareName(text) ? text : "`" + text + "`";
}

bool TermSlot(const Term& term, Slot* slot)
{
  if (!term.simple)
    return false;
  if (term.device.empty())
  {
    *slot = Slot::Controller;
    return true;
  }
  const std::string_view source = Source(term.device);
  if (IsKeyboardSource(source))
  {
    *slot = Slot::Keyboard;
    return true;
  }
  // Android keyboards are Android devices like the controllers: a qualified Android term is a
  // keyboard's (the controller's are unqualified)
  if (source == "Android")
  {
    *slot = Slot::Keyboard;
    return true;
  }
  return false;
}

std::string SlotControl(std::string_view expression, Slot slot, std::string* device)
{
  for (const Term& term : SplitExpression(expression))
  {
    Slot term_slot;
    if (TermSlot(term, &term_slot) && term_slot == slot)
    {
      if (device)
        *device = term.device;
      return term.control;
    }
  }
  return {};
}

std::string SetSlot(std::string_view expression, Slot slot, std::string_view device,
                    std::string_view control)
{
  std::vector<Term> terms = SplitExpression(expression);
  std::erase_if(terms, [&](const Term& term) {
    Slot term_slot;
    return TermSlot(term, &term_slot) && term_slot == slot;
  });
  if (!control.empty())
  {
    Term term;
    term.text = ControlTerm(device, control);
    term.simple = true;
    term.device = std::string(device);
    term.control = std::string(control);
    // The controller's first, then the keyboard's, then anything else
    const auto position = slot == Slot::Controller ? terms.begin() :
                                                     std::ranges::find_if(terms, [](const Term& t) {
                                                       Slot s;
                                                       return !TermSlot(t, &s) || s != Slot::Controller;
                                                     });
    terms.insert(position, std::move(term));
  }
  return JoinTerms(terms);
}

std::string ControlLabel(std::string_view control)
{
  for (const auto& [name, label] : CONTROL_LABELS)
  {
    if (control == name)
      return label;
  }
  for (const KeyName& key : KEYS)
  {
    if (control == key.dinput || control == key.x11 || control == key.android ||
        (*key.quartz && control == key.quartz))
      return key.label;
  }
  return std::string(control);
}
}  // namespace SkiesBindings
