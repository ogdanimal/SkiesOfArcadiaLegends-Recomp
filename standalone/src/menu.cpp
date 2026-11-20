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
#include <atomic>
#include <mutex>
#include <bit>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <vector>

#ifdef SKIES_STANDALONE_AURORA
#include <SDL3/SDL.h>
#include <imgui.h>

#include <aurora/aurora.h>
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
  ShipHud,
  FrameRate,
  Resolution,
  Antialiasing,
  TextureFilter,
  Anisotropy,
  Scaling,
  HdTextures,
  VSync,
  ShowFps,
  Renderer,
  Camera,
  MouseCamera,
  BattleSpeed,
  FieldSpeed,
  Encounters,
  HoldB,
  Exp,
  Gold,
  MagicExp,
  Invincible,
  Cheats,
  Volume,
  DeadZone,
  Rumble,
  ControllerLight,
  TouchControls,
  TouchOpacity,
  TouchSize,
  Mapping,
  Language,
  Music,
  Uncensored,
  Quit,
};

constexpr std::array SAVES_ROWS{Option::SaveState, Option::LoadState, Option::AutoSave, Option::Quit};
constexpr std::array GRAPHICS_ROWS{Option::Picture,    Option::Hud,          Option::ShipHud,
                                   Option::FrameRate,  Option::Resolution,   Option::Antialiasing,
                                   Option::TextureFilter, Option::Anisotropy, Option::Scaling,
#if defined(__ANDROID__) || defined(__APPLE__)
                                   Option::HdTextures, Option::VSync,        Option::ShowFps};
#else
                                   Option::HdTextures, Option::VSync,        Option::ShowFps,
                                   Option::Renderer};
#endif
#if defined(__ANDROID__)
constexpr std::array GAMEPLAY_ROWS{Option::Camera,     Option::BattleSpeed, Option::FieldSpeed,
                                   Option::Encounters, Option::HoldB,       Option::Exp,
                                   Option::Gold,       Option::MagicExp,    Option::Invincible,
                                   Option::Cheats};
#else
constexpr std::array GAMEPLAY_ROWS{Option::Camera,     Option::MouseCamera, Option::BattleSpeed,
                                   Option::FieldSpeed, Option::Encounters,  Option::HoldB,
                                   Option::Exp,        Option::Gold,        Option::MagicExp,
                                   Option::Invincible, Option::Cheats};
#endif
constexpr std::array LANGUAGE_ROWS{Option::Language};
constexpr std::array SOUND_ROWS{Option::Volume};
constexpr std::array DREAMCAST_ROWS{Option::Music, Option::Uncensored};
#if defined(__ANDROID__)
constexpr std::array CONTROLS_ROWS{Option::Mapping,       Option::DeadZone,     Option::Rumble,
                                   Option::ControllerLight, Option::TouchControls, Option::TouchOpacity,
                                   Option::TouchSize};
#else
constexpr std::array CONTROLS_ROWS{Option::Mapping, Option::DeadZone, Option::Rumble, Option::ControllerLight};
#endif
struct Tab
{
  const char* name;
  std::span<const Option> rows;
};
// Saves first, as in the Dolphin build: in game that's the one used most
constexpr std::array TABS{Tab{"Saves", SAVES_ROWS}, Tab{"Graphics", GRAPHICS_ROWS},
                          Tab{"Gameplay", GAMEPLAY_ROWS}, Tab{"Language", LANGUAGE_ROWS},
                          Tab{"Sound", SOUND_ROWS}, Tab{"Dreamcast", DREAMCAST_ROWS},
                          Tab{"Controls", CONTROLS_ROWS}};

// Save states (state.cpp) where and as the Dolphin build's menu kept them (StateDirectory: StateSaves,
// or another version's or a romhack's folder), so the launchers list, Continue from and back them up
// as before: the player's five slots are state-skies-slot1..5.sav, auto-saves
// recovery-skies-<date>-<time>.sav, the newest three kept. A slot the Dolphin build saved loads too
// (state.cpp converts its state), and the next save to it writes this host's.
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
// The Dolphin build's choices (Dolphin's enum values: TextureFilteringMode, AnisotropicFilteringMode,
// OutputResamplingMode)
constexpr std::array TEXTURE_FILTERS{0, 2, 1};
constexpr std::array ANISOTROPIES{-1, 1, 2, 3, 4};
constexpr std::array SCALINGS{0, 5, 6, 4};
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
// 0 = the ship battle HUD spread out with the rest, 1 = centred (issue #25)
constexpr Key SHIP_HUD{SettingFile::Main, "Skies", "ShipHud", 0};
// -1 (unset) and 0: 30 fps, 1: 60 fps (interp60.cpp)
constexpr Key FRAME_MODE{SettingFile::Main, "Skies", "FrameMode", -1};
constexpr Key CAMERA_CONTROL{SettingFile::Main, "Skies", "CameraControl", 1};
constexpr Key MOUSE_CAMERA{SettingFile::Main, "Skies", "MouseCamera", 0};  // desktop: the mouse turns the camera
constexpr Key CONTROLLER_LIGHT{SettingFile::Main, "Skies", "ControllerLight", 1};  // a pad's light by the game
constexpr Key ENCOUNTERS{SettingFile::Main, "Skies", "Encounters", 100};
constexpr Key EXP{SettingFile::Main, "Skies", "Exp", 100};
constexpr Key GOLD{SettingFile::Main, "Skies", "Gold", 100};
constexpr Key MAGIC_EXP{SettingFile::Main, "Skies", "MagicExp", 100};
constexpr Key BATTLE_SPEED{SettingFile::Main, "Skies", "BattleSpeed", 1};
constexpr Key FIELD_SPEED{SettingFile::Main, "Skies", "FieldSpeed", 100};
constexpr Key INVINCIBLE{SettingFile::Main, "Skies", "Invincible", 0};
// Cheat codes (cheats.cpp): the enabled ones of Cheats/<disc ID>.ini (then GameSettings/), applied every frame while on
constexpr Key CHEATS{SettingFile::Main, "Skies", "Cheats", 0};
// Desktop: the graphics API tried first at the next start (main.cpp): 0 Auto, 1 D3D12, 2 Vulkan, 3 D3D11, 4 GL ES
constexpr Key RENDERER{SettingFile::Main, "Skies", "Renderer", 0};
#if defined(_WIN32)
constexpr std::array RENDERERS{0, 1, 2, 3};
#else
constexpr std::array RENDERERS{0, 2, 4};
#endif
const char* RendererText(int r)
{
  return r == 1 ? "Direct3D 12" : r == 2 ? "Vulkan" : r == 3 ? "Direct3D 11" : r == 4 ? "OpenGL ES" : "Auto";
}
constexpr Key HOLD_B{SettingFile::Main, "Skies", "HoldB", 0};
constexpr Key LAUNCHER_TEXT_SIZE{SettingFile::Main, "Skies", "LauncherTextSize", 100};
constexpr Key AUTO_SAVE{SettingFile::Main, "Skies", "AutoSave", 10};
// The standalone's own keys from before it read the controller profile (ReadDeadZone / ReadRumble below):
// controller 1's stick dead zone
constexpr Key DEAD_ZONE{SettingFile::Main, "Skies", "StandaloneDeadZone", 0};
// Controller vibration on / off
constexpr Key RUMBLE{SettingFile::Main, "Skies", "StandaloneRumble", 1};
// The launchers (desktop and Android) keep the dead zone and rumble in controller 1's profile, as the
// Dolphin build reads them: [GCPad1] Main Stick/Dead Zone (and C-Stick), Rumble/Motor/Range (percent).
// Those come first; the standalone's own keys above are the fallback (states before 2026-11-18), and the
// menu writes both, so the launchers' rows and this menu show the same choice.
int ReadDeadZone()
{
  const int pad = SettingGet(SettingFile::Pad, "GCPad1", "Main Stick/Dead Zone", -1);
  return std::clamp(pad >= 0 ? pad : SettingGet(DEAD_ZONE.file, DEAD_ZONE.section, DEAD_ZONE.name, DEAD_ZONE.fallback), 0,
                    DEAD_ZONES.back());
}
int ReadRumble()
{
  const int pad = SettingGet(SettingFile::Pad, "GCPad1", "Rumble/Motor/Range", -1);
  return pad >= 0 ? (pad > 0 ? 1 : 0) : (SettingGet(RUMBLE.file, RUMBLE.section, RUMBLE.name, RUMBLE.fallback) != 0 ? 1 : 0);
}
// Android's touch controls: 0 Auto (hidden while a controller is in use), 1 Always, 2 Off
constexpr Key TOUCH_CONTROLS{SettingFile::Main, "Skies", "TouchControls", 0};
constexpr std::array TOUCH_MODES{0, 1, 2};
// Their look, percent: how opaque, and how big (and how far in from the edges)
constexpr Key TOUCH_OPACITY{SettingFile::Main, "Skies", "TouchOpacity", 75};
constexpr Key TOUCH_SIZE{SettingFile::Main, "Skies", "TouchSize", 100};
constexpr std::array TOUCH_OPACITIES{25, 50, 75, 100};
// Up to 110%: the layout is as tall as a 16:9 screen allows (past ~112% the stick meets L, the face buttons Z / R)
constexpr std::array TOUCH_SIZES{75, 90, 100, 110};
constexpr Key VOLUME{SettingFile::Main, "DSP", "Volume", 100};
// The European disc's language (0 English, 1 Deutsch, 2 Français, 3 Español): the IPL's, which the
// game reads once at boot (hw_exi.cpp InitSram)
constexpr Key LANGUAGE{SettingFile::Main, "Skies", "Language", 0};
constexpr Key EFB_SCALE{SettingFile::Gfx, "Settings", "InternalResolution", 0};
constexpr Key MSAA{SettingFile::Gfx, "Settings", "MSAA", 1};
constexpr Key TEXTURE_FILTER{SettingFile::Gfx, "Enhancements", "ForceTextureFiltering", 0};
constexpr Key ANISOTROPY{SettingFile::Gfx, "Enhancements", "MaxAnisotropy", -1};
constexpr Key SCALING{SettingFile::Gfx, "Enhancements", "OutputResampling", 0};
constexpr Key SHOW_FPS{SettingFile::Gfx, "Settings", "ShowFPS", 0, true};
constexpr Key VSYNC{SettingFile::Gfx, "Hardware", "VSync", 0, true};
// Dolphin's Load Custom Textures: the imported texture pack (hd_textures.cpp)
constexpr Key HD_TEXTURES{SettingFile::Gfx, "Settings", "HiresTextures", 0, true};
// The Dreamcast tab (dreamcast.cpp): the Dreamcast music, and the Japanese Dreamcast models
constexpr Key DC_MUSIC{SettingFile::Main, "Skies", "DreamcastMusic", 0};
constexpr Key UNCENSORED{SettingFile::Main, "Skies", "Uncensored", 0};

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
  int ship_hud = 0;  // 1 = the ship battle HUD centred (issue #25)
  int frame_mode = 0;
  int efb_scale = 0;
  int msaa = 1;
  int texture_filter = 0;  // 0 the game's, 1 nearest (Sharp), 2 linear (Smooth)
  int anisotropy = -1;     // -1 the game's, 0-4 = 1x-16x
  int scaling = 0;         // OutputResamplingMode
  bool texture_pack = false;  // a texture pack is imported (texture_pack.h), read with the rest
  int texture_pack_bc7 = 0;   // its BC7 textures, when this GPU can't draw them
  int hd_textures = 0;
  bool vsync = false;
  bool show_fps = false;
  int camera = 1;
  int mouse_camera = 0;
  int encounters = 100;
  int hold_b = 0;
  int exp = 100;
  int gold = 100;
  int magic_exp = 100;
  int battle_speed = 1;
  int field_speed = 100;
  int invincible = 0;
  int cheats = 0;
  int renderer = 0;
  int volume = 100;
  int language = 0;
  int dead_zone = 0;
  int rumble = 1;
  int controller_light = 1;
  int touch = 0;  // TOUCH_CONTROLS
  int touch_opacity = 75;
  int touch_size = 100;
  int auto_save = 10;
  bool dc_music_installed = false;  // the Dreamcast disc's music is imported (dreamcast.cpp)
  bool uncensor_available = false;  // the running game has converted uncensored files
  int music = 0;                    // 1 = the Dreamcast music
  int uncensored = 0;
};

