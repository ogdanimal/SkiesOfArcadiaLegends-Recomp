// The in-game menu and the player options, as the Dolphin build has them
// (VideoCommon/SkiesMenu.cpp, Core/SkiesSettings.cpp, which this follows): Z+Start (or Esc, or a
// controller's Back / Select) opens the menu over the game, drawn with Dear ImGui through Aurora's
// overlay; the settings go to the same [Skies] keys and reach the game through the game patch's
// config words (ws_config, ws_hud_aspect, qol_input, cam_input); save states go through state.cpp.
// What the standalone host can't do yet isn't offered: 60 fps, the Dreamcast music and models, the
// European languages, HD textures, button mapping.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

#ifdef SKIES_STANDALONE_AURORA
#include <SDL3/SDL.h>
#include <imgui.h>

#include <dolphin/vi.h>

#include "gxruntime/aurora_backend.h"

extern "C" {
void aurora_backend_set_overlay(const void* vertices, uint32_t vertex_count, const uint16_t* indices,
                                uint32_t index_count, const AuroraOverlayCmd* cmds, uint32_t cmd_count,
                                float display_width, float display_height);
void aurora_backend_set_overlay_texture(uint32_t id, uint32_t width, uint32_t height, const uint8_t* rgba);
void aurora_enable_vsync(bool enabled);
}
#endif

namespace host
{
namespace
{
using Clock = std::chrono::steady_clock;
using std::chrono::milliseconds;

constexpr u16 PAD_BUTTON_LEFT = 0x0001;
constexpr u16 PAD_BUTTON_RIGHT = 0x0002;
constexpr u16 PAD_BUTTON_DOWN = 0x0004;
constexpr u16 PAD_BUTTON_UP = 0x0008;
constexpr u16 PAD_TRIGGER_Z = 0x0010;
constexpr u16 PAD_TRIGGER_R = 0x0020;
constexpr u16 PAD_TRIGGER_L = 0x0040;
constexpr u16 PAD_BUTTON_A = 0x0100;
constexpr u16 PAD_BUTTON_B = 0x0200;
constexpr u16 PAD_BUTTON_X = 0x0400;
constexpr u16 PAD_BUTTON_Y = 0x0800;
constexpr u16 PAD_BUTTON_START = 0x1000;
constexpr u8 STICK_CENTER = 0x80;

// Z is held back from the game this long after it goes down, to see whether Start follows (the
// open combo); a Z the game was denied that way is replayed as a tap when it's released early.
constexpr milliseconds Z_HOLD_BACK{150};
constexpr milliseconds REPEAT_DELAY{350};
constexpr milliseconds REPEAT_RATE{110};
constexpr int STICK_THRESHOLD = 0x40;

enum class Option
{
  SaveState,
  LoadState,
  AutoSave,
  Picture,
  Hud,
  Resolution,
  VSync,
  ShowFps,
  Camera,
  BattleSpeed,
  FieldSpeed,
  Encounters,
  HoldB,
  Exp,
  Gold,
  MagicExp,
  Invincible,
  Volume,
  DeadZone,
};

constexpr std::array SAVES_ROWS{Option::SaveState, Option::LoadState, Option::AutoSave};
constexpr std::array GRAPHICS_ROWS{Option::Picture, Option::Hud, Option::Resolution, Option::VSync,
                                   Option::ShowFps};
constexpr std::array GAMEPLAY_ROWS{Option::Camera,     Option::BattleSpeed, Option::FieldSpeed,
                                   Option::Encounters, Option::HoldB,       Option::Exp,
                                   Option::Gold,       Option::MagicExp,    Option::Invincible};
constexpr std::array SOUND_ROWS{Option::Volume};
constexpr std::array CONTROLS_ROWS{Option::DeadZone};
struct Tab
{
  const char* name;
  std::span<const Option> rows;
};
// Saves first, as in the Dolphin build: in game that's the one used most
constexpr std::array TABS{Tab{"Saves", SAVES_ROWS}, Tab{"Graphics", GRAPHICS_ROWS},
                          Tab{"Gameplay", GAMEPLAY_ROWS}, Tab{"Sound", SOUND_ROWS},
                          Tab{"Controls", CONTROLS_ROWS}};

// Save states (state.cpp) in USER/StateSaves/Standalone: the player's five slots are
// <game>-slot1..5.sst, auto-saves <game>-auto-<date>-<time>.sst, the newest three kept. Not the
// Dolphin build's files (state-skies-slotN.sav): the two builds' states don't load in each other.
constexpr int SAVE_SLOTS = 5;
constexpr int AUTO_KEEP = 3;
// Minutes between auto-saves (0 = off).
constexpr std::array AUTO_SAVE_MINUTES{0, 5, 10, 15, 30};

// Quality-of-life choices, in percent of the game's (patches/game/qol.c).
constexpr std::array ENCOUNTER_RATES{0, 25, 50, 100, 200, 400};
constexpr std::array REWARD_RATES{0, 50, 100, 200, 300, 400};
constexpr std::array BATTLE_SPEEDS{1, 2, 3, 4};
constexpr std::array FIELD_SPEEDS{100, 150, 200, 300};
constexpr std::array DEAD_ZONES{0, 5, 10, 15, 20, 25, 30};
// Internal resolution: 0 = the window's own (Aurora sizes the EFB to it), then the launchers' scales
constexpr std::array EFB_SCALES{0, 1, 2, 3, 4, 6};
// [Skies] Widescreen in menu order: 16:9, Match screen, the original 4:3.
constexpr std::array PICTURES{1, 2, 0};
// [Skies] Hud in menu order: the screen's edges, a 21:9 area, a 16:9 area, the original 4:3.
constexpr std::array HUDS{2, 3, 1, 0};

// The settings' keys (the Dolphin build's: Core/Config/SkiesSettings.cpp, GraphicsSettings.cpp)
struct Key
{
  SettingFile file;
  const char* section;
  const char* name;
  int fallback;
  bool boolean = false;
};
constexpr Key WIDESCREEN{SettingFile::Main, "Skies", "Widescreen", -1};
constexpr Key HUD{SettingFile::Main, "Skies", "Hud", -1};
constexpr Key CAMERA_CONTROL{SettingFile::Main, "Skies", "CameraControl", 1};
constexpr Key ENCOUNTERS{SettingFile::Main, "Skies", "Encounters", 100};
constexpr Key EXP{SettingFile::Main, "Skies", "Exp", 100};
constexpr Key GOLD{SettingFile::Main, "Skies", "Gold", 100};
constexpr Key MAGIC_EXP{SettingFile::Main, "Skies", "MagicExp", 100};
constexpr Key BATTLE_SPEED{SettingFile::Main, "Skies", "BattleSpeed", 1};
constexpr Key FIELD_SPEED{SettingFile::Main, "Skies", "FieldSpeed", 100};
constexpr Key INVINCIBLE{SettingFile::Main, "Skies", "Invincible", 0};
constexpr Key HOLD_B{SettingFile::Main, "Skies", "HoldB", 0};
constexpr Key LAUNCHER_TEXT_SIZE{SettingFile::Main, "Skies", "LauncherTextSize", 100};
constexpr Key AUTO_SAVE{SettingFile::Main, "Skies", "AutoSave", 10};
// The standalone's own: controller 1's stick dead zone (the Dolphin build keeps it in its
// controller profile, GCPadNew.ini, which this host doesn't read)
constexpr Key DEAD_ZONE{SettingFile::Main, "Skies", "StandaloneDeadZone", 0};
constexpr Key VOLUME{SettingFile::Main, "DSP", "Volume", 100};
constexpr Key EFB_SCALE{SettingFile::Gfx, "Settings", "InternalResolution", 0};
constexpr Key SHOW_FPS{SettingFile::Gfx, "Settings", "ShowFPS", 0, true};
constexpr Key VSYNC{SettingFile::Gfx, "Hardware", "VSync", 0, true};

int Get(const Key& key)
{
  return SettingGet(key.file, key.section, key.name, key.fallback);
}

void Set(const Key& key, int value)
{
  SettingSet(key.file, key.section, key.name, value, key.boolean);
}

struct Values
{
  int widescreen = 1;
  int hud = 2;
  int efb_scale = 0;
  bool vsync = false;
  bool show_fps = false;
  int camera = 1;
  int encounters = 100;
  int hold_b = 0;
  int exp = 100;
  int gold = 100;
  int magic_exp = 100;
  int battle_speed = 1;
  int field_speed = 100;
  int invincible = 0;
  int volume = 100;
  int dead_zone = 0;
  int auto_save = 10;
};

enum class Mode
{
  Closed,
  Open,
  Closing,  // closed, but the buttons that closed it are still down: the game doesn't get them yet
};

Mode s_mode = Mode::Closed;
int s_tab = 0;
int s_selected = 0;  // row within the tab, TAB_BAR = the tab bar itself (as in the launchers)
constexpr int TAB_BAR = -1;
int s_text_percent = 100;
Values s_staged;
Values s_current;

// Z hold-back for the open combo (and, where the camera can be moved, for the camera hold).
bool s_z_held_back = false;
bool s_z_camera_used = false;
bool s_speed_chord = false;  // Z+X switched the field speed; both held back until let go
bool s_z_passed = false;
Clock::time_point s_z_down;
int s_z_replay_polls = 0;

// Save states: the slot each row points at, the load row's "press A again", when each slot was
// saved (0 = empty; [0] is the newest auto-save; read when the menu opens).
int s_save_slot = 1;
int s_load_slot = 0;  // 0 = the newest auto-save
bool s_load_confirm = false;
std::array<std::time_t, SAVE_SLOTS + 1> s_slot_time{};
std::string s_newest_auto;

// Menu navigation edges and repeat.
u16 s_previous = 0;
u16 s_repeat_button = 0;
Clock::time_point s_repeat_next;

// The values the host applies itself (the game patch's words are written from the settings
// every frame and poll); -1 = not applied yet.
int s_applied_efb_scale = -1;
int s_applied_vsync = -1;
// Cached for the audio path and the pad filter (read once per change, not per sample block)
int s_volume = 100;
int s_dead_zone = 0;

// The hotkeys outside the GameCube's buttons: the menu (Esc, a controller's Back / Select) and
// Invincible (L3 / R3), from SDL directly.
bool s_menu_key_down = false;
bool s_stick_click = false;

enum class Region
{
  Usa,
  Europe,
  Japan,
};

Region GameRegion()
{
  const u8 code = DiscHeader()[3];
  return code == 'P' ? Region::Europe : code == 'J' ? Region::Japan : Region::Usa;
}

// A game address (or an instruction word) in the USA release, the European one and the Japanese
// one (patches/game/GEAP8P.addresses.json, GEAJ8P.addresses.json). The patch's own blocks in the
// low-memory gap are at the same addresses in all three.
struct RegionAddress
{
  u32 usa;
  u32 europe;
  u32 japan;
  operator u32() const
  {
    const Region r = GameRegion();
    return r == Region::Europe ? europe : r == Region::Japan ? japan : usa;
  }
};

PadStatus Released(const PadStatus& raw)
{
  PadStatus status;
  status.connected = raw.connected;
  return status;
}

// ---- the picture (Core/SkiesSettings.cpp) ----------------------------------------------------------
constexpr u32 WS_CONFIG_ADDRESS = 0x80002A00;
constexpr u32 WS_CONFIG_MAGIC = 0x57534346;  // 'WSCF'
constexpr u32 WS_DISPLAY_ASPECT_OFFSET = 8;
constexpr float WIDE_ASPECT = 16.0f / 9.0f;
// The game patch's ws_hud_aspect: 0 = the HUD at the picture's edges, an aspect = at the edges of
// a centred area that wide, < 0 = the original 4:3 layout.
constexpr u32 WS_HUD_ASPECT_ADDRESS = 0x80002E9C;
constexpr int MATCH_SCREEN = 2;
constexpr float MIN_SCREEN_ASPECT = 4.0f / 3.0f;
constexpr float MAX_SCREEN_ASPECT = 32.0f / 9.0f;

float ScreenAspect()
{
  unsigned width = 0, height = 0;
  if (!VideoSurfaceSize(&width, &height) || width == 0 || height == 0)
    return WIDE_ASPECT;  // no window yet: the first frames as 16:9
  return std::clamp(static_cast<float>(width) / static_cast<float>(height), MIN_SCREEN_ASPECT,
                    MAX_SCREEN_ASPECT);
}

// ---- options read from the settings ----------------------------------------------------------------
Values ReadValues()
{
  Values values;
  const int widescreen = Get(WIDESCREEN);
  values.widescreen = widescreen < 0 ? 1 : widescreen;
  const int hud = Get(HUD);
  values.hud = hud < 0 ? 2 : hud;
  values.efb_scale = Get(EFB_SCALE);
  values.vsync = Get(VSYNC) != 0;
  values.show_fps = Get(SHOW_FPS) != 0;
  values.camera = Get(CAMERA_CONTROL) != 0 ? 1 : 0;
  values.encounters = Get(ENCOUNTERS);
  values.hold_b = Get(HOLD_B) != 0 ? 1 : 0;
  values.exp = Get(EXP);
  values.gold = Get(GOLD);
  values.magic_exp = Get(MAGIC_EXP);
  values.battle_speed = Get(BATTLE_SPEED);
  values.field_speed = std::clamp(Get(FIELD_SPEED), 100, FIELD_SPEEDS.back());
  values.invincible = Get(INVINCIBLE) != 0 ? 1 : 0;
  values.volume = std::clamp(Get(VOLUME), 0, 100);
  values.dead_zone = std::clamp(Get(DEAD_ZONE), 0, DEAD_ZONES.back());
  values.auto_save = std::max(0, Get(AUTO_SAVE));
  return values;
}

// The menu just closed: the changed values to the settings files; the frame end and the next
// polls pick them up.
void Apply(const Values& values, const Values& before)
{
  bool changed = false;
  const auto set = [&](const Key& key, int value, int old) {
    if (value != old)
    {
      Set(key, value);
      changed = true;
    }
  };
  set(WIDESCREEN, values.widescreen, before.widescreen);
  set(HUD, values.hud, before.hud);
  set(EFB_SCALE, values.efb_scale, before.efb_scale);
  set(VSYNC, values.vsync, before.vsync);
  set(SHOW_FPS, values.show_fps, before.show_fps);
  set(CAMERA_CONTROL, values.camera, before.camera);
  set(ENCOUNTERS, values.encounters, before.encounters);
  set(HOLD_B, values.hold_b, before.hold_b);
  set(EXP, values.exp, before.exp);
  set(GOLD, values.gold, before.gold);
  set(MAGIC_EXP, values.magic_exp, before.magic_exp);
  set(BATTLE_SPEED, values.battle_speed, before.battle_speed);
  set(FIELD_SPEED, values.field_speed, before.field_speed);
  set(INVINCIBLE, values.invincible, before.invincible);
  set(VOLUME, values.volume, before.volume);
  set(DEAD_ZONE, values.dead_zone, before.dead_zone);
  set(AUTO_SAVE, values.auto_save, before.auto_save);
  s_volume = values.volume;
  s_dead_zone = values.dead_zone;
  if (changed)
    SettingsSave();
}

// The next value in a list: Left/Right stop at the ends, A (wrap) goes round. A value that isn't in
// the list (set elsewhere) moves to the nearest entry in that direction.
int Step(std::span<const int> list, int value, int direction, bool wrap)
{
  const int count = static_cast<int>(list.size());
  int index = -1;
  for (int i = 0; i < count; ++i)
  {
    if (list[i] == value)
      index = i;
  }
  if (index < 0)
  {
    for (int i = 0; i < count; ++i)
    {
      if (direction > 0 ? list[i] > value : list[i] < value)
        index = direction > 0 ? (index < 0 ? i : index) : i;
    }
    return index < 0 ? (wrap ? list[0] : value) : list[index];
  }
  index += direction;
  if (wrap)
    index = (index + count) % count;
  return list[std::clamp(index, 0, count - 1)];
}

std::string ResolutionText(int scale)
{
  switch (scale)
  {
  case 0:
    return "Match window";
  case 1:
    return "Native (480p)";
  case 2:
    return "2x (720p)";
  case 3:
    return "3x (1080p)";
  case 4:
    return "4x (1440p)";
  case 6:
    return "6x (4K)";
  default:
    return std::to_string(scale) + "x";
  }
}

std::string RateText(int percent)
{
  switch (percent)
  {
  case 0:
    return "Off";
  case 25:
    return "1/4";
  case 50:
    return "1/2";
  case 100:
    return "Normal";
  default:
    return percent % 100 == 0 ? std::to_string(percent / 100) + "x" : std::to_string(percent) + "%";
  }
}

std::string SpeedText(int speed)
{
  return speed <= 1 ? "Normal (Z in battle)" : std::to_string(speed) + "x (Z in battle)";
}

std::string Decimal(double value)
{
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%g", value);
  return buf;
}

// Percent
std::string FieldSpeedText(int speed)
{
  return speed <= 100 ? "Normal (Z+X)" : Decimal(speed / 100.0) + "x (Z+X)";
}

// ---- save state files --------------------------------------------------------------------------
std::string GameCode()
{
  return std::string(reinterpret_cast<const char*>(DiscHeader()), 6);
}

std::string SlotPath(int slot)
{
  return (std::filesystem::path(StateDirectory()) / (GameCode() + "-slot" + std::to_string(slot) + ".sst"))
      .string();
}

std::string AutoPrefix()
{
  return GameCode() + "-auto-";
}

// The auto-saves, oldest first (their names sort by time)
std::vector<std::filesystem::path> AutoSaves()
{
  std::vector<std::filesystem::path> found;
  std::error_code ec;
  const std::string prefix = AutoPrefix();
  for (const auto& entry : std::filesystem::directory_iterator(StateDirectory(), ec))
  {
    const std::string name = entry.path().filename().string();
    if (name.rfind(prefix, 0) == 0 && entry.path().extension() == ".sst")
      found.push_back(entry.path());
  }
  std::sort(found.begin(), found.end());
  return found;
}

// When a file was last written, 0 if it isn't there.
std::time_t WriteTime(const std::filesystem::path& path)
{
  std::error_code ec;
  const auto written = std::filesystem::last_write_time(path, ec);
  if (ec)
    return 0;
  const auto system = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      written - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
  return std::chrono::system_clock::to_time_t(system);
}

void ReadSlotTimes()
{
  s_slot_time = {};
  s_newest_auto.clear();
  if (StateDirectory().empty())
    return;
  for (int slot = 1; slot <= SAVE_SLOTS; ++slot)
    s_slot_time[slot] = WriteTime(SlotPath(slot));
  const auto autos = AutoSaves();
  if (!autos.empty())
  {
    s_newest_auto = autos.back().string();
    s_slot_time[0] = WriteTime(autos.back());
  }
}

std::string SlotTime(int slot)
{
  if (s_slot_time[slot] == 0)
    return "empty";
  char text[32];
  const std::tm* local = std::localtime(&s_slot_time[slot]);
  if (!local || !std::strftime(text, sizeof(text), "%b %d %H:%M", local))
    return "saved";
  return text;
}

void Change(Option option, int direction, bool wrap)
{
  Values& v = s_staged;
  switch (option)
  {
  case Option::SaveState:
    s_save_slot = (s_save_slot - 1 + direction + SAVE_SLOTS) % SAVE_SLOTS + 1;
    break;
  case Option::LoadState:
    s_load_slot = (s_load_slot + direction + SAVE_SLOTS + 1) % (SAVE_SLOTS + 1);
    s_load_confirm = false;
    break;
  case Option::AutoSave:
    v.auto_save = Step(AUTO_SAVE_MINUTES, v.auto_save, direction, wrap);
    break;
  case Option::Picture:
    v.widescreen = Step(PICTURES, v.widescreen, direction, wrap);
    break;
  case Option::Hud:
    v.hud = Step(HUDS, v.hud, direction, wrap);
    break;
  case Option::Resolution:
    v.efb_scale = Step(EFB_SCALES, v.efb_scale, direction, wrap);
    break;
  case Option::VSync:
    v.vsync = !v.vsync;
    break;
  case Option::ShowFps:
    v.show_fps = !v.show_fps;
    break;
  case Option::Camera:
    v.camera = v.camera ? 0 : 1;
    break;
  case Option::DeadZone:
    v.dead_zone = Step(DEAD_ZONES, v.dead_zone, direction, wrap);
    break;
  case Option::Encounters:
    v.encounters = Step(ENCOUNTER_RATES, v.encounters, direction, wrap);
    break;
  case Option::HoldB:
    v.hold_b = v.hold_b ? 0 : 1;
    break;
  case Option::Exp:
    v.exp = Step(REWARD_RATES, v.exp, direction, wrap);
    break;
  case Option::Gold:
    v.gold = Step(REWARD_RATES, v.gold, direction, wrap);
    break;
  case Option::MagicExp:
    v.magic_exp = Step(REWARD_RATES, v.magic_exp, direction, wrap);
    break;
  case Option::BattleSpeed:
    v.battle_speed = Step(BATTLE_SPEEDS, v.battle_speed, direction, wrap);
    break;
  case Option::FieldSpeed:
    v.field_speed = Step(FIELD_SPEEDS, v.field_speed, direction, wrap);
    break;
  case Option::Invincible:
    v.invincible = v.invincible ? 0 : 1;
    break;
  case Option::Volume:
    v.volume = std::clamp(v.volume + direction * 10, 0, 100);
    break;
  }
}

// The buttons that act in the menu (confirm, back, tabs, close): held when it closes, the game
// doesn't get them until they're let go
constexpr u16 CLOSING_BUTTONS = PAD_BUTTON_A | PAD_BUTTON_B | PAD_BUTTON_X | PAD_BUTTON_Y |
                                PAD_TRIGGER_Z | PAD_BUTTON_START | PAD_TRIGGER_L | PAD_TRIGGER_R;

// Buttons, with the main stick folded into the D-pad.
u16 MenuButtons(const PadStatus& raw)
{
  u16 buttons = raw.buttons;
  if (raw.stick_y > STICK_CENTER + STICK_THRESHOLD)
    buttons |= PAD_BUTTON_UP;
  if (raw.stick_y < STICK_CENTER - STICK_THRESHOLD)
    buttons |= PAD_BUTTON_DOWN;
  if (raw.stick_x > STICK_CENTER + STICK_THRESHOLD)
    buttons |= PAD_BUTTON_RIGHT;
  if (raw.stick_x < STICK_CENTER - STICK_THRESHOLD)
    buttons |= PAD_BUTTON_LEFT;
  // L / R switch tabs: the analog triggers count once pressed most of the way
  if (raw.trig_l > 0xA0)
    buttons |= PAD_TRIGGER_L;
  if (raw.trig_r > 0xA0)
    buttons |= PAD_TRIGGER_R;
  return buttons;
}

// Up/Down repeat while held, and Left/Right too on a ranged option (repeating them on a toggle
// would flip it back and forth); other buttons act once per press.
u16 Pressed(u16 buttons, bool repeat_sideways)
{
  const Clock::time_point now = Clock::now();
  u16 pressed = buttons & ~s_previous;
  const u16 repeating = PAD_BUTTON_UP | PAD_BUTTON_DOWN |
                        (repeat_sideways ? PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT : 0);
  const u16 new_direction = pressed & repeating;
  if (new_direction)
  {
    s_repeat_button = new_direction & -new_direction;  // one of them
    s_repeat_next = now + REPEAT_DELAY;
  }
  else if ((s_repeat_button & repeating) && (buttons & s_repeat_button) && now >= s_repeat_next)
  {
    pressed |= s_repeat_button;
    s_repeat_next = now + REPEAT_RATE;
  }
  if (!(buttons & s_repeat_button))
    s_repeat_button = 0;
  s_previous = buttons;
  return pressed;
}

void Open()
{
  s_mode = Mode::Open;
  s_tab = 0;
  s_selected = 0;
  s_text_percent = std::clamp(Get(LAUNCHER_TEXT_SIZE), 80, 160);
  s_current = ReadValues();
  s_staged = s_current;
  s_previous = 0xFFFF;  // nothing counts as pressed until it's released once
  s_repeat_button = 0;
  s_load_confirm = false;
  ReadSlotTimes();
  Log("[menu] open\n");
}

void Close()
{
  s_mode = Mode::Closing;
  Apply(s_staged, s_current);
  Log("[menu] closed\n");
}

// A ship battle runs inside field mode 6, on a map from 500 to 599 (the game's own test, in
// fn_8012A26C); the game patch's qol_ship_live (patches/game/qol.c) is set from the battle data's
// allocation until its results.
bool InShipBattle()
{
  const RegionAddress GAME_MODE{0x803475CC, 0x8034ACEC, 0x80347048};
  const RegionAddress MAP_ID{0x80311AC4, 0x80315154, 0x80311544};
  constexpr u32 QOL_SHIP_LIVE = 0x80002BA8;
  const u32 map = Read32(MAP_ID);
  return Read32(GAME_MODE) == 6 && map >= 500 && map <= 599 && Read32(QOL_SHIP_LIVE) != 0;
}

// Camera control: hold Z, then the C-stick tilts the camera (and on the world map turns it around
// the ship) and the L/R triggers zoom; the game patch (patches/game/camera.c) applies the offsets
// it finds in cam_input. Only where the game's camera manager follows the field camera object:
// towns, dungeons and the world map.
namespace camera
{
const RegionAddress GAME_MODE{0x803475CC, 0x8034ACEC, 0x80347048};  // 6 = field / world map
const RegionAddress MANAGER{0x80311970, 0x80314FF0, 0x803113F0};    // +0 mode (2 = follow), +8 eye source
const RegionAddress FIELD_CAMERA_EYE{0x80302F94, 0x80306614, 0x80302A14};
const RegionAddress FIELD_CAMERA_PITCH{0x80302FA0, 0x80306620, 0x80302A20};
const RegionAddress MAP_ID{0x80311AC4, 0x80315154, 0x80311544};  // 0x63 = the world map
constexpr u32 WORLD_MAP = 0x63;
const RegionAddress HOOK_SITE{0x8022D910, 0x80230BB0, 0x8022D494};  // bl fn_8022D1F8 in the unpatched game
constexpr u32 HOOK_ORIGINAL = 0x4BFFF8E9;
constexpr u32 INPUT = 0x80002EC0;        // camera.c's cam_input
constexpr u32 INPUT_MAGIC = 0x43414D43;  // 'CAMC'

constexpr float DEGREES = 65536.0f / 360.0f;    // Ninja angle units
constexpr float PITCH_SPEED = 80.0f * DEGREES;  // per second, stick fully over
constexpr float YAW_SPEED = 120.0f * DEGREES;
constexpr float ZOOM_SPEED = 0.8f;  // distance factor per second, trigger fully in
constexpr float YAW_RETURN = 3.0f;  // world map: the turn eases back at this rate
constexpr int STICK_DEAD = 24;
constexpr int TRIGGER_DEAD = 40;

struct Offsets
{
  float pitch = 0.0f;
  float yaw = 0.0f;
  float zoom = 1.0f;
};
struct Limits
{
  float pitch_lo, pitch_hi;  // the tilted pitch, in Ninja units (negative: looking down)
  float zoom_lo, zoom_hi;
  float rise;  // > 0: the eye stays within this of the game's own eye height (camera.c)
};
// Field: the camera stays between the game's own camera and Vyse (rooms have low ceilings and the
// game has no camera collision), down to -40 degrees.
constexpr Limits FIELD_LIMITS{-40.0f * DEGREES, -1.0f * DEGREES, 0.5f, 1.0f, 0.5f};
constexpr Limits WORLD_LIMITS{-35.0f * DEGREES, 25.0f * DEGREES, 0.6f, 2.0f, 0.0f};

Offsets s_field;
Offsets s_world;
u64 s_last_poll = 0;  // guest cycles

struct Context
{
  bool movable = false;  // the game is following the field camera object
  bool world_map = false;
  float game_pitch = 0.0f;
};

Context ReadContext()
{
  Context context;
  if (Read32(HOOK_SITE) == HOOK_ORIGINAL)  // a DOL without camera.c
    return context;
  context.movable = Read32(GAME_MODE) == 6 && Read32(MANAGER) == 2 &&
                    Read32(MANAGER + 8) == FIELD_CAMERA_EYE;
  context.world_map = Read32(MAP_ID) == WORLD_MAP;
  context.game_pitch = static_cast<float>(static_cast<s16>(Read32(FIELD_CAMERA_PITCH)));
  return context;
}

float Axis(u8 value)
{
  const int v = static_cast<int>(value) - STICK_CENTER;
  if (std::abs(v) < STICK_DEAD)
    return 0.0f;
  return std::clamp(static_cast<float>(v) / 100.0f, -1.0f, 1.0f);
}

float Trigger(u8 value)
{
  return value < TRIGGER_DEAD ? 0.0f : static_cast<float>(value - TRIGGER_DEAD) / (255 - TRIGGER_DEAD);
}

// Applies one poll of camera input (z: Z is held). Returns true if the player moved the camera.
bool Update(const Context& context, const PadStatus& raw, bool z, float dt)
{
  Offsets& offsets = context.world_map ? s_world : s_field;
  const Limits& limits = context.world_map ? WORLD_LIMITS : FIELD_LIMITS;
  bool moved = false;
  if (z)
  {
    const float stick_y = Axis(raw.sub_y);
    const float stick_x = Axis(raw.sub_x);
    const float zoom = Trigger(raw.trig_r) - Trigger(raw.trig_l);
    // Up: the camera rises and looks down more.
    offsets.pitch -= stick_y * PITCH_SPEED * dt;
    offsets.zoom += zoom * ZOOM_SPEED * dt;
    if (context.world_map)
      offsets.yaw -= stick_x * YAW_SPEED * dt;
    // In the field the game turns the camera with C-stick X itself; that counts too (a Z hold that
    // turned the camera mustn't become a tap on release).
    moved = stick_y != 0.0f || zoom != 0.0f || stick_x != 0.0f;
  }
  if (context.world_map && !(z && Axis(raw.sub_x) != 0.0f))
    offsets.yaw -= offsets.yaw * std::min(1.0f, YAW_RETURN * dt);
  // Keep the total pitch in range, so pushing past a limit doesn't wind up.
  offsets.pitch = std::clamp(offsets.pitch, std::min(0.0f, limits.pitch_lo - context.game_pitch),
                             std::max(0.0f, limits.pitch_hi - context.game_pitch));
  offsets.zoom = std::clamp(offsets.zoom, limits.zoom_lo, limits.zoom_hi);
  return moved;
}

void Write(const Context& context, bool enabled)
{
  if (Read32(HOOK_SITE) == HOOK_ORIGINAL)
    return;
  const Offsets& offsets = context.world_map ? s_world : s_field;
  const Limits& limits = context.world_map ? WORLD_LIMITS : FIELD_LIMITS;
  Write32(INPUT, INPUT_MAGIC);
  Write32(INPUT + 4, enabled ? 1 : 0);
  Write32(INPUT + 8, static_cast<u32>(static_cast<s32>(offsets.pitch)));
  Write32(INPUT + 12, static_cast<u32>(static_cast<s32>(offsets.yaw)));
  Write32(INPUT + 16, std::bit_cast<u32>(offsets.zoom));
  Write32(INPUT + 20, static_cast<u32>(static_cast<s32>(limits.pitch_lo)));
  Write32(INPUT + 24, static_cast<u32>(static_cast<s32>(limits.pitch_hi)));
  Write32(INPUT + 28, std::bit_cast<u32>(limits.rise));
}
}  // namespace camera

// Quality-of-life options: the settings go to the game patch (patches/game/qol.c) through
// qol_input every poll; a Z tap in battle cycles the battle speed, Z+X in the field the field speed.
namespace qol
{
const RegionAddress HOOK_SITE{0x800C207C, 0x800C2430, 0x800C2064};  // bl fn_8025ECC4 (rand) in the unpatched game
const RegionAddress HOOK_ORIGINAL{0x4819CC49, 0x4819EFD5, 0x4819C711};
constexpr u32 INPUT = 0x80002EA0;        // qol.c's qol_input
constexpr u32 INPUT_MAGIC = 0x514F4C43;  // 'QOLC'
const RegionAddress GAME_MODE{0x803475CC, 0x8034ACEC, 0x80347048};  // 7 = battle (ship battles: InShipBattle)
const RegionAddress MAP_ID{0x80311AC4, 0x80315154, 0x80311544};
// In qol_input's speed word (ws26): the speed holds in the field (game mode 6) too.
constexpr u32 SPEED_FIELD = 0x100;

enum class Place
{
  Other,
  Battle,
  Field,
};

// Speeds are in quarters here (4 = normal, 6 = 1.5x). A battle at 3x or 4x the host can't keep up
// with (under 95% for 1.5 s) steps down a whole step, to 2x at the lowest, until it ends. The field
// steps down from 1.25x on, straight to the quarter the measured speed says the host holds, to
// normal at the lowest, until the map changes or a try a quarter higher holds.
int s_speed_cap = 16;
Place s_cap_place = Place::Other;
u32 s_cap_map = 0;
Clock::time_point s_slow_since{};
constexpr std::chrono::seconds RETRY_FIRST{30};
constexpr std::chrono::seconds RETRY_MOST{600};
std::chrono::seconds s_retry_wait = RETRY_FIRST;
Clock::time_point s_full_since{};
bool s_retrying = false;  // the cap was raised by a try
double s_field_emulation = 1.0;  // the field's speed, smoothed over about half a second
constexpr std::chrono::seconds FIELD_GRACE{3};  // not judged for a moment after a new map
Clock::time_point s_cap_since{};

bool Present()
{
  const u32 site = Read32(HOOK_SITE);
  return site != HOOK_ORIGINAL && site >> 26 == 18;  // a bl, to qol.c
}

bool InBattle()
{
  return Read32(GAME_MODE) == 7 || InShipBattle();
}

Place WhereNow()
{
  if (InBattle())
    return Place::Battle;
  return Read32(GAME_MODE) == 6 ? Place::Field : Place::Other;
}

int FieldSpeed()
{
  return std::clamp(Get(FIELD_SPEED), 100, FIELD_SPEEDS.back());
}

std::string QuartersText(int quarters)
{
  return quarters <= 4 ? "normal" : Decimal(quarters / 4.0) + "x";
}

int EffectiveQuarters(Place place, int quarters)
{
  // The field steps down per map (the world map is the dearest), and so tries again on the next
  const u32 map = place == Place::Field ? Read32(MAP_ID) : 0;
  if (place != s_cap_place || map != s_cap_map)
  {
    s_cap_place = place;
    s_cap_map = map;
    s_speed_cap = 16;
    s_slow_since = {};
    s_retry_wait = RETRY_FIRST;
    s_full_since = {};
    s_retrying = false;
    s_field_emulation = 1.0;
    s_cap_since = Clock::now();
  }
  if (place == Place::Other)
    return quarters;
  const int effective = std::min(quarters, s_speed_cap);
  const Clock::time_point now = Clock::now();
  double emulation = EmulationSpeed();
  if (place == Place::Field)
  {
    if (now - s_cap_since < FIELD_GRACE)
      return effective;
    s_field_emulation += (emulation - s_field_emulation) * 0.03;  // per poll, 60 a second
    emulation = s_field_emulation;
  }
  if (place == Place::Field && effective < quarters)
  {
    if (emulation < 0.98)
    {
      s_full_since = {};
    }
    else if (s_full_since == Clock::time_point{})
    {
      s_full_since = now;
    }
    else if (now - s_full_since > s_retry_wait)
    {
      s_speed_cap = effective + 1;
      s_retrying = true;
      s_full_since = {};
      s_slow_since = {};
      return std::min(quarters, s_speed_cap);
    }
  }
  if (effective < (place == Place::Field ? 5 : 12))
    return effective;
  // The field plays on for a long time, so it holds a little closer to full speed
  if (emulation >= (place == Place::Field ? 0.98 : 0.95))
  {
    s_slow_since = {};
  }
  else if (s_slow_since == Clock::time_point{})
  {
    s_slow_since = now;
  }
  else if (now - s_slow_since > std::chrono::milliseconds(1500))
  {
    s_speed_cap = place == Place::Battle ?
                      effective - 4 :
                      std::clamp(static_cast<int>(effective * emulation), 4, effective - 1);
    s_slow_since = {};
    s_full_since = {};
    if (s_retrying)
    {
      // A try that didn't hold: quietly back, and wait longer before the next
      s_retry_wait = std::min(s_retry_wait * 2, RETRY_MOST);
      s_retrying = false;
    }
    else
    {
      MenuMessage(std::string(place == Place::Battle ? "Battle" : "Field") + " speed: " +
                      QuartersText(s_speed_cap) + " (the most this device keeps up with)",
                  2500);
    }
  }
  return std::min(quarters, s_speed_cap);
}

void Write()
{
  const Place place = WhereNow();
  const int wanted = place == Place::Field ? (FieldSpeed() * 4 + 50) / 100 :
                                             std::clamp(Get(BATTLE_SPEED), 1, 4) * 4;
  const int quarters = EffectiveQuarters(place, wanted);
  Write32(INPUT, INPUT_MAGIC);
  // Percentages, held to the menus' range: a hand-edited Dolphin.ini can say anything.
  const auto percent = [](const Key& key, int most) {
    return static_cast<u32>(std::clamp(Get(key), 0, most));
  };
  Write32(INPUT + 4, percent(ENCOUNTERS, ENCOUNTER_RATES.back()));
  Write32(INPUT + 8, percent(EXP, REWARD_RATES.back()));
  Write32(INPUT + 12, percent(GOLD, REWARD_RATES.back()));
  Write32(INPUT + 16, percent(MAGIC_EXP, REWARD_RATES.back()));
  // The field's in quarters, the battle's in whole steps (all an earlier game patch knows)
  Write32(INPUT + 20, place == Place::Field ? SPEED_FIELD | static_cast<u32>(quarters) :
                                              static_cast<u32>(quarters / 4));
  Write32(INPUT + 24, Get(INVINCIBLE) ? 1 : 0);
  Write32(INPUT + 28, Get(HOLD_B) != 0 ? 1 : 0);

  // A world map or ship battle frame takes about a retrace of the emulated CPU's time, so skipping
  // the waits takes it to 2x and no further: faster needs the CPU clocked up (the Dolphin build's
  // overclock, with the same factors).
  const bool fast = place != Place::Other && quarters > 4;
  const float speed = quarters / 4.0f;
  g_overclock = !fast                  ? 1.0f :
                place == Place::Field ? (quarters <= 5 ? 1.0f : std::max(1.25f, speed - 1.0f)) :
                speed >= 3            ? speed / 2.0f :
                                        1.0f;
}

// Z tapped in battle: next battle speed; Z+X in the field: next field speed.
void CycleSpeed(Place place)
{
  if (place == Place::Battle)
  {
    const int speed = Get(BATTLE_SPEED) >= 4 ? 1 : Get(BATTLE_SPEED) + 1;
    Set(BATTLE_SPEED, speed);
    SettingsSave();
    MenuMessage(speed <= 1 ? "Battle speed: normal" : "Battle speed: " + std::to_string(speed) + "x",
                1500);
  }
  else if (place == Place::Field)
  {
    const auto next = std::upper_bound(FIELD_SPEEDS.begin(), FIELD_SPEEDS.end(), FieldSpeed());
    const int speed = next == FIELD_SPEEDS.end() ? FIELD_SPEEDS.front() : *next;
    Set(FIELD_SPEED, speed);
    SettingsSave();
    MenuMessage(speed <= 100 ? "Field speed: normal" : "Field speed: " + Decimal(speed / 100.0) + "x",
                1500);
  }
}

// The Invincible hotkey (L3 / R3, the stick clicks of a controller, which aren't GameCube buttons)
void ToggleInvincible()
{
  const int on = Get(INVINCIBLE) ? 0 : 1;
  Set(INVINCIBLE, on);
  SettingsSave();
  MenuMessage(on ? "Invincible: on (HP, MP and Spirit full in battle)" : "Invincible: off", 2000);
}
}  // namespace qol

PadStatus FilterClosed(const PadStatus& raw)
{
  PadStatus out = raw;
  const bool z = raw.buttons & PAD_TRIGGER_Z;
  const bool start = raw.buttons & PAD_BUTTON_START;
  const bool x = raw.buttons & PAD_BUTTON_X;
  const Clock::time_point now = Clock::now();
  // The camera moves in guest time (the same at full speed, and runs with the same input repeat)
  const u64 guest = Now();
  const float dt = camera::s_last_poll == 0 ?
                       0.0f :
                       std::clamp(static_cast<float>(guest - camera::s_last_poll) / CPU_HZ, 0.0f, 0.1f);
  camera::s_last_poll = guest;

  const bool qol_present = qol::Present();
  if (qol_present)
    qol::Write();
  else
    g_overclock = 1.0f;
  const camera::Context context = camera::ReadContext();
  const bool camera_on = context.movable && Get(CAMERA_CONTROL) != 0;

  if (z && !s_z_held_back && !s_z_passed)
  {
    s_z_held_back = true;
    s_z_camera_used = false;
    s_z_down = now;
  }
  // Z+Start opens the menu however long Z was held first: also once Z has been passed on to the
  // game (where it was held back only briefly).
  if (z && start && (s_z_held_back || s_z_passed))
  {
    s_z_held_back = false;
    Open();
    return Released(raw);
  }
  // Z+X in the field: the field speed (a Z tap there is the game's: the minimap on foot, the world
  // map's menu). Neither reaches the game until both are let go.
  const qol::Place place = qol_present ? qol::WhereNow() : qol::Place::Other;
  if (s_speed_chord && !z && !x)
    s_speed_chord = false;
  if (!s_speed_chord && z && x && (s_z_held_back || s_z_passed) && place == qol::Place::Field)
  {
    s_speed_chord = true;
    s_z_held_back = false;
    s_z_passed = false;
    qol::CycleSpeed(place);
  }
  if (s_speed_chord)
  {
    out.buttons &= ~(PAD_TRIGGER_Z | PAD_BUTTON_X);
    return out;
  }
  if (camera_on)
  {
    if (camera::Update(context, raw, s_z_held_back && z, dt))
      s_z_camera_used = true;
    camera::Write(context, true);
    if (s_z_held_back && z)
    {
      // The camera has the C-stick's tilt (and on the world map its turn) and the triggers.
      out.sub_y = STICK_CENTER;
      if (context.world_map)
        out.sub_x = STICK_CENTER;
      out.trig_l = 0;
      out.trig_r = 0;
      out.buttons &= ~(PAD_TRIGGER_Z | PAD_TRIGGER_L | PAD_TRIGGER_R);
      return out;
    }
  }
  else if (context.movable)
  {
    camera::Write(context, false);
  }
  if (s_z_held_back)
  {
    if (!z)
    {
      // Released before Start came (and without moving the camera): in battle that's the battle
      // speed (the game doesn't use Z there), elsewhere the game gets its tap late.
      s_z_held_back = false;
      if (place == qol::Place::Battle)
        qol::CycleSpeed(place);
      else if (!s_z_camera_used)
        s_z_replay_polls = 4;
    }
    else if (now - s_z_down < Z_HOLD_BACK || place == qol::Place::Battle)
    {
      // In battle the game has no use for Z: held back until it's released (a tap: the battle
      // speed) or Start comes (the menu).
      out.buttons &= ~PAD_TRIGGER_Z;
      return out;
    }
    else
    {
      s_z_held_back = false;
      s_z_passed = true;
    }
  }
  if (!z)
    s_z_passed = false;
  if (s_z_replay_polls > 0)
  {
    --s_z_replay_polls;
    out.buttons |= PAD_TRIGGER_Z;
  }
  return out;
}

PadStatus FilterOpen(const PadStatus& raw)
{
  const std::span<const Option> rows = TABS[s_tab].rows;
  const bool on_tabs = s_selected == TAB_BAR;
  const u16 pressed = Pressed(MenuButtons(raw), !on_tabs && rows[s_selected] == Option::Volume);
  const int tab_count = static_cast<int>(TABS.size());
  const auto switch_tab = [&](int direction) {
    s_tab = (s_tab + direction + tab_count) % tab_count;
    s_load_confirm = false;
  };
  if (pressed & (PAD_TRIGGER_L | PAD_TRIGGER_R))
  {
    switch_tab((pressed & PAD_TRIGGER_R) ? 1 : -1);
    if (!on_tabs)
      s_selected = 0;
    return Released(raw);
  }
  const int count = static_cast<int>(rows.size());
  if (on_tabs)
  {
    // The tab bar: Left / Right switch tabs, Down or A go into the tab's settings
    if (pressed & PAD_BUTTON_LEFT)
      switch_tab(-1);
    if (pressed & PAD_BUTTON_RIGHT)
      switch_tab(1);
    if (pressed & (PAD_BUTTON_DOWN | PAD_BUTTON_A))
      s_selected = 0;
    else if (pressed & PAD_BUTTON_UP)
      s_selected = static_cast<int>(TABS[s_tab].rows.size()) - 1;
    else if (pressed & (PAD_BUTTON_B | PAD_BUTTON_START))
      Close();
    return Released(raw);
  }
  if (pressed & (PAD_BUTTON_UP | PAD_BUTTON_DOWN))
    s_load_confirm = false;
  // Up from the first setting and Down from the last reach the tab bar
  if (pressed & PAD_BUTTON_UP)
    s_selected = s_selected == 0 ? TAB_BAR : s_selected - 1;
  else if (pressed & PAD_BUTTON_DOWN)
    s_selected = s_selected == count - 1 ? TAB_BAR : s_selected + 1;
  if (s_selected == TAB_BAR)
    return Released(raw);
  const Option option = rows[s_selected];
  if (pressed & PAD_BUTTON_LEFT)
    Change(option, -1, false);
  if (pressed & PAD_BUTTON_RIGHT)
    Change(option, 1, false);
  const bool saves = !StateDirectory().empty();
  if ((pressed & PAD_BUTTON_A) && option == Option::SaveState)
  {
    if (saves)
    {
      StateRequestSave(SlotPath(s_save_slot), "Saved to slot " + std::to_string(s_save_slot));
      Close();
    }
  }
  else if ((pressed & PAD_BUTTON_A) && option == Option::LoadState)
  {
    // Loading throws away the play since that state, so it takes a second A.
    if (saves && s_slot_time[s_load_slot] != 0)
    {
      if (s_load_confirm)
      {
        StateRequestLoad(s_load_slot == 0 ? s_newest_auto : SlotPath(s_load_slot),
                         s_load_slot == 0 ? std::string("Loaded the auto-save") :
                                            "Loaded slot " + std::to_string(s_load_slot));
        Close();
      }
      s_load_confirm = !s_load_confirm;
    }
  }
  else if (pressed & PAD_BUTTON_A)
    Change(option, 1, true);
  else if (pressed & (PAD_BUTTON_B | PAD_BUTTON_START))
    Close();
  return Released(raw);
}

// Auto-save: every [Skies] AutoSave minutes a save state goes to a new auto-save file (the oldest
// go, so that AUTO_KEEP remain), but only at a quiet moment: on the field or world map (game mode
// 6, outside a ship battle, which runs in mode 6 too) and with the memory card left alone for a
// while (a state holds no card transfer half done). Waits for such a moment once it's due.
namespace autosave
{
const RegionAddress GAME_MODE{0x803475CC, 0x8034ACEC, 0x80347048};  // 6 = field / world map
constexpr double CARD_QUIET_SECONDS = 10.0;

Clock::time_point s_due{};
int s_minutes = 0;

// SKIES_AUTO_SAVE_SECONDS=N: every N seconds instead (tests)
const int s_test_seconds = [] {
  const char* v = std::getenv("SKIES_AUTO_SAVE_SECONDS");
  return v ? std::atoi(v) : 0;
}();

std::chrono::seconds Interval(int minutes)
{
  return std::chrono::seconds(s_test_seconds > 0 ? s_test_seconds : minutes * 60);
}

void Poll()
{
  const int minutes = StateDirectory().empty() ? 0 : Get(AUTO_SAVE);
  const Clock::time_point now = Clock::now();
  if (minutes <= 0)
  {
    s_due = {};
    s_minutes = 0;
    return;
  }
  if (s_due == Clock::time_point{} || minutes != s_minutes)
  {
    s_minutes = minutes;
    s_due = now + Interval(minutes);
    return;
  }
  if (now < s_due || s_mode != Mode::Closed || g_state_request || Read32(GAME_MODE) != 6 ||
      InShipBattle() || CardSecondsSinceLastAccess() < CARD_QUIET_SECONDS)
    return;
  s_due = now + Interval(minutes);
  auto autos = AutoSaves();
  std::error_code ec;
  while (autos.size() > AUTO_KEEP - 1)
  {
    std::filesystem::remove(autos.front(), ec);
    autos.erase(autos.begin());
  }
  char stamp[32];
  const std::time_t t = std::time(nullptr);
  const std::tm* local = std::localtime(&t);
  if (!local || !std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", local))
    std::snprintf(stamp, sizeof(stamp), "%lld", static_cast<long long>(t));
  const std::string path =
      (std::filesystem::path(StateDirectory()) / (AutoPrefix() + stamp + ".sst")).string();
  Log("[menu] auto-save to %s\n", path.c_str());
  StateRequestSave(path, "");
}
}  // namespace autosave

// Continue from a card save (the Dolphin build's SkiesMenu.cpp autoload): the game patch's
// autoload.c loads file N at boot when the request block holds 'LOAD'; the host writes it once the
// DOL is in RAM and follows its status.
namespace autoload
{
constexpr Key AUTO_LOAD_CARD{SettingFile::Main, "Skies", "AutoLoadCard", 0};
constexpr u32 REQUEST = 0x80002EF0;        // autoload.c's load_request: magic, file, status, error
constexpr u32 REQUEST_MAGIC = 0x4C4F4144;  // 'LOAD'
const RegionAddress HOOK_SITE{0x801DC350, 0x801DDFDC, 0x801DC170};  // the title's call, redirected to dl_title
const RegionAddress HOOK_ORIGINAL{0x4804C6D1, 0x4804DC89, 0x4804C625};  // bl fn_80228A20
const RegionAddress GAME_MODE{0x803475CC, 0x8034ACEC, 0x80347048};
const RegionAddress TITLE_STATE{0x80311AE0, 0x80315170, 0x80311560};  // lbl_80311AD4[3]
enum : u32
{
  PENDING = 0,
  RUNNING = 1,
  DONE = 2,
  FAILED = 3,
};

int s_requested = 0;    // MenuContinue's file (0 = the key's)
bool s_checked = false;  // the request was considered (one shot per boot)
int s_file = 0;         // 1..99: the file asked for (0 = nothing running)
Clock::time_point s_deadline;

void Stop(const char* message)
{
  s_file = 0;
  if (message)
    MenuMessage(message, 6000);
  else
    Log("[menu] auto-load: done at %.2f s guest\n", static_cast<double>(Now()) / CPU_HZ);
}

void Start()
{
  // Not before the DOL is in memory (its data section would overwrite the request)
  const u32 site = Read32(HOOK_SITE);
  if (site >> 26 != 18)  // not a bl yet
    return;
  s_checked = true;
  int file = s_requested != 0 ? s_requested : Get(AUTO_LOAD_CARD);
  if (Get(AUTO_LOAD_CARD) != 0)
  {
    Set(AUTO_LOAD_CARD, 0);
    SettingsSave();
  }
  if (file == CONTINUE_NEWEST)
  {
    file = CardNewestSaveFile();
    if (file == 0)
    {
      MenuMessage("No memory card save to continue from", 6000);
      return;
    }
  }
  if (file < 1 || file > 99)
    return;
  if (site == HOOK_ORIGINAL)
  {
    Stop("This game patch can't load a save directly: load it on the title screen");
    return;
  }
  // The game reads the magic, then the file: the magic goes in last
  Write32(REQUEST + 8, 0);
  Write32(REQUEST + 12, 0);
  Write32(REQUEST + 4, static_cast<u32>(file));
  Write32(REQUEST, REQUEST_MAGIC);
  s_file = file;
  s_deadline = Clock::now() + std::chrono::seconds(30);
  MenuMessage("Loading your memory card save (file " + std::to_string(file) + ")...", 3000);
}

void Poll()
{
  if (s_file == 0)
  {
    if (!s_checked)
      Start();
    return;
  }
  const u32 status = Read32(REQUEST + 8);
  if (status == DONE)
  {
    Write32(REQUEST, 0);
    Stop(nullptr);
  }
  else if (status == FAILED)
  {
    const u32 error = Read32(REQUEST + 12);
    Write32(REQUEST, 0);
    Stop(error == 1 ? "Your memory card save wasn't found on card A" :
         error == 2 ? "The memory card couldn't be read" :
                      "That memory card save is damaged");
  }
  else if (status == PENDING &&
           ((Read32(GAME_MODE) == 3 && Read32(TITLE_STATE) > 1) || Read32(GAME_MODE) == 6))
  {
    // The title went on (to the title proper, or the intro's field scenes) before the request
    // arrived
    Write32(REQUEST, 0);
    Stop("Couldn't load your save directly: load it on the title screen");
  }
  else if (Clock::now() > s_deadline)
  {
    Write32(REQUEST, 0);
    Stop("Loading your save took too long");
  }
}
}  // namespace autoload

// Controller 1's sticks with the dead zone: inside it centred, outside it scaled to the full range.
void ApplyDeadZone(u8& x, u8& y, int percent)
{
  if (percent <= 0)
    return;
  const float fx = (static_cast<int>(x) - STICK_CENTER) / 127.0f;
  const float fy = (static_cast<int>(y) - STICK_CENTER) / 127.0f;
  const float r = std::sqrt(fx * fx + fy * fy);
  const float dz = percent / 100.0f;
  if (r <= dz)
  {
    x = y = STICK_CENTER;
    return;
  }
  const float k = std::min(1.0f, (r - dz) / (1.0f - dz)) / r;
  x = static_cast<u8>(std::clamp(std::lround(STICK_CENTER + fx * k * 127.0f), 0l, 255l));
  y = static_cast<u8>(std::clamp(std::lround(STICK_CENTER + fy * k * 127.0f), 0l, 255l));
}

// ---- the hotkeys, from SDL -------------------------------------------------------------------------
struct Hotkeys
{
  bool menu = false;        // Esc, or a controller's Back / Select
  bool stick_click = false;  // L3 or R3
};

Hotkeys ReadHotkeys()
{
  Hotkeys keys;
#ifdef SKIES_STANDALONE_AURORA
  if (!VideoSurfaceSize(nullptr, nullptr))
    return keys;
  if (const bool* state = SDL_GetKeyboardState(nullptr))
    keys.menu = state[SDL_SCANCODE_ESCAPE];
  int count = 0;
  if (SDL_JoystickID* ids = SDL_GetGamepads(&count))
  {
    for (int i = 0; i < count; ++i)
    {
      SDL_Gamepad* pad = SDL_GetGamepadFromID(ids[i]);
      if (!pad)
        continue;
      keys.menu |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_BACK);
      keys.stick_click |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_STICK) ||
                          SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    }
    SDL_free(ids);
  }
#endif
  return keys;
}

// ---- messages and the overlay ----------------------------------------------------------------------
struct Message
{
  std::string text;
  Clock::time_point until;
};
std::vector<Message> s_messages;

// Frames per second for Show FPS: XFB copies counted over the last second
u64 s_fps_frames = 0;
Clock::time_point s_fps_since{};
double s_fps = 0.0;
}  // namespace