enum class Mode
{
  Closed,
  Open,
  Closing,  // closed, but the buttons that closed it are still down: the game doesn't get them yet
};

Mode s_mode = Mode::Closed;
// Language (European disc): the game reads it once, at boot, so a second A on a changed language
// restarts the game in it. The values are applied and saved with the close; then the run ends and
// main starts it again (MenuTakeRestartRequest).
bool s_restart_confirm = false;
bool s_restart_requested = false;

// When the menu last closed, and whether a slow release has been logged since
Clock::time_point s_closing_since;
bool s_closing_logged = false;

// Controls, Button mapping: a page of its own over the tabs, a row per GameCube input (input.cpp)
// and two columns (controller, keyboard), then the two resets. A on a binding waits for the new one.
bool s_mapping = false;
int s_map_row = 0;
int s_map_column = 0;  // 0 = controller, 1 = keyboard
bool s_capturing = false;
std::vector<std::array<std::string, 2>> s_map_labels;  // the bindings as shown
// The hints name keyboard keys while the keyboard is the one in use: these, read when the menu opens
bool s_keyboard_hints = false;
bool s_has_keyboard = false;
struct HintKeys
{
  std::string a = "A", b = "B", x = "X", l = "L", r = "R";
};
HintKeys s_hint_keys;
int s_tab = 0;
int s_selected = 0;  // row within the tab, TAB_BAR = the tab bar itself (as in the launchers)
constexpr int TAB_BAR = -1;

// Taps and clicks on the menu (MenuTap): Draw records where the tabs, the rows and their value boxes are, as
// fractions of the screen; a tap found there waits for the game thread (FilterOpen) as a pending action.
enum class HitKind : int
{
  None,
  Tab,        // index: the tab
  Row,        // index: the row (its label)
  ValueLeft,  // the row's value box, left third: Left
  ValueMid,   // middle: A
  ValueRight, // right third: Right
  Outside,    // off the window: close, as B
};
struct HitArea
{
  float x0, y0, x1, y1;
  HitKind kind;
  int index;
};
std::mutex s_hits_lock;
std::vector<HitArea> s_hits;  // empty while the menu isn't drawn
std::atomic<int> s_tap{0};    // pending: kind << 8 | index + 1, 0 = none
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
// Quit game takes a second A too (play since the last save is lost)
bool s_quit_confirm = false;
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
int s_applied_msaa = -1;
int s_applied_texture_filter = -2;
int s_applied_anisotropy = -2;
int s_applied_scaling = -1;
// Cached for the audio path and the pad filter (read once per change, not per sample block)
int s_volume = 100;
int s_dead_zone = 0;
bool s_rumble = true;
// 60 fps is held off while the game runs fast (qol::Write)
bool s_interp_held = false;
// Whether a texture pack is imported: at the first frame, then whenever the menu opens
bool s_texture_pack = false;

// The hotkeys outside the GameCube's buttons: the menu (Esc, a controller's Back / Select) and
// Invincible (L3 / R3), from SDL directly.
bool s_menu_key_down = false;
// Android's Back (SkiesGameActivity.onBackPressed, any thread): counts as one menu key press
std::atomic<bool> s_back_request{false};
// F1 (desktop): save a state to the menu's slot, as the Dolphin build's F1 (issue #19), unless F1 is bound
std::atomic<bool> s_f1_request{false};
// The Invincible hotkey's keyboard key (none unless bound), latched as it goes down like the menu key
std::atomic<bool> s_invincible_request{false};
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

bool IsEurope()
{
  return GameRegion() == Region::Europe;
}