float g_overclock = 1.0f;

void MenuContinue(int file)
{
  autoload::s_requested = file;
}

void MenuVideoFrame()
{
  autoload::Poll();
}

void MenuMessage(const std::string& text, int milliseconds)
{
  Log("[menu] %s\n", text.c_str());
  s_messages.push_back({text, Clock::now() + std::chrono::milliseconds(milliseconds)});
  if (s_messages.size() > 6)
    s_messages.erase(s_messages.begin());
}

int MenuVolume()
{
  return s_volume;
}

void MenuApplyGame()
{
  static bool s_first = true;
  if (s_first)
  {
    s_first = false;
    s_volume = std::clamp(Get(VOLUME), 0, 100);
    s_dead_zone = std::clamp(Get(DEAD_ZONE), 0, DEAD_ZONES.back());
  }
  autosave::Poll();
  if (Read32(WS_CONFIG_ADDRESS) == WS_CONFIG_MAGIC)
  {
    int widescreen = Get(WIDESCREEN);
    if (widescreen < 0)
      widescreen = 1;  // the patch's own default
    const float aspect = widescreen == MATCH_SCREEN ? ScreenAspect() : widescreen ? WIDE_ASPECT : 0.0f;
    const u32 address = WS_CONFIG_ADDRESS + WS_DISPLAY_ASPECT_OFFSET;
    if (Read32(address) != std::bit_cast<u32>(aspect))
    {
      Write32(address, std::bit_cast<u32>(aspect));
      Log("[menu] picture %s (%.3f)\n",
          widescreen == MATCH_SCREEN ? "match screen" : widescreen ? "16:9" : "4:3", aspect);
    }
    int hud = Get(HUD);
    if (hud < 0)
      hud = 2;
    const float hud_aspect = hud == 0 ? -1.0f : hud == 1 ? WIDE_ASPECT : hud == 3 ? 64.0f / 27.0f : 0.0f;
    if (Read32(WS_HUD_ASPECT_ADDRESS) != std::bit_cast<u32>(hud_aspect))
      Write32(WS_HUD_ASPECT_ADDRESS, std::bit_cast<u32>(hud_aspect));
  }
#ifdef SKIES_STANDALONE_AURORA
  if (!VideoSurfaceSize(nullptr, nullptr))
    return;
  // SKIES_EFB_SCALE overrides the setting (tests)
  static const char* s_env_scale = std::getenv("SKIES_EFB_SCALE");
  const int scale = s_env_scale ? std::atoi(s_env_scale) : std::max(0, Get(EFB_SCALE));
  if (scale != s_applied_efb_scale)
  {
    s_applied_efb_scale = scale;
    VISetFrameBufferScale(static_cast<float>(scale));
  }
  static const bool s_env_vsync = std::getenv("SKIES_VSYNC") != nullptr;
  const int vsync = s_env_vsync || Get(VSYNC) != 0 ? 1 : 0;
  if (vsync != s_applied_vsync)
  {
    // The window opened with the setting; later changes go to the swap chain
    if (s_applied_vsync >= 0)
      aurora_enable_vsync(vsync != 0);
    s_applied_vsync = vsync;
  }
#endif
}

void MenuFilterPad(PadStatus& pad)
{
  const Hotkeys keys = ReadHotkeys();
  const bool menu_key = keys.menu && !s_menu_key_down;
  s_menu_key_down = keys.menu;
  if (keys.stick_click && !s_stick_click && s_mode == Mode::Closed)
    qol::ToggleInvincible();
  s_stick_click = keys.stick_click;
  ApplyDeadZone(pad.stick_x, pad.stick_y, s_dead_zone);
  ApplyDeadZone(pad.sub_x, pad.sub_y, s_dead_zone);

  switch (s_mode)
  {
  case Mode::Closed:
    // While a Continue loads, the game gets the controller as it is and the menu stays shut
    if (autoload::s_file > 0)
      break;
    if (menu_key)
    {
      Open();
      pad = Released(pad);
      break;
    }
    pad = FilterClosed(pad);
    break;
  case Mode::Open:
    if (menu_key)
    {
      Close();
      pad = Released(pad);
      break;
    }
    pad = FilterOpen(pad);
    break;
  case Mode::Closing:
    // Only the buttons that act in the menu wait to be let go: the sticks and the D-pad reach the
    // game at once.
    if ((pad.buttons & CLOSING_BUTTONS) != 0 || pad.trig_l > 0xA0 || pad.trig_r > 0xA0)
    {
      pad = Released(pad);
    }
    else
    {
      s_mode = Mode::Closed;
      s_z_held_back = false;
      s_z_passed = false;
    }
    break;
  }
}