// A restart for a new language: the desktop starts its own executable again (main.cpp). Not on
// Android yet (the activity would have to be relaunched; untested), where it applies next start.
bool CanRestart()
{
  return IsEurope();
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
// The game patch's ws_ship_hud (ws35; free in earlier patches): 0 = spread out, 1 = centred.
constexpr u32 WS_SHIP_HUD_ADDRESS = 0x80002EEC;
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

// Anti-aliasing: the MSAA counts the device renders (Aurora probes them: WebGPU's 1 and 4, plus 2 and
// 8 where its Dawn and the GPU allow, as the Dolphin build's backends list them)
// (asked once the window is open; fixed for the run)
const std::vector<int>& MsaaModes()
{
  static const std::vector<int> s_modes = [] {
    std::vector<int> modes;
#ifdef SKIES_STANDALONE_AURORA
    std::array<u32, 8> counts{};
    const u32 n = std::min<u32>(aurora_get_msaa_modes(counts.data(), counts.size()), counts.size());
    modes.assign(counts.begin(), counts.begin() + n);
#endif
    if (modes.empty())
      modes.push_back(1);
    return modes;
  }();
  return s_modes;
}

// A setting's count as it runs: the largest mode not above it (a count from the Dolphin build this
// device doesn't have)
int MsaaRunning(int samples)
{
  int running = 1;
  for (const int mode : MsaaModes())
    if (mode <= samples)
      running = mode;
  return running;
}

// ---- options read from the settings ----------------------------------------------------------------
Values ReadValues()
{
  Values values;
  const int widescreen = Get(WIDESCREEN);
  values.widescreen = widescreen < 0 ? 1 : widescreen;
  const int hud = Get(HUD);
  values.hud = hud < 0 ? 2 : hud;
  values.ship_hud = Get(SHIP_HUD) == 1 ? 1 : 0;
  values.frame_mode = std::clamp(Get(FRAME_MODE), 0, 2);
  values.efb_scale = Get(EFB_SCALE);
  values.msaa = MsaaRunning(Get(MSAA));
  values.texture_filter = std::clamp(Get(TEXTURE_FILTER), 0, 2);
  values.anisotropy = std::clamp(Get(ANISOTROPY), -1, 4);
  values.scaling = Get(SCALING);
#ifdef SKIES_STANDALONE_AURORA
  values.texture_pack = TexturePackImported();
  values.texture_pack_bc7 = values.texture_pack ? TexturePackBc7Skipped() : 0;
  s_texture_pack = values.texture_pack;
#endif
  values.hd_textures = Get(HD_TEXTURES) != 0 ? 1 : 0;
  values.vsync = Get(VSYNC) != 0;
  values.show_fps = Get(SHOW_FPS) != 0;
  values.camera = Get(CAMERA_CONTROL) != 0 ? 1 : 0;
  values.mouse_camera = Get(MOUSE_CAMERA) != 0 ? 1 : 0;
  values.encounters = Get(ENCOUNTERS);
  values.hold_b = Get(HOLD_B) != 0 ? 1 : 0;
  values.exp = Get(EXP);
  values.gold = Get(GOLD);
  values.magic_exp = Get(MAGIC_EXP);
  values.battle_speed = Get(BATTLE_SPEED);
  values.field_speed = std::clamp(Get(FIELD_SPEED), 100, FIELD_SPEEDS.back());
  values.invincible = Get(INVINCIBLE) != 0 ? 1 : 0;
  values.cheats = Get(CHEATS) != 0 ? 1 : 0;
  values.renderer = Get(RENDERER);
  values.volume = std::clamp(Get(VOLUME), 0, 100);
  values.language = std::clamp(Get(LANGUAGE), 0, 3);
  values.dead_zone = ReadDeadZone();
  values.rumble = ReadRumble();
  values.controller_light = Get(CONTROLLER_LIGHT) != 0 ? 1 : 0;
  values.touch = std::clamp(Get(TOUCH_CONTROLS), 0, 2);
  values.touch_opacity = std::clamp(Get(TOUCH_OPACITY), 25, 100);
  values.touch_size = std::clamp(Get(TOUCH_SIZE), 75, 110);
  values.auto_save = std::max(0, Get(AUTO_SAVE));
#ifdef SKIES_STANDALONE_AURORA
  values.dc_music_installed = DreamcastMusicInstalled();
  values.uncensor_available = DreamcastUncensorAvailable();
#endif
  values.music = Get(DC_MUSIC) != 0 ? 1 : 0;
  values.uncensored = Get(UNCENSORED) != 0 ? 1 : 0;
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
  set(SHIP_HUD, values.ship_hud, before.ship_hud);
  set(FRAME_MODE, values.frame_mode, before.frame_mode);
  set(EFB_SCALE, values.efb_scale, before.efb_scale);
  if (values.efb_scale != before.efb_scale)
    FrontendResolutionChanged(values.efb_scale);
  set(MSAA, values.msaa, before.msaa);
  set(TEXTURE_FILTER, values.texture_filter, before.texture_filter);
  set(ANISOTROPY, values.anisotropy, before.anisotropy);
  set(SCALING, values.scaling, before.scaling);
  set(HD_TEXTURES, values.hd_textures, before.hd_textures);
  set(VSYNC, values.vsync, before.vsync);
  set(SHOW_FPS, values.show_fps, before.show_fps);
  set(CAMERA_CONTROL, values.camera, before.camera);
  set(MOUSE_CAMERA, values.mouse_camera, before.mouse_camera);
  set(ENCOUNTERS, values.encounters, before.encounters);
  set(HOLD_B, values.hold_b, before.hold_b);
  set(EXP, values.exp, before.exp);
  set(GOLD, values.gold, before.gold);
  set(MAGIC_EXP, values.magic_exp, before.magic_exp);
  set(BATTLE_SPEED, values.battle_speed, before.battle_speed);
  set(FIELD_SPEED, values.field_speed, before.field_speed);
  set(INVINCIBLE, values.invincible, before.invincible);
  set(CHEATS, values.cheats, before.cheats);
  set(RENDERER, values.renderer, before.renderer);
  set(VOLUME, values.volume, before.volume);
  set(LANGUAGE, values.language, before.language);
  set(DEAD_ZONE, values.dead_zone, before.dead_zone);
  set(RUMBLE, values.rumble, before.rumble);
  set(CONTROLLER_LIGHT, values.controller_light, before.controller_light);
  if (values.dead_zone != before.dead_zone)
  {
    SettingSet(SettingFile::Pad, "GCPad1", "Main Stick/Dead Zone", values.dead_zone);
    SettingSet(SettingFile::Pad, "GCPad1", "C-Stick/Dead Zone", values.dead_zone);
  }
  if (values.rumble != before.rumble)
    SettingSet(SettingFile::Pad, "GCPad1", "Rumble/Motor/Range", values.rumble ? 100 : 0);
  set(TOUCH_CONTROLS, values.touch, before.touch);
  set(TOUCH_OPACITY, values.touch_opacity, before.touch_opacity);
  set(TOUCH_SIZE, values.touch_size, before.touch_size);
  TouchModeSet(values.touch, values.touch_opacity, values.touch_size);
  set(AUTO_SAVE, values.auto_save, before.auto_save);
  set(DC_MUSIC, values.music, before.music);
  set(UNCENSORED, values.uncensored, before.uncensored);
  s_volume = values.volume;
  s_dead_zone = values.dead_zone;
  s_rumble = values.rumble != 0;
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

constexpr int FRAME_MODES[] = {0, 1, 2};

std::string FrameRateText(int mode)
{
  if (mode == 0)
    return "30 fps (original)";
  if (mode == 1)
    return "60 fps (interpolated)";
  const double hz = VideoDisplayHz();
  if (hz < 1.0)
    return "Display refresh (interpolated)";
  char text[64];
  std::snprintf(text, sizeof(text), "Display refresh (%.0f fps)", hz);
  return text;
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
std::string SlotPath(int slot)
{
  return (std::filesystem::path(StateDirectory()) / ("state-skies-slot" + std::to_string(slot) + ".sav"))
      .string();
}

constexpr std::string_view AUTO_PREFIX = "recovery-skies-";

// The auto-saves, oldest first (their names sort by time)
std::vector<std::filesystem::path> AutoSaves()
{
  std::vector<std::filesystem::path> found;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(StateDirectory(), ec))
  {
    const std::string name = entry.path().filename().string();
    if (name.starts_with(AUTO_PREFIX) && entry.path().extension() == ".sav")
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
  case Option::Quit:
    s_quit_confirm = false;
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
  case Option::ShipHud:
    v.ship_hud = v.ship_hud ? 0 : 1;
    break;
  case Option::FrameRate:
    v.frame_mode = Step(FRAME_MODES, v.frame_mode, direction, wrap);
    break;
  case Option::Resolution:
    v.efb_scale = Step(EFB_SCALES, v.efb_scale, direction, wrap);
    break;
  case Option::Antialiasing:
    v.msaa = Step(MsaaModes(), v.msaa, direction, wrap);
    break;
  case Option::TextureFilter:
    v.texture_filter = Step(TEXTURE_FILTERS, v.texture_filter, direction, wrap);
    break;
  case Option::Anisotropy:
    v.anisotropy = Step(ANISOTROPIES, v.anisotropy, direction, wrap);
    break;
  case Option::Scaling:
    v.scaling = Step(SCALINGS, v.scaling, direction, wrap);
    break;
  case Option::HdTextures:
    if (v.texture_pack)
      v.hd_textures = v.hd_textures ? 0 : 1;
    break;
  case Option::Music:
    if (v.dc_music_installed)
      v.music = v.music ? 0 : 1;
    break;
  case Option::Uncensored:
    if (v.uncensor_available)
      v.uncensored = v.uncensored ? 0 : 1;
    break;
  case Option::VSync:
    v.vsync = !v.vsync;
    break;
  case Option::ShowFps:
    v.show_fps = !v.show_fps;
    break;
  case Option::Renderer:
    v.renderer = Step(RENDERERS, v.renderer, direction, wrap);
    break;
  case Option::Camera:
    v.camera = v.camera ? 0 : 1;
    break;
  case Option::MouseCamera:
    v.mouse_camera = v.mouse_camera ? 0 : 1;
    break;
  case Option::DeadZone:
    v.dead_zone = Step(DEAD_ZONES, v.dead_zone, direction, wrap);
    break;
  case Option::ControllerLight:
    v.controller_light = v.controller_light ? 0 : 1;
    break;
  case Option::Rumble:
    v.rumble = v.rumble ? 0 : 1;
    break;
  case Option::TouchControls:
    v.touch = Step(TOUCH_MODES, v.touch, direction, wrap);
    break;
  case Option::TouchOpacity:
    v.touch_opacity = Step(TOUCH_OPACITIES, v.touch_opacity, direction, wrap);
    break;
  case Option::TouchSize:
    v.touch_size = Step(TOUCH_SIZES, v.touch_size, direction, wrap);
    break;
  case Option::Mapping:
    break;
  case Option::Language:
    if (IsEurope())
      v.language = (v.language + direction + 4) % 4;
    s_restart_confirm = false;
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
  case Option::Cheats:
    v.cheats = v.cheats ? 0 : 1;
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

// The keyboard's keys for the hints' buttons (rows A, B, X, L, R of the mapping page)
void ReadHintKeys()
{
  const auto key = [](int row, const char* fallback) {
    const std::string label = InputBindingLabel(row, InputSlot::Keyboard);
    return label.empty() || label == "-" ? std::string(fallback) : label;
  };
  s_hint_keys = {key(0, "A"), key(1, "B"), key(2, "X"), key(6, "L"), key(7, "R")};
}

void Open()
{
  s_tap.store(0);  // a tap from before it opened isn't for it
  s_mode = Mode::Open;
  s_tab = 0;
  s_selected = 0;
  s_text_percent = std::clamp(Get(LAUNCHER_TEXT_SIZE), 80, 160);
  s_current = ReadValues();
  s_staged = s_current;
  s_previous = 0xFFFF;  // nothing counts as pressed until it's released once
  s_repeat_button = 0;
  s_load_confirm = false;
  s_restart_confirm = false;
  s_quit_confirm = false;
  s_mapping = false;
  s_capturing = false;
  s_keyboard_hints = InputKeyboardInUse();
  s_has_keyboard = InputHasKeyboard();
  ReadHintKeys();
  ReadSlotTimes();
  CheatsReload();  // an edited cheat file counts at the next open
  InputHotkeysReload();  // and the hotkeys as the settings now hold them (the mapping page writes them)
  Log("[menu] open\n");
}

void Close()
{
  s_mode = Mode::Closing;
  s_closing_since = Clock::now();
  s_closing_logged = false;
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
// ---- the mouse camera (desktop) ----------------------------------------------------------------------
// Dusklight's mouse look (src/dusk/mouse.cpp, TwilitRealm/dusklight): with [Skies] MouseCamera on, a click
// on the picture takes the mouse (SDL's relative mode) while the menu is closed; Esc, the menu or another
// window lets it go. Its motion goes to the camera control (camera::ApplyMouse) where that works (the field
// and the world map), the wheel zooms. Anywhere, the cursor hides after a second without moving.
namespace mouse
{
std::atomic<float> s_dx{0.0f}, s_dy{0.0f}, s_wheel{0.0f};  // since the game's last poll
std::atomic<bool> s_wanted{false};    // the setting on and the menu closed (the game's thread)
std::atomic<bool> s_captured{false};  // the video thread's
std::atomic<s64> s_moved_ns{0};       // the cursor's last motion
std::atomic<bool> s_cursor_hidden{false};

s64 NowNs()
{
  return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

#ifdef SKIES_STANDALONE_AURORA
void Release(SDL_Window* window)
{
  if (!s_captured.exchange(false))
    return;
  if (window)
    SDL_SetWindowRelativeMouseMode(window, false);
  s_dx = s_dy = s_wheel = 0.0f;
}

void ReleaseAny()
{
  int count = 0;
  SDL_Window** windows = SDL_GetWindows(&count);
  Release(windows && count > 0 ? windows[0] : nullptr);
  SDL_free(windows);
}

// From MenuKeyWatch, on the video thread. True: the event was the mouse camera's (not a menu tap).
bool Event(SDL_Event* event)
{
  switch (event->type)
  {
  case SDL_EVENT_MOUSE_MOTION:
    if (s_captured)
    {
      s_dx.fetch_add(event->motion.xrel);
      s_dy.fetch_add(event->motion.yrel);
      return true;
    }
    s_moved_ns = NowNs();
    if (s_cursor_hidden.exchange(false))
      SDL_ShowCursor();
    return false;
  case SDL_EVENT_MOUSE_WHEEL:
    if (s_captured)
      s_wheel.fetch_add(event->wheel.y);
    return s_captured;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    if (s_captured)
      return true;
    if (event->button.button == SDL_BUTTON_LEFT && s_wanted)
    {
      if (SDL_Window* window = SDL_GetWindowFromEvent(event); window && SDL_SetWindowRelativeMouseMode(window, true))
      {
        s_captured = true;
        s_dx = s_dy = s_wheel = 0.0f;
        return true;
      }
    }
    return false;
  case SDL_EVENT_WINDOW_FOCUS_LOST:
    Release(SDL_GetWindowFromEvent(event));
    return false;
  default:
    return false;
  }
}
#endif
}  // namespace mouse

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

// The mouse camera (mouse:: above): its motion since the last poll on the offsets: up and down tilt, the
// wheel zooms, and on the world map left and right turn. In the field the game turns its own camera (and
// Vyse walks by it): the turn goes to the game as C-stick X, which fn_80090DBC makes a turn of
// X / -44 * 512 a frame, steered toward the yaw the mouse asks for (the camera object's target yaw, +0x74,
// in the rooms of camera type 2, else its output yaw). Returns the C-stick X to send (STICK_CENTER: the
// pad's own). SKIES_MOUSE_TRACE=1: each poll in the log.
constexpr float MOUSE_TURN = 0.15f * DEGREES;  // per pixel (Dusklight's 0.0025 rad is 0.14 degrees)
constexpr float MOUSE_TILT = 0.12f * DEGREES;
constexpr float MOUSE_ZOOM = 0.08f;  // per wheel notch
const RegionAddress FIELD_CAMERA_TYPE{0x80302F20, 0x803065A0, 0x803029A0};  // the object's +0x8 halfword
const RegionAddress FIELD_YAW_TARGET{0x80302F8C, 0x8030660C, 0x80302A0C};   // +0x74
struct MouseYaw
{
  bool active = false;
  s32 goal = 0;  // 0..0xFFFF
  int settled = 0;
};
MouseYaw s_mouse_yaw;

s32 Wrap16(s32 v)
{
  v &= 0xFFFF;
  return v >= 0x8000 ? v - 0x10000 : v;
}

int ApplyMouse(const Context& context, float dx, float dy, float wheel)
{
  Offsets& offsets = context.world_map ? s_world : s_field;
  const Limits& limits = context.world_map ? WORLD_LIMITS : FIELD_LIMITS;
  offsets.pitch -= dy * MOUSE_TILT;
  offsets.zoom = std::clamp(offsets.zoom - wheel * MOUSE_ZOOM, limits.zoom_lo, limits.zoom_hi);
  offsets.pitch = std::clamp(offsets.pitch, std::min(0.0f, limits.pitch_lo - context.game_pitch),
                             std::max(0.0f, limits.pitch_hi - context.game_pitch));
  if (context.world_map)
  {
    offsets.yaw -= dx * MOUSE_TURN;
    s_mouse_yaw.active = false;
    return STICK_CENTER;
  }
  const bool target = (Read32(FIELD_CAMERA_TYPE) >> 16) == 2;
  const s32 current = static_cast<s32>(Read32(target ? FIELD_YAW_TARGET : FIELD_CAMERA_PITCH + 4) & 0xFFFF);
  MouseYaw& yaw = s_mouse_yaw;
  if (dx != 0.0f)
  {
    if (!yaw.active)
      yaw.goal = current;
    yaw.active = true;
    yaw.settled = 0;
    yaw.goal = (yaw.goal + static_cast<s32>(std::lround(-dx * MOUSE_TURN))) & 0xFFFF;
  }
  if (!yaw.active)
    return STICK_CENTER;
  const s32 error = Wrap16(yaw.goal - current);
  static const bool s_trace = std::getenv("SKIES_MOUSE_TRACE") != nullptr;
  if (s_trace)
    Log("[mouse] dx %.1f dy %.1f: yaw %04x goal %04x (%s) error %d\n", dx, dy, current, yaw.goal,
        target ? "target" : "output", error);
  if (std::abs(error) < 16)
  {
    // There: the game's own camera again after a moment (its follow, the pad's C-stick)
    if (++yaw.settled > 30)
      yaw.active = false;
    return STICK_CENTER;
  }
  // The stick that turns the rest of the way in one frame, at most fully over
  const int stick = std::clamp(static_cast<int>(std::lround(-error * 44.0f / 512.0f)), -100, 100);
  return STICK_CENTER + stick;
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

// The controllers' colour light (Dusklight's gamepad_color.cpp: the light follows the game), every poll on the
// game's thread: sky blue in the field, lighter over the world map, red-orange in battle, easing between
// them. [Skies] ControllerLight 0 leaves the light alone (from the moment it's turned off).
void ControllerLight()
{
  static bool s_was_on = false;
  if (Get(CONTROLLER_LIGHT) == 0)
  {
    s_was_on = false;
    return;
  }
  const u32 mode = Read32(camera::GAME_MODE);
  const bool battle = mode == 7;
  const bool world = mode == 6 && Read32(camera::MAP_ID) == camera::WORLD_MAP;
  const float target[3] = {battle ? 255.0f : world ? 90.0f : 40.0f, battle ? 60.0f : world ? 190.0f : 120.0f,
                           battle ? 20.0f : 255.0f};
  static float s_color[3] = {40.0f, 120.0f, 255.0f};
  static u8 s_sent[3] = {0, 0, 0};
  static u64 s_last = 0;
  const u64 now = Now();
  const float dt = s_last == 0 ? 1.0f : std::clamp(static_cast<float>(now - s_last) / CPU_HZ, 0.0f, 1.0f);
  s_last = now;
  const float k = std::min(1.0f, dt * 3.0f);  // about a third of a second
  u8 c[3];
  for (int i = 0; i < 3; ++i)
  {
    s_color[i] += (target[i] - s_color[i]) * k;
    c[i] = static_cast<u8>(std::clamp(std::lround(s_color[i]), 0l, 255l));
  }
  if (s_was_on && c[0] == s_sent[0] && c[1] == s_sent[1] && c[2] == s_sent[2])
    return;
  s_was_on = true;
  std::memcpy(s_sent, c, 3);
  VideoPadColor(c[0], c[1], c[2]);
}

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

// The player choosing commands in a battle, which plays at normal speed (at speed the command wheel
// turns too fast to pick from): a ground battle's command input task (fn_8007CAB0) in the game's task
// list (lbl_803475A8 -> the root task, linked through +4; the wheel's and the party panels' tasks
// come and go with it), a ship battle's turn not playing (lbl_803472DC, 1.0 while it plays).
const RegionAddress TASK_ROOT{0x803475A8, 0x8034ACC0, 0x80347028};
const RegionAddress COMMAND_INPUT_TASK{0x8007CAB0, 0x8007CE74, 0x8007CA98};
const RegionAddress SHIP_TURN_PLAYING{0x803472DC, 0x8034A9F4, 0x80346D5C};

bool ChoosingCommands()
{
  if (InShipBattle())
    return Read32(SHIP_TURN_PLAYING) == 0;
  if (Read32(GAME_MODE) != 7)
    return false;
  const u32 root = Read32(TASK_ROOT);
  const u32 input = COMMAND_INPUT_TASK;
  u32 task = root;
  for (int i = 0; i < 1024 && task >= 0x80000000u && task < 0x81800000u; ++i)
  {
    if (Read32(task) == input)
      return true;
    task = Read32(task + 4);
    if (task == root)
      break;
  }
  return false;
}

void Write()
{
  const Place place = WhereNow();
  const int wanted = place == Place::Field ? (FieldSpeed() * 4 + 50) / 100 :
                                             std::clamp(Get(BATTLE_SPEED), 1, 4) * 4;
  const int quarters = place == Place::Battle && wanted > 4 && ChoosingCommands() ?
                           4 :
                           EffectiveQuarters(place, wanted);
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
  // 60 fps is held off while the game runs fast, as in the Dolphin build: its frames come quicker
  // than in-between ones could add to. A spell or special attack in a battle at speed plays at normal
  // speed (the game patch's qol_normal_speed, ws28), so interpolation comes back for it.
  constexpr u32 NORMAL_SPEED = 0x80002BDC;
  s_interp_held = fast && !(place == Place::Battle && Read32(NORMAL_SPEED) == 1);
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
  // The mouse camera: wanted while its setting is on and the menu closed; the motion goes to the camera
  // where the camera control works (else it's dropped, as the cursor is the game's then)
  {
    const bool wanted = Get(MOUSE_CAMERA) != 0 && s_mode != Mode::Open;
    mouse::s_wanted = wanted;
#if defined(SKIES_STANDALONE_AURORA) && !defined(__ANDROID__)
    if (!wanted && mouse::s_captured)
      VideoPost([] { mouse::ReleaseAny(); });
    // The cursor out of the way a second after it stopped (it comes back as it moves: mouse::Event)
    const s64 moved = mouse::s_moved_ns.load();
    if (moved != 0 && !mouse::s_cursor_hidden && mouse::NowNs() - moved > 1'000'000'000)
    {
      mouse::s_cursor_hidden = true;
      VideoPost([] {
        if (mouse::s_cursor_hidden)
          SDL_HideCursor();
      });
    }
#endif
  }
  ControllerLight();
  int mouse_stick = STICK_CENTER;
  // SKIES_MOUSE_SCRIPT=+T:DX,DY[,WHEEL];... (tests): that motion at T guest seconds after the start, as if
  // the mouse were taken (no window or pointer needed)
  static const std::vector<std::array<double, 4>> s_mouse_script = [] {
    std::vector<std::array<double, 4>> list;
    const char* spec = std::getenv("SKIES_MOUSE_SCRIPT");
    for (std::string rest = spec ? spec : ""; !rest.empty();)
    {
      const size_t semi = rest.find(';');
      const std::string one = rest.substr(0, semi);
      rest = semi == std::string::npos ? "" : rest.substr(semi + 1);
      std::array<double, 4> e{0, 0, 0, 0};
      if (std::sscanf(one.c_str(), "+%lf:%lf,%lf,%lf", &e[0], &e[1], &e[2], &e[3]) >= 3)
        list.push_back(e);
    }
    return list;
  }();
  static size_t s_mouse_next = 0;
  const bool scripted = !s_mouse_script.empty();
  while (scripted && s_mouse_next < s_mouse_script.size() &&
         static_cast<double>(Now()) / CPU_HZ >= g_run_start_seconds + s_mouse_script[s_mouse_next][0])
  {
    const auto& e = s_mouse_script[s_mouse_next++];
    mouse::s_dx.fetch_add(static_cast<float>(e[1]));
    mouse::s_dy.fetch_add(static_cast<float>(e[2]));
    mouse::s_wheel.fetch_add(static_cast<float>(e[3]));
  }
  if (mouse::s_captured || scripted)
  {
    const float dx = mouse::s_dx.exchange(0.0f), dy = mouse::s_dy.exchange(0.0f);
    const float wheel = mouse::s_wheel.exchange(0.0f);
    if (camera_on)
      mouse_stick = camera::ApplyMouse(context, dx, dy, wheel);
  }
  if (camera_on)
  {
    if (camera::Update(context, raw, s_z_held_back && z, dt))
      s_z_camera_used = true;
    camera::Write(context, true);
    if (mouse_stick != STICK_CENTER && !context.world_map)
      out.sub_x = static_cast<u8>(mouse_stick);
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

// The mapping page's rows: the inputs, then the two resets
int MappingRows()
{
  return InputMappingRows() + 2;
}

void ReadMapping()
{
  const int count = InputMappingRows();
  s_map_labels.resize(count);
  for (int i = 0; i < count; ++i)
    s_map_labels[i] = {InputBindingLabel(i, InputSlot::Controller), InputBindingLabel(i, InputSlot::Keyboard)};
  ReadHintKeys();
}

void OpenMapping()
{
  s_mapping = true;
  s_map_row = 0;
  s_map_column = InputKeyboardInUse() ? 1 : 0;
  s_capturing = false;
  ReadMapping();
}

PadStatus FilterMapping(const PadStatus& raw)
{
  const InputSlot slot = s_map_column ? InputSlot::Keyboard : InputSlot::Controller;
  if (s_capturing)
  {
    // The press goes to the binding, not the menu, and counts once it's been released
    if (!InputPollCapture())
    {
      s_capturing = false;
      s_previous = 0xFFFF;
      ReadMapping();
    }
    return Released(raw);
  }
  const u16 pressed = Pressed(MenuButtons(raw), false);
  const int count = InputMappingRows();
  const int rows = MappingRows();
  if (pressed & PAD_BUTTON_UP)
    s_map_row = (s_map_row + rows - 1) % rows;
  else if (pressed & PAD_BUTTON_DOWN)
    s_map_row = (s_map_row + 1) % rows;
  else if (pressed & (PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT))
    s_map_column = s_map_column ? 0 : 1;
  else if (pressed & PAD_BUTTON_A)
  {
    if (s_map_row < count)
    {
      s_capturing = InputStartCapture(s_map_row, slot);
    }
    else
    {
      const bool controller = s_map_row == count;
      if (InputResetDefaults(controller ? InputSlot::Controller : InputSlot::Keyboard))
      {
        ReadMapping();
        MenuMessage(controller ? "Controller buttons reset" : "Keyboard keys reset", 2000);
      }
    }
  }
  else if ((pressed & PAD_BUTTON_X) && s_map_row < count && InputClearable(s_map_row, slot))
  {
    InputClear(s_map_row, slot);
    ReadMapping();
  }
  else if (pressed & (PAD_BUTTON_B | PAD_BUTTON_START))
  {
    s_mapping = false;
  }
  return Released(raw);
}

PadStatus FilterOpen(const PadStatus& raw)
{
  s_keyboard_hints = InputKeyboardInUse();
  if (s_mapping)
  {
    s_tap.store(0);  // the mapping page takes the controller only
    return FilterMapping(raw);
  }
  // A tap or click: select what it hit, then act on it as the matching button
  u16 tapped = 0;
  if (const int tap = s_tap.exchange(0); tap != 0)
  {
    const HitKind kind = static_cast<HitKind>(tap >> 8);
    const int index = (tap & 0xFF) - 1;
    if (kind == HitKind::Outside)
      tapped = PAD_BUTTON_B;
    else if (kind == HitKind::Tab && index >= 0 && index < static_cast<int>(TABS.size()))
    {
      if (s_tab != index)
      {
        s_tab = index;
        s_load_confirm = s_restart_confirm = s_quit_confirm = false;
      }
      s_selected = TAB_BAR;
    }
    else if (index >= 0 && index < static_cast<int>(TABS[s_tab].rows.size()))
    {
      if (s_selected != index)
      {
        s_selected = index;
        s_load_confirm = s_restart_confirm = s_quit_confirm = false;
      }
      tapped = kind == HitKind::ValueLeft ? PAD_BUTTON_LEFT :
               kind == HitKind::ValueRight ? PAD_BUTTON_RIGHT :
               kind == HitKind::ValueMid  ? PAD_BUTTON_A :
                                            0;
    }
  }
  const std::span<const Option> rows = TABS[s_tab].rows;
  const bool on_tabs = s_selected == TAB_BAR;
  const u16 pressed = static_cast<u16>(Pressed(MenuButtons(raw), !on_tabs && rows[s_selected] == Option::Volume) |
                                       tapped);
  const int tab_count = static_cast<int>(TABS.size());
  const auto switch_tab = [&](int direction) {
    s_tab = (s_tab + direction + tab_count) % tab_count;
    s_load_confirm = false;
    s_restart_confirm = false;
    s_quit_confirm = false;
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
  {
    s_load_confirm = false;
    s_restart_confirm = false;
    s_quit_confirm = false;
  }
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
  else if ((pressed & PAD_BUTTON_A) && option == Option::Quit)
  {
    // Back to the launcher (Android) or the desktop: the card is flushed on the way out
    if (s_quit_confirm)
    {
      Close();
      Log("[menu] quit\n");
      RequestQuit();
    }
    s_quit_confirm = !s_quit_confirm;
  }
  else if ((pressed & PAD_BUTTON_A) && option == Option::Mapping)
    OpenMapping();
  else if ((pressed & PAD_BUTTON_A) && option == Option::Language && s_staged.language != s_current.language &&
           CanRestart())
  {
    // Restarting throws away the play since the last save, so it takes a second A too.
    if (s_restart_confirm)
    {
      Close();
      s_restart_requested = true;
      Log("[menu] restart for the language\n");
    }
    s_restart_confirm = !s_restart_confirm;
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

std::chrono::seconds Interval(int minutes)
{
  // SKIES_AUTO_SAVE_SECONDS=N: every N seconds instead (tests). Read on first use, not at static init:
  // Android applies env.txt in main().
  static const int s_test_seconds = [] {
    const char* v = std::getenv("SKIES_AUTO_SAVE_SECONDS");
    return v ? std::atoi(v) : 0;
  }();
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
      (std::filesystem::path(StateDirectory()) / (std::string(AUTO_PREFIX) + stamp + ".sav")).string();
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
  bool menu = false;        // Esc / Android Back, or a controller's Back / Select
  bool stick_click = false;  // L3 or R3
};


#ifdef SKIES_STANDALONE_AURORA
// Esc / Back latched as they go down (the video thread pumps SDL's events): polling the keyboard state missed a
// tap shorter than a pad read (a quick Back, adb's input keyevent), and polling it as well would count a press twice.
bool SDLCALL MenuKeyWatch(void*, SDL_Event* event)
{
  // The Menu hotkey's key (Escape unless rebound) and Android's Back key
  if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat &&
      (static_cast<int>(event->key.scancode) == InputHotkeyKey(0) || event->key.scancode == SDL_SCANCODE_AC_BACK))
  {
    s_back_request.store(true);
    mouse::Release(SDL_GetWindowFromEvent(event));  // the menu opens: the mouse is the player's again
  }
#if !defined(__ANDROID__)
  if (mouse::Event(event))
    return true;
#endif
  if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat && event->key.scancode == SDL_SCANCODE_F1)
    s_f1_request.store(true);
  if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat &&
      static_cast<int>(event->key.scancode) == InputHotkeyKey(1))
    s_invincible_request.store(true);
  // A left click on the menu (a desktop touch screen's tap arrives as one too)
  if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT)
  {
    int w = 0, h = 0;
    if (SDL_Window* window = SDL_GetWindowFromEvent(event); window && SDL_GetWindowSize(window, &w, &h) && w > 0 &&
                                                              h > 0)
      MenuTap(event->button.x / static_cast<float>(w), event->button.y / static_cast<float>(h));
  }
  return true;
}
#endif

Hotkeys ReadHotkeys()
{
  Hotkeys keys;
#ifdef SKIES_STANDALONE_AURORA
  if (!VideoSurfaceSize(nullptr, nullptr))
    return keys;
  // The bindings MenuKeyWatch reads (loaded here, on the game thread)
  InputHotkeysRefresh();
  static bool s_watching = false;
  if (!s_watching)
    s_watching = SDL_AddEventWatch(MenuKeyWatch, nullptr);
  // Esc and Android's Back key (SDL's AC_BACK) count through MenuKeyWatch's latch only, once per press; a
  // gesture Back comes through MenuRequestBack
  int count = 0;
  if (SDL_JoystickID* ids = SDL_GetGamepads(&count))
  {
    for (int i = 0; i < count; ++i)
    {
      SDL_Gamepad* pad = SDL_GetGamepadFromID(ids[i]);
      if (!pad)
        continue;
      // As the profile binds them (InputHotkeyHeld: Back and the stick clicks unless rebound)
      keys.menu |= InputHotkeyHeld(pad, 0);
      keys.stick_click |= InputHotkeyHeld(pad, 1);
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

bool MenuTap(float x, float y)
{
  if (VideoShaderPrepSkip())
    return true;  // the start's shader preparation took it as its skip
  std::lock_guard lk(s_hits_lock);
  if (s_hits.empty())
    return false;  // the menu isn't open
  // The last area holding the point is the most specific (the window comes first)
  const auto in = [&](const HitArea& h) { return x >= h.x0 && x < h.x1 && y >= h.y0 && y < h.y1; };
  const auto found = std::find_if(s_hits.rbegin(), s_hits.rend(), in);
  const HitKind kind = found == s_hits.rend() ? HitKind::Outside : found->kind;
  if (kind != HitKind::None)
    s_tap.store(static_cast<int>(kind) << 8 | ((found == s_hits.rend() ? 0 : found->index) + 1));
  return true;
}

void MenuContinue(int file)
{
  autoload::s_requested = file;
}

void MenuRequestBack()
{
  if (VideoShaderPrepSkip())
    return;  // skips the start's shader preparation rather than opening the menu once the game starts
  Log("[menu] Back pressed (Android)\n");
  s_back_request.store(true);
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

bool MenuTakeRestartRequest()
{
  const bool requested = s_restart_requested;
  s_restart_requested = false;
  return requested;
}

bool MenuInterpHeld()
{
  return s_interp_held;
}

bool MenuRumbleAllowed()
{
  return s_rumble && s_mode != Mode::Open;
}

void MenuApplyGame()
{
  static bool s_first = true;
  if (s_first)
  {
    s_first = false;
    s_volume = std::clamp(Get(VOLUME), 0, 100);
    s_dead_zone = ReadDeadZone();
    s_rumble = ReadRumble() != 0;
    TouchModeSet(std::clamp(Get(TOUCH_CONTROLS), 0, 2), std::clamp(Get(TOUCH_OPACITY), 25, 100),
                 std::clamp(Get(TOUCH_SIZE), 75, 110));
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
    const u32 ship_hud = Get(SHIP_HUD) == 1 ? 1 : 0;
    if (Read32(WS_SHIP_HUD_ADDRESS) != ship_hud)
    {
      Write32(WS_SHIP_HUD_ADDRESS, ship_hud);
      Log("[menu] ship battle HUD %s\n", ship_hud ? "centred" : "spread out");
    }
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
    VideoPost([scale] { VISetFrameBufferScale(static_cast<float>(scale)); });
  }
  // Anti-aliasing, texture filtering, anisotropy and scaling: live in Aurora (fork 0038)
  const int msaa = MsaaRunning(Get(MSAA));
  if (msaa != s_applied_msaa)
  {
    s_applied_msaa = msaa;
    Log("[menu] anti-aliasing %s\n", msaa > 1 ? (std::to_string(msaa) + "x MSAA").c_str() : "off");
    VideoPost([msaa] { aurora_set_msaa(static_cast<u32>(msaa)); });
  }
  const int texture_filter = std::clamp(Get(TEXTURE_FILTER), 0, 2);
  if (texture_filter != s_applied_texture_filter)
  {
    s_applied_texture_filter = texture_filter;
    VideoPost([texture_filter] { aurora_set_texture_filtering(texture_filter); });
  }
  const int anisotropy = std::clamp(Get(ANISOTROPY), -1, 4);
  if (anisotropy != s_applied_anisotropy)
  {
    s_applied_anisotropy = anisotropy;
    VideoPost([anisotropy] { aurora_set_max_anisotropy(anisotropy); });
  }
  const int scaling = Get(SCALING);
  if (scaling != s_applied_scaling)
  {
    s_applied_scaling = scaling;
    // Dolphin's OutputResamplingMode on Aurora's samplers: the cubic family as Catmull-Rom, nearest
    // and default as bilinear
    const auto sampler = scaling == 5 ? SAMPLER_SHARP_BILINEAR :
                         scaling == 6 ? SAMPLER_AREA :
                         scaling == 2 || scaling == 3 || scaling == 4 || scaling == 8 ? SAMPLER_BICUBIC :
                                                                                         SAMPLER_BILINEAR;
    VideoPost([sampler] { aurora_set_resampler(sampler); });
  }
  // HD textures: the imported pack, live (hd_textures.cpp)
  static const bool s_pack_read = [] {
    s_texture_pack = TexturePackImported();
    return true;
  }();
  (void)s_pack_read;
  VideoHdTextures(s_texture_pack && Get(HD_TEXTURES) != 0);
  static const bool s_env_vsync = std::getenv("SKIES_VSYNC") != nullptr;
  const int vsync = s_env_vsync || Get(VSYNC) != 0 ? 1 : 0;
  if (vsync != s_applied_vsync)
  {
    // The window opened with the setting; later changes go to the swap chain
    if (s_applied_vsync >= 0)
      VideoPost([vsync] { aurora_enable_vsync(vsync != 0); });
    s_applied_vsync = vsync;
  }
#endif
}

void MenuFilterPad(PadStatus& pad)
{
  const Hotkeys keys = ReadHotkeys();
  const bool menu_key = (keys.menu && !s_menu_key_down) || s_back_request.exchange(false);
  s_menu_key_down = keys.menu;
  // Taken every frame, so an F1 pressed in the menu or during a Continue doesn't save later
  const bool f1 = s_f1_request.exchange(false);
  (void)f1;  // used by the Aurora build only
  // Taken every frame too, so a key pressed in the menu doesn't toggle after it closes
  const bool invincible_key = s_invincible_request.exchange(false);
  if (((keys.stick_click && !s_stick_click) || invincible_key) && s_mode == Mode::Closed)
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
#ifdef SKIES_STANDALONE_AURORA
    if (f1 && !InputKeyBound(SDL_SCANCODE_F1) && !StateDirectory().empty())
      StateRequestSave(SlotPath(s_save_slot), "Saved to slot " + std::to_string(s_save_slot));
#endif
    pad = FilterClosed(pad);
    break;
  case Mode::Open:
    // Esc / Back during a capture cancel it (input.cpp), not the menu
    if (menu_key && !s_capturing)
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
      if (!s_closing_logged && Clock::now() - s_closing_since > std::chrono::seconds(2))
      {
        s_closing_logged = true;
        Log("[menu] closed 2 s ago, still waiting for release: buttons %04x triggers %u,%u\n",
            pad.buttons, pad.trig_l, pad.trig_r);
      }
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
  // The launchers' faces: Segoe UI on Windows, Roboto on Android, DejaVu Sans on Linux (the Dolphin
  // build's list); ImGui's own (monospace) otherwise.
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
  s_ui_font = add({windows.empty() ? "" : windows + "segoeui.ttf", "/system/fonts/Roboto-Regular.ttf",
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
        // A copy for the video thread (ImGui changes the atlas in place)
        const auto* pixels = static_cast<const uint8_t*>(tex->GetPixels());
        auto rgba = std::make_shared<std::vector<uint8_t>>(
            pixels, pixels + static_cast<size_t>(tex->Width) * tex->Height * 4);
        VideoPost([id = static_cast<u32>(tex->GetTexID()), width = static_cast<u32>(tex->Width),
                   height = static_cast<u32>(tex->Height), rgba] {
          aurora_backend_set_overlay_texture(id, width, height, rgba->data());
        });
        tex->SetStatus(ImTextureStatus_OK);
      }
      else if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames > 0)
      {
        tex->SetTexID(ImTextureID_Invalid);
        tex->SetStatus(ImTextureStatus_Destroyed);
      }
    }
  }
  // Handed to the video thread with the frame
  struct Overlay
  {
    std::vector<u8> vertices;
    std::vector<u16> indices;
    std::vector<AuroraOverlayCmd> cmds;
  };
  auto overlay = std::make_shared<Overlay>();
  std::vector<u8>& vertices = overlay->vertices;
  std::vector<u16>& indices = overlay->indices;
  std::vector<AuroraOverlayCmd>& cmds = overlay->cmds;
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
  VideoPost([overlay, width = draw_data->DisplaySize.x, height = draw_data->DisplaySize.y] {
    aurora_backend_set_overlay(overlay->vertices.data(),
                               static_cast<u32>(overlay->vertices.size() / sizeof(ImDrawVert)),
                               overlay->indices.data(), static_cast<u32>(overlay->indices.size()),
                               overlay->cmds.data(), static_cast<u32>(overlay->cmds.size()), width, height);
  });
}

const char* OnOff(bool value)
{
  return value ? "On" : "Off";
}

void OverlayInit()
{
  if (s_overlay_init)
    return;
  s_overlay_init = true;
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
  LoadFonts();
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
  // The buttons' names in hints, in the keyboard's keys while it's the one in use
  const HintKeys keys = s_keyboard_hints ? s_hint_keys : HintKeys{};

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
              s_load_confirm   ? "Press " + keys.a + " again to load" :
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
    case Option::ShipHud:
      return {"Ship battle HUD", v.ship_hud ? "Centred" : "Spread out",
              "Spread out moves the ship battle HUD to the sides with the rest. Centred keeps the "
              "game's own layout, with the grid under the Spirit bar."};
    case Option::FrameRate:
      return {"Frame rate", FrameRateText(v.frame_mode),
              "60 fps adds in-between frames for smoother motion. Display refresh adds one for every "
              "refresh of the screen (120 Hz, 144 Hz and up), a frame later than 60 fps. 30 fps is the "
              "original."};
    case Option::Resolution:
      return {"Resolution", ResolutionText(v.efb_scale),
              "Higher is sharper but needs a faster graphics chip. Match window draws at the "
              "window's own size."};
    case Option::Antialiasing:
      return {"Anti-aliasing", v.msaa > 1 ? std::to_string(v.msaa) + "x MSAA" : "Off",
              "Smooths jagged edges. Higher settings need a faster graphics chip."};
    case Option::TextureFilter:
      return {"Texture filtering",
              v.texture_filter == 2 ? "Smooth" : v.texture_filter == 1 ? "Sharp" : "Original",
              "Smooth blends textures' pixels; Sharp shows them as crisp pixels."};
    case Option::Anisotropy:
      return {"Anisotropic filtering",
              v.anisotropy < 1 ? std::string("Off") : std::to_string(1 << v.anisotropy) + "x",
              "Keeps textures seen at a slant sharp."};
    case Option::Scaling:
      return {"Scaling",
              v.scaling == 5 ? "Sharp" : v.scaling == 6 ? "Smooth" : v.scaling == 4 ? "Bicubic" :
                                                                          "Default",
              "How the picture fits the screen. Sharp keeps a low Resolution crisp."};
    case Option::HdTextures:
      if (!v.texture_pack)
      {
        return {"HD textures", "Off",
                "Needs a texture pack you downloaded: import it in the launcher's Settings, "
                "Graphics tab."};
      }
      if (v.texture_pack_bc7 > 0)
      {
        static std::string s_bc7_note;
        s_bc7_note = "Replaces the game's textures with the imported pack's. This graphics chip "
                     "can't draw its " +
                     std::to_string(v.texture_pack_bc7) +
                     " BC7 DDS textures: import the pack's PNG download instead.";
        return {"HD textures", OnOff(v.hd_textures != 0), s_bc7_note.c_str()};
      }
      return {"HD textures", OnOff(v.hd_textures != 0),
              "Replaces the game's textures with the imported pack's."};
    case Option::Music:
      if (!v.dc_music_installed)
      {
        return {"Music", "GameCube",
                "The Dreamcast version's music needs your Dreamcast disc: import it in the "
                "launcher's Settings, Dreamcast tab."};
      }
      return {"Music", v.music ? "Dreamcast" : "GameCube",
              "Dreamcast plays the original Dreamcast music, from your own disc."};
    case Option::Uncensored:
      if (!v.uncensor_available)
      {
        return {"Uncensored", "Off",
                "Needs Eternal Arcadia, the Japanese Dreamcast release: import it in the "
                "launcher's Settings, Dreamcast tab."};
      }
      return {"Uncensored", v.uncensored ? "On" : "Off",
              "The original Japanese models and textures the Western releases changed (cigars, "
              "Bellena's outfit, the tavern sign, the drinks, Gilder's victory pose, Vigoro's "
              "outfit), from the next area or scene."};
    case Option::VSync:
      return {"V-Sync", OnOff(v.vsync), "Stops screen tearing. Can add a little input delay."};
    case Option::ShowFps:
      return {"Show FPS", OnOff(v.show_fps), "Show the frame rate in a corner of the screen."};
    case Option::Renderer:
    {
      static std::string s_text;  // Row keeps a pointer
      s_text = std::string("The graphics API, from the next start. Auto picks the best that works; try another "
                           "if the game crashes or draws wrong. In use now: ") +
               VideoRendererName() + ".";
      return {"Renderer", RendererText(v.renderer), s_text.c_str()};
    }
    case Option::Camera:
      return {"Camera", v.camera ? "Hold Z to move" : "Original",
              "Hold Z, then tilt with the C-stick and zoom with L / R. Original keeps the game's "
              "fixed camera."};
    case Option::MouseCamera:
      return {"Mouse camera", v.mouse_camera ? "On" : "Off",
              "Click the picture to turn the camera with the mouse (the wheel zooms). Esc or the menu "
              "lets the mouse go."};
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
    case Option::Cheats:
    {
      const int n = CheatsEnabledCount(), skipped = CheatsSkippedCount();
      const std::string file = "Cheats/" + CheatsFileName();
      std::string value = v.cheats ? "On" : "Off";
      if (n > 0)
        value += " (" + std::to_string(n) + (n == 1 ? " code)" : " codes)");
      static std::string s_text;  // Row keeps a pointer: the text must outlive this call
      s_text = n == 0 && skipped == 0 ?
                  "Action Replay / Gecko memory codes from " + file + " in the data folder, Dolphin's format "
                  "(none enabled there yet)." :
                  "The enabled Action Replay / Gecko memory codes in " + file +
                      (skipped ? " (" + std::to_string(skipped) + " skipped: they patch game code or are encrypted; "
                                 "see the log)." : ".");
      return {"Cheats", value, s_text.c_str()};
    }
    case Option::Volume:
      return {"Volume", v.volume == 0 ? std::string("0% (muted)") : std::to_string(v.volume) + "%",
              "Music and sound effects."};
    case Option::DeadZone:
      return {"Stick dead zone", v.dead_zone <= 0 ? std::string("Off") : std::to_string(v.dead_zone) + "%",
              "Ignores small stick movements. Raise it if a stick drifts."};
    case Option::ControllerLight:
      return {"Controller light", v.controller_light ? "By the game" : "Off",
              "A controller with a colour light (DualSense, DualShock 4, Switch Pro) glows sky blue in the "
              "field and on the world map and red in battle. Off leaves the light to the controller."};
    case Option::Rumble:
      return {"Rumble", v.rumble ? "On" : "Off", "Controller vibration."};
    case Option::TouchControls:
      return {"Touch controls", v.touch == 1 ? "Always" : v.touch == 2 ? "Off" : "Auto",
              "Auto shows them while no controller is in use (a touch brings them back). Always keeps them "
              "on screen; Off hides them while a controller is connected."};
    case Option::TouchOpacity:
      return {"Touch controls opacity", std::to_string(v.touch_opacity) + "%",
              "How solid the touch controls are drawn: lower shows more of the game through them."};
    case Option::TouchSize:
      return {"Touch controls size", std::to_string(v.touch_size) + "%",
              "How big the touch controls are (and how far in from the screen's edges)."};
    case Option::Language:
    {
      static constexpr const char* LANGUAGES[] = {"English", "Deutsch", "Fran\xc3\xa7" "ais", "Espa\xc3\xb1ol"};
      if (GameRegion() == Region::Japan)
        return {"Language", "Japanese", "The Japanese disc is in Japanese only."};
      if (!IsEurope())
        return {"Language", "English", "Other languages need the European disc."};
      if (v.language == s_current.language)
        return {"Language", LANGUAGES[v.language],
                "The game's text. The European disc has English, German, French and Spanish."};
      if (s_restart_confirm)
        return {"Language", "Press " + keys.a + " again to restart",
                "The game starts again at its title screen: play since your last save is lost."};
      return {"Language", LANGUAGES[v.language],
              CanRestart() ? "Restart the game to use it now, or it changes when the game next starts." :
                             "Changes when the game next starts."};
    }
    case Option::Quit:
      return {"Quit game", s_quit_confirm ? "Press " + keys.a + " again to quit" : std::string("Quit..."),
              "Ends the game: play since your last save is lost."};
    case Option::Mapping:
      return {"Button mapping", "Change...",
              s_keyboard_hints || !s_has_keyboard ?
                  "The controller button and keyboard key for each GameCube button." :
                  "The controller button and keyboard key for each GameCube button. "
                  "The keyboard plays too: Esc opens this menu."};
    }
    return {"", "", ""};
  };
  const std::span<const Option> options = TABS[tab].rows;
  const bool on_tabs = selected == TAB_BAR;
  // The controls for the selected row, in the keyboard's keys while it's the one in use
  const bool mapping = s_mapping;
  const int map_row = s_map_row, map_column = s_map_column;
  const int input_count = static_cast<int>(s_map_labels.size());
  const InputSlot map_slot = map_column ? InputSlot::Keyboard : InputSlot::Controller;
  const std::string tabs = keys.l + " / " + keys.r + ": tab    ";
  const std::string close = "    " + keys.b + " or Esc: close";
  const Option current = on_tabs ? Option::Picture : options[selected];
  const std::string hint =
      mapping   ? (s_capturing ? std::string("Press the new button or key (Esc, Back or wait: keep the old one)") :
                                 "Up / Down: choose    Left / Right: column    " + keys.a +
                                     (map_row < input_count ? ": change    " : ": reset    ") +
                                     (InputClearable(map_row, map_slot) ? keys.x + ": clear    " : "") +
                                     keys.b + ": back") :
      on_tabs   ? "Left / Right or " + keys.l + " / " + keys.r + ": tab    Down or " + keys.a + ": settings" + close :
      current == Option::SaveState ? tabs + "Up / Down: choose    Left / Right: slot    " + keys.a + ": save" + close :
      current == Option::LoadState ? tabs + "Up / Down: choose    Left / Right: slot    " + keys.a + ": load" + close :
      current == Option::Mapping   ? tabs + "Up / Down: choose    " + keys.a + ": open" + close :
      current == Option::Quit      ? tabs + "Up / Down: choose    " + keys.a + ": quit" + close :
      current == Option::Language && v.language != s_current.language && CanRestart() ?
                                     tabs + "Up / Down: choose    Left / Right: change    " + keys.a + ": restart" + close :
                                     tabs + "Up / Down: choose    Left / Right or " + keys.a + ": change" + close;

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
  // Where a tap lands (MenuTap), as fractions of the screen; the window itself first, so a tap inside it
  // that hits nothing does nothing, and one outside it closes the menu
  std::vector<HitArea> hits;
  const auto hit = [&](ImVec2 a, ImVec2 b, HitKind kind, int index) {
    hits.push_back({(a.x - viewport->Pos.x) / viewport->Size.x, (a.y - viewport->Pos.y) / viewport->Size.y,
                    (b.x - viewport->Pos.x) / viewport->Size.x, (b.y - viewport->Pos.y) / viewport->Size.y, kind,
                    index});
  };
  const struct HitsOut
  {
    std::vector<HitArea>& hits;
    ~HitsOut()
    {
      std::lock_guard lk(s_hits_lock);
      s_hits = std::move(hits);
    }
  } hits_out{hits};
  hit(origin, ImVec2(origin.x + size.x, origin.y + size.y), HitKind::None, 0);

  float y = PAD;
  draw->AddText(bold, px(TITLE), at(PAD, y), white, mapping ? "Button mapping" : "Settings");
  y += TITLE_H + 6.0f;
  const float footer_y = size.y / u - PAD * 0.6f - FOOTER_H;

  if (mapping)
  {
    // Columns: the GameCube input, its controller binding, its keyboard binding; the rows scroll
    // with the selection
    constexpr float MAP_BOX_W = 240.0f, MAP_GAP = 10.0f, MAP_ROW_H = 30.0f;
    const float column_x[2] = {PAD + LABEL_W, PAD + LABEL_W + MAP_BOX_W + MAP_GAP};
    const float header_y = y;
    draw->AddText(ui, px(SMALL), at(PAD, header_y), dim, "GameCube");
    draw->AddText(ui, px(SMALL), at(column_x[0] + 4.0f, header_y), map_column == 0 ? gold : dim, "Controller");
    draw->AddText(ui, px(SMALL), at(column_x[1] + 4.0f, header_y), map_column == 1 ? gold : dim, "Keyboard");
    y += 22.0f;
    const int row_count = input_count + 2;
    const int visible = std::max(1, static_cast<int>((footer_y - 8.0f - y) / MAP_ROW_H));
    const int first = std::clamp(map_row - visible / 2, 0, std::max(0, row_count - visible));
    const ImU32 box = IM_COL32(26, 41, 77, 255), box_selected = IM_COL32(41, 66, 118, 255);
    for (int i = first; i < std::min(row_count, first + visible); ++i)
    {
      const float text_y = y + (BOX_H - TEXT * 1.33f) * 0.5f;
      const bool row_selected = i == map_row;
      if (i >= input_count)
      {
        // The resets: one box across both columns
        const char* label = i == input_count ? "Reset controller buttons" : "Reset keyboard keys";
        const ImVec2 a = at(column_x[0], y), b = at(column_x[1] + MAP_BOX_W, y + BOX_H);
        draw->AddRectFilled(a, b, row_selected ? box_selected : box, 6.0f * u);
        if (row_selected)
          draw->AddRect(a, b, gold, 6.0f * u, 0, std::max(1.0f, 2.0f * u));
        draw->AddText(ui, px(TEXT), at(column_x[0] + 12.0f, text_y), row_selected ? gold : white, label);
      }
      else
      {
        draw->AddText(ui, px(TEXT), at(PAD, text_y), row_selected ? gold : white, InputMappingName(i));
        for (int column = 0; column < 2; ++column)
        {
          const bool cell_selected = row_selected && column == map_column;
          const ImVec2 a = at(column_x[column], y), b = at(column_x[column] + MAP_BOX_W, y + BOX_H);
          draw->AddRectFilled(a, b, cell_selected ? box_selected : box, 6.0f * u);
          if (cell_selected)
            draw->AddRect(a, b, gold, 6.0f * u, 0, std::max(1.0f, 2.0f * u));
          const bool waiting = cell_selected && s_capturing;
          const std::string& text = waiting ? std::string(column ? "Press a key..." : "Press a button...") :
                                              s_map_labels[i][column];
          draw->AddText(ui, px(TEXT), at(column_x[column] + 12.0f, text_y),
                        waiting ? gold : text == "-" ? dim : white, text.c_str());
        }
      }
      y += MAP_ROW_H;
    }
    // More above / below
    if (first > 0)
      draw->AddText(ui, px(SMALL), at(PAD + CONTENT_W - 14.0f, header_y), dim, "^");
    if (first + visible < row_count)
      draw->AddText(ui, px(SMALL), at(PAD + CONTENT_W - 14.0f, footer_y - 20.0f), dim, "v");
    draw->AddRectFilled(at(PAD, footer_y), at(PAD + CONTENT_W, footer_y + 1.0f), IM_COL32(255, 255, 255, 40));
    draw->AddText(ui, px(SMALL), at(PAD, footer_y + 9.0f), dim, hint.c_str());
    return;
  }

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
    hit(a, b, HitKind::Tab, i);
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
    hit(at(PAD, y), at(PAD + LABEL_W, y + BOX_H), HitKind::Row, i);
    hit(box_a, at(PAD + LABEL_W + VALUE_W / 3.0f, y + BOX_H), HitKind::ValueLeft, i);
    hit(at(PAD + LABEL_W + VALUE_W / 3.0f, y), at(PAD + LABEL_W + VALUE_W * 2.0f / 3.0f, y + BOX_H),
        HitKind::ValueMid, i);
    hit(at(PAD + LABEL_W + VALUE_W * 2.0f / 3.0f, y), box_b, HitKind::ValueRight, i);
    draw->AddRectFilled(box_a, box_b, is_selected ? IM_COL32(41, 66, 118, 255) : IM_COL32(26, 41, 77, 255),
                        6.0f * u);
    if (is_selected)
      draw->AddRect(box_a, box_b, gold, 6.0f * u, 0, std::max(1.0f, 2.0f * u));
    // Arrows where Left / Right change the value
    if (is_selected && options[i] != Option::Mapping && options[i] != Option::Quit)
    {
      const float mid = y + BOX_H * 0.5f, s = 5.0f;
      const float lx = PAD + LABEL_W + 10.0f, rx = PAD + LABEL_W + VALUE_W - 10.0f;
      draw->AddTriangleFilled(at(lx, mid), at(lx + s, mid - s), at(lx + s, mid + s), gold);
      draw->AddTriangleFilled(at(rx, mid), at(rx - s, mid - s), at(rx - s, mid + s), gold);
    }
    const bool confirm = (options[i] == Option::LoadState && s_load_confirm) ||
                         (options[i] == Option::Language && s_restart_confirm) ||
                         (options[i] == Option::Quit && s_quit_confirm);
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

// The start's shader preparation (video.cpp VideoPrepareShaders): a progress bar over the black window while
// Aurora compiles the pipelines it knows the game uses (the packaged initial_pipeline_cache.db and the player's
// own cache), so the game's first scenes don't draw with pieces missing (issue #34: the Valuan gate came in
// late on an AYN Thor). Video thread, before the game's thread starts.
void MenuShaderProgress(unsigned width, unsigned height, unsigned done, unsigned total, bool touch)
{
#ifdef SKIES_STANDALONE_AURORA
  if (width == 0 || height == 0)
    return;
  OverlayInit();
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
  io.DeltaTime = 1.0f / 60.0f;
  ImGui::NewFrame();
  const float unit = std::max(1.0f, static_cast<float>(height) / 720.0f);
  ImDrawList* draw = ImGui::GetForegroundDrawList();
  ImFont* ui = s_ui_font ? s_ui_font : ImGui::GetFont();
  ImFont* title = s_title_font ? s_title_font : ui;
  const float cx = static_cast<float>(width) / 2.0f, cy = static_cast<float>(height) / 2.0f;
  const auto centred = [&](ImFont* font, float size, float y, ImU32 colour, const char* text) {
    const float w = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x;
    draw->AddText(font, size, ImVec2(std::floor(cx - w / 2.0f), std::floor(y)), colour, text);
  };
  centred(title, std::floor(24.0f * unit), cy - 52.0f * unit, IM_COL32(242, 242, 242, 255), "Preparing shaders");
  const float bar_w = std::min(static_cast<float>(width) * 0.6f, 520.0f * unit), bar_h = std::floor(8.0f * unit);
  const ImVec2 bar0(std::floor(cx - bar_w / 2.0f), std::floor(cy - bar_h / 2.0f));
  const ImVec2 bar1(bar0.x + bar_w, bar0.y + bar_h);
  const float part = total ? std::clamp(static_cast<float>(done) / static_cast<float>(total), 0.0f, 1.0f) : 0.0f;
  draw->AddRectFilled(bar0, bar1, IM_COL32(255, 255, 255, 40), bar_h / 2.0f);
  if (part > 0.0f)
    draw->AddRectFilled(bar0, ImVec2(bar0.x + bar_w * part, bar1.y), IM_COL32(255, 230, 140, 255), bar_h / 2.0f);
  char count[64];
  std::snprintf(count, sizeof(count), "%u of %u", done, total);
  centred(ui, std::floor(15.0f * unit), cy + 16.0f * unit, IM_COL32(242, 242, 242, 200), count);
  centred(ui, std::floor(14.0f * unit), cy + 44.0f * unit, IM_COL32(242, 242, 242, 140),
          touch ? "Tap to skip" : "Press any button to skip");
  ImGui::Render();
  Sink(ImGui::GetDrawData());
  s_overlay_shown = true;
#else
  (void)width, (void)height, (void)done, (void)total, (void)touch;
#endif
}

// The preparation's bar taken off (video thread, the game's thread not started yet)
void MenuShaderProgressEnd()
{
#ifdef SKIES_STANDALONE_AURORA
  aurora_backend_set_overlay(nullptr, 0, nullptr, 0, nullptr, 0, 0.0f, 0.0f);
  s_overlay_shown = false;
#endif
}

void MenuFrame()
{
  const Clock::time_point now = Clock::now();
  // The game's frames and, in 60 fps mode, the in-between pictures shown with them (issue #35: the
  // counter said 30 with 60 fps on, where the Dolphin build's counted every picture)
  s_fps_frames += 1 + VideoTakeInbetweensShown();
  if (s_fps_since == Clock::time_point{})
    s_fps_since = now;
  const double elapsed = std::chrono::duration<double>(now - s_fps_since).count();
  if (elapsed >= 1.0)
  {
    s_fps = s_fps_frames / elapsed;
    s_fps_frames = 0;
    s_fps_since = now;
    VideoSetTitleFps(s_fps);
  }
#ifdef SKIES_STANDALONE_AURORA
  // The menu's tap areas only while it's open (the returns below skip drawing it): a tap on the game
  // afterwards isn't the menu's
  if (s_mode != Mode::Open)
  {
    std::lock_guard lk(s_hits_lock);
    s_hits.clear();
  }
  unsigned width = 0, height = 0;
  if (!VideoSurfaceSize(&width, &height) || width == 0 || height == 0)
    return;
  const bool show_fps = Get(SHOW_FPS) != 0;
  const bool anything = s_mode == Mode::Open || !s_messages.empty() || show_fps;
  if (!anything)
  {
    if (s_overlay_shown)
    {
      VideoPost([width, height] {
        aurora_backend_set_overlay(nullptr, 0, nullptr, 0, nullptr, 0, static_cast<float>(width),
                                   static_cast<float>(height));
      });
      s_overlay_shown = false;
    }
    return;
  }
  OverlayInit();
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