#ifdef SKIES_STANDALONE_AURORA
namespace
{
ImFont* s_ui_font = nullptr;
ImFont* s_title_font = nullptr;
bool s_overlay_init = false;
bool s_overlay_shown = false;  // the last frame drew something (an empty frame clears it once)
Clock::time_point s_last_frame{};

void LoadFonts()
{
  // The launchers' faces: Segoe UI on Windows, DejaVu Sans on Linux; ImGui's own otherwise.
  const auto add = [](std::initializer_list<std::string> candidates) -> ImFont* {
    for (const std::string& path : candidates)
    {
      if (path.empty())
        continue;
      if (std::FILE* f = std::fopen(path.c_str(), "rb"))
      {
        std::fclose(f);
        return ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str());
      }
    }
    return nullptr;
  };
  ImGui::GetIO().Fonts->AddFontDefault();
  const char* windir = std::getenv("windir");
  const std::string windows = windir ? std::string(windir) + "\\Fonts\\" : std::string();
  s_ui_font = add({windows.empty() ? "" : windows + "segoeui.ttf",
                   "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"});
  s_title_font = add({windows.empty() ? "" : windows + "segoeuib.ttf",
                      "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"});
  if (!s_title_font)
    s_title_font = s_ui_font;
}

// Aurora's present pass draws ImGui's draw data over the picture (fork 0026): the textures (the
// font atlas, which ImGui 1.92 grows as new sizes are drawn) uploaded whole when created or changed.
void Sink(ImDrawData* draw_data)
{
  static u32 s_next_texture = 1;
  if (draw_data->Textures != nullptr)
  {
    for (ImTextureData* tex : *draw_data->Textures)
    {
      if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates)
      {
        if (tex->Status == ImTextureStatus_WantCreate)
          tex->SetTexID(static_cast<ImTextureID>(s_next_texture++));
        aurora_backend_set_overlay_texture(static_cast<u32>(tex->GetTexID()),
                                           static_cast<u32>(tex->Width), static_cast<u32>(tex->Height),
                                           static_cast<const uint8_t*>(tex->GetPixels()));
        tex->SetStatus(ImTextureStatus_OK);
      }
      else if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames > 0)
      {
        tex->SetTexID(ImTextureID_Invalid);
        tex->SetStatus(ImTextureStatus_Destroyed);
      }
    }
  }
  static std::vector<u8> vertices;
  static std::vector<u16> indices;
  static std::vector<AuroraOverlayCmd> cmds;
  vertices.clear();
  indices.clear();
  cmds.clear();
  static_assert(sizeof(ImDrawVert) == 20, "the overlay expects ImGui's default vertex layout");
  static_assert(sizeof(ImDrawIdx) == 2, "the overlay expects 16-bit ImGui indices");
  const ImVec2 pos = draw_data->DisplayPos;
  for (int n = 0; n < draw_data->CmdListsCount; ++n)
  {
    const ImDrawList* list = draw_data->CmdLists[n];
    const u32 base_vertex = static_cast<u32>(vertices.size() / sizeof(ImDrawVert));
    const u32 base_index = static_cast<u32>(indices.size());
    const auto* vb = reinterpret_cast<const u8*>(list->VtxBuffer.Data);
    vertices.insert(vertices.end(), vb, vb + static_cast<size_t>(list->VtxBuffer.Size) * sizeof(ImDrawVert));
    indices.insert(indices.end(), list->IdxBuffer.Data, list->IdxBuffer.Data + list->IdxBuffer.Size);
    for (const ImDrawCmd& cmd : list->CmdBuffer)
    {
      if (cmd.UserCallback != nullptr || cmd.ElemCount == 0 || cmd.GetTexID() == ImTextureID_Invalid)
        continue;
      cmds.push_back(AuroraOverlayCmd{
          .clip = {cmd.ClipRect.x - pos.x, cmd.ClipRect.y - pos.y, cmd.ClipRect.z - pos.x, cmd.ClipRect.w - pos.y},
          .texture = static_cast<u32>(cmd.GetTexID()),
          .index_offset = base_index + cmd.IdxOffset,
          .index_count = cmd.ElemCount,
          .vertex_offset = base_vertex + cmd.VtxOffset,
      });
    }
  }
  aurora_backend_set_overlay(vertices.data(), static_cast<u32>(vertices.size() / sizeof(ImDrawVert)),
                             indices.data(), static_cast<u32>(indices.size()), cmds.data(),
                             static_cast<u32>(cmds.size()), draw_data->DisplaySize.x,
                             draw_data->DisplaySize.y);
}

const char* OnOff(bool value)
{
  return value ? "On" : "Off";
}

// Text with a dark edge, readable over any picture (messages, FPS)
void ShadowText(ImDrawList* draw, ImFont* font, float size, ImVec2 at, ImU32 colour, const char* text)
{
  const float d = std::max(1.0f, size / 14.0f);
  draw->AddText(font, size, ImVec2(at.x + d, at.y + d), IM_COL32(0, 0, 0, (colour >> 24) * 3 / 4), text);
  draw->AddText(font, size, at, colour, text);
}

void DrawMessages(float unit)
{
  const Clock::time_point now = Clock::now();
  s_messages.erase(std::remove_if(s_messages.begin(), s_messages.end(),
                                  [&](const Message& m) { return now >= m.until; }),
                   s_messages.end());
  ImFont* font = s_ui_font ? s_ui_font : ImGui::GetFont();
  const float size = std::floor(15.0f * unit);
  float y = 12.0f * unit;
  for (const Message& m : s_messages)
  {
    const float left = std::chrono::duration<float>(m.until - now).count();
    const int alpha = static_cast<int>(255 * std::clamp(left / 0.4f, 0.0f, 1.0f));
    ShadowText(ImGui::GetForegroundDrawList(), font, size, ImVec2(14.0f * unit, y),
               IM_COL32(255, 230, 140, alpha), m.text.c_str());
    y += size * 1.35f;
  }
}

void DrawFps(float unit)
{
  char text[32];
  std::snprintf(text, sizeof(text), "FPS %.0f", s_fps);
  ImFont* font = s_ui_font ? s_ui_font : ImGui::GetFont();
  const float size = std::floor(15.0f * unit);
  const float w = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x;
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ShadowText(ImGui::GetForegroundDrawList(), font, size,
             ImVec2(viewport->Size.x - w - 14.0f * unit, 12.0f * unit), IM_COL32(242, 242, 242, 255),
             text);
}

void Draw(float scale)
{
  const Values v = s_staged;
  const int tab = s_tab, selected = s_selected;

  struct Row
  {
    const char* label;
    std::string value;
    const char* description;
  };
  const bool patched = Read32(WS_CONFIG_ADDRESS) == WS_CONFIG_MAGIC;
  const bool saves = !StateDirectory().empty();
  const char* const no_saves = "Save states need a user folder (start with --user).";
  const auto row_of = [&](Option option) -> Row {
    switch (option)
    {
    case Option::SaveState:
      return {"Save state", "Slot " + std::to_string(s_save_slot) + "   " + SlotTime(s_save_slot),
              saves ? "Left / Right picks a slot, A saves to it." : no_saves};
    case Option::LoadState:
      return {"Load state",
              s_load_confirm   ? std::string("Press A again to load") :
              s_load_slot == 0 ? "Auto-save   " + SlotTime(0) :
                                 "Slot " + std::to_string(s_load_slot) + "   " + SlotTime(s_load_slot),
              saves ? "Left / Right picks a slot or the newest auto-save, A twice loads it." : no_saves};
    case Option::AutoSave:
      return {"Auto-save",
              v.auto_save <= 0 ? std::string("Off") : "Every " + std::to_string(v.auto_save) + " min",
              saves ? "Saves your place at quiet moments." : no_saves};
    case Option::Picture:
      return {"Picture",
              v.widescreen == 2 ? "Match screen" :
              v.widescreen      ? "Widescreen 16:9" :
                                  "Original 4:3",
              patched ? "Widescreen fills a 16:9 screen; Match screen fills any shape, ultrawide too; "
                        "Original keeps the GameCube's 4:3 picture." :
                        "Needs the game patch (widescreen DOL): this game runs unpatched, in 4:3."};
    case Option::Hud:
      return {"HUD",
              v.hud == 3 ? "21:9 area" :
              v.hud == 1 ? "16:9 area" :
              v.hud == 0 ? "Original 4:3" :
                           "Screen edges",
              "Where the HUD sits on a wide picture: at the edges, or nearer the middle in a 16:9 "
              "or 21:9 area (easier to see on an ultrawide). Original keeps the GameCube's layout."};
    case Option::Resolution:
      return {"Resolution", ResolutionText(v.efb_scale),
              "Higher is sharper but needs a faster graphics chip. Match window draws at the "
              "window's own size."};
    case Option::VSync:
      return {"V-Sync", OnOff(v.vsync), "Stops screen tearing. Can add a little input delay."};
    case Option::ShowFps:
      return {"Show FPS", OnOff(v.show_fps), "Show the frame rate in a corner of the screen."};
    case Option::Camera:
      return {"Camera", v.camera ? "Hold Z to move" : "Original",
              "Hold Z, then tilt with the C-stick and zoom with L / R. Original keeps the game's "
              "fixed camera."};
    case Option::BattleSpeed:
      return {"Battle speed", SpeedText(v.battle_speed),
              "Plays battles faster. Tap Z during a battle to switch speeds."};
    case Option::FieldSpeed:
      return {"Field speed", FieldSpeedText(v.field_speed),
              "Plays cutscenes, walking and sailing faster. Hold Z and press X outside battle to "
              "switch speeds."};
    case Option::Encounters:
      return {"Encounters", RateText(v.encounters), "How often random battles happen. Off means none."};
    case Option::HoldB:
      return {"Hold B", v.hold_b ? "No battles while held" : "Off",
              "While you hold B, on foot or sailing, no random battle starts."};
    case Option::Exp:
      return {"EXP", RateText(v.exp), "Experience from battles, for levelling up."};
    case Option::Gold:
      return {"Gold", RateText(v.gold), "Gold won from battles."};
    case Option::MagicExp:
      return {"Magic EXP", RateText(v.magic_exp), "Magic experience from battles, for learning new spells."};
    case Option::Invincible:
      return {"Invincible", v.invincible ? "On (L3 / R3)" : "Off (L3 / R3)",
              "Keeps the party's HP and MP full. L3 or R3 switches it in game."};
    case Option::Volume:
      return {"Volume", v.volume == 0 ? std::string("0% (muted)") : std::to_string(v.volume) + "%",
              "Music and sound effects."};
    case Option::DeadZone:
      return {"Stick dead zone", v.dead_zone <= 0 ? std::string("Off") : std::to_string(v.dead_zone) + "%",
              "Ignores small stick movements. Raise it if a stick drifts."};
    }
    return {"", "", ""};
  };
  const std::span<const Option> options = TABS[tab].rows;
  const bool on_tabs = selected == TAB_BAR;
  const std::string close = "    B or Esc: close";
  const Option current = on_tabs ? Option::Picture : options[selected];
  const std::string hint =
      on_tabs                          ? "Left / Right or L / R: tab    Down or A: settings" + close :
      current == Option::SaveState     ? "L / R: tab    Up / Down: choose    Left / Right: slot    A: save" + close :
      current == Option::LoadState     ? "L / R: tab    Up / Down: choose    Left / Right: slot    A: load" + close :
                                         "L / R: tab    Up / Down: choose    Left / Right or A: change" + close;

  // The layout is in units of the picture (a 480-line picture = 1 unit per pixel), times the
  // launchers' text size, shrunk to fit the screen. Every length comes from these constants, not
  // from last frame's measurements, so the menu can't settle between two sizes.
  constexpr float PAD = 22.0f, LABEL_W = 190.0f, VALUE_W = 340.0f, CONTENT_W = 690.0f;
  constexpr float TITLE = 24.0f, TEXT = 15.0f, SMALL = 12.5f;
  constexpr float TITLE_H = 34.0f, TAB_H = 28.0f, BOX_H = 28.0f, ROW_GAP = 9.0f, FOOTER_H = 30.0f;
  ImFont* const ui = s_ui_font ? s_ui_font : ImGui::GetFont();
  ImFont* const bold = s_title_font ? s_title_font : ui;
  const float desc_w = CONTENT_W - LABEL_W;
  // One row's height (units): its value box and its description, wrapped
  const auto row_height = [&](const Row& row, float unit) {
    const float px = std::max(1.0f, std::floor(SMALL * unit));
    const float desc = ui->CalcTextSizeA(px, FLT_MAX, desc_w * unit, row.description).y / unit;
    return BOX_H + 3.0f + desc + ROW_GAP;
  };
  const auto window_height = [&](float unit) {
    float tallest = 0.0f;
    for (const Tab& t : TABS)
    {
      float h = 0.0f;
      for (Option option : t.rows)
        h += row_height(row_of(option), unit);
      tallest = std::max(tallest, h);
    }
    return PAD + TITLE_H + 6.0f + TAB_H + 12.0f + tallest + FOOTER_H + PAD * 0.6f;
  };
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float base = std::max(scale, viewport->Size.y / 480.0f) * s_text_percent / 100.0f;
  const float width_units = CONTENT_W + 2.0f * PAD;
  float u = base;
  // Fit: twice, since the wrapped descriptions' height depends on the size
  for (int pass = 0; pass < 2; ++pass)
  {
    const float fit = std::min(viewport->Size.x * 0.94f / (width_units * u),
                               viewport->Size.y * 0.94f / (window_height(u) * u));
    if (fit >= 1.0f)
      break;
    u *= fit;
  }
  const auto px = [&](float size) { return std::max(1.0f, std::floor(size * u)); };

  const ImVec2 size(width_units * u, window_height(u) * u);
  const ImVec2 origin(viewport->Pos.x + (viewport->Size.x - size.x) * 0.5f,
                      viewport->Pos.y + (viewport->Size.y - size.y) * 0.5f);
  const auto at = [&](float x, float y) { return ImVec2(origin.x + x * u, origin.y + y * u); };
  ImDrawList* draw = ImGui::GetForegroundDrawList();
  const ImU32 white = IM_COL32(242, 242, 242, 255);
  const ImU32 gold = IM_COL32(255, 201, 74, 255);
  const ImU32 dim = IM_COL32(168, 184, 214, 255);
  const ImU32 blue = IM_COL32(41, 87, 173, 255);

  // The game dimmed behind, the window
  draw->AddRectFilled(viewport->Pos,
                      ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
                      IM_COL32(0, 5, 15, 110));
  draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(9, 14, 28, 242),
                      14.0f * u);
  draw->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(255, 255, 255, 56),
                14.0f * u, 0, std::max(1.0f, 1.5f * u));

  float y = PAD;
  draw->AddText(bold, px(TITLE), at(PAD, y), white, "Settings");
  y += TITLE_H + 6.0f;
  const float footer_y = size.y / u - PAD * 0.6f - FOOTER_H;

  // Tabs: the current one blue with a gold line on top
  float x = PAD;
  for (int i = 0; i < static_cast<int>(TABS.size()); ++i)
  {
    const float text_w = ui->CalcTextSizeA(px(TEXT), FLT_MAX, 0.0f, TABS[i].name).x / u;
    const float w = text_w + 26.0f;
    const ImVec2 a = at(x, y), b = at(x + w, y + TAB_H);
    draw->AddRectFilled(a, b, i == tab ? blue : IM_COL32(26, 41, 77, 230), 6.0f * u,
                        ImDrawFlags_RoundCornersTop);
    if (i == tab)
      draw->AddRectFilled(a, ImVec2(b.x, a.y + std::max(1.0f, 2.5f * u)), gold, 6.0f * u,
                          ImDrawFlags_RoundCornersTop);
    // The tab bar has the focus: the current tab gets the gold edge, like a selected value
    if (i == tab && on_tabs)
      draw->AddRect(a, b, gold, 6.0f * u, ImDrawFlags_RoundCornersTop, std::max(1.0f, 2.0f * u));
    draw->AddText(ui, px(TEXT), at(x + 13.0f, y + (TAB_H - TEXT * 1.33f) * 0.5f),
                  i == tab ? white : IM_COL32(242, 242, 242, 200), TABS[i].name);
    x += w + 6.0f;
  }
  y += TAB_H;
  draw->AddRectFilled(at(PAD, y), at(PAD + CONTENT_W, y + 1.5f), blue);
  y += 12.0f;

  // Rows: label, value box (the selected one gold-edged, with arrows), description
  for (int i = 0; i < static_cast<int>(options.size()); ++i)
  {
    const Row row = row_of(options[i]);
    const bool is_selected = !on_tabs && i == selected;
    const float text_y = y + (BOX_H - TEXT * 1.33f) * 0.5f;
    draw->AddText(ui, px(TEXT), at(PAD, text_y), is_selected ? gold : white, row.label);
    const ImVec2 box_a = at(PAD + LABEL_W, y), box_b = at(PAD + LABEL_W + VALUE_W, y + BOX_H);
    draw->AddRectFilled(box_a, box_b, is_selected ? IM_COL32(41, 66, 118, 255) : IM_COL32(26, 41, 77, 255),
                        6.0f * u);
    if (is_selected)
    {
      draw->AddRect(box_a, box_b, gold, 6.0f * u, 0, std::max(1.0f, 2.0f * u));
      // Arrows: Left / Right change the value
      const float mid = y + BOX_H * 0.5f, s = 5.0f;
      const float lx = PAD + LABEL_W + 10.0f, rx = PAD + LABEL_W + VALUE_W - 10.0f;
      draw->AddTriangleFilled(at(lx, mid), at(lx + s, mid - s), at(lx + s, mid + s), gold);
      draw->AddTriangleFilled(at(rx, mid), at(rx - s, mid - s), at(rx - s, mid + s), gold);
    }
    const bool confirm = options[i] == Option::LoadState && s_load_confirm;
    draw->AddText(ui, px(TEXT), at(PAD + LABEL_W + 22.0f, text_y), confirm ? gold : white,
                  row.value.c_str());
    draw->AddText(ui, px(SMALL), at(PAD + LABEL_W, y + BOX_H + 3.0f), dim, row.description, nullptr,
                  desc_w * u);
    y += row_height(row, u);
  }

  // Footer: the controls for the selected row
  draw->AddRectFilled(at(PAD, footer_y), at(PAD + CONTENT_W, footer_y + 1.0f), IM_COL32(255, 255, 255, 40));
  draw->AddText(ui, px(SMALL), at(PAD, footer_y + 9.0f), dim, hint.c_str());
}
}  // namespace
#endif

void MenuFrame()
{
  const Clock::time_point now = Clock::now();
  ++s_fps_frames;
  if (s_fps_since == Clock::time_point{})
    s_fps_since = now;
  const double elapsed = std::chrono::duration<double>(now - s_fps_since).count();
  if (elapsed >= 1.0)
  {
    s_fps = s_fps_frames / elapsed;
    s_fps_frames = 0;
    s_fps_since = now;
  }
#ifdef SKIES_STANDALONE_AURORA
  unsigned width = 0, height = 0;
  if (!VideoSurfaceSize(&width, &height) || width == 0 || height == 0)
    return;
  const bool show_fps = Get(SHOW_FPS) != 0;
  const bool anything = s_mode == Mode::Open || !s_messages.empty() || show_fps;
  if (!anything)
  {
    if (s_overlay_shown)
    {
      aurora_backend_set_overlay(nullptr, 0, nullptr, 0, nullptr, 0, static_cast<float>(width),
                                 static_cast<float>(height));
      s_overlay_shown = false;
    }
    return;
  }
  if (!s_overlay_init)
  {
    s_overlay_init = true;
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    LoadFonts();
  }
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
  const float dt = s_last_frame == Clock::time_point{} ?
                       1.0f / 60.0f :
                       std::chrono::duration<float>(now - s_last_frame).count();
  s_last_frame = now;
  io.DeltaTime = std::clamp(dt, 1.0f / 1000.0f, 0.5f);
  ImGui::NewFrame();
  // Messages and the counter sized to the picture's height, as the menu
  const float unit = std::max(1.0f, static_cast<float>(height) / 720.0f);
  DrawMessages(unit);
  if (show_fps)
    DrawFps(unit);
  if (s_mode == Mode::Open)
    Draw(1.0f);
  ImGui::Render();
  Sink(ImGui::GetDrawData());
  s_overlay_shown = true;
#endif
}
}  // namespace host
