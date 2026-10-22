# Skies of Arcadia Legends (GEAE8P / GEAP8P / GEAJ8P): static recomp notes

This repo is [ModernGekko-Template](https://github.com/ExpansionPak/ModernGekko-Template) applied to
Skies of Arcadia Legends (USA; the European disc since 2026-10-17, *The European release*). Releases:
[handoffs/HANDOFF-release.md](handoffs/HANDOFF-release.md) (current: v0.10.0-beta). DolRecomp translates `main.dol` to C; ModernGekko (Dolphin-derived,
GPL-3) provides video, audio, disc and input. The matching decomp lives separately in
`../SkiesOfArcadiaLegends-Decomp` and supplies symbol names.

## Running

The disc isn't copied: `extracted/SkiesOfArcadiaLegends` is a symlink to the already extracted
disc in `../GameCube-Research/targets/SkiesOfArcadiaLegends/disc` (same `sys/` + `files/` layout
`make extract` produces). Recreate it on a new machine with:

```sh
mkdir -p extracted
ln -s /path/to/extracted/disc extracted/SkiesOfArcadiaLegends
```

Then:

```sh
make run GAME=SkiesOfArcadiaLegends
```

or run the built module directly with a project-local user directory:

```sh
lib/ModernGekko/build/moderngekko-run --game extracted/SkiesOfArcadiaLegends \
  --module "$(cat build/modules-llvm-pgo-gp/GEAE8P/active-module.txt)" --user-dir build/user
```

That is the WSL module for the **unpatched** DOL (LLVM + PGO, see *Platforms*) and `tools/play.sh`'s
default. A module linked before the gather-pipe patches (late 2026-10-06), including `make run`'s
`build/modules`, is refused at load with "native module was rejected: CPU state size mismatch" until it is
rebuilt.

The game with the recomp's features (widescreen, camera, options; *The player build*) runs from the
patched DOL with a module built from that same DOL. Released: game patch **ws32**, in **v0.82.0-beta**
(2026-10-22; `patches/game/README.md` has the history): ws31 (Drachma's victory pose, issue #9, *Battle
screen plane*) and ws32 (two more cutscene shots, issue #10, *Cutscene shots framed for widescreen*) on
top of 0.81's ws30; `build/game/GEAE8P-ws32` / `GEAP8P-ws32` / `GEAJ8P-ws32` in the `pal60` worktree,
WSL static module `build/ws32`. Older notes for this paragraph: on `dev` since 2026-10-18 **ws24** (*HUD placement*, on top of ws23's Continue fix, *Memory card
auto-load*; `build/game/GEAE8P-ws24` / `GEAP8P-ws24`, WSL modules `../skies-aspect/build/us-genhud/module-linux`
and `build/eu-gen24/module-linux`, no PGO); ws23 was built for Android (`build/game/GEAE8P-ws23` /
`GEAP8P-ws23`, `build/android/apk/skies-ws23-2026-10-17.apk` on the debug test app); the desktop trees
below are still ws22. On Windows `build\game\GEAE8P-ws22` with `build\modules-llvm-pgo-ws22b\gGEAE8P_recomp.dll`
(European: `build\modules-llvm-eu-ws22b`, no PGO; dev runner `lib\ModernGekko\build\moderngekko-run.exe`)
or the branded release (`Skies of Arcadia Legends.exe`, *Launcher and release*); on WSL
`build/game/GEAE8P-ws22` / `GEAP8P-ws22` with `build/us-gen22/module-linux` / `build/eu-gen22/module-linux`
(no PGO) and the EGL-enabled runtime `lib/ModernGekko/build-egl/moderngekko-run` (through
`tools/play.sh`: it caps memory; see *Known issues* on the 10-11 whole-VM OOM), or a release build
configured with `REGIONS="us eu"` (`lib/ModernGekko/build-release-linux`). A release build refuses any
DOL but its pinned ws22 output. On Android the branded app imports the player's disc: the Retroid runs
the **0.9.1 release app** (`com.skiesofarcadia.recomp`, the user's data; 0.10.0-beta installs over it,
ask first) and the debug test app (`org.dolphinemu.dolphinemu.debug`) for dev APKs (now ws23, both
games: `build/android/apk/skies-ws23-2026-10-17.apk`; ws22 `skies-ws22-2026-10-17.apk` and with the
European module `skies-ws22eu-2026-10-17.apk`); handoffs/HANDOFF-android.md.

### Controller and keyboard (2026-10-17)

No profile needs copying any more. In Skies (GEAE8P / GEAP8P) the runtime gives controller 1 a
keyboard layout beside the controller's on the first start (`Core/SkiesInput.cpp`), so a fresh user
directory plays from the keyboard; the launcher does the same when it creates or upgrades the profile
(`tools/launcher_controls.cpp`). Default keys (README *Controls*): W A S D stick, Space / Left Shift =
A / B, F / R = X / Y, Tab = Z, Q / E = L / R, Enter = Start, I J K L = C-stick, arrows = D-pad,
**Esc = the settings menu**. Any button or key changes in the launcher's **Settings → Controls** or
in game under **Controls → Button mapping**.

- What the player gets, the profile format, "Always Connected", controller 1 following the pad in
  use, the hotkeys and the A / B rule: *Keyboard and button mapping* (under *The player build*).
- The old Dolphin keyboard profiles in `config/` (`GCPadNew.keyboard.ini` X11, `GCPadNew.keyboard.windows.ini`
  DirectInput; X / Z / C / S face buttons, arrows on the stick) still work: on the first start their
  bare keys are qualified with the keyboard and the controller layout is added.
- WSL has no `/dev/input`, so a USB pad isn't visible to the Linux build; the keyboard comes through
  X11 / XInput2 (an X11 session or XWayland on real Linux).

On the Retroid the app maps its built-in controller (`Android/1/Retroid Pocket Controller`): L1 (or
R1 on a fresh profile) is Z, L2 / R2 are L / R, the right stick is the C-stick, Select opens the menu.
Android's input backend names a gamepad's axes by their MotionEvent id (`Axis 0` / `1` left stick,
`11` / `14` right stick, `15` / `16` D-pad hat, `22` / `23` triggers) and its buttons `Button A`,
`Button L2`, `Start`, `Select`; SDL-style names (`Left Y+`, `Pad N`, `Trigger L`) don't exist there. The
branded app writes the Android names itself when the profile lacks them (*Known issues*, "Only the
face buttons worked in Skies Recomp"); Button mapping shows them as "Left stick up", "L2" and so on.

L3 / R3 (Invincible) aren't GameCube buttons: they're the `Invincible` hotkey in `[SkiesHotkeys]`
(`` `Thumb L` | `Thumb R` `` on SDL, `` `Button L3` | `Button R3` `` on Android), so Button mapping can
move it or give it a key. The other added controls (*Controls the recomp adds*) use GameCube buttons
and follow the profile.

**Testing input by hand** (details in `docs/handoffs/HANDOFF-input.md`): automation (`drive.py`) feeds
the GameCube pad directly and bypasses the bindings, so test bindings with real events. WSLg: `xdotool
keydown/keyup` (XTEST) reach the focused window only (`--window` events are ignored by SDL), and other
sessions' game windows take focus, so check `xdotool getwindowfocus` before each key. Windows:
`SendInput` with scan codes while the game window is in the foreground (`C:\dev\SkiesInput\build\input-test22.ps1`).
Retroid: `adb shell sendevent /dev/input/event8 ...` (the built-in controller, writable by the shell's
`input` group; BTN_SOUTH 304, EAST 305, NORTH 307, WEST 308, TR2 313, SELECT 314, START 315, THUMBL 317,
`ABS_X` 0, `ABS_HAT0Y` 17); adb's `input keyevent` comes from the "Virtual" device, which the game
doesn't treat as a controller or keyboard.

### Scripted runs (automation)

`tools/play.sh [LOG] [--load-state FILE]` runs the module memory-capped (`systemd-run`
`MemoryMax=8G`, so a runaway run dies alone instead of taking the WSL VM with it) with
`--automation-dir build/auto`. `tools/drive.py` then feeds it pad input, screenshots and savestates:

```sh
tools/play.sh &
tools/drive.py wait:30 shot:/tmp/title.png tap:a wait:3 hold:main_x=1:20 save:/tmp/s.sav status
```

Automation input goes through Dolphin's input overrider, so it reaches the game whatever
`GCPadNew.ini` says; it can't press the real controller's L3 / R3 (only GameCube pad fields).
`press:k=v` holds fields until `release` while other commands run (camera checks); `config:SYS.SECTION.KEY=V`
sets a setting live; `peek` / `poke` read and write guest memory (over adb too). `speed:30` averages the reported speed over 30 s and prints the slowest and fastest
second (`SPEED_TRACE=1` prints each second, with a `wall=` timestamp for lining it up with profiles). With `ADB_AUTO` set, the same script drives the Android build over adb (see *Android*). Screenshots are full-resolution PNGs written asynchronously; `drive.py` waits
for the file to stop growing.

### Standard test state

**All testing and measurement starts from `build/savestates/01-opening-cutscene.sav`** (the
opening cutscene on the Valuan ship, made 2026-10-05), so numbers stay comparable across sessions:

```sh
tools/play.sh build/bench.log --load-state build/savestates/01-opening-cutscene.sav --mute &
```

Loading takes ~16 s before the game runs (`frame_count` jumps to ~3,850); attach profilers or start
sampling after that. The states are tied to the **extracted** disc layout (`extracted/` →
`sys/` + `files/`): a run that boots the disc image instead reads the wrong data at the next
scene transition. Windows and Android boot the same folder, and Android runs the four states with
`tools/bench-android.sh`. Later states sit beside it (`02-first-battle`, `03-valuan-ship-field`,
`04-valuan-ship-later`; `05-save-point` and `06-save-menu` are for memory-card tests, see *Known issues*;
the full list is under *Test tools and savestates*). Savestates hold game RAM: they stay in gitignored `build/` and are never
committed. They hold guest state, so they should survive module rebuilds of the same DOL; remake
them if the DOL changes.

**Run `moderngekko-run` from `lib/ModernGekko/build/`.** It finds Dolphin's `Sys/` (fonts, and the
`GameSettings/GEA.ini` that forces single-core for Skies) next to its own executable. A copy
elsewhere logs "Trying to access Windows-1252 fonts but they are not loaded", runs dual-core and
hangs on a gray screen, while still reporting a plausible speed. To A/B two runtime builds, put
both binaries in that directory.

To dump every frame instead: put `[Movie] DumpFrames = True` in `build/user/Config/Dolphin.ini` and
`[Settings] DumpFramesAsImages = True` in `build/user/Config/GFX.ini`; frames land in
`build/user/Dump/Frames/`. Remove them afterwards (it's slow).

## The player build (settings, menu, save states, save import, camera, options, release; 2026-10-10 / 12)

What a player gets on top of the game, and where each piece lives (save states: *Save states and auto-save* below; bringing saves in: *Importing saves*; keyboard and remapping: *Keyboard and button mapping*; the European disc and the Dreamcast music: *The European release* and *Dreamcast music*; HD texture packs: *HD textures*; the Japanese Dreamcast art: *Uncensored*). The game side is the DOL
patch **ws22** (`build/game/GEAE8P-ws22`, SHA-1 `ac40e5cf`; European `GEAP8P-ws22`, `55ab3d55`; history in `patches/game/README.md`), shipped in v0.10.0-beta;
the frontend side is `recompcore-skies-settings.patch` (`VideoCommon/SkiesMenu.cpp`,
`Core/SkiesSettings.cpp`) and `moderngekko-local.patch` (launcher, runner). Plans and session logs:
[handoffs/HANDOFF-settings.md](handoffs/HANDOFF-settings.md). Research: the decomp's `docs/widescreen.md`,
`docs/camera.md`, `docs/qol_cheats.md`.

### Android: a second controller (2026-10-22)

The Android profile names one device (`ensurePadProfile`: the first gamepad with a stick, the built-in
one on a handheld), and desktop's controller-follow (`SkiesInput::Follow`) took only SDL devices, so on
Android a Bluetooth pad connected later did nothing (Reddit: an Odin 2 docked to a TV). `Follow` now
takes Android gamepads too (`Axis 0+` and `Button A`, the names the profile's bindings use, which every
standard Android gamepad reports), and switches to one by its buttons only, since some pads' axes rest
away from 0. Checked on the Retroid Pocket 5 with a DualSense Edge: both ways. The D-pad is bound to the
hat axes (`Axis 15` / `16`); a pad that reports its D-pad as keys (`Up`, `Down`, ...) would need those added.

### Controls the recomp adds

| Input | Where | What it does |
|---|---|---|
| Esc, or the controller's Back / View (Android: Select) | anywhere | the settings menu (the `Menu` hotkey, since 0.10) |
| Hold Z, press Start | anywhere (also in battle, however long Z is held first) | the settings menu (L / R or Up to the tab bar: tabs; D-pad / stick choose, Left/Right or A change, B / Start close); its Saves tab saves and loads states |
| Hold Z + C-stick up/down | towns, dungeons, world map | tilt the camera |
| Hold Z + R / L (analog) | towns, dungeons, world map | zoom out / in |
| Hold Z + C-stick left/right | world map | turn the camera around the ship (eases back behind when let go); in towns it's the game's own turn |
| Tap Z | battle | battle speed Normal -> 2x -> 3x -> 4x -> Normal (message on screen, saved) |
| Hold Z, press X | field (game mode 6 outside a ship battle: cutscenes, towns, dungeons, world map) | field speed Normal -> 1.5x -> 2x -> 3x -> Normal (message on screen, saved); neither button reaches the game until both are let go |
| Tap Z | elsewhere | the game's own Z (minimap on foot, Map / To Bridge / Save on the world map), sent on release |
| L3 or R3 | anywhere | Invincible on / off (message on screen, saved); the `[SkiesHotkeys] Invincible` hotkey, not a GameCube button, so Button mapping can move it or give it a key |

Z is held back from the game until it's clear what it's for (released alone: a late tap; with Start:
the menu; with the C-stick or triggers: the camera); where the camera can't move and outside battle
it's passed on after 150 ms. The game's own controls per mode (C-stick and triggers turn the camera on
foot, climb / descend on the world map, X status, Y first person or map, Start skips battle scenes) are
in the decomp's `docs/camera.md`. B + X + Start held for half a second is the game's soft reset.

### Settings

`[Skies]` in the user directory's `Config/Dolphin.ini` (`Core/Config/SkiesSettings.cpp`), applied live:

| Key | Values (default) | Applied by |
|---|---|---|
| `Widescreen` | 1 = 16:9, 2 = Match screen (the window's shape, 4:3 to 32:9), 0 = original 4:3 (first boot: the DOL's) | `Core/SkiesSettings.cpp` -> `ws_config.display_aspect` (0x80002A08) and Dolphin's aspect |
| `Hud` | 2 = at the picture's edges, 3 = in a centred 21:9 area, 1 = in a 16:9 area (a narrower picture keeps its own edges), 0 = the original 4:3 layout (2) | `Core/SkiesSettings.cpp` -> `ws_hud_aspect` (0x80002E9C: 0, 64:27, 16:9, -1) |
| `FrameMode` | 0 = 30 fps, 1 = 60 fps interpolated (first boot: the old switches) | `FrameInterpolator::RequestEnabled` |
| `CameraControl` | 1 = hold Z to move the camera, 0 = the game's only (1) | `SkiesMenu.cpp` -> `cam_input` (0x80002EC0) |
| `Encounters` | percent of the encounter chance: 0, 25, 50, 100, 200, 400 (100) | `SkiesMenu.cpp` -> `qol_input` (0x80002EA0) |
| `Exp`, `Gold`, `MagicExp` | percent of what battles give: 0, 50, 100, 200, 300, 400 (100) | same |
| `BattleSpeed` | 1-4 (1) | same |
| `FieldSpeed` | percent: 100, 150, 200, 300 (100) | same (ws26: `QOL_SPEED_FIELD` and the speed in quarters in the speed word) |
| `Invincible` | 0 / 1 (0) | same |
| `AutoSave` | minutes between auto-saves: 0 (off), 5, 10, 15, 30 (10) | `SkiesMenu.cpp` (namespace `autosave`) |
| `Language` | European disc only: 0 English, 1 German, 2 French, 3 Spanish (0); at the next boot | `SConfig::GetLanguageAdjustedForRegion` (*The European release*) |
| `DreamcastMusic` | 1 = the Dreamcast music, 0 = GameCube (0); needs the import | `Core/SkiesMusic.cpp` (*Dreamcast music*) |
| `Uncensored` | 1 = the Japanese Dreamcast models and textures, 0 = the GameCube's (0); needs the Eternal Arcadia import; per file from its next load | `DiscIO/SkiesUncensor.cpp` (*Uncensored*) |

The bindings and the `Menu` / `Invincible` hotkeys are in `Config/GCPadNew.ini` (`[GCPad1]`,
`[SkiesHotkeys]`; *Keyboard and button mapping*).

The menu also changes Dolphin's own `GFX.ini` `InternalResolution` and `MSAA`, `VSync`, `ShowFPS`,
`HiresTextures` (HD textures, once a pack is imported), and `Dolphin.ini` `Volume`. On desktop the runner sets the resolution from the frontend's `config.ini` at
every boot, so a change in game is written back there too. Any key can be set from a script:
`tools/drive.py config:Dolphin.Skies.Encounters=0` (the first part is Dolphin's system name: `Dolphin`,
`Graphics` for `GFX.ini`, `GCPad`, ...; `GFX` is refused).

### The in-game menu

`VideoCommon/SkiesMenu.cpp`, drawn with ImGui in the on-screen UI and fed by `Pad::GetStatus`
(controller 1). Since 2026-10-11 (late night, user request) it looks like the launchers' Settings
windows: a rounded navy window over the dimmed game, **tabs** Saves (Save state, Load state, Auto-save),
Graphics (Picture, HUD, Frame rate, Resolution 1 / 2 / 3 / 4 / 6x, Anti-aliasing = the backend's MSAA modes,
Texture filtering, Anisotropic filtering, Scaling (since 2026-10-21, *Texture filtering, anisotropic
filtering, scaling*), HD textures (since 2026-10-17; Off with a hint until a pack is imported), V-Sync, Show FPS), Gameplay (Camera, Battle speed, Field speed, Encounters, Hold B, EXP, Gold, Magic EXP, Invincible),
Language (since 2026-10-18; the European disc's four, a second A restarts the game in a new one), Sound (Volume), Dreamcast (since 2026-10-20: Music once a Dreamcast disc is imported, Uncensored once the game has converted files), Controls
(Stick dead zone, Rumble, Button mapping since 0.10); each row a label, a value box (the selected one gold-edged
with arrows) and the launchers' one-line description; a hint line for the selected row. **L / R** (digital,
or the analog triggers most of the way) switch tabs from anywhere; **Up** from the first setting (or Down
from the last) puts the focus on the tab bar, like the launchers: the current tab gets the gold edge,
Left / Right switch tabs, Down or A go back into the settings. Up / Down choose, Left / Right or A
change, B or Start close (no Close row any more); Esc or a controller's Back / Select (the `Menu`
hotkey) open and close it too. **Button mapping** (2026-10-17) is a page of its own: a row per GameCube
input with controller and keyboard columns, A waits for the new button or key (Dolphin's
`InputDetector`, 6 s), X clears (not the controller's A / B), then Reset controller buttons / Reset
keyboard keys; B goes back. While the keyboard is the one in use the hints name its keys. Typeface: Segoe UI / Roboto (`/system/fonts`) / DejaVu Sans, added by
`SkiesMenu::LoadFonts()` from `OnScreenUI::Initialize` after the OSD font (which stays the default for
Dolphin's messages). Text size: the launchers' `[Skies] LauncherTextSize`. Layout: every length is a
constant in picture units (480 lines = 1), scaled to the picture and the text size and shrunk to fit,
with the window as tall as the tallest tab; nothing is measured from the previous frame, so the old
pulsing (a font size flipping 35 <-> 34 px on the Retroid, fixed 2026-10-11 by a cap) can't happen. The
game keeps running and sees a released pad while it's open; changes are staged and applied and saved
when it closes. Same code on Windows, Linux and Android. Checked on Windows (tabs, the tab bar from Up,
change + close + save, window captures) and on the Retroid (by automation: D-pad and R; by the user
after the controller profile fix: "it mostly works", the Up-to-tabs build installed 20:44 for their
look; "it works great now"; 10-11: "perfect"). On the Retroid the D-pad, stick and triggers first did nothing in the menu: not the menu, the
app's controller profile (*Known issues*).

### Keyboard and button mapping (2026-10-17, v0.10.0-beta)

Details, checks and what's untested: [handoffs/HANDOFF-input.md](handoffs/HANDOFF-input.md);
`patches/recompcore-input.patch` (`Core/SkiesBindings.cpp`, `Core/SkiesInput.cpp`, menu, the Win32 /
X11 window keys) and `moderngekko-local.patch` (`tools/launcher_controls.cpp`).

- **Keyboard** on Windows and Linux, with a controller or alone: W A S D stick, Space / Left Shift =
  A / B, F / R = X / Y, Tab = Z, Q / E = L / R, Enter = Start, I J K L = C-stick, arrows = D-pad, Esc =
  the settings menu. Each GameCube input's binding is `<controller terms> | <keyboard terms>`: the
  controller terms are unqualified, so they follow controller 1's device; the keyboard terms name the
  keyboard device.
- **Button mapping:** launcher Settings → Controls (a table: GameCube input, controller button,
  keyboard key) and the in-game Controls → Button mapping (Dolphin's `InputDetector`, so on Android
  too). The controller's A and B can be changed but not cleared.
- **Controller 1 follows the pad in use** (desktop): it moves to an SDL gamepad when its own device
  isn't connected, or when another gamepad is pressed while it has been idle 750 ms. Controller 1 is
  "Always Connected" (without it Dolphin drops the keyboard too while the profile's pad is unplugged).
  The launcher no longer needs a controller to start.
- **Hotkeys** `[SkiesHotkeys] Menu` (controller Back / Android Select, Esc) and `Invincible` (L3 / R3);
  `Version = 1` marks a profile that has had the keyboard layout added (a 0.9.x profile gets it once).
  A key the player unbinds stays unbound. Dolphin's own keyboard profile (the keyboard as `Device`,
  bare keys) keeps its keys, now qualified with the keyboard, and gets the controller layout.
- Shared code: `Core/SkiesBindings.{h,cpp}` (the inputs, defaults, each key's SDL / DirectInput / X11 /
  Android name, expression handling) is used by the runtime, the menu and the launcher.
- Steam Input on SteamOS (the report this was for) is still untried; a Steam Controller on Windows,
  started directly and through Steam, works in the launcher and the game (after 0.10.0-beta, below).
- The game window's developer keys (Space fast-forward, F-key states, Esc quit) are off in Skies;
  F11 and Alt+Enter switch fullscreen.
- **After 0.10.0-beta (2026-10-17, `0e3a0cf`, `recompcore-controller-names.patch` + launcher):**
  - The launcher opens each gamepad for its name, as the game's SDL backend does. An opened Steam
    virtual gamepad is named from Steam's `SteamVirtualGamepadInfo` file (on SteamOS an untranslated
    key, `#SettingsController_SteamController`), an unopened one after its mapping; the mismatch made
    the launcher call the pad the game uses "(not connected)". `SkiesBindings::DeviceLabel` shows such
    keys as words; a saved device whose label matches a connected pad counts as that pad.
  - The launcher's home menu items need `ImGuiButtonFlags_EnableNav` (Dear ImGui 1.92 leaves
    `InvisibleButton`s out of pad navigation).
  - A profile without any controller binding gets the controller's layout (game `ControllerLayoutIfNone`,
    launcher `Upgrade`), and the launcher fills an empty `GCPadNew.ini` before a launch
    (`PrepareProfile`); before, a keyboard-only profile left the pad with nothing bound once controller
    1 moved to it. Details: [handoffs/HANDOFF-input.md](handoffs/HANDOFF-input.md) *Steam Controller*.

### Save states and auto-save (2026-10-11)

In the menu's first three rows, on every platform (Android had no player-usable states before):

- **Save state**: Left/Right picks slot 1-5 (the row shows when it was saved), A saves there and
  closes the menu.
- **Load state**: Left/Right picks the newest auto-save or slot 1-5; A asks "Press A again to load",
  a second A loads. Up/Down or a slot change cancels the question.
- **Auto-save** (`[Skies] AutoSave`, default every 10 min): when it's due, the next quiet moment
  saves: game mode 6 (towns, dungeons, world map), not during a ship battle (mode 6 on a map from
  500 to 599; the ship battle data pointer `0x8034727C` stays set after the first one, so it's no test),
  and the memory card untouched for 10 s (`CEXIMemoryCard::SecondsSinceLastAccess`, new; a state
  doesn't hold a card transfer in progress). Never in a ground battle, a battle that was due waits
  for the field. The newest three are kept.

Files, in the user directory's `StateSaves` (a romhack's in `StateSaves/romhack-<tag>`, *Romhacks*) with ModernGekko's names (`Core/SavestateLayout.h`):
`state-skies-slot1.sav` .. `slot5`, `recovery-skies-<date>-<time>.sav` (pruned with
`PruneAutomatic`), and imported ones `state-skies-import-<date>-<time>.sav` (*Importing saves*). The launcher lists the same directory (Dolphin's `lastState.sav` undo backup is
now left out) and has a **Continue** button that boots the newest state, whatever wrote it, or, since
2026-10-11, the newest memory card save when it is newer (*Memory card auto-load*). Saves and
loads run as host jobs (`Core::QueueHostJob`): the host thread pauses the emulation at a safe point,
where a load from inside the game's pad read would not be safe. Dolphin leaves the memory card's
contents out of states (unless a movie is recorded), so loading a state never rolls back a
memory-card save. A state from one build usually loads in the next (`recompcore-restore-boot-dol.patch`),
but not always (states from before late 2026-10-07 don't), so memory-card saves stay the real saves.

Checked (2026-10-11): Windows (`tools/states-test-win.sh`, dev runner, ws13): menu save in a battle, load
from the world map brings the battle back, no auto-save during 80 s of battle, auto-save every minute
on the world map with three kept, one A doesn't load and two do, 1.000x throughout; the release
runner boots a recovery file with `--load-state` (what Continue passes). WSL (ws10 module): menu save
and load. Retroid (`skies-ws13c-states-2026-10-11.apk`): save in battle, load, the 18-row menu fits,
auto-save costs one second at 0.94-0.99x (a hitch of a few frames, caught up the next second).

### Camera control (DOL ws11, 2026-10-11)

`patches/game/camera.c` wraps the camera manager's call to the shake function (`0x8022D910`): in the
manager's mode 2, while it follows the field camera object (`0x80302F18`, shared by towns, dungeons and
the world map), the eye is turned around the object's look-at point (Vyse, or the ship) by the offsets
the frontend writes to `cam_input`. The game's camera object is never written, so its logic and
camera-relative movement don't change. Limits: on foot the camera stays between the game's camera and
Vyse (no higher than the game's eye, no further than its distance, pitch -40 to -1 degrees), because
rooms have low ceilings and the game has no camera collision (a looser first version went through the
Valuan ship cabin's ceiling); on the world map pitch -35 to +25 degrees, zoom 0.6-2x. First-person look
and battles are untouched. Checked on Windows and the Retroid: `tools/camera-test-win.sh`, 60 fps at
1.00x.

**European and Japanese discs (ws33, 2026-10-22, review D1).** Until ws33 the patch's tilt, zoom and
world-map orbit did nothing on those discs: `FIELD_CAMERA_EYE` was written `0x80302F94u`, and the suffix
kept `gamepatch.py`'s address pattern from translating it, so `cam_view` compared the manager's eye
source against the USA address (Europe `0x80306614`, Japan `0x80302A14`). The hook was installed, so the
frontend's check passed. Turning with the C-stick still worked there: that is the game's own camera.
`gamepatch.py`'s pattern now takes a suffix (and an untranslated address stops the build, so only the
tables' identity entries pass through). Checked on WSL (JIT, European disc, a cold boot into a card
save in P. Isle Underground, Z + C-stick up + L for 1.2 s): `cam_input.applied` 0 on ws32, 51 on ws33.
The USA DOL is byte-identical to ws32; the Japanese one differs in the same halfword, by its table
(not run: no Japanese module on WSL).

**Japanese Continue's card work (ws34, 2026-10-22, review D9).** The Japanese `fn_801A4070` (at
`0x801A4204`) stores the `EA_LEGEND_EXTRA #00`-`#02` probe's bit mask at work +0x270, and the Japanese
load and save screens allocate 0x274 bytes of work (`li r4, 0x274` at `0x801A23BC` / `0x801A24D8`;
USA and Europe 0x270). `autoload.c` allocated 0x270 on every disc, so a Japanese Continue wrote one
word past its heap block (the earlier tests passed, likely on the allocator's rounding). The size is
now 0x274 under `GAME_GEAJ8P`; `gamepatch.py` passes `-DGAME_<ID>` to every source. The USA and
European DOLs are byte-identical to ws33; the Japanese one differs in two bytes. Checked on WSL (JIT,
cold boot, `build/game/GEAJ8P-ws34`, the converted Japanese card files 2 and 3 in `build/user10`):
Continue done in about 6 s both times, no invalid reads.

### QOL options and Invincible (DOL ws12, 2026-10-11)

`patches/game/qol.c` / `qol.S`, all whole-call hooks (decomp `docs/qol_cheats.md`):

- Encounters: the encounter roll (`0x800C207C`) is remapped so the chance is scaled exactly; Off also
  clears the frames-since-battle counter.
- EXP / Gold / Magic EXP: the battle data is scaled once per battle right before the results read it
  (ground: `0x8006F5B8`; ship: `0x8015C6B4`), so the results screen shows what's awarded.
- Battle speed: the end-of-frame retrace waits (`0x801DC498`, `0x801DC4D8`) are skipped in battle
  (game mode 7). Music should keep its tempo (MusyX runs off the audio interrupt, not the frame loop;
  not listened to yet). The runtime turns frame
  interpolation off while a battle runs fast, and steps a battle down (to 2x at the lowest) when the
  emulation stays under 95% for 1.5 s.
- Spells and special attacks at normal speed (ws27, 2026-10-21, issue #7 and Reddit): a spell's or an
  S-move's sound is one pre-mixed stereo stream (`SOUND/KO00NN_L/R.dsp`, NN = the skill id, voice and
  effects together; an enemy special plays a voice stream, `B702NNNN`), and the action waits for it to
  end, so at speed the animation was over seconds before the sound (Cutlass Fury: animation 10.1 s at
  1x, 5.0 s at 2x, 2.5 s at 4x, the stream 11.6 s; at 4x the battle sat ~8.6 s). Speeding the sound up
  would mean the whole recording (pitch or a time-stretch): there are no separate effects to time.
  So `qol_fast` is false in a ground or deck battle while any unit's action task (`lbl_80309700[unit]`,
  the task's first word is its function) runs the party's spell / S-move action `fn_80085064` (the
  party dispatcher `fn_80086C68` puts it there for command types 1 and 2 in `lbl_80309174[unit]`,
  0x20 each: +0 type, +6 skill id) or the enemies' spell / special action `fn_80087074` (their
  dispatcher `fn_8008B9E0`, types 1 and 0xC; their AI `fn_8008A424` makes rule codes 500+ type 1 with
  id - 500, other codes type 0xC). Every enemy action then switches to `fn_80087054` (wait, then end);
  the party's S-move holds its step 13 until the stream ends. Always on (the user's choice), spells
  included; ship battles followed in ws28 (next entry). The frontend didn't know until ws28: its
  interpolation stayed held (30 fps at speed) and its overclock on. Measured on Windows (ws27 module,
  release runner): Cutlass Fury's animation 10.1 / 10.1 / 10.4 s at 1x / 2x / 4x, the stream ending 1.7
  s after it at each; Antonio's special (`19-retroid-antonio-start.sav`, unit 4, type 0xC, id 5) 7.3 s
  at 1x and at 4x; plain attacks keep the speed (enemy attacks 1.0 s at 4x, 4.2-6.3 s at 1x). Retroid
  Pocket 5 (test app, the Japanese game, `jp-03-worldmap-battle-start.sav`): Cutlass Fury's action
  ran about 12 s at 1x and 13 s at 4x (polled every 2-4 s), the rest of the 4x battle at 60 game
  frames a second, and the battle went on. Before,
  the earlier research (2026-10-21) found nothing by diffing memory because the test states needed
  Aika's command too before the round started. Tools: the game state read through automation `peek`
  (the task list from `*0x803475A8`, linked by +4; the command records; the stream channels), audio
  dumps (`[DSP] DumpAudio`).
- Ship battles' spells and special attacks at normal speed, at 60 fps (ws28, 2026-10-21): ship battles
  play no streams (every caller of the `KO` / `B702` battle streams is ground battle code), so their
  spells and specials are sound effects on a frame timeline that nothing waits for; at speed the
  effects ran behind the picture. A turn plays its phases on that timeline (`lbl_803472E0`, a float
  frame, 500 per phase; `lbl_803472DC` 1.0 while it plays), each phase's record (`lbl_803472A4`, 0x58)
  holding its windows: set-up from +0x8, the side acting first from +0xC to +0x14, the turn-around, the
  second side from +0x18 (the patch counts it from +0x14) to the end. `lbl_80306E38` holds the two
  sides' actions **in acting order** (0x90 each; 0x10 per action: +0 kind, +1 side, +5 weapon slot, +8
  id), so window k reads record k: a first version read them by side and slowed the enemy's Focus
  instead of Aika's Sacri whenever the enemy acted first (which side does changes from turn to turn).
  `qol_fast` is false while the acting side has magic (kind 5), a crew member's special (7), an enemy's
  special attack (8, ids 3-7: Hull Ram, Red Ray, Fist of Grendel, Boulder Throw, Stomp) or the party
  fires an S-Cannon (kind 4 from the slot holding the Harpoon Cannon 0x1AE or MoonStone Cannon 0x1AF;
  the party's weapons are at `0x803063F0 + 0x298`, 0x22 each, item id +0x12). Kinds and ids from the
  banner function `fn_80159D48`; decomp `docs/qol_cheats.md` *Ship battles*. The patch also sets
  `qol_normal_speed` (`0x80002BDC`, a `.late_bss_ws28` section after `.late_bss` in `cave.ld`, so the
  variables ws27 had keep their addresses) while a battle at speed plays one of these, ground or ship,
  and the frontend keeps frame interpolation on then (`recompcore-battle-specials.patch`); the
  overclock stays as the speed set it. While the speed holds interpolation off, the frontend now asks
  for off every poll: `SkiesSettings.cpp` asks for the Frame rate setting on every state load and
  option change, which had turned interpolation on in a fast battle (seen while testing). It comes
  back on only if it was on when the hold began or the setting changed meanwhile, so a device whose
  second EFB doesn't fit doesn't try again at every spell. Measured on Windows (release runner, ws28
  module) from a random encounter: from the Baltor state, the map changed to `me550a` (the Black
  Pirates; maps 550+ are the sky's random battles, no tutorial: the Baltor state's command menu is
  locked by Drachma's tutorial), saved as `ship550-magic.sav` with Aika's Magic picked
  (`C:\dev\SkiesUncensor\app\User`). Aika's Sacri, its window: 29.8-30.1 game frames a second at
  2x and 4x, whichever side acted first (both orders seen in four 2x runs); the enemy's Focus in the
  same phase 60 at 2x, 121 at 4x; the rest of the turn at speed; 1x unchanged (the flag never set).
  Interpolation (Frame rate 60): off at the fast battle, on for Sacri, off after it, staying off at
  the next command input. Ground battles unchanged: Cutlass Fury at 4x held normal speed for its whole
  action (14.8 s by 0.3 s polling), interpolation on for it. Not seen live: the S-Cannon (offered only
  on a phase marked C!, which came at random and not in the time tested), crew specials and the
  enemies' special attacks (the code path is the same comparison as magic). macOS (the Mac session,
  Apple Silicon, worktree from these commits, local codegen us/eu/jp: patched DOLs matching the
  dolpatches' outputs; USA run, Invincible, Frame rate 60, the same two states): Sacri's window 29-31
  game frames a second at 2x and at 4x, outside 57-66 (2x) and 116-122 (4x); interpolation off at the
  fast battle, on as the flag rose, off as it fell; Cutlass Fury at 4x the flag up ~14.3 s with
  interpolation on for it; 1x never set; fallback 0, smc_failed 0. Retroid Pocket 5 (the test app,
  ws28 test APK, the USA disc imported into it from `Download/skies.rvz` beside its Japanese one; the
  import's DOL `7af78727`; the same two states over adb): Sacri's window 31 game frames a second at 2x
  and 30 at 4x, the enemy's Focus after it 63 (2x) and 60 at "4x" (the device's limit, the frontend
  steps it down); interpolation on for 8.4 s / 8.1 s across the window; Cutlass Fury at 4x with
  interpolation on 14.5 s. The party acted first in both device runs.
- Field speed (ws26, 2026-10-20): the same waits skipped in the field (game mode 6: cutscenes, towns,
  dungeons, the world map; a ship battle counts as battle), when the frontend sets `QOL_SPEED_FIELD`
  (0x100) in `qol_input`'s speed word, which it does only outside battle (an earlier DOL takes the word
  for the battle speed and never sees the flag in a battle). With the flag the low byte is the speed in
  quarters (6 = 1.5x), and `qol_frame` gives each frame 2, 1 or 0 retraces from a budget (8 a frame,
  less the quarters for each retrace taken), so it averages 2 / speed: 1.25x is 1,2,1,2,2, 1.5x is
  1,1,2, 3x is 0,1,1 (the battle's whole steps come out as before: 2x 1,1; 3x 0,1,1; 4x 0,1).
  Normal / 1.5x / 2x / 3x (`[Skies] FieldSpeed` 100-300), Z+X cycles it (a Z tap in the field is the
  game's: minimap, world map menu). The step-down in the field works on the emulation speed smoothed
  over about half a second (Android's reading jitters by a few percent, and a mean of 0.97 never
  stayed under a line for 1.5 s): under 0.98 for 1.5 s it goes straight to the quarter that speed
  allows (quarters x speed, at least one less, normal at the lowest). It isn't judged for 3 s after
  a new map (loading stalls the emulation, 0.6x on the Retroid), starts over on every new map
  (`MAP_ID`), and after 30 s without a dip under 0.98 tries a quarter higher (quietly; a try that
  fails doubles the wait, up to 10 minutes), so a passing dip doesn't hold a whole map down (the
  world map is one map). Battles keep the old rule (0.95, a whole step, no retry). The emulated CPU
  is overclocked in the field from 1.5x: 1.25x, or speed - 1 above 2x (3x = 2.0: a world map frame
  takes about a retrace like a ship battle's; 1.5x stayed at 60 frames a second; at 1.5x without it
  the European world map's frames given one retrace spilled into a second, 37-43 game frames a second
  at full speed, 45.6 with it). Not at 1.25x, where the Retroid settles on the world map: there the
  clock cost it (its polling runs longer).
  Measured with `tools/qol-test-win.sh ... field` on Windows (release runner, ws26 PGO module, 1x):
  1.5x / 2x = 46 / 60 game frames a second at 1.00x on the world map (10) and the Valuan ship (03),
  2x also in the opening cutscene (01) and 07; 3x ~86 frames at 0.96x on the world map, ~84 on the
  ship (stepped to 2.5x): a desktop PC can't quite hold 3x in the field, hence no 4x. Battle speed
  2x / 3x / 4x unchanged (60 / 92 / 118). The CI package of `1e7e586` (Windows) the same, and with the
  European game 1.5x / 2x = 45 / 60 on the world map (eu-10) and in eu-07. A 3x start on the world
  map that dipped to 2.25x climbed back a quarter every 30 s to 3x. Retroid Pocket 5 (test app,
  `[Skies] FieldSpeed` 200, the Dreamcast music on): plain 2x ran 0.85x / 0.72x / 0.71x emulation
  (ship / world map / cutscene; the CPU thread at 90%, the video thread at 80%); with the final
  rules it settles at 1.5x on the ship and 1.25x in the cutscene within 4 s of the load, on the world
  map at normal and 1.25x 30 s later, all at 1.00x, and its tries at 1.75x on the ship came back
  after 3 s, the next one 60 s later. A 3x field speed into a 1x battle
  drops the overclock at the battle's start (the battle runs at 30 game frames a second). Thirteen
  Z+X presses during the opening cutscene's dialogue left it running, and A went on through it.
- Invincible: party HP, MP and Spirit refilled every battle frame (magic and S-moves cost nothing),
  damage to the party zeroed in the damage function `fn_8002DC14` (13 calls and a handler table entry);
  turning it off leaves the stats as they are. Members already knocked out (HP 0) aren't revived. Turned
  on while commands are chosen, the Spirit gauge fills at once but the party status box keeps the HP / MP
  it showed at the start of the turn until the round plays out (memory is full at once; the box isn't
  redrawn from the battle HUD panel during command input; decomp `docs/qol_cheats.md`).

Checked on Windows (`tools/qol-test-win.sh`): first battle EXP / gold / magic EXP 3 / 10 / 2+4 at Normal
and 12 / 40 / 8+16 at 4x; first battle from the world-map state after 29 / 20 / 8 s at 1/4 / Normal / 4x,
none in 60 s Off; deck battle 30 / 61 / 92 / 122 game frames per second at 1x-4x at 1.00x; Invincible
keeps Vyse at full HP where the run without it is knocked out. On the Retroid: 2x at full speed, 3x / 4x
reach only ~2.6x / 2.5x (0.85x / 0.65x emulation) so the cap settles at 2x; Invincible holds HP.
**Ship-versus-ship battles** (ws14, 2026-10-11) run inside field mode 6 on maps 500-599; qol.c and the
frontend count them as battle (`InShipBattle` in `SkiesMenu.cpp`, `qol_ship_battle` in qol.c).
Invincible zeroes the enemy's damage to the ship (`qol_ship_phase` around `fn_8014419C`) and refills
the ship's HP, the Spirit gauge and the crew's MP every frame; Spirit and MP show from the next turn,
since the command input works from what the turn began with. Battle speed skips the same waits, and
at 3x / 4x the frontend also overclocks the emulated CPU by speed / 2 (CurrentRun layer, removed when
the battle ends or slows down): a ship battle frame costs about a retrace of emulated CPU time, so
without it 3x and 4x stay at 2x. Checked on Windows from the Baltor savestate
(`17b-ship-battle-baltor-commands`, made with `tools/shipbattle-state-win.sh`): 30 / 59 / 92 / 116 game
frames per second at 1x-4x at 1.00x; Invincible keeps the Little Jack at 10000 HP (8140 after the same
turn without it); won the normal way, the battle goes on to Nasr with 410 EXP / 300 gold at 100% and
twice that at 200%. Ground battles unchanged (30 / 61 / 92, Invincible). Decomp `docs/qol_cheats.md`
*Ship battles*.

### Launcher and release (bring your own disc)

`tools\build-release-win.bat configure|build|package MODULE` builds the branded Windows release in
`lib\ModernGekko\build-release`: `Skies of Arcadia Legends.exe` (ModernGekko's launcher; `SkiesRecomp.exe`
before 2026-10-11) and `skies-run.exe`, user directory `%LOCALAPPDATA%\Skies of Arcadia Legends` (was
`SkiesRecomp`; the user's folder was copied over, the old one kept as a backup). `package MODULE
[VERSION]` makes the players' zip `build\SkiesOfArcadiaLegends-<version>-windows.zip` (+ `.sha256`; one
folder: exes, module, `Sys`, `patches\skies-widescreen.dolpatch`, art, app-local Visual C++ runtime,
README, COPYING, notices). Linux: `tools/build-release-linux.sh` does the same in
`lib/ModernGekko/build-release-linux` (launcher `SkiesOfArcadiaLegends`, user directory
`~/.local/share/Skies of Arcadia Legends`, a `.tar.gz`; checked on WSLg 2026-10-11). Those are the
defaults: since 2026-10-17 the player can move the folder or keep it beside the programs (*Data
folder* below). The launcher takes the player's disc image (RVZ, ISO, GCM, GCZ, WIA or CISO; see below),
extracts it, patches its `main.dol` with `release/skies-widescreen.dolpatch` (whole-file format
`moderngekko-dol-file-patch 1`, made by `tools/mkdolpatch.py`: it carries only the bytes that change),
checks the disc ID (GEAE8P, or GEAP8P with the European patch `release/skies-widescreen-eu.dolpatch`;
*The European release*) and the patched DOL's SHA-256 (pinned per region at configure), and boots;
`Skies of Arcadia Legends.exe --extract IMAGE` does the same without the window.

The launcher's look (2026-10-11 late night, the user's design calls, modelled on the N64 recomps'
RmlUi launchers): with `launcher-background.png` beside the exe (`release/launcher-background.png`,
copied by the bat's `build` step; the user's picture with its black right-hand strip cropped off) the
window opens at 1280x760, the art fills it (fitted to cover, cropped from the left so the characters on
the right stay in view) under a scrim that fades out to the right; at the top left "Skies of Arcadia
Legends: Recompiled" (gold) over the title; at the bottom left a menu: **Continue** (newest save state or
memory card save, when there is one) / **Start Game**, or **Select Disc** with its hint as the item's second line (picking a
disc imports it and starts the game), then **Settings** and **Exit**; the item under the mouse or the
keyboard / pad focus gets a gold bullet and fade. Start Game carries "Ready: <game> [<disc ID>]" as
its second line, like Select Disc's hint. **Continue** (2026-10-11, user request) is the newer of the
newest save state and the game's newest memory card save (`GC/<region>/Card A|B/*GEAE*.gci`, by file
time: Dolphin rewrites a `.gci` only when that save changed): a state is loaded at boot; a card save
is loaded by the game itself, so Continue writes `[Skies] AutoLoadCard = <file>` (one shot; the file
number from the `.gci` name, `...SA_LEGENDS.000.gci` = file 1, card A only, files 1-99) and the game
patch loads it at boot (*Memory card auto-load* below; when the newest card save can't be auto-loaded,
e.g. card B, Continue offers the newest state instead). Its second line says which, e.g. "Save
state: Slot 2  Oct 11 18:00" or "Memory card save: file 1  Oct 11 17:30". Start Game clears the key. Same rule in the Android app. Checked on Windows with dummy files both ways; import progress and errors sit under the menu. Segoe UI from `%windir%\Fonts` (DejaVu Sans on Linux, ImGui's font otherwise; note
SDL's own environment is case-sensitive, Windows spells it `windir`). **Settings** opens a rounded modal
window over the dimmed home screen, tabs **Graphics** (Picture, Frame rate, Anti-aliasing, V-Sync; since 2026-10-21 also Texture
filtering, Anisotropic filtering, Scaling),
**Display** (Screen windowed / fullscreen, Resolution, Show FPS, FPS in title bar, Graphics backend),
**Gameplay** (Camera, Battle speed, Field speed, Encounters, Hold B, EXP, Gold, Magic EXP, Invincible), **Language** (since
2026-10-18: English / Deutsch / Français / Español with the European disc, else a line saying the USA disc
is English only), **Sound** (Volume), **Dreamcast** (since 2026-10-20: Music, Import a Dreamcast disc..., Remove, Intro music, Uncensored with its conversion state: *Dreamcast music*, *Uncensored*),
**Controls** (Controller: picking one moves controller 1 to it and keeps the bindings; a table of
every GameCube input with its controller button and keyboard key, click then press to change,
right-click / Delete / pad X to unbind, two resets; Stick dead zone, Rumble), **Saves** (Auto-save, the "Start from
savestate" list), **Disc**: each a label, a dropdown with the in-game menu's choices and a one-line
description, written to the same keys the in-game menu uses (`[Skies]` in `Dolphin.ini`, `GFX.ini`,
`[DSP] Volume`, `GCPadNew.ini [GCPad1]`; booleans Dolphin wrote as True / False read back as 1 / 0), so
either side shows the other's changes. **A- / A+** beside Close set the text size (80-160 %, default
120 %, `[Skies] LauncherTextSize`); the layout widths follow it. Netplay is left out of the branded
window. Esc or pad B closes Settings; pad navigation is on. Without the picture (any other ModernGekko
build) the launcher keeps its original single panel. Checked from window captures at 100 / 120 / 140 %;
clicks in the window (A- / A+, a setting written by the launcher) not yet tried by hand. The runner finds `gGEAE8P_recomp.dll` beside
itself or in `<user>\StaticRecompModules`. Tested end to end from the USA RVZ (ws10) and from the
original DOL (ws11, ws12, ws13: the release runner patched it and the game ran with the new features at
1.00x). The patch applies to the original DOL or leaves its own output alone, and since 2026-10-18
also to a romhack that keeps the original's code (*Romhacks*); any other DOL is refused. An
existing game root from an earlier release (its DOL patched by that release) is updated when the
player picks the disc again (2026-10-11: before that, picking the disc failed with "existing
extraction patching failed"; the user hit it with their 10-10 ws10 import). Until 2026-10-18 only
`sys/` was taken again and an import with the same disc ID was otherwise kept ("Using existing
extraction"); now the disc picked is always extracted in full, since a romhack and its original share
the disc ID. Since `61ac70e` (0.10.0-beta) the launcher also says so: with an earlier
release's import, Select Disc's line reads "This update needs your disc once more. Your saves and
settings are kept.", and the runner started on its own shows a message box ("The game needs updating")
and exits with code 2 instead of failing silently (checked: the Linux launcher, the Windows runner;
the refresh kept 0.9.1 card saves and states, which load on ws22). Android keeps `main.dol.orig` and
repatches by itself. **Disc formats (checked 2026-10-11):** the release launcher's import (`--extract`, the same code as
Select Disc) with the user's RVZ converted by `dolphin-tool` (built with
`-DMODERNGEKKO_ENABLE_DISC_TOOL=ON` in `lib/ModernGekko/build`) to ISO, GCZ and WIA, a renamed ISO as
GCM, and `tools/mkciso.py` for CISO: all six gave the same 5,552 files and the patched DOL
`7b4c416c` (RVZ 6 s, ISO / GCM / CISO 3-4 s, GCZ 7 s, WIA 47 s). WBFS (a Wii backup format) is no
longer offered in the GameCube build. The picker, the home hint and the Disc tab list the six; the Disc
tab shows the imported game and a Change Disc button. Android reads discs with the same DiscIO code
(its picker takes any file); only RVZ was tried on the device.
The launcher no longer hashes all game files (`InspectGame(root, false)` unless
the build pins `MODERNGEKKO_REQUIRED_ASSETS_SHA256`): that held the window frozen for seconds at
startup and made the re-import check take 39 s (now 0.2 s).
Android: the branded app imports the disc itself (`recompcore-skies-android-app.patch`,
handoffs/HANDOFF-android.md) and has the same home screen (art, title, Continue / Start Game / Change Disc /
Settings) and a Settings window built like the desktop one (`SkiesSettingsPanel.kt`; tabs Graphics,
Gameplay, Language, Sound, Dreamcast, Controls, Saves, Disc, About; no Display tab: Resolution and Show FPS sit under
Graphics, with Renderer (Vulkan / OpenGL ES) and, on devices that can load one, GPU driver; About has
the device info and the licences; *Slower devices and GPU drivers*). Name "Skies of Arcadia Legends",
app ID `com.skiesofarcadia.recomp` (2026-10-11; before: "Skies Recomp" under an ID with a personal
domain). Dolphin's own screens are unreachable: its game list, TV UI and deep link aren't exported and
the Back menu has only Pause, Overlay Controls and Exit. It runs full
screen (immersive, like the game), has its own icon (Reckoner's logo, `release/android-icon/`) and its
own banner (`android:banner`: the app drawer of launchers like the Retroid's Radikal shows it instead of
the icon; `release/android-icon/drawable-xhdpi/skies_banner.png` from the launcher art and the logo), its
own version: `versionCode` = minutes since 2026-01-01 at build time, `versionName` "dev build N" (a
release: the tag's, `-PskiesVersionName`) (Dolphin's commit-count version was the same for every build, so launchers such as the Retroid's
Radikal kept the cached icon). On every start it makes sure controller 1's profile uses Android's input
names (`SkiesNative.ensurePadProfile`). The game screen turns with the device between both
landscapes (`SCREEN_ORIENTATION_USER_LANDSCAPE`, so the system's rotation lock still holds it;
`recompcore-android-orientation.patch`, issue #12, 2026-10-22): it took Dolphin's own setting before,
plain landscape, which no Skies screen can change. A 180-degree turn keeps the configuration, so the
Activity isn't recreated. On the Retroid Pocket Nova (ws31 test APK) the game Activity requests
`SCREEN_ORIENTATION_USER_LANDSCAPE` (`dumpsys activity`), but the display stays at the normal landscape
whatever the rotation lock says: the Retroid firmware keeps a 1x1 invisible overlay (`com.rp.settings`,
`ty=MAGNIFICATION_OVERLAY`) that requests landscape and decides the orientation, so Retroids can't show
it either way. Phones (the request came from a Pixel) have no such window.

### A missing game module (2026-10-17 night)

Without `g<ID>_recomp.dll` beside the runner (or in the user folder's `StaticRecompModules`) the
runtime failed with "no native module was supplied; use allow_interpreter explicitly" and the
launcher said only "The game process exited with code 1" (seen with a dev build folder; for players:
a partly extracted zip or antivirus). The runner now shows "Game files missing" naming the file and
the folder, and exits with 20 (`RUNNER_EXIT_EXPLAINED`), which the earlier-release DOL message uses
too; the launcher shows nothing more for 20. `--headless` prints the line without a box (WSLg shows
no SDL message box, and the box would block a script). Checked on Windows (box captured, exit 20) and
Linux headless (exit 20).

### Slower devices and GPU drivers (2026-10-11)

- **60 fps below full speed:** `slowprompt` in `SkiesMenu.cpp` (all platforms): with Frame rate at
  60 fps, not in a fast battle (interpolation held) and after the first minute, 20 s in a row under
  90% emulation speed open the in-game menu once on Graphics / Frame rate with a gold note; nothing
  switches by itself; `[Skies] SlowFrameRateNoted = True` stops it for good. Checked on WSL (slow at
  60 fps there: the menu opened at ~80 s, the key was saved). **Automation and benchmark user
  directories that run 60 fps on WSL should carry `SlowFrameRateNoted = True`**, or the menu takes
  the pad mid-run.
- **Android GPU driver and device info:** Settings, Graphics, GPU driver (Adreno devices where
  Dolphin's loader works: a driver package .zip, e.g. Mesa Turnip) with the Goemon / Hybrid Heaven
  crash safeguard (`SkiesDriverGuard.kt`), and an About tab with Copy device info for bug reports
  (patches/README.md, `recompcore-skies-android-app.patch`). Built (debug, Gradle only); not yet
  tried on the device or with a real driver package.
- **OpenGL ES as the fallback renderer** (Android Settings, Graphics, Renderer: Vulkan default /
  OpenGL ES; the desktop launcher already had a backend choice). Checked on the Retroid (dev app,
  2026-10-11, config backed up and restored): OpenGL ES runs the opening, the Valuan ship field, the
  first battle, the world map and a deck battle at 1.00x / 59.9 fps with 60 fps interpolation, and
  side by side with Vulkan the pictures match (widescreen HUD at the edges included; opening frames
  PSNR 33-39 dB, timing only). Adreno's GLES driver can't link Dolphin's geometry-shader programs
  (line / point expansion; no reason given in the driver's log): the shader cache precompiles 53 of
  them at start, none were needed in those scenes, and `PreferVSForLinePointExpansion` doesn't help
  (Dolphin doesn't offer the vertex-shader expansion on GLES). Each failure was a toast with a file
  path: now a log line (`recompcore-skies-branding.patch`, ProgramShaderCache.cpp).

### Memory card auto-load (2026-10-11; direct since ws20, 2026-10-12)

A card save is loaded by the game, not by a state, so the launchers hand the runtime the file number
(`AutoLoadCard`) and the game patch loads it at boot without the logos, intro, title or load screen
(`patches/game/autoload.c`, user request "isn't number 2 the point of a decomp?"). The runtime
(`SkiesMenu.cpp` namespace `autoload`) waits until the DOL is in memory (the title's call at
`0x801DC350` is a `bl`; the unpatched `bl fn_80228A20`, `0x4804C6D1`, means a DOL without the patch: an
OSD message and nothing else), then on the first frame drawn writes `load_request` at `0x80002EF0`
(`'LOAD'`, file, status 0, error 0). Controller polls start too late for this: by the first one the
title has left state 1. The patch's `dl_title` holds title state 1 (black screen) while it pumps
`FontUpdate` until `FontIsReady` (the logos otherwise wait out the font), then runs the load screen's
own card functions on a stand-in task (`fn_801A4070` read, `fn_801A3A10` poll, XOR and
`"EA_LEGEND v1.01"` check, `fn_801A4354` apply) and starts the field as the title's Continue does
(title state `0x12`, `fn_801DBE6C(6)`). Status 2 = done, 3 = failed (error 1 no file / no card, 2 read
error, 3 bad data): the runtime shows a message and the title goes on to the intro as usual. The runtime
also withdraws a request the title has already passed (title state > 1, or game mode 6) and gives up
after 30 s. Research: decomp `docs/ui.md` *Loading a save without the load screen*.

Checks (2026-10-12): Windows, file 5 -> Shrine Island landing (map `0x67`) ~6 s from launch, file 1 ->
map `0x65`; RAM after the direct load identical to a load through the title and load screen except the
uptime counter `lbl_803474A4`; a missing file 7 -> status 3 error 1, intro and title. Retroid (ws20
APK): Continue from file 5 into the field, user-confirmed.

**The first battle after it crashed (ws20-ws22, fixed in ws23, 2026-10-17).** The second logo (title
state 4) also loads 16 resident files into ARAM and lists them in a checksummed table (`fn_8022A854`,
table `lbl_803165C8`; decomp `docs/ui.md` *The logos also load the resident files*); the battle's
loading looks entries 0 and 3 up and reads `+4` from them unchecked. Without the logos the table
stayed empty, so the first battle after a Continue from a card save stopped on "Invalid read from
0x00000004" (USA PC `0x801D67F0`, European `0x801D7F20`) with a black screen, both games, every
platform; save states made in such a session (auto-saves too) carried the empty table. The 10-12 RAM
comparison above didn't show it. ws23: `dl_title` steps the loader until it's done before reading the
card, as it waits for the font; `qol_frame` (`qol_resident`) steps it once a frame in the field
whenever its state isn't 5 or the table's sum is wrong (only the loader writes the table), and
`qol_encounter_rand` holds random encounters back until then, which repairs those states in about a
second. A loop instead of a step a frame never ends (the reads complete on another thread). Checked on
the Retroid (ws23 debug test app, both games): Continue from the world map save (card file 2) and fly
into a random battle; the ws22-made `eu-10-worldmap.sav`, repaired in ~1 s, then a battle; a missing
file 9 falls back to the intro with the loader done; a normal boot loads it once at the logo and the
repair never runs.

Before ws20 (2026-10-11 to 10-12) the runtime pressed the buttons: Start through the logos, Continue,
slot A, the file, Yes, telling the screens apart by the game's task objects in the heap (task function
`0x801D8DBC` New Game / Continue, `SaveLoadTask` `0x8019E248` steps 4 / 9 / 15 / 19). That took 30-40 s
and, until ws19b, steered by the file list's on-screen row (work `+5`) instead of its absolute cursor
(work `+7`; `+6` = the window's top row), so it stopped on files past the first screen. The task layout
is in the decomp's `docs/ui.md` *Save and load*.

### Importing saves (2026-10-12)

User request: an Import button that behaves like the N64 recomps' (Goemon 64 / Hybrid Heaven
`src/ui/ui_saves.cpp`), for card saves **and** save states, in the launchers' Settings, **Saves** on all
three platforms. One implementation, `lib/ModernGekko/tools/save_import.cpp` (`moderngekko-local.patch`),
used by the desktop launcher and, through `SkiesNative.importSave` / `describeSaves` in
`jni/SkiesImport.cpp`, by the Android app (`recompcore-skies-android-app.patch`), so both accept and
refuse exactly the same files with the same words.

Behaviour, as in the N64 recomps:

- **A prompt before the picker** says what will happen (a card save replaces the file with the same
  number, kept as `.pre-import.bak`; "nothing will be replaced" when the card is empty; a state is
  added), and **Cancel has the focus** (desktop: ImGui modal over Settings, Escape / B cancel it without
  closing Settings; Android: `AlertDialog`, pad A on the focused button).
- **Not while the game runs:** the game holds the card in memory and writes it back. The desktop
  launcher is closed while the game runs; Android checks `NativeLibrary.IsRunning()` before the prompt
  and again after the picker.
- **Checked as strictly as the code that loads it, no more:** the kind is told from the contents (an
  Android content URI has no name, and `.sav` is both MaxDrive and Dolphin). A Dolphin state has its
  version cookie at offset 24 (`COOKIE_BASE + STATE_VERSION`) and `lzo_size` 0; then game ID
  `GEAE8P`, the exact `STATE_VERSION` and extended header version 1 are required, like
  `StateFile.cpp` `ValidateHeaders` / `LoadFileStateData`. One card save: size modulo 8 KiB is the
  header size (0x40 GCI, 0x150 GCS, 0xC0 SAV), read with Dolphin's `Memcard::ReadSavefile` (block
  count checked against the size); its game code + maker must be `GEAE` + `8P`. A card image: a
  multiple of 8 KiB from 512 KiB, `GCMemcard::Open` (checksums, sizes), its `GEAE8P` files exported,
  others ignored. Other regions' saves (`GEAP` / `GEAJ`) and other games are refused with a reason.
- **Nothing half written, nothing lost:** card saves go to card A's GCI folder (`GC/USA/Card A`) under
  Dolphin's own name (`Memcard::GenerateFilename`, e.g. `8P-GEAE-SA_LEGENDS.001.gci` = file 2). Every
  `.gci` there with the **same identity** (game code, maker, file name: what Dolphin's GCI folder goes
  by; it refuses a second file with the same identity) is copied to `<name>.pre-import.bak` first
  (`<name>.pre-import.2.bak` and up when an earlier import's copy is there, so importing twice keeps
  the original too), then the new file is written to `<name>.importtemp` and renamed; a same-identity file under another
  name is removed after its backup. A plain file where the folder should be is reported. States are
  added as `StateSaves/state-skies-import-<date>-<time>.sav` (a romhack's in its own folder, *Romhacks*) (a new name each time; shown as
  "Imported" by the launchers).
- **Continue picks it up:** everything is dated now. A card image's saves are written in the order of
  the card's own last-written times, a second apart, so Continue auto-loads the one saved last.

The tab also shows what is there ("Memory card: files 1 and 3", "Save states: 4"; desktop: card A's
folder path) and the result line (gold, or red for a refusal). `--import-save FILE` on the branded
desktop launcher does the import without the window (tests).

Checked (2026-10-12): `tools/save-import-test.py` on WSL (16 cases, 0 failures; imported files
byte-identical to the originals), the same files through the Windows release exe (scratch
`LOCALAPPDATA`), ImGui screenshots of the tab / prompt / result on WSLg (through a temporary build: SDL
doesn't get xdotool clicks there). Retroid (`skies-recomp-ws16-saveimport-2026-10-12.apk`): a European
save refused; a card image imported files 2 and 3, and Continue booted into the imported file 2 by
itself (59.9 fps); an imported state started from Continue; pad A on the prompt cancels. Android's
Downloads view lists files from the media index, which doesn't see files pushed with adb: browse from
the device's root ("Retroid Pocket 5" -> Download) instead.

### Data folder (2026-10-17)

User request: the data folder somewhere other than `%LOCALAPPDATA%`, on every platform if possible.
User decisions: on desktop a **Settings row that moves the data** plus a **portable mode**; on Android
an **opt-in move with All files access**.

One implementation, `lib/ModernGekko/tools/user_directory.{hpp,cpp}` (`moderngekko-local.patch`,
standard library only, unit test `tests/user_directory_test.cpp`), used by the launcher, the runner and,
through `jni/SkiesImport.cpp` (`recompcore-data-folder.patch`), the Android app:

- **Which folder:** `portable.txt` beside the programs → `<exe dir>/User`; else `data-folder.txt` in
  the default folder (one UTF-8 line, an absolute path) → that folder; else the default
  (`%LOCALAPPDATA%` through `_wgetenv`, `$XDG_DATA_HOME` if absolute, `~/.local/share`). A pointer whose
  folder is gone (a drive not connected) is reported, not used: the launcher asks (Try Again / Use
  Default / Quit; short lines, as Windows' message box doesn't wrap) before anything reads the folder, the runner started by itself says so and
  stops (it would otherwise create an empty folder there). The launcher still passes `--user-dir`.
- **Moving** (`MoveUserDirectory`): into `<picked>/Skies of Arcadia Legends` (or the picked folder if
  it has that name). Refused into itself, its own parent chain or a folder that holds data; such a
  folder can be **used as it is** instead (`PointUserDirectory`: reinstalling, another PC's copy). Same
  drive: one rename. Otherwise a copy into `<target>.moving` with file times kept (Continue picks the
  newest card save / state by time), renamed into place, then the pointer, then the old files are
  removed (the pointer file itself stays when the old folder is the default). A failed or cancelled
  copy removes `.moving` and changes nothing. `default-game.txt` holds an absolute path into the folder:
  rewritten into the new one, and `ResolveDefaultGame` falls back to `games/<name>` in the data folder
  when the stored path is gone (a folder copied by hand, a USB drive with a new letter).
- **Desktop launcher:** Settings → Disc → Data folder (path, Change..., Move Back to the Default when
  moved, Open). Change opens `SDL_ShowOpenFolderDialog` (Documents, or beside a moved folder), then an
  ImGui modal says from where to where (Cancel focused); the move runs on a thread with a byte progress
  bar and Cancel; when it's done the launcher **starts again in-process** (`main` loops over
  `RunLauncher`; the launcher keeps no Dolphin globals: every reader takes `user_directory`). Disabled
  while a disc or Dreamcast-music import runs. `--move-data-folder DIR` does the same without the window
  (tests). Portable shows the folder with Change disabled.
- **Portable mode, [issue #6](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/6)
  (2026-10-20):** "portable.txt still puts 1.5 GB in AppData". Not reproduced on v0.60.0 (command-line
  and GUI extract, two minutes of play, Latin/Japanese folder names, another working directory: all in
  `User\`, AppData only had Windows' own files); it fails silently only with `portable.txt.txt` (Explorer
  hiding extensions), `portable.txt` in the zip's outer folder, a pre-0.50 release, or data left from
  before the file was added. So: `portable.txt.txt` counts (`PORTABLE_MARKER_TXT_TXT`); not portable
  with one in the folder above (`MisplacedPortableMarker`) → a red line under Data folder; portable with
  data in the moved or default folder (`EarlierUserDirectory`) → the line names it, with **Move It
  Here** while the portable folder holds only its first start's `config.ini` (`HoldsOnlyFirstStartFiles`),
  and at the portable folder's very first start Settings opens on the Disc tab with the prompt.
  `MoveUserDirectoryIntoPortable`: the first start's `config.ini` set aside (put back if the move
  fails), `MoveUserDirectory`, then its pointer and the emptied default folder removed, so nothing stays
  outside. `--move-earlier-data` does it without the window (tests). A folder the launcher can't write
  (`CheckUserDirectoryWritable`: a portable copy in Program Files) gets a "Data folder" box saying so
  and what to do, instead of "Invalid ... config.ini". Tested: unit test; Linux release build (CLI move,
  the prompt opening by itself, Move Here → restart, the XDG folder gone); Windows release build in
  copies of the v0.60.0 zip with `LOCALAPPDATA` redirected (`C:\dev\SkiesIssue6`): `portable.txt.txt`,
  the outer-folder line, the unwritable box, a real install moved in by the prompt (rename, 1.4 GB),
  Start Game from it, the note once a game is there. One unrelated Windows extraction failed once
  with "can't move the new game folder into place: Access is denied" and worked on the retry (likely
  a scanner holding the new files; the rename has no retry: `docs/backlog.md`). On `dev` as `3f4c4ec`,
  validate 39474645790 green.
- **Android:** the pointer lives in app storage (`getExternalFilesDir`), the desktop format;
  `DirectoryInitialization` takes `SkiesDataFolder.userDirectory`: the moved folder when it can be used
  (All files access granted and the folder there), app storage when nothing was moved, and for a moved
  folder that can't be reached a throwaway `cache/data-folder-unreachable` (falling back to app storage
  filled it with Dolphin's default configs and `Wii/`, which then made app storage "hold data" and
  refused the move back). `SkiesLauncherActivity` asks before showing the home screen (Allow Access /
  Try Again, Use App Storage, Quit); each answer starts the process again on the right folder. Change asks for All files access first (`ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION`, back
  in `onResume`), then lists internal storage and each mounted SD card / USB volume
  (`StorageManager.storageVolumes`) plus app storage when moved and "Another folder..."
  (`OpenDocumentTree`, a tree URI turned into a path: `primary:` → `/storage/emulated/0`, else
  `/storage/<volume>`); the folder in use isn't listed. A folder in shared storage gets an empty
  `.nomedia` (else the gallery indexes screenshots and HD texture PNGs). The move is the native one on a thread (`moveDataFolder`, progress from
  `dataMoveProgress`, Cancel), then `Intent.makeRestartActivityTask` + `exitProcess(0)` so Dolphin takes
  the new user directory from the start. **Android 11 or later only:** the branded app drops the old
  storage permission (`src/skies/AndroidManifest.xml`) and Android 10's scoped storage allows it no raw
  paths; older Android says so. `MANAGE_EXTERNAL_STORAGE` is declared in `src/main` (fine for a
  sideloaded APK; Google Play would restrict it).
- **Copy speed:** on Linux / Android files are copied with `read` / `write` in 4 MiB pieces (progress
  per piece, Cancel within a piece); `std::filesystem::copy_file` managed ~4.5 MB/s from app storage to
  shared storage on the Retroid (1.36 GB, 5,573 files: ~7.5 min). With the buffered copy, shared storage
  back to app storage took 72 s including deleting the old files and the restart. Windows keeps
  `copy_file` (`CopyFileW`): C: → D: 1.36 GB in a few seconds.
- **Tested 2026-10-17:** unit test (control: a copy without the file times fails it). Linux release
  build: `--move-data-folder` tmpfs → disk (copy), disk → disk (rename), into itself (refused), back to
  the default, a folder with data (used), runner with a moved and a missing folder; Settings rows by
  screenshot. Windows release build (scratch `LOCALAPPDATA`, scripts in `C:\dev\SkiesDataDir\build`):
  picker → `D:\...` → prompt → copy with progress → in-process restart → Start Game ran from D:
  (29.7 fps, logs there); Open; Cancel mid-copy (nothing moved); Move Back (6 s); missing folder box
  (Use Default, Escape = Quit); portable. Retroid (`com.skiesofarcadia.recomp.test`, uninstalled
  after): disc import, All files access screen and back, Internal storage move, game boots from
  `/storage/emulated/0/Skies of Arcadia Legends`, access revoked → prompt → Allow Access → restart,
  move back to app storage, missing folder → app storage untouched → Use App Storage. Card save times
  kept on all three. No SD card was in the Retroid: SD card folders are untested.
- **Test build beside the release app:** `tools/android-ws-build.sh BRAND=1 TEST_ID=1` (gradle
  `-PskiesAppIdSuffix=.test`) builds `com.skiesofarcadia.recomp.test`, "Skies of Arcadia Legends
  (test)", so a device's release app and its data are never touched (the branded debug build is
  otherwise `com.skiesofarcadia.recomp`, the release app's ID, with another signature).

### The European release (GEAP8P; 2026-10-17)

Requested by a player: the European disc, for its languages (English, German, French, Spanish). It
is its own build of the game: a `main.dol` 15 KB larger (SHA-1 `295a9fc9`), the text in
`english.sot` / `german.sot` / `french.sot` / `spanish.sot` (tables the USA build keeps in the DOL),
multi-language scripts (`field/*.sct` about twice the size) and language-suffixed `.mld` / `.mll`
files (`g`, `f`, `s`; English has none). So it gets its own patched DOL and module; USA code can't run
European data.

- **Game patch:** the same sources, addresses translated (`patches/game/README.md`, *The European
  release*; `tools/dolmatch.py`, `patches/game/GEAP8P.addresses.json`). `build/game/GEAP8P-ws20` to
  `GEAP8P-ws22` build; the cave layout is identical to the USA one. The functions the patches use that
  differ in Europe were read side by side (2026-10-17): the title state table has the same 19 states
  (Europe rewrote the logo and Press Start handlers, slots 2 and 10-13, and added a flag the title's
  cancel path sets), so autoload's state 1 / 0x12 hold; the save payload (`EA_LEGEND v1.01`, XOR over
  0x31FC bytes at +0x1440) is the same, so a USA card save loads in the European game with its GCI's
  game code changed to GEAP (`build/user-eu` has the user's saves that way); `fn_8014419C`'s ship-sunk
  branch stores both reward words in Europe too (dolmatch's "one word" was its aligner pairing reordered
  instructions; the enemy rows are 104 bytes instead of 120), so the ship battle EXP / gold scaling is
  right; the other differences are address shifts and a European video / language init block in the
  mode dispatcher's mode 0.
- **Language:** the game calls `OSGetLanguage` once at boot (EU `0x801DE38C`): 0 English, 1 German,
  2 French, 3 Spanish (Italian / Dutch become English); no menu, not in the save. New setting
  `[Skies] Language` (`Config::SKIES_LANGUAGE`); `SConfig::GetLanguageAdjustedForRegion` returns it
  for GEAP8P. The in-game menu's Gameplay tab has a Language row (European disc only, "Changes when
  the game next starts").
- **Language tab and restart (2026-10-18, user request):** Language has its own tab in the in-game
  menu, the desktop launcher's Settings and the Android Settings (European disc only; with the USA disc
  a line says it is English only). In the in-game menu a language other than the running one makes A
  ask "Press A again to restart"; the second A applies and saves the menu's values and stops the
  emulation (`Host_Message(WMUserStop)`), and the frontend starts the game again at its title screen:
  the desktop runner restarts itself with the same arguments less `--load-state`, Android's launcher
  activity relaunches once the session has closed (`SkiesNative.takeRestartRequest`). Not in netplay.
  Closing the menu instead keeps the old behaviour (the next start). Up / Down, a tab switch or a new
  value cancel the question. A save state still holds the language it was made in (the text pack is in
  guest RAM), so loading one made in another language brings that language back until the next start.
  Checked on WSL: English to German and French to Spanish restarts (opening text), one A then B saves
  without a restart, the launcher's tab reads and writes `[Skies] Language`. Windows (MSVC release
  exes in the CI package of `db0f23a`): English to German and English to Spanish, the first runner
  waiting on the second, both gone after a stop; the prompt and footer name the key in use (X with the
  keyboard, A with a controller). Retroid (branded test app): the Settings tab (Français saved, the
  game booted in French), then Deutsch to Español in game. The first Android try came back to a black,
  paused game: the launcher activity resumes while the finished game screen is still being torn down,
  and that screen's fragment then pauses the emulation (`onPause`) and releases the surface, hitting
  the session just started; the relaunch now waits until it has detached
  (`NativeLibrary.getEmulationActivity()` back to null, at most 10 s). A live switch without a
  restart was researched the same day (*Live language switch* below).
- **60 Hz:** the game picks its render mode from the TV format (`VIGetTvFormat`): PAL gives 50 Hz
  (5/6 speed; no PAL compensation in the code), EuRGB60 panics ("Invalid TV Format."). So
  `SConfig::SetPathsAndGameMetadata` sets the region to NTSC-U for GEAP8P (VI preset NTSC, `0xCC` =
  0). Memory card saves are `8P-GEAP-SA_LEGENDS.*` in the USA card folder (Import a save put them in
  `GC/EUR` until 2026-10-20, where the game never looked: `save_import.cpp`'s `RegionFolder` now
  says `USA` for GEAP8P).
- **Frontend addresses:** `VideoCommon/SkiesMenu.cpp` has `RegionAddress{usa, europe}` (converts to
  the running disc's) for its game addresses; two hook-check words differ (qol `0x4819EFD5`,
  autoload `0x4804DC89`). `IsSkies()` accepts both IDs.
- **Checked (WSL, 2026-10-17):** booted to the French opening text, the French title ("Nouveau Jeu /
  Continuer"), new game, into the opening cutscene; widescreen on; 27-29 fps at 0.89-0.99x (so the
  60 Hz mode). Later the same day, in English: a new game through the opening and its first battle
  (widescreen HUD at the edges), the launcher-style auto-load of a card save, the title's Continue and
  load screen (after the DolRecomp fix below), loading a save, the camp menu, the red alert room's
  screen-copy pulse in widescreen (after the `fullwidth.S` fix), the Dreamcast music. The user's USA card
  saves work in the European game with the GCI's game code changed to GEAP (`build/user-eu`). Then
  (all WSL, after the DolRecomp fix): saving from a save point (a new file 4, loaded back by
  auto-load), a savestate round trip, and Baltor's ship battle at 1.00x through
  `tools/shipbattle-state-win.sh`'s method with European addresses (`SHIP_REQUEST` `0x8031DFE4`,
  weapons `0x803103F4`, map name `0x80309370`; state `build/savestates/eu-17-ship-battle-baltor.sav`;
  HANDOFF-dcmusic-eu.md). The menu's Language row itself wasn't seen (the menu is an ImGui overlay that
  automation screenshots don't show); its effect, the boot language, was. Not measured: European speed
  on the Retroid (the module has no PGO).
- **Module:** `build/eu-gen22/module-linux/gGEAP8P_recomp.so` (DolRecomp 3 min with `-j8`, no PGO:
  the profiles are keyed by fixed address chunks such as `func_8023B000`, which don't line up with
  the shifted European code, so Europe needs its own profiles). `tools/module-linux.sh` takes
  `GAME_ID=GEAP8P`.
- **The text packs and a DolRecomp bug (2026-10-17):** the European game loads its language's text
  pack at boot (`ENGLISH.SOT` etc., 104 entries; loader EU `0x801CE570`) and builds a directory of it
  with `sthbrx` (EU `0x801DD504`). DolRecomp's IR / LLVM path swapped all four bytes for `lhbrx` /
  `sthbrx` and then stored the low two, so the count `0x0068` was stored as 0, every lookup (EU
  `0x801DD238`, about 90 callers) got the fallback "SOT ERROR", and the strings the save / load
  screens take from the pack (11 pointers at EU `0x80349ED0`, split at boot from the entry
  "WmemTbl") were garbage: the title's Continue and the save point menu then flooded the log with
  invalid reads in `FontExpandRle` (no range check on the glyph code) and stalled, with any save.
  `patches/dolrecomp-halfword-byterev.patch` (the C emitter's `bswap16` was right). The USA game has
  two `sthbrx` in `fn_80298798`, so USA LLVM modules change too.
- **Releases (desktop):** the release pins are per region: `MODERNGEKKO_REQUIRED_DISC_ID`,
  `MODERNGEKKO_REQUIRED_DOL_SHA256`, `MODERNGEKKO_DOL_PATCH_MANIFEST` and
  `MODERNGEKKO_GAME_SETTINGS_OVERRIDE` are lists in the same order, CMake writes `release_games.inc`
  and `tools/release_games.hpp` looks a game up by disc ID (the runner reads it from `sys/boot.bin`
  before patching). The runner loads `g<disc ID>_recomp`, so a release carries both modules.
  `release/GEAP8P.ini` has the European idle PC (`0x8023B02C`, `SelectThread` at `0x8023AEE0` + the
  same offset) and `release/skies-widescreen-eu.dolpatch` the European patch. `build-release-linux.sh`
  / `.bat`: `REGIONS="us eu"` at configure, `package USA EUROPE [VERSION]`; without them, USA only as
  before. Continue's card saves go by the game's own code (`GEAP` files), Import a save by the game.
- **Releases (Android):** `jni/SkiesImport.cpp` picks the region from the disc ID, imports into
  `Games/<disc ID>` with that region's patch (its `output-sha256` is the build check) and removes the
  other region's folder (one game at a time); `installedGameId` / `patchName` for the Kotlin side,
  saves by the installed game. The APK carries `libgGEAP8P_recomp.so` and the European assets when
  `tools/android-ws-build.sh` gets a European DOL or `PREGEN_EU`; `skies_dol_sha256_eu` from the brand
  dir's `dol-sha256-eu.txt`. Save states aren't per region: a state from the other disc's game won't
  load. A European APK (`skies-ws22eu-2026-10-17.apk`) was built but not run on the device; the release
  APK carries both modules.
- **CI:** `tools/ci-codegen.sh` does both regions when the secrets repo has `main-eu.dol` (else the
  USA alone; `REGIONS` sets it), into `windows-eu/` etc., without PGO until there are European
  profiles (`pgo/*-eu.profdata`; on Android 60 fps likely needs one, trained on the device). The
  secrets repo has `main-eu.dol` since 2026-10-17 and `validate.yml` sets `REGIONS: us eu`, so every
  package carries both modules (`2436886`); the publish check knows both DOL hashes. Since 2026-10-19
  CI runs it once per target and region (`TARGETS` / `REGIONS`), each its own job and cache key,
  with the region's DOL patch only (BUILDING *CI and releases*).
- **Shipped** in v0.10.0-beta (2026-10-17, `61ac70e`).

### The Japanese release (GEAJ8P; 2026-10-20, branch `japan`)

Requested by the user: the Japanese disc, *Eternal Arcadia Legends* (`GEAJ8P`, an ISO; SHA-1
`bb06f5ba...`, the Redump entry's: `../GameCube-Research/targets/SkiesOfArcadiaLegends/README.md`). Extracted with `dolphin-tool extract` to
`../GameCube-Research/targets/SkiesOfArcadiaLegends/jp/disc` (the ISO beside it). Its own build: a
`main.dol` 1.5 KB *smaller* than the USA one (SHA-1 `74d8ff5d`, SHA-256 `4aa0f3dc`), far closer to the
USA code than the European build (the text is in the DOL as in the USA, no `.sot` packs); the files
are the USA ones but for `FontData.jp`, Japanese voices (`sound/cr*`, `e*`, `stv*` and others differ),
four extra `beff/f2911?f3.mlk`, and no `field/mb*.std`, `list2.txt`, `shop_hrse.mll`. The music banks
(`m0231`-`m0465`, `m01`-`m04` streams) have the same names as the USA's.

- **Game patch:** ws26 on the Japanese DOL through `patches/game/GEAJ8P.addresses.json` (209 addresses,
  `tools/dolmatch.py --target GEAJ8P`, which now writes the table itself; on the European DOL it gives
  the existing table exactly). 663 call sites as in the USA. Differences found: `patches/game/README.md`
  (*The Japanese release*); the only one the patches feel is the direct card load skipping the
  Japanese load screen's three bonus-file event flags (`EA_LEGEND_EXTRA`), harmless. Release patch
  `release/skies-widescreen-jp.dolpatch` (output `d4867b46`); `release/GEAJ8P.ini` (idle PC
  `0x8023772C`, dual core).
- **Region:** left as Dolphin sets it, NTSC-J: 60 Hz, the Japanese IPL settings, the font encoding
  Japanese, card saves in `GC/JAP/Card A` (the launcher, Android and Import a save already look there
  by the disc ID's region letter). The card file is `EA_LEGEND.nnn`, so Import a save's name check
  (`SaveGame` / `SkiesSaves`) takes `EA_LEGEND.` for GEAJ8P. Language: Japanese only (the menu's and
  the launchers' Language tabs say so).
- **Frontend:** `recompcore-japan.patch` (last in the series): `SkiesMenu.cpp` `RegionAddress{usa,
  europe, japan}` (game mode `0x80347048`, map `0x80311544`, camera manager `0x803113F0`, field
  camera `0x80302A14` / `0x80302A20`, title state `0x80311560`; hook sites camera `0x8022D494`
  (`0x4BFFF8E9`), QOL `0x800C2064` (`0x4819C711`), autoload `0x801DC170` (`0x4804C625`); all
  high-confidence matches, the same method gives the European column), `SkiesMusic.cpp` `JAPAN`
  (box `0x8031A310` ...), `SkiesInput.cpp`, Android import (third region). `moderngekko-local.patch`:
  the launcher's `IsSkies`, Language tab text, `EA_LEGEND.`.
- **Release plumbing:** `REGIONS` takes `jp` everywhere `eu` was: `tools/build-release-{linux,macos}.sh`
  and `build-release-win.bat` (package takes the modules by name), `tools/ci-codegen.sh` (`main-jp.dol`,
  `-jp` outputs and profiles), `tools/android-ws-build.sh` (`DOL_JP` / `PREGEN_JP`, assets,
  `dol-sha256-jp.txt`), validate.yml (matrix region `jp`, `DOL_SHA256_JP`, `REGIONS: us eu jp`, the
  modules in every package) and release.yml (the publish check knows the Japanese DOLs and wants the
  `gGEAJ8P_recomp` module in every package). CI reads `main-jp.dol` (the original Japanese DOL)
  from the secrets repo (added 2026-10-20, RecompSecrets `83fe499`).
- **Checked (WSL, 2026-10-20):** dev runner (`build-egl`, Linux module `build/jp-gen26/module-linux`,
  no PGO): boots to the Japanese opening text, title, new game, rumble prompt, the opening on
  Alfonso's ship, the first battle won and its results screen, 1.00x (llvmpipe dips while loading),
  widescreen, no invalid reads; state `build/savestates/jp-01-first-battle-results.sav`. Release
  build (`REGIONS="us eu jp"`, portable copy `build/rt-jp`): `--extract` of the ISO imports
  `games/GEAJ8P` with exactly the pinned DOL; the runner boots it; the QOL hook live (`QOLC`), camera
  control (`CAMC`, Z + C-stick turns the camera); Continue: the user's USA card file 3 converted (game
  code `GEAJ`, name `EA_LEGEND.002`) auto-loads into Pirate Isle's underground (`load_request` done);
  save states made in either runner load; the Dreamcast music (European test import) takes the field
  song (`dcm_box` live, the GameCube song muted); Continue from file 2 (world map), a random battle
  with Z cycling the battle speed 1 / 2 / 3 (`qol_input.battle_speed`), won with Invincible at 2x:
  results, back on the map (without Invincible Vyse alone lost: a game over, the title). **Windows**
  (2026-10-20, `C:\dev\SkiesRomhack`: release build `REGIONS=us eu jp`, module
  `build\modules-llvm-jp26` from `tools/module-llvm-win.sh` without PGO, package `vjp-test` with the
  USA ws26 module and the Japanese one, portable copy `build\rt-jp-win`): the launcher's `--extract`
  of the ISO over `\\wsl.localhost` (pinned DOL), Continue to the world map, 1.000x flying (n=20),
  a battle at 2x at 0.999x (59.8 game frames a second), Japanese text and the widescreen HUD.
- **Android (2026-10-20, Retroid Pocket 5 and Retroid Nova, test app):** the app's own picker import
  of an RVZ of the disc (`Games/GEAJ8P`, the pinned DOL `27a4261c`, `main.dol.orig` kept; the USA
  game's folder removed, one game at a time), Start Game to the Japanese opening text (Nova, fresh
  install), Continue from the converted card save (file 1: the Valuan ship's red alert, game mode 6).
  **Speed without a Japanese PGO profile is full:** Continue into each state, 16 s, `speed:30`, 60 fps
  mode, two rounds per device (`build/android/bench/jp26b-r{1,2}`, `jp26b-nova-r{1,2}`, script
  `build/android/bench/jp-bench.sh`): Retroid Pocket 5 1.000-1.007 (59.8-59.9 fps) in
  `jp-04-red-alarm` (new, saved there), `jp-03-worldmap-battle-start`, `jp-02-pirate-isle-save-point`,
  `jp-01-first-battle-results`; Retroid Nova (Snapdragon 8 Gen 2, Adreno 740, stock driver) 1.000-1.005 (59.8-59.9 fps) in the same four. Europe needed its own profile (0.93); the Japanese
  module apparently doesn't, but there is no Japanese ship-battle state to try the heaviest scene.
  The first-run text said "USA or Europe": now names Japan too.
- **Save states per version (found here, fixed 2026-10-20):** with the Japanese game imported on the
  Retroid, Continue still offered the USA game's newest auto-save: every version shared `StateSaves`,
  and nothing checks a state's game ID before loading it (`State.cpp` doesn't; only Import a save
  does). It booted the USA state on the Japanese module: "Invalid write to 0x00008032, PC =
  0x80237bb0" at once. The European game has had the same problem since 0.10.0-beta (desktop keeps
  both games, so a USA state newer than the European ones was always Continue's pick there). Now the
  release's first game (USA) keeps `StateSaves`, the others use `StateSaves/<disc ID>`
  (`dol_patch.cpp` `StateDirectory`'s `disc_folder`; `release_games.hpp` `StateFolderFor` on desktop,
  the app's first region in `SkiesImport.cpp`), for Continue, the launcher's list, Import a save and
  the in-game menu and auto-save (the runner sets `RuntimeConfig::state_directory`). States an
  earlier release left in `StateSaves` move to their game's folder by the game ID at the start of
  the header (`MoveDiscStates`; only `state-skies-*` / `recovery-skies-*`, a name already there is
  left). Romhacks keep `StateSaves/romhack-<tag>`. Checked: WSL (release copy `build/rt-jp`, all
  three games: a USA, a European and a Japanese state in `StateSaves`; the Japanese one moved, the
  launcher's Continue skips the newer USA one and boots the Japanese one; the auto-save goes to
  `StateSaves/GEAJ8P`; the European one moved when that game was picked and Continue booted it on the
  world map; the USA game still offers its auto-save from `StateSaves`), Windows (`build\rt-jp-win`:
  the move, Continue, the auto-save in `StateSaves\GEAJ8P`), Retroid Pocket 5 (Continue hidden with
  only USA states, then the Japanese states and auto-save in `StateSaves/GEAJ8P`). `--headless`
  release runs stop on an ImGui assert (`GetIO`, no context) in any build, unrelated.
- **Not done:** saving from the Japanese game itself (early in the story only at save points; the
  scripted walk didn't open the save menu there; the format is the USA one, statically); the in-game
  menu's Language row on desktop and Android (automation input doesn't open the menu; by hand).
- **macOS (2026-10-20, the Mac session, M4, `japan` at `8d3c50a`):** the patches apply; the Japanese
  arm64 module without PGO (codegen 2.5 min, DOL `d4867b46` / `27a4261c`) beside the 0.60 CI zip's USA
  and European modules, release build `REGIONS="us eu jp"`. `--extract` of the RVZ (pinned DOL, 2.6
  s), the Japanese opening text and title, New Game through the opening at 1.00x to the first battle
  (1.00x; Z to 2x at 1.00x, 59.9 fps), the in-game menu's Language tab ("Japanese: the Japanese disc
  is in Japanese only"), a menu save state in `StateSaves/GEAJ8P` that boots, the state folders (a
  newer USA state in `StateSaves` not offered for the Japanese game; a European state there moved to
  `StateSaves/GEAP8P` and offered), the USA and European games still boot in the same build. Then,
  with the converted Japanese card files sent over: Import a save puts them in `GC/JAP/Card A`
  (a USA card save is refused for the Japanese game, as designed; so is a USA save state, and a
  Japanese state imports into `StateSaves/GEAJ8P`); Continue from file 2 reaches the world map and
  file 3 Pirate Isle's underground at 1.00x; flying the world map 1.00x at 30 fps, field speed 2x and
  3x (encounters off) 1.000x at 59.9 fps with no step-down; field speed in the opening 2x. The user
  once saw the window turn green (Metal, windowed, probably at the end of the field-speed run); a
  rerun with a capture every 10 s didn't show it. Not reproduced.

### Linux on older distributions (2026-10-20)

The AppImage catalog (AppImageHub) has a bot that finds GitHub projects publishing AppImages; it
opened a pull request to list v0.60.0-beta and its test failed: the test runs the AppImage on the
oldest Ubuntu LTS in standard support (22.04, glibc 2.35), and the release, built on 24.04, needed
`GLIBC_2.38` / `GLIBCXX_3.4.32` (the binaries and the libraries linuxdeploy bundles from the builder).
The `linux` job now runs in an `ubuntu:22.04` container with GCC 12 (22.04's newest, Dolphin's
minimum; CMake 3.22 is enough). Two things didn't build there: `std::ranges::contains` (GCC 13) in
upstream ModernGekko, now `std::ranges::find`; and Dolphin's `PlatformWayland.cpp` listeners, which
name events 22.04's Wayland headers don't have (`recompcore-wayland-old-headers.patch`, with the bind
versions capped to match). Measured on the container build: everything in the AppImage needs glibc
2.34 and `GLIBCXX_3.4.30` at most, the module glibc 2.4. A stock `ubuntu:22.04` with Xvfb, the usual
desktop libraries (ALSA, X11, GTK, Mesa) and no network (`unshare -n`) shows the launcher's home
screen; `--extract` of the Japanese RVZ gives the pinned DOL and the runner boots to the title
(software GL; a container needs `--shm-size`: the default 64 MB `/dev/shm` gives a bus error, as
Dolphin's memory arena lives there). The newer build machines are unaffected (both builds compile; WSL
`--wayland` still runs). The AppImage's name drops `-linux-` (the catalog's naming rule; `release.yml`
matches `-x86_64.AppImage`). Not yet in a release; after one, `/retest` on the catalog's PR.

### Romhacks (2026-10-18, branch `romhack`)

User request: play romhacks of the game, the first being **Maeson's rebalance** ("Skies of Arcadia:
Legends Maeson" 1.51, romhacking.net 2859: enemies, equipment, magic, ship parts, EXP curves; an
xdelta against the USA ISO, MD5 `3e7fa503...`, plus an optional 98-byte IPS, cleartonic's "hold B for no
random encounters", romhacking.net 5314). Research and the first tests: `docs/handoffs/HANDOFF-romhacks.md`.

**What a romhack can change, and what the module runs.** Files in `files/` are read from the
extracted folder, and nothing pins them (`MODERNGEKKO_REQUIRED_ASSETS_SHA256` is empty), so a romhack's
data files just work. `main.dol`'s data sections are read from RAM by the compiled code, so changed
tables work too. Changed **code** doesn't: the module runs the code it was compiled from, and the
runtime's SMC check only catches a changed chunk where it enters it through the dispatcher. Maeson's
disc has the hold-B code built in (a one-word branch at `0x800C1FC8` in the encounter function and a
cave made by rewriting the unrolled zero-fill at `0x800C88E4` as a loop): with the shipped ws22 module
the runtime flagged the cave's chunk (`smc_failed=1`) but the branch's chunk is reached by native calls,
so the old code ran and holding B changed nothing; a module compiled from the romhacked DOL ran it
(no battle in 60 s with B held, twice; one after 15 s without; `smc_failed=0`).

**What the release takes** (the user's choice, "options 1 + 2" of the handoff): a romhack whose
`main.dol` has the original's code (header and text sections) and any data, patched with the release's
own `.dolpatch`, on the release's module; and a few known romhack code changes, undone on import and
replaced by an option of the release's own (Hold B). Anything else that changes code is refused at
import with a message.

- `tools/mkdolpatch.py` writes `input-size`, `input-code-sha256` and `output-code-sha256` (SHA-256 of
  the 0x100-byte header and every text section in order: a change of layout changes it too),
  `data-original` lines for the original game data the patch changes (ws25: three bytes, the battle
  action table's first entry at `0x802DBB10`, `redirect_pointers`; Europe the same table) and the
  `known-change` lines of `patches/game/GEAE8P.known` (checked against the original). An older
  frontend never sees the new keys: the patch ships with its programs.
- `tools/dol_patch.cpp`: a DOL that is neither the original nor the output is checked by its code
  hash after undoing every known change whose bytes are all there (by name); the original's code
  means the patch applies (after cutting the file back to the original's size, so the new sections
  start clean), and the result must have the output's code hash; a romhack that changed a
  `data-original` byte is refused (it would lose that change or break the patch); the output's code
  hash with every run in place means already patched. `DolMatchesPin` is the release check: the
  pinned whole-file SHA-256, or the patch that makes that build (its `output-sha256` is the pin) and
  the same code with its runs in place. The pins (`MODERNGEKKO_REQUIRED_DOL_SHA256`, Android's
  `skies_dol_sha256`) and every build script stay as they were.
- Desktop: the runner and the launcher check through `DolMatchesPin`, the runner's log says
  "main.dol: modified game data (a romhack)", and the launcher shows "Ready: Skies of Arcadia Legends
  [GEAE8P, modified]". The launcher used to keep an existing import with the same disc ID ("Using
  existing extraction", or with an earlier release's import, only `sys/` taken again): a romhack and its
  original share the disc ID and can share the file table and the DOL too, so the disc picked is now
  always extracted (3-7 s).
- Android: `PatchDol` and the launcher's check at every resume (`SkiesNative.dolMatches`, which
  replaced the activity's own SHA-256 of `main.dol`) go through the same code; a romhack that changes
  code gets "This disc's game code is modified...".
- **Hold B** (ws25, Settings → Gameplay, the in-game menu, Android Settings; `[Skies] HoldB`): no
  random battle while controller 1's B is held, on foot or sailing, as cleartonic's patch; it reads the
  game's own `PADStatus` buffer, so remapped controls count (`patches/game/README.md`, ws25).

Checked 2026-10-18 on WSL (Linux release, USA and European games), Windows (release, LLVM + PGO
module) and the Retroid (the debug test app): a Maeson import plays at 1.00x on the release's module,
Hold B stops encounters only while B is held, a disc with changed code is refused; details in the
handoff.

**Save states per disc** (2026-10-18, after the merge): a state holds the DOL's data, so one made with
the original game brought the original's item and magic tables into the romhack (and the other way
round; `RestoreBootDolCode` brings back code only), and a romhack keeps the disc ID, so the game-ID
check doesn't stop it. `RomhackTag(game_root, patch)` (`tools/dol_patch.cpp`): "" for the original
disc, else 8 hex digits of SHA-256(data sections + `sys/fst.bin`), the data sections only those inside
the original's size (the patch adds its own past the end) with the patch's `data-original` bytes put
back, compared with the patch's `input-data-sha256` / `input-fst-sha256` (`mkdolpatch.py --fst`). A
romhack's states live in `StateSaves/romhack-<tag>`: the runner passes it to the runtime
(`RuntimeConfig::state_directory`, `File::SetUserPath(D_STATESAVES_IDX)`, so the in-game menu and
auto-save use it), the launcher's Continue, state list and Import a save use it
(`GameStateDirectory`, `SaveImportGame::state_directory`), and on Android the launcher activity calls
`SkiesNative.useStateFolder` at every refresh (the process's state folder, and Continue's); Back up
saves takes the romhack folders too. Memory card saves stay shared, as the romhack's author meant.
Checked: Maeson's tag `4d5febec` on WSL, Windows and the Retroid (auto-save there, Continue offers it),
the USA and European originals "" (states in `StateSaves`). Android's Start Game then got the
desktop's second line, "Ready: Skies of Arcadia Legends [GEAE8P, modified]" (when the state folder is
a `romhack-` one), and its no-disc hint names both discs; the picker import (a code-modified disc
refused with its message, Maeson imported) was checked on the Retroid too (HANDOFF-romhacks).

Not covered: romhacks of the European disc are taken the same way, but there's no known-change
table for it (none found yet).

### Live language switch (research, 2026-10-18)

Static analysis of the European DOL (EU addresses; V = read in the disassembly, I = inference), for
switching the language without a restart. Not built.

- **Boot only (V):** the game-mode dispatcher `0x801DDE9C` (mode at `r13-28916`) runs mode 0 once:
  clamp `g_langIndex` (`r13-28744`), store gameLangId (`r13-32448`, getter `0x801DD188`), load the
  `.SOT` (`0x801CE570`), build its directory (`0x801DD504`), then `0x801DD3EC`, which calls 63
  initialisers that fill fixed string-pointer tables (items, magic, ship parts, shops, pause menu,
  WmemTbl's 11 pointers via `0x801A5E54`, ...). Each of the three has one caller; nothing re-enters
  mode 0. The initialisers allocate nothing and can be re-run. The text font byte `0x803140F3` is
  rewritten from `g_langIndex` every frame (`0x801DE980`).
- **Memory (V):** the `.SOT` and its 4992-byte directory come from a text heap of 0x22500 bytes
  (created at `0x801DE4CC`, handle `r13-28708`), read synchronously, never freed. Sizes: English
  132111, French 134921, German 135308, Spanish 134145 (104 entries, same names). A second stock load
  can't fit: a switch must reread into the boot buffer in place, so the boot allocation has to be
  German's size whatever the language.
- **Follows a change:** the string tables after re-running `0x801DD3EC`, the font byte, the disc
  error screens and per-frame draw offsets at once; language-named files (`.mll` suffix via
  `0x8012B208`, battle command windows, shop / bounty graphics) at their next load; field scripts carry
  all four languages (labels `MA` / `MFA` / `MGA` / `MSA`, the VM has no language branch of its own, so
  probably a script test on the font byte, I) from the next event. A window open at the switch keeps
  pointers it copied (I).
- **Design, if built:** a game patch with a boot hook (allocate 135308 bytes) and a cave routine run
  from the top of the main loop on a request word from the menu: set the language words, reread the
  file into the same buffer, rebuild the directory without allocating, call `0x801DD3EC`, ack. Only
  where no text window is alive (the title before its menu, field free-roam); the flag for that is not
  known yet. Save states would need the language re-applied after a load.

### Dreamcast music (2026-10-17)

Requested by a player: the Dreamcast version's music as an option. The Dreamcast music is
sequenced, played by Sega's Manatee-family driver (`BONITO.DRV`, "bonito ver.2.00.00.00 alpha3",
DigitalMedia 2000) on the AICA's ARM7 from MLT banks, so the port runs that driver: the player
imports the files from their own Dreamcast disc, and an emulated AICA plays them while the game
patch mutes the GameCube music (`patches/game/dcmusic.c`, see `patches/game/README.md`; game patch
ws22).

- **The data:** the GameCube banks were converted from the Dreamcast ones and keep their names:
  `sound/m0231.samp` + `.info` is `SOUND/M0231.MLT`; `tone`, `s1000000`, `b7000000` are banks 0, 1
  and 7 of `ARESNEW.MLT`. The game asks for music as Dreamcast (bank, data number) pairs and the bank
  slot is the Dreamcast bank number (M02xx bank 2, M03xx 3, M04xx 4, M5xxxxxx 5); sequence counts
  match for every M02/M03/M04 bank. GameCube-only music: `m0383`, `m0384` (and `s1000000` data
  34-38); those keep the GameCube music, as does any bank without an imported MLT (allow bit 0).
  Disc 1 has 116 `M*.MLT`, disc 2 adds `M0282`. The European disc 2's event banks (`M5*`) are slowed
  6/5 for 50 Hz (tempo 600 vs 500 ms per beat), so the import keeps an NTSC disc's copy over a PAL one.
  Research: the decomp's `docs/sound.md`.
- **The streamed songs (2026-10-18):** `m01`-`m04` aren't sequences but DSP ADPCM streams
  (`sound/m0n_L.dsp` / `_R.dsp`, 22050 Hz; decomp `docs/sound.md` *Streams*): `m01` the intro
  (`field/me299a.sct`, from the text screen on; the intro has no sequence at all), `m02` the title and
  the staff roll, `m03` a story scene (`me298a.sct`), `m04` the ending. The Dreamcast's
  (`SOUND/M0nS.P16`: 16-bit PCM, `.P04`: the AICA's 4-bit ADPCM, low nibble first; 44100 Hz, stereo
  in 0x2000-byte blocks, left and right in turn; no header; `M02S.P04` decodes to `M02S.P16`, envelope
  r = 0.999) compare to the GameCube's (lengths GameCube / Dreamcast 128.25 / 122.60, 93.42 / 93.44,
  121.12 / 124.46, 340.02 / 340.03 s):
  - `m02` and `m04` are **the same recordings** (sample-aligned, same level; converted, they
    correlate 0.999 with the GameCube files), so they stay the GameCube's and aren't imported.
  - `m03`: the same recording (correlation 0.98-0.999 in 20 s windows, 0.80 at 20-40 s) with
    another volume balance (the GameCube's quieter at 40-60 s, louder from 80 s), 3.3 s more at the
    end, mastered hotter.
  - `m01`: a different cut, 5.6 s shorter (about 0.9 s behind by 15 s, apart after 45 s), mastered
    about 2.5x hotter.
  Disc 1 has `M01S.P04` (and `M02S.P16`), disc 2 `M03S.P04` (and `M02S.P04`, `M04S.P16`); the
  European disc 2's are the USA's. (Until 2026-10-18 these notes said the GameCube `m04` was 234 s
  and that the recordings differed in length, the reason the streams stayed GameCube: wrong.)
  How `m01` and `m03` play: `DiscIO/SkiesStreams.cpp` (`recompcore-dcmusic-streams.patch`) converts
  each, once, into a file laid out exactly as the GameCube one (its header, size and coefficient
  pairs; the recording resampled to its rate, windowed sinc, and matched to its RMS over their common
  length: gain 0.23 / 0.25 for `m01`, 0.75 for `m03`; a longer one faded out over 1.5 s at the
  GameCube's end; each 14-sample frame encoded with the pair and scale that reproduce it best;
  against the Dreamcast recording the result is 31-37 dB down), cached as
  `DreamcastMusic/STREAMS/m0n_l-<key>.dsp` (the key hashes both GameCube headers and sizes, the
  source's size and a version), and `DirectoryBlob` reads it in place of the GameCube file while the
  track is enabled (`SetEnabled` from `SkiesMusic::Poll`: `m03` with the Dreamcast music, `m01` too
  with `[Skies] DreamcastIntro`, default on; the launcher's and Android's **Intro music** row under
  the import). The game streams it as before, so its starts, fades, stops and savestates are
  untouched; with the same coefficients a switch mid-stream changes the data from the next read (a
  click at worst; checked: Music off and on during the intro, the stream plays on, no burst in the
  DSP dump). Until a track's file is made (a background thread started by the first poll that
  enables it; both tracks in about 3.5 s on WSL), the GameCube one plays. The import copies
  `M01S` / `M03S` (16 MB limit); music imported before that says "import again for the streamed
  songs". Checked on WSL 2026-10-18 (Linux release build, USA disc 1 + European disc 2 CHDs): the
  intro served from the converted files 16 s after boot; with Intro music GameCube only `m03` is
  enabled and the intro reads the GameCube files. Windows (release build of the branch, CHDs imported
  over `\\wsl.localhost`): the same converted files byte for byte; the intro recorded with the A/V
  rig differs from a GameCube recording through the whole song and runs at 1.00x. `m03`'s scene
  itself not reached in game. Android (Retroid, branded debug test app `skies-recomp-dcs` around the
  ws25 module, a ws25 USA game and the music pushed by adb, the cache removed first): the Sound tab
  shows **Intro music** under the import ("both streamed songs"); the device converted both tracks in
  5.3 s, byte for byte the desktop files (MD5), before the intro reached its song; the intro played
  stream `m01` at 1.003x. The Android import itself (picker, CHD) not tried.
- **Driver protocol:** as dc-manatee's `host_protocol.md` (Manatee 1.1) with Bonito's work table at
  image +0xC0: port status `0x12600`, flag byte `0x12400`, bank table `0x14000`, host commands
  `0x12200`; SMSB version 2. Commands used: 0x01 play, 0x02 stop, 0x81 master volume (0xF0 = 15, the
  maximum), 0x86 MIDI message (the towns' channel volume), 0x80 stop all. A Bonito sequence doesn't
  end by itself: at its end the port status word at +0x18 becomes 1 and the port holds (still
  "playing"); the Dreamcast game advanced its lists on that. Here the GameCube sequences stay the
  timing master (their end starts the next pair), and the segment lengths match: `M0460` segments 9,
  0-3 are 3.733 / 19.870 / 13.253 / 13.264 / 13.259 s on the Dreamcast against 3.72 / 19.92 / 13.28 /
  13.18 / 13.28 s start to start in the game (frame resolution), so lists stay in step.
- **Reverb (2026-10-21, issue #8):** the driver plays through the AICA's DSP only once the game
  selects an effects program (`sdSndSetFxPrg(prg, out)`: host command 0x84 with the program in
  byte 2, then 0x82 with the output set; dc-manatee's `host_protocol.md` and `docs/notes/fx_format.md`).
  The port never sent it, so every voice's effect send went into a DSP with no program and silent
  outputs (EFSDL 0), and the music played dry, as the reporter heard against the DSF rips in
  Highly Theoretical. Skies has one program and one output set, both in `ARESNEW.MLT` (SFPB
  `ares2_1st.FPD`, ring buffer code 2 = 64 KB, SFPW 0x10040 bytes at 0x18000; SFOB version 2, EFREG
  0-7 at level 15 panned left / right in turn); no song MLT carries FX data, so each player sends
  `sdSndSetFxPrg(0, 0)` once after `ARESNEW.MLT` (`recompcore-dcmusic-fx.patch`). Our `sdSndStopAll`
  (bytes 6 and 7 zero) keeps it. Checked offline with a render tool around the same Highly
  Theoretical calls (`GameCube-Research/.../dcfx/render.cpp`): the program loads (86 COEF, 346 MPRO
  words), the outputs open (EFSDL 15), no driver error; the town theme `M0231` comes out 0.8 dB
  louder and wider (side signal +56%, left/right correlation 0.84 to 0.66). Cost: Highly
  Theoretical runs its DSP dynarec only on 32-bit x86, so 64-bit builds interpret it: a chip renders
  at 17x real time instead of 28x on the WSL PC, 9.9x instead of 15.5x on the Retroid Pocket 5; the
  game's speed on the Retroid was unchanged (Pirate Isle, 0.994 without, 0.999 / 0.995 with). The
  user compared both on Windows (Pirate Isle village) and preferred the reverb ("sounds right").
- **Engine:** `AudioCommon/DCMusic.cpp`. Two players, each its own emulated AICA with the driver
  (`DCMusic::PLAYERS`): a paused song (`MusicPause`: the field or world music during a battle) keeps
  its chip, frozen (not rendered at all), while the battle music and the fanfare play on the other,
  and resumes exactly on its continue. A player with nothing to play isn't rendered (2 s of release
  tail after a stop), so the second chip costs CPU only while both are heard. Each loads the driver
  image at sound RAM 0, downloads `ARESNEW.MLT` and each song's MLT as `sdMultiUnitDownload` does (bank
  table entry, then the data), and runs Highly Theoretical's `dcsound` (GPL-3, `lib/HighlyTheoretical`,
  pinned `2998a4b`) inside `Mixer::Mix`: real time whatever the emulation speed, silent while paused,
  not in savestates. Its own Hermite resampler (44.1 kHz to the output rate; Dolphin's `MixerFifo`
  would rescale the pitch with the emulation speed). Fades and the stop / freeze after them are applied
  per player to its output in AICA samples (`DCMusic::Fade`; the driver's own downward fades jump to
  the target). Level: the driver at full master volume is about 1.4x (3 dB) quieter than the
  GameCube's music (battle theme `m0459` from its start, RMS 2355 against 3412; peaks 19839), so it is
  mixed at 1.4x (`DEFAULT_GAIN` 358/256). The player's volume applies after `Mixer::Mix` (the output
  stream's), so it covers this too. About 28x real time per chip on the desktop.
- **Frontend:** `Core/SkiesMusic.cpp` (polled from `SkiesMenu::FilterPad`, and after a state load):
  starts / stops the engine with `[Skies] DreamcastMusic`, writes the patch's allow bits per bank slot
  from the imported files' SMSB counts, tracks the current and the paused song (sequence, player,
  the bank's default volume at slot +0x38 as the Dreamcast sequence's level 1), and turns the ring's
  events into requests to the player that holds the sequence. The magic goes into the patch's block
  whatever the option, so the patch follows the music while the GameCube's plays and switching the
  option on mid-song takes it over (`dcm_frame` mutes it and posts its start). A block without the
  magic (a savestate from before ws22, or the first poll) is initialised with no song (zeros would say
  sequence 0, a valid MusyX id). After a state load, or a song the patch didn't see start (an old
  savestate), `Restart` reads what plays from the game (`bgm_seq` and its music list; `SndInit`'s flag
  `0x803474E0` first, as before the sound layer starts everything is zeros) and the song starts again
  on the Dreamcast from its beginning; a paused one starts again at its continue. The towns' adaptive
  music sets controller 7 of channels 10-14 every frame (and 17, beside 14: no Dreamcast channel, left
  out): a per-channel table in the block (ws22), sent on change and every 30 polls, not events (6 a
  frame would overrun the 12-entry ring). The in-game menu's Dreamcast tab (Sound until 2026-10-20) has
  Music (GameCube / Dreamcast; without the import it says to import the disc in the launcher).
- **Import:** `AudioCommon/DCMusicImport.cpp` reads a GDI (track files beside it) or a CHD (libchdr,
  BSD-3, `lib/libchdr` pinned `607694c`; MAME stores tracks padded to 4 frames, so disc 1's track 3
  starts at CHD frame 45004), checks IP.BIN ("SEGA SEGAKATANA", title with "ARCADIA"; the disc number
  is at +0x2B in "GD-ROMn/m"), and copies `BONITO.DRV`, `ARESNEW.MLT` and `SOUND/M*.MLT` into
  `<user>/DreamcastMusic/` with `sources.txt` (file and region) and `discs.txt`. Desktop: the
  launcher's Settings, Dreamcast (Sound until 2026-10-20; Music row once imported, "Import a
  Dreamcast disc..." with a GDI / CHD picker, Remove) and `--import-dc-music IMAGE`; Android:
  Settings, Dreamcast (CHD only: a GDI's track files can't be opened beside a document the picker
  hands over), `SkiesNative.importDreamcastMusic`. Since 2026-10-20 the import also takes extra files
  (`DCMusicImport::ExtraFile`: disc, path, SHA-1, destination) for *Uncensored*, and Remove deletes
  those too.
- **Checked in game (WSL, ws22 USA module, `SKIES_DCMUSIC_DUMP` debug dumps on game time against
  Dolphin's DSP dump):** the music muted and played on the Dreamcast; battle start (fade to pause,
  battle music on the other player), victory (fade-stop, fanfare from `ARESNEW` bank 1, the world
  music continuing on its frozen player), game over (fade-stop, `M0367`), the camp menu (`SndMasterVol`
  to 37/100 and back), the option switched off and on mid-song both ways, old savestates (song taken
  over from the music list), patch-made savestates, a 5-pair list (`M0460`, poked) and the world map's
  channel volumes. Not listened to on WSL (no audio there); the GameCube mix has a slowly varying DC
  offset in silence, also without the option. The user's sign-off before it shipped in v0.10.0-beta:
  "We've verified everything pretty much works right" (2026-10-17). Import: both of the user's CHDs and the disc 1 GDI
  (116 + 1 banks).
- **Builds:** `AudioCommon/CMakeLists.txt` enables it when `lib/HighlyTheoretical/Core/dcsound.c`
  exists (`HAVE_DCMUSIC`, `SKIES_HT_DIR`) and CHD reading when `lib/libchdr` does
  (`HAVE_LIBCHDR`, `SKIES_LIBCHDR_DIR`); without them the port builds as before. HT's `yam.c` defines
  `__fastcall` as x86's `regparm(3)` on every non-Windows target, so arm64 builds pass
  `-Dregparm(x)=` (a compile option: CMake drops function-style macros from compile definitions).
  Licences in `release/THIRD-PARTY.md`; packages carry `licenses/libchdr-LICENSE.txt`.

### Uncensored (2026-10-20)

User request: a setting for the art the Western releases changed. Research (GameCube-Research,
`targets/SkiesOfArcadiaLegends`, *Uncensor*): only the Japanese Dreamcast release, Eternal Arcadia
(v1.009, Redump track 1 MD5s `2d53dfff…` / `b83bd0f8…`), has the originals. The Japanese GameCube
disc was censored too (CERO 12+, 2002) and its Pirate Isle and Sailors' Island art matches the USA's.
The Dreamcast and GameCube share the formats (MLD archives of Ninja chunk models, NJCM / NJTL / POF0 /
NMDM); the GameCube is big-endian, AKLZ-compressed and has GVR textures instead of PVR.

- **What converts** (`tools/uncensor/`, README there): for each Dreamcast file that differs between
  the Japanese and US discs, a model is replaced when the US Dreamcast block converts byte for byte
  into the GameCube one (so Legends didn't remake it) and it names a character texture (`tc…`) or
  uses a texture the US release added (the tavern sign's `tt04a500/501`); character textures are
  re-encoded in the GameCube original's format (CMPR, C4 / C8 with their palette, RGB5A3; header,
  mips and size kept); a Japanese-only texture is added (CMPR, GCIX index = the Dreamcast GBIX index)
  only when a swapped-in model uses it; US-added textures nothing uses any more are blanked. 70 files
  on each of GEAE8P, GEAP8P and GEAJ8P: 56 model swaps, 108 texture re-encodes, 3 additions, 2
  blanks (Dyne `tc0023`, Gilder `tc9015` + his cigar texture, Don `tc0713`, Bellena `tc0406`, the
  tavern houses, drunk faces / drinks `tc0100`, `tc0113`, `tc0206`, `tc0628-0630`, `tc9034` wine,
  crew `CR012` / `CR018` in battle).
- **Same size:** the result is recompressed to exactly the original file's byte count. The game's
  AKLZ reader (`fn_801C6F1C`, USA) streams the file in 128 KiB pieces through a 4 KiB ring from
  `0xFEE` and stops when the **input** runs out, and a savestate keeps the disc's file table in RAM:
  a file of another size, after loading a state, is read at its old place and the game overwrites
  its own code. The compressor parses optimally (literal 9 bits, reference 17, every candidate in
  the window; it beats Sega's by up to 4 %), then grows the stream to the exact size by turning a
  reference into a literal plus a shorter reference (+1 byte) or splitting one (+2).
- **Pitfalls found:** MLD entry lists also hold global texture IDs (`0x80000000 | n`): a splice must
  shift only values inside the file (else the village sky went black); battle character archives
  (`bchara`) don't align their first texture to 32 bytes; identical NPC entries share one GameCube
  block and the GameCube orders entries differently (pair by table ID and name, duplicates by
  content).
- **The manifest** `Data/Sys/Skies/uncensor-manifest.json` (`tools/uncensor/make_manifest.py`, 135 KB
  compact): per game ID and file, the Dreamcast source (disc, path, SHA-1), the edits in order (`model`
  with entry / slot and both blocks' SHA-1s, `tex`, `tex_add`, `tex_blank`), the GameCube file's SHA-1
  and the result's (`dec_sha1` of the decompressed result, `size`). No game data, and the US Dreamcast
  disc is only needed to make it: players import only Eternal Arcadia.
- **The converter** `DiscIO/SkiesUncensorConvert.{h,cpp}` (C++20, no Dolphin headers, no exceptions):
  applies one entry and checks every SHA-1; `tools/uncensor/convert_test.cpp` runs all 210 against the
  Python reference outputs (all byte-identical decompressed, exact size; 12 s on WSL, 0.3 s worst).
  CMPR endpoint selection is double-precision power iteration: `fp-contract` is off in that file so
  ARM builds give the same bytes.
- **Import and runtime** `DiscIO/SkiesUncensor.{h,cpp}`: the Dreamcast import (`DCMusicImport`, now
  also walking `FIELD/`, `BCHARA/`, `BATTLE/`) copies the manifest's files from a matching disc into
  `<user>/DreamcastUncensor/dc/disc1|disc2/`; `Prepare(game_root)` converts the game's files into
  `<user>/DreamcastUncensor/<disc ID>/<files path>` with a `.src` stamp (the source's size and time),
  which the launchers run after an import and the runner on a thread at boot when files are missing
  (a game extracted again). `DirectoryBlob` registers each file and asks `Redirect(path, offset)` on
  every read: the converted file while `[Skies] Uncensored` is on, decided when a file is read from
  offset 0 so a load in progress never mixes the two. Same size, so states made either way load.
- **Settings:** launcher Settings, **Dreamcast** tab (the Dreamcast disc import with Music and Intro
  music moved from Sound, then Uncensored: a dropdown once files are converted, else Off with what to
  import, and a line like "70 of 70 files converted"); the in-game menu's Dreamcast tab (Music,
  Uncensored, Off with a hint until `SkiesUncensor::Available()`); Android's Dreamcast tab
  (`SkiesNative.describeUncensor`, the import converting right away). `--import-dc-music IMAGE` also
  copies and converts.
- **Checked** (WSL, Linux release build of branch `uncensor`, Japanese game GEAJ8P, portable test
  folder): `--import-dc-music` of the two Eternal Arcadia GDIs copied 66 + 4 files and converted 70
  in about 5 s; the runner from a savestate made with the original files, warped to Pirate Isle `e`
  (`me002e.sct` into `0x80305770` (JP), field state 0xF): RAM holds the Japanese Dyne block with
  `Uncensored = 1` and the GameCube one with 0. Earlier the same files (Python outputs, identical when
  decompressed) were seen in game on Windows: Dyne smoking in his cabin, the village sky, eight towns
  warped into. Windows release build of the branch (2026-10-20, `C:\dev\SkiesRomhack`, portable test
  app `C:\dev\SkiesUncensor\app`): `--import-dc-music` of both GDIs converted 70 of 70 for each of
  GEAE8P, GEAP8P and GEAJ8P, all 210 byte-identical to the Python reference. Not yet: the GUI picker
  import, macOS / Android builds.
- **Gilder's victory pose (2026-10-20):** not an
  effect format after all. `beff/pcwin.mlk` (identical on all three GameCube discs) is an archive of
  (id, offset, size, type) whose entries are small whole MLDs; the type is only the padding Sega
  added (the loader `fn_8006E244` reads id, offset and size). The Japanese pose needs entries
  1800100 (`e1800100`: one quad and six ARGB4444 VQ smoke frames, `e0200502-507`; the US holds Willy,
  `mc9015`) and 6700526 (`ob9001`, the pistol: same model, a different motion), plus the end of
  Gilder's battle script `bchara/ma0050.std`. That script is a table of typed records (u16 type, 3,
  0, size, offset; each type a fixed size, `fn_8003617C`; records 8-aligned); a record's first u16 is
  its **action**, and actions 26, 27 and 28 are the three victory variants the game picks from. Only
  variant 26 differs: Willy in the US, the smoke in Japan (twelve spawns of effect 1800100, type 3
  records). The other two (pistol spin, hand to the mouth) are the same in both, which is what tests
  kept showing until a test build swapped the labels of 26 and 27; then the puffs drew. Converter:
  `tools/uncensor/victory.py` (first written in the research folder's `uncensor/victory/`): `mld_to_gc` converts a whole
  Dreamcast MLD (header, entries, lists, texlists, NJCM via `block2gc`, NMDM = all 32-bit words but
  the type/interpolation pair, PVR to GVR: ARGB4444 to RGB5A3, plain RGB565 stays RGB565, the rest
  CMPR) and matches the 37 unchanged entries byte for byte up to the texture pixels; `std_plan`
  keeps the GameCube's records up to 978 and converts the Japanese ones with a per-type word layout
  (16-bit pair, 32-bit word or kept bytes) learned from the US Dreamcast and GameCube files (968 US
  records reproduce exactly). Sega's tools: entry names are 16 bytes and the words after them get
  swapped anyway; a texture name's terminator can overwrite the `P` of the next `POF0`. In the
  manifest as three ops: `mlk_entry` (entry id, GCIX index), `std_records` (runs of GameCube or
  Japanese records, and the word layouts of the record types it converts: structure, not game data)
  and `pad`; the C++ port (`MldToGc`, `MlkReplace`, `StdRebuild`) matches the Python for all three
  discs (`convert_test`, now 216 files).
- **Decompressed size:** a converted file must not decompress smaller than the original. A
  `pcwin.mlk` of 651,480 bytes (original 761,016) made a later `malloc(0xA0)` for a file handle fail
  at battle load (`fn_801C63EC`, its assert, then writes through NULL); padded with zeros to the
  original size the same data loads.
- **Testing battles by poking the party** (Japanese game: party list `0x8030B59C`, count
  `0x8030B228`, records `0x8030B274` + 0x5C each with current HP at +0x18, join flags
  `0x8031063C` = `0x638C` for Vyse, Aika and Gilder): Gilder alone freezes at victory variant 26
  even with the original files (the same failed allocation), so test with three members (Vyse and
  Aika at 0 HP still act). States in the WSL test folder `~/projects/skies-uncensor-e2e/states`
  (`jp-09` world map with Gilder, `jp-12` the battle), scripts in its `tools/`. Vyse and Aika only
  stand at 0 HP because of the **Invincible** cheat; with it off (`config:Dolphin.Skies.Invincible=0`)
  they don't count for the victory, which is then Gilder's. **A state taken in a battle already holds
  the battle's files:** from `jp-12` (`victory12.sh`) the smoke played four times of four, but the
  log (below) showed nothing loaded during those runs: it was the converted data the state had been
  made with. The real test starts on the world map: `victory11.sh GAME` (from `jp-11`, walk into a
  battle at 4x encounters, Attack until it ends) loads `bchara/ma0050.std` at the battle's start and
  `beff/pcwin.mlk` at the victory, both served converted.
- **The log line:** `SkiesUncensor::Redirect` writes "Uncensored: PATH from CONVERTED" (Disc IO,
  notice level) each time a file is served converted from its start; with `Logger.ini` `[Logs] DIO =
  True`, `[Options] WriteToFile = True` it lands in `Logs/dolphin.log` (desktop and Android). A
  runner saves its config on exit, so edit `Logger.ini` only with the game stopped.
- **Checked in a normal build (2026-10-20, WSL, Linux release build):** the import of disc 1 copied
  `BEFF/PCWIN.MLK` and `BCHARA/MA0050.STD` and converted both (byte-identical to the reference,
  `pcwin.mlk` padded to 761,016 decompressed); the original Japanese game with `Uncensored = 1`
  played Gilder's smoke pose (seen by the user).
- **Vigoro in Aika's cell (2026-10-21):** only `A116AG02` (the Grand Fortress prison scene, event
  `e6g002` of `me116a.sct`, Gilder in the party) differs: the West gave Vigoro a model of his own,
  `tc9014` (a black sleeveless top), where the Japanese scene uses his regular outfit `tc9003` (the
  blue armour). The Japanese model has 79 bones against 50 and Legends remade the GameCube one
  (0x8e80 against 0x6600), so entry 2 is swapped whole (`uncensor.py` `WHOLE_CHARACTERS`): the model, its five motions (op
  `motion`: the NMDM / POF0 chain padded to 32, converted like the victory pose's; the US Dreamcast
  ones convert exactly into the GameCube's) and its textures, which take the Western model's nine
  texture slots (op `tex_swap`: renamed, Japanese pixels as new CMPR with mips). Adding the textures
  and blanking the old ones came out 5,699 bytes too big (zero runs cost 2 bytes per 18 in AKLZ);
  the slots fit the original's 687,375 bytes exactly. Converted on WSL, Windows (all three games)
  and the C++ test (219 of 219).
- **Vigoro checked in game (2026-10-21, WSL, the Japanese game):** no save reaches the scene, so the
  test pokes its start condition. `me116a.sct` (decoded with SALSA, SoAModTools' script tool) calls
  `me116ag02` from `loop` when `IntVar 15` is 2300 (a switch on it; the other cases are the other
  ways in) and `BitVar 301` is clear. `IntVar` / `BitVar` are at USA `0x8030E3E4` / `0x80310B3C`,
  Japanese `0x8030DE64` / `0x803105BC` (European, unchecked: `+0x3680`, like `BitVar` in the address
  map); bit N is byte N / 8, mask `1 << (N % 8)` (SALSA's reading). Poking both and warping into `me116a` plays the scene: the log shows
  `field/a116ag02.mld` served converted, and Vigoro wears his blue armour (with Uncensored off, the
  black top); the whole scene plays to the end with no errors. The test folder's import had to be
  redone first: it predated the Vigoro conversion. Then on **Windows** (the test app, all three
  games: the European addresses above confirmed) and on **Android** (Retroid Pocket 5, test app:
  disc 1 imported again through the picker, 73 files, `a116ag02.mld` identical to the reference; the
  scene from `jp-02` the same way, full speed) and on **macOS** (arm64, the Mac session: disc 1
  imported again, converted in 0.5 s, the Japanese game from a card save's state): the same result
  on each.
- **Before the release (2026-10-21):** the Windows launcher's own import (the user, through the file
  picker, both GDIs, into an empty data folder): 73 files for the selected USA game, identical to
  the reference, the copied disc files and music identical to the command-line import; the USA game
  served the 6 converted area files on the warp tour, and the Japanese game converted its 73 by
  itself at start (identical) and played Vigoro's scene. Android's update path: a test build from
  before Uncensored, then the current one over it: `Sys` extracted again with the manifest, "73 of 73
  files converted" (debug-signed test app; the release-signed update over v0.60.0 needs a CI build).
- **Test APKs packed an old `Sys`:** `jni/CMakeLists.txt` copies `Data/Sys` into the app's assets
  only when CMake configures, which a reused build folder skips, so the first Vigoro test APK had the
  previous manifest, and a fresh local folder got no `Sys` at all on its first build;
  `tools/android-ws-build.sh` now copies `Data/Sys` itself before Gradle (CI's APKs were fine).
- **Checked 2026-10-21:** Windows test app (all three games imported, 216 then 219 files identical to
  the reference; each game warped through Pirate Isle `e`, the tavern, Maramba and three more areas:
  the log shows the 6 converted area files served, no errors); macOS arm64 (the Mac session: build,
  both Eternal Arcadia GDIs imported, 72 converted per game, no mismatch); Android, Retroid Pocket 5
  test app (CHDs made with `chdman createcd`, picker import 68 + 4 files in about 18 s, 72 byte-exact
  pulled back, the Japanese game's warp tour serves the same 6 files at 59.9 fps, a battle loaded
  from disc serves both victory files).
- **Android Sys:** the app extracted `Sys` again only when Dolphin's git revision changed, which our
  releases never do (patches on one revision), so an updated install would have kept a `Sys` without
  `Skies/uncensor-manifest.json` and imported no Uncensored files. `DirectoryInitialization` now adds
  the package's `lastUpdateTime`. (A first APK from a fresh worktree also lacked `assets/Sys`
  entirely: CMake copies `Data/Sys` into the assets during the build, after Gradle packed them; a
  second build has them. CI's APKs have them.)
- **Not covered:** the drunk sailor model Legends remade (`tc0102` in `A004AB05`, 0xc300
  against 0x8520), and everything that needs a script: the drunk sailor's entry (`0xc21` in `A004A`),
  the Hamachou party (disc 2), Ramirez's blood (`ME131I.SCT`), the "loqua" text.

### HD textures (2026-10-17)

Players import an HD texture pack they downloaded; the port never ships one (the pack tested, "Skies
of Arcadia HD v0.06" by Bighead and frozenwings, has no licence to redistribute). Status, test setup and
history: [handoffs/HANDOFF-hdtextures.md](handoffs/HANDOFF-hdtextures.md); the patch:
`recompcore-hd-textures.patch` (+ the launcher in `moderngekko-local.patch`).

- **Loading** is Dolphin's own custom texture loader (`GFX.ini [Settings] HiresTextures`, textures in
  the user directory's `Load/Textures/GEA/`; `GEA` covers both discs). Speed with the pack on: Windows
  3x 1.000, Retroid first battle 1.003 (PNG pack).
- **Import** (`VideoCommon/TexturePackImport.cpp`, shared by the launcher, `--import-texture-pack`
  and the Android app): a `.7z`, a `.zip` or a folder; only `tex1_*.png/.dds` inside the pack's
  `GEA` / `GEAE8P` / `GEAP8P` folder are copied (the pack's `Optional/` extras, French text and the
  other flight meter, are left out), or every `tex1_` file when the pack has no such folder (the "HD
  Texture Pack 4x Upscaled DDS v1" by Shoopkid, six archives with `UI/`, `Characters/`... at the top;
  `__MACOSX/` resource forks skipped), CRC-checked, into `Load/Textures/GEA.importing/`, then moved
  into `GEA/`: an import **adds** to the packs imported before (since 2026-10-18; it used to replace
  them), a texture of the same file name (Dolphin's key) is the new pack's; duplicate names inside a
  pack dropped. `skies-texture-pack.txt` there records the packs' names (one `name=` line each) and
  the counts of everything in `GEA/`, recounted after an import that adds. Remove deletes `GEA/`.
  A pack with no textures of its own but `.7z` / `.zip` files in it (a pack's archives put into one
  folder or archive by the player, 2026-10-19) imports each of them in name order as if picked one
  after another (`recompcore-hd-textures-nested.patch`): unpacked into `GEA.unpacking/` first, one at
  a time, and deleted after; one level only; archives without textures skipped. The Android picker
  takes several files at once (imported in a row, "Archive N of M"). The six 4x archives zipped
  into one 7.3 GB `.zip`: 274 s on Linux, the same 15497 textures.
  The six 4x archives one after another on Linux (2026-10-18, `--import-texture-pack`): 15497
  textures, all BC7, 8.7 GB; 2-5 s each for 01-05, 263 s for `06_SoAL_HD_Unsorted_Assets` (6.5 GB,
  one 7.5 GB solid LZMA2 block, 64 MB dictionary), under 100 MB of memory. Only 23 names are in both
  06 and the others. Its `02_SoAL_HD_Characters.7z` is a .zip (the import goes by the first bytes).
  `.7z`: the LZMA SDK 23.01 (`lib/lzma-sdk`); LZMA / LZMA2 / stored folders are decoded as a stream
  with a window no larger than the folder (the packs are one solid 256 MB-dictionary block: 732 MB out,
  a few hundred MB of memory); other methods through the SDK in memory (refused above 1 GiB). Times:
  Linux DDS .7z 4.2 s, PNG .7z 3.8 s, PNG .zip 0.7 s; Windows DDS .7z 6.4 s; Retroid DDS .7z 6 s
  (441 MB peak), PNG .7z 5 s.
- **Switching**: launcher Settings, Graphics, HD textures (a new import turns it on); in game the
  menu's Graphics row, which the texture cache picks up at its next frame.
- **mGBA's LZMA SDK (Android).** The Android app builds Dolphin's mGBA (desktop: `USE_MGBA OFF` in
  ModernGekko), which links an older copy of the SDK with the same function names; the import called
  mGBA's `SzArEx_Open` with our struct layout, read a garbage file count and built a 6 GB vector
  (the app killed for memory, nothing written). Our copy is compiled with
  `VideoCommon/Skies7zPrefix.h` forced in (`Skies7z_` names, the 95 the library defines on x86-64 and
  arm64); a new SDK file needs its names added there. Found with simpleperf page-fault samples
  (`simpleperf record --app ... -e page-faults -g`, no root for a debuggable app).
- **BC7 on Adreno.** The stock Qualcomm driver (Adreno 650, V@0746.0) reports `textureCompressionBC`
  but samples BC7 as coloured noise; `DriverDetails::BUG_BROKEN_BPTC` (Vulkan, Qualcomm, Android)
  turns `bSupportsBPTCTextures` off, so Dolphin skips BC7 DDS files and the game's own textures stay.
  The pack's DDS download is 535 BC7 + 210 uncompressed; its PNG download has everything. The Android
  settings warn when an imported pack has BC7 textures. Mesa Turnip (v26 R8) draws BC7 correctly
  (2026-10-19): Dolphin sees it as `VENDOR_MESA` / `DRIVER_FREEDRENO`, so the bug isn't set; BC1 /
  BC3 DDS work on the stock driver too.
- **EFB copies**: the pack's old note that a UI texture needs EFB copies to texture only doesn't
  apply: this game needs copies to RAM (Dolphin's `GEA.ini`), so that element shows what the game draws.
- **Flight meter**: the pack's `GEA` has the 4:3 meter, which is right in every picture: the port's
  widescreen keeps the meter's shape. The pack's `Optional` widescreen meter (squeezed art for cheat
  codes that stretch the HUD) would come out too narrow. (A widescreen stretch reported 2026-10-17
  turned out to be a Picture change made while the world map's menu was open, *Known issues*.)

### Test tools and savestates

| Tool | What it does |
|---|---|
| `tools/drive.py` | automation: pad (`tap`, `hold`, `press` / `release`), `shot`, `save` / `load`, `peek` / `poke`, `config`, `speed`; over adb with `ADB_AUTO` (peek included) |
| `tools/dump-ab-win.sh` | frame-exact A/B of two builds from a state (`run`, `cmp`) |
| `tools/button-probe-win.sh` | what each input does in a state (screenshots per input; sticks held) |
| `tools/camera-test-win.sh` | camera tilt / zoom / orbit / Z tap sequence with screenshots and `cam_input` |
| `tools/qol-test-win.sh` | `rewards`, `encounters`, `speed`, `invincible` checks with the options set |
| `tools/capture-window-win.ps1` | screenshot of one window (DPI-aware `PrintWindow`, no other windows); used for the launcher. Posted key messages don't reach SDL's keyboard state, so to see the Settings window a temporary build opened it on start |
| `tools/save-import-test.py` | Settings, Saves' Import through the launcher's `--import-save` (formats, refusals, backups), 16 cases |
| `tools/states-test-win.sh` | menu save / load and auto-save (battle skip, rotation, speed, load confirmation), ~7 min |
| `tools/shipbattle-state-win.sh` | makes the ship battle states with the debug DOL (`shipdebug.json`): gives the Little Jack, equips cannons, changes the map to Baltor's battle (European addresses: *The European release*) |
| `tools/statecheck.py` | whether a savestate is caught in game-patch code (*Known issues*) |
| `tools/dolmatch.py` | matches the USA DOL to the European one: functions, data, the patch and frontend addresses |
| `tools/bench-android.sh` | the standard Android speed measurement (fresh boot per state, `speed:30`, screenshot) |
| `tools/build-release-win.bat` | branded release (see above) |

Savestates (gitignored `build/savestates/`, also in `C:\dev\SkiesRecomp\build\savestates`): `01`-`07`
(opening cutscene, first battle, Valuan ship field in first person, later, save point, save menu, red
alarm room), `03b-valuan-ship-thirdperson`, `12-streaks`, `13-pre-battle(-early)`, `14-cliff-cracks`,
`15-ladder`, `16-deck-battle-ws10` (a random battle on the world map), and the world map
(`android-worldmap-2026-10-08` on WSL, `10-worldmap-android` on Windows and, since 2026-10-11, on WSL
too). States made with older DOLs
load in newer builds (`recompcore-restore-boot-dol.patch` puts this build's code back).

## Platforms

Same generated C, the same patches and the same `GEAE8P.ini` (idle skip) everywhere, no mods. Update
this table with every change.

Speeds are with the frame limiter off (`EmulationSpeed = 0`), so they show headroom: anything at or
above 1.00× plays at a locked 60 VI/s with the limiter on. **Skies renders 30 fps on real hardware**
(a new frame every second VI), so 1.00× is a solid 30 fps, which the user confirmed by playing on
Windows (2026-10-06). More headroom doesn't raise the frame rate; 60 fps comes from frame interpolation
(see *60 fps* below, shipped on every platform since 2026-10-08), which these numbers don't include. "worst s" is
the slowest single second of the 30 s window. Desktop: 2026-10-06, see *Desktop headroom*; the bold desktop
rows were re-measured late 2026-10-06 with the gather-pipe patches.

| Platform | Module | 01 cutscene | 03 field | Worst second, states 01–04 | Picture |
|---|---|---|---|---|---|
| Linux/WSL, 1× (llvmpipe) | **LLVM + state-in-memory + PGO + `mtmsr` fix + gather-pipe stores** | **1.22×** | **1.75×** (scene varies, see below) | **1.13×** | correct |
| Linux/WSL, 1× (llvmpipe) | LLVM + state-in-memory + PGO, before the `mtmsr`/gather-pipe patches (stale) | 1.23× | 1.46× | 1.15× | correct |
| Linux/WSL, 1× | LLVM + state-in-memory | 1.08× | 1.29× | — | correct |
| Linux/WSL, 1× | C backend, clang 20 `-O2` ThinLTO | 0.89× | 1.00× | — | correct |
| Linux/WSL, 1× | C backend, gcc 15 `-O2` | 0.85× | 0.94× | — | correct |
| Windows, 1× and 3× | **LLVM + state-in-memory + PGO (WSL profile) + `mtmsr` fix + gather-pipe stores + Win64 sjlj fix (cross-generated)** | **2.08× (1×), 2.07× (3×)** | **2.53× (1×), 2.54× (3×)** | **2.04× (1× and 3×)** | correct; all 16 runs exited cleanly (2026-10-07) |
| Windows, 1× and 3× | **C backend, clang 20.1.8 + PGO + gather-pipe stores (GXRuntime inline `mem_write*`)** | **1.35× (1×), 1.39× (3×)**; 1.51× / 1.49× re-measured 2026-10-07 | **1.56× (1×), 1.51× (3×)**; 1.66× / 1.65× | **1.27× (1×), 1.29× (3×)**; 1.48× / 1.48× | correct; all 16 runs exited cleanly |
| Windows, 1× and 3× | C backend, clang 20.1.8 + PGO, before the gather-pipe patches (stale) | 1.33× | 1.44× | 1.15× (1×), 1.20× (3×) | correct; user approved 3× |
| Windows, 1× | C backend, clang 20.1.8 | 1.13× | 1.26× | — | correct |
| Windows, 1× and 3× | LLVM + state-in-memory (cross-generated), before the Win64 sjlj fix (stale) | 1.58× | 1.87× | 1.38× | correct; crashed on every exit in states 01/02 (fixed, see *Known issues*) |
| Windows | C backend, MSVC | 0.505× (limiter on, don't use) | — | — | correct |
| Android (Retroid Pocket 5), 1× | **LLVM + state-in-memory + gather-pipe stores + device PGO + `cortex-a77`** | **1.05×** | **1.20×** | **1.020** (04; ≥1.03 in 01, ≥1.52 in 02, ≥1.17 in 03): **stable 30 fps in all four** | correct, text readable; played by hand to the world map at 29–30 fps (2026-10-07) |
| Android (Retroid Pocket 5), 1× | LLVM + state-in-memory + device PGO + `mtmsr` fix, before the gather-pipe work | 0.90× | 1.07× | 0.888 (01), 0.897 (04) | correct |
| Android (Retroid Pocket 5), 1× | C backend, NDK 29 clang | 0.53× (limiter on) | 0.53× | — | correct |
| macOS (Apple M4 Mac mini), 1× | LLVM + state-in-memory + gather-pipe stores, no PGO, `apple-m1`, Metal (2026-10-19; with the Sys and SDL fixes, see *macOS*) | **2.90×** | **3.90×** | **2.85×** (01); 2.81× at 4× + 2× MSAA, 2.63× at 4× + 60 fps | correct; v0.50 macOS build, fullscreen |

All desktop rows run the speed-patched runtime (`recompcore-staticrecomp-speed.patch`). Every platform must
boot the extracted disc folder to use the shared savestates (see *Android*). The bold rows (WSL LLVM, Windows
clang, Android LLVM) have every patch in `patches/`; the others were measured before the `mtmsr` and
gather-pipe patches. `dolrecomp-mtmsr-entry.patch` only affects LLVM-backend modules.

**Only the bold desktop modules match the current runtimes.** The gather-pipe patches grow `CPUState`
(3536 → 3560 bytes), and `moderngekko-run` refuses a module whose `cpu_state_size` differs ("native module
was rejected: CPU state size mismatch", then exit 1). Every other desktop module, including the old
launcher defaults (`build/modules`, `build\modules-clang`) and the old Windows LLVM module (`build\modules-llvm-sim`),
needs re-linking (LLVM: regenerating) before it runs. The desktop runtime checks against ModernGekko's own copy of the struct,
`include/moderngekko/cpu_state.h`, which `moderngekko-local.patch` now updates too (it was missed at first).

### Desktop headroom (2026-10-06)

Protocol: `tools/bench-wsl.sh` / `tools/bench-win.sh` with `USER_DIR=build/user-bench` (1×,
limiter off; `build\user-bench3` on Windows is the same at 3×): load the state, 16 s, then 30
one-second samples. One discarded warmup per module, then three interleaved rounds of states 01 and 03
(medians above, spread within ±0.03 except WSL LLVM 03 at 1.28–1.45), then two rounds of all four
states for the best modules (worst-second column). WSL and Windows never ran at the same time.

- **Full speed with no dips on both desktops.** Mean / worst second per state:

  | Module | 01 | 02 battle | 03 | 04 |
  |---|---|---|---|---|
  | WSL LLVM + PGO, 1× | 1.19 / 1.15 | 2.11 / 2.04 | 1.43 / 1.40 | 1.18 / 1.15 |
  | Windows clang + PGO, 1× | 1.31 / 1.15 | 1.58 / 1.40 | 1.42 / 1.28 | 1.32 / 1.25 |
  | Windows clang + PGO, 3× | 1.30 / 1.20 | 1.59 / 1.50 | 1.40 / 1.29 | 1.30 / 1.21 |
  | Windows LLVM, 1× | 1.54 / 1.38 | 2.60 / 2.49 | 1.87 / 1.83 | 1.57 / 1.44 |
  | Windows LLVM, 3× | 1.53 / 1.38 | 2.64 / 2.30 | 1.86 / 1.67 | 1.56 / 1.40 |

  Android's 1–2 s dips mostly didn't show on desktop, though they had the same cause in LLVM modules
  (fixed 2026-10-06, see *Android → Dips*; an old-module WSL battle run did show one 0.65 second).
  Resolution doesn't matter on Windows (RTX 5080).
- **Speed patch on desktop:** +4% / +6% on WSL LLVM (01 / 03), +4% / +2% on Windows clang. Smaller
  than on Android (+4–14%); plausibly the ICache search it removes costs less on desktop caches.
- **PGO:** +14% on WSL LLVM, +18% / +14% on the Windows clang C module. Trained on the four
  benchmark states (same overlap caveat as Android).
- **LLVM vs C on Windows:** +39% / +49% over clang C, the largest lever. Its exit crash is fixed and the
  LLVM + PGO module is measured (2026-10-07, below).
- WSL is limited by llvmpipe as well as the game thread; compare WSL modules with each other, not
  with Windows.

**With the gather-pipe patches (2026-10-06, late).** Rebuilt and retrained with every patch:
`build/modules-llvm-pgo-gp` (WSL, also has the `mtmsr` fix) and `build\modules-clang-pgo\use-build` (Windows;
the previous one is `build\modules-clang-pgo-pre-gp`, which the current runtime rejects). Same protocol, two
rounds of all four states, WSL and Windows interleaved but never concurrent. Raw: `build/bench-states-gp.txt`.
The old runtimes were overwritten, so this compares with the table above across sessions, not in one A/B.

| Module | 01 | 02 battle | 03 | 04 |
|---|---|---|---|---|
| WSL LLVM + PGO, 1× | 1.22 / 1.16 | 2.10 / 1.87 | 1.75 / 1.61 | 1.25 / 1.13 |
| Windows clang + PGO, 1× | 1.35 / 1.27 | 1.64 / 1.54 | 1.56 / 1.52 | 1.46 / 1.39 |
| Windows clang + PGO, 3× | 1.39 / 1.29 | 1.76 / 1.62 | 1.51 / 1.40 | 1.44 / 1.37 |

- **Windows clang: +3–11% at 1×, +6–10% at 3×**, worst seconds up by 0.11–0.24. This is the first run of the
  C backend's gather fast path (GXRuntime's inline `mem_write*`): pictures correct in all four states and
  every run exited with code 0.
- **WSL: 01 +2%, 04 +5%, 02 flat** (llvmpipe-bound). WSL round 1 ran slower than round 2 in 03/04
  (04: 1.19 vs 1.31), and its worst second (1.13, 04 round 1) is slightly below the old 1.15. That is
  still well above 1.00.
- **State 03 isn't a fixed scene:** the camera ends somewhere different from run to run (ceiling, wall or
  Vyse in the screenshots, old and new alike), so its numbers spread more and the +10–22% there is the
  least reliable figure.
- Training ran with the two platforms overlapping, which only affects how many frames each window
  covers. WSL profile: 11,015 functions, no `hash mismatch`; Windows: 0 out-of-date/mismatch warnings.
  The Windows instrumented run exited cleanly this time, so `pgo-run` built its own PGO module too
  (under `build\modules-clang-pgo\GEAE8P\`), alongside `pgo-win.sh`'s `use-build`.

**Windows LLVM + PGO (2026-10-07).** After the exit-crash fixes (*Known issues*): `build\modules-llvm-pgo-fix`,
cross-generated on WSL with every patch and the WSL profile (`tools/module-llvm-win.sh llvm-pgo-fix --profile-use
build/modules-llvm-pgo-gp/GEAE8P/pgo/merged.profdata`, no `hash mismatch`; its DLL differs from the non-PGO
`build\modules-llvm-fix`, so the profile applied). Interleaved with clang + PGO in the same session, two rounds of
all four states at 1× and 3×. Raw: `build/bench-states-fix.txt`. Mean / worst second:

| Module | 01 | 02 battle | 03 | 04 |
|---|---|---|---|---|
| Windows LLVM + PGO, 1× | 2.08 / 2.04 | 3.46 / 3.30 | 2.53 / 2.46 | 2.17 / 2.14 |
| Windows LLVM + PGO, 3× | 2.07 / 2.04 | 3.41 / 3.01 | 2.54 / 2.44 | 2.17 / 2.13 |
| Windows clang + PGO, 1× | 1.51 / 1.49 | 1.84 / 1.69 | 1.66 / 1.63 | 1.53 / 1.51 |
| Windows clang + PGO, 3× | 1.49 / 1.48 | 1.87 / 1.82 | 1.65 / 1.62 | 1.53 / 1.50 |

- **LLVM + PGO is +37–42% over clang + PGO in 01/04, +52–54% in 03 and +82–88% in the battle.** Worst
  second 2.04× (clang 1.48×). Pictures correct (battle HUD text included); all 32 runs exited with code 0.
- clang + PGO measured 10–12% higher than on 2026-10-06 with the same module and an equivalent runtime (only
  `PlatformWin32.cpp` changed). Compare within a session, not across.
- One clang 3× 01 sample read `speed=` as 0.000 (a status file caught mid-write); left out of the worst second.

### Widescreen (2026-10-07)

A game patch (`patches/game/widescreen.c`, built into the DOL by `tools/gamepatch.py`; see
[patches/game/README.md](../patches/game/README.md) and the decomp's `docs/widescreen.md`) widens the 3D
view for a 16:9 picture (Hor+, culling follows), keeps the HUD at its original shape in the centre,
and keeps full-screen 2D (fades, screen copies, backgrounds and overlays that reach both edges) at
full width. Because it's game code in the DOL, the module needs rebuilding from the patched DOL, and
it runs the same on Android. Dolphin has to output an exact 16:9 picture: in the user directory's
`Config/GFX.ini`, `AspectRatio = 5` (custom stretch), `CustomAspectRatioWidth = 16`,
`CustomAspectRatioHeight = 9` (on Windows: `build\user-ws`). Run it with
`GAME=build\game\GEAE8P-ws` and `MODULE=build\modules-llvm-pgo-ws\gGEAE8P_recomp.dll`.

**Android (2026-10-07):** installed on the Retroid: `build/android/apk/skies-ws-2026-10-07.apk`
(module from the patched DOL, PGO + cortex-a77, with `recompcore-restore-boot-dol.patch`), the patched
`main.dol` in `files/Games/SkiesOfArcadiaLegends/sys/` and `AspectRatio = 5` / 16:9 in `Config/GFX.ini`.
The originals are in `files/Backup-2026-10-07/` (`main.dol.orig`, `GFX.ini.orig`); to go back, restore
them and install `skies-dolphin-debug-gp-pgo-a77.apk`. Battle and cutscene correct in 16:9; cutscene
speed 1.009 mean, 0.986 slowest second, limiter on (the margin it had before).
Reinstalled late on 2026-10-07 with the scissor fix and the full-width script fade (DOL SHA-1
`85176f05…`). The clip build it replaced is kept: `main.dol.clip-52e799c7` in the same backup folder and
`build/android/apk/skies-ws-2026-10-07-clip.apk`. Checked on the device: the red alert room (07) full
width, the save menu (06) and the camp menu's party status clipped with nothing at the sides, the forced
script fade over the whole picture, battle labels at their characters; opening cutscene 1.008 mean,
0.986 slowest second over 30 s, limiter on (unchanged).

Checked on Windows (LLVM + PGO, 3x): opening cutscene (the afterimage effect no longer leaves ghost
bands), first battle (HUD, target selection, attack round), ship field, save menu (a centred 4:3 panel
with black sides), cold boot to the text crawl, and battle name labels, HP bars and damage numbers
at their characters (they're placed with `njProjectScreen`; the battle UI's calls get HUD-space x,
found with the 2D tracer `patches/game/trace2d.c`). Squeezed 2D is clipped to the 4:3 area, because menus park
UI just off-screen (party status's next page, the save/continue cursor arm): checked in both menus.
The clip is a scissor that every projection load sets (see below for why every load).
Savestates made before the patch load fine
(`recompcore-restore-boot-dol.patch`).

**The 4:3 clip and display lists (found and fixed 2026-10-07).** The first version of the clip
(f82e437) cut the 3D scene to 4:3 in the Valuan ship's red alert room
(`build/savestates/07-valuan-red-alarm.sav`): the room's solid geometry ended at the 4:3 edges, while
railings and light beams still reached the sides. Reproduced on Windows with the module built from that
DOL, frame interpolation on or off. That version remembered whether the clip was on and only wrote the
scissor when that changed. The game records its scene into display lists and replays them later
(decomp `docs/frame_loop.md`), so the order of the CPU's projection calls isn't the GPU's: a skipped
write could leave the clip from the last frame's HUD in force for the layers replayed first. Now
`ws_set_projection` writes the matching scissor on every projection load: the 4:3 clip for the squeezed
2D projection, the game's own scissor for anything else. The scissor then follows the projection in
GPU order, recorded or not. All five of Ninja's projection loads go through it (nothing else sets
the projection after `GXInit`). Checked on Windows (LLVM + PGO, 3x): states 07 and 05 full width,
the save menu (06) and the camp menu's party status clipped with nothing parked at the sides, the
opening cutscene, and the first battle at speed 1.000 / 59.94 fps with interpolation.

A WSL run of the f82e437 DOL with the older WSL module (built from 92d1c29's DOL) rendered the room
correctly. That run didn't test anything: the module's compiled callers still called `GXSetProjection`
directly, so the clip code most likely never ran (`scissor_clipped` read 0 there, 1 on Windows). Compare
builds only with a module made from the same DOL.

**Script fade:** `ScreenFadeDraw` (0x80210390, the script's 640 x 480 colour fade) draws through Ninja's
own primitive (`fn_8028A0F4`), which `ws_poly` doesn't see, so fades left the side bars unfaded. It's now
wrapped at full width like the screen effects (`ws_full_80210390` in `fullwidth.S`). Checked on Windows
by switching the fade on through memory (`poke:0x8030DAFC:00000001A0000000`: enable flag, then ARGB at
`lbl_8030CEA0 + 0xC5C / 0xC60`): the previous build darkened only the 4:3 area, the new one the whole
picture.

**Battle-entry transitions fixed (2026-10-09, DOL `a4f9ef7c`, `GEAE8P-ws8`):** both are 3D, not the 2D
tiles the earlier notes chased. Found with `trace2d` (the recompiled core is needed: only it restores the
booted DOL's code after a state load, so under the JIT a savestate brings the old code back and the trace
stays empty): the only projection chain new during the transition was `fn_8029C8A0` (perspective load)
<- `fn_80287C14` <- `fn_80288DE4` <- `fn_80287B38`. The tiles are sized to the game's own field of
view, so the widened one squeezed them into the middle 4:3. `ws_unwide_begin/end` (widescreen.c) and the
`UNWIDE` wrappers (fullwidth.S) draw them at the unwidened aspect (`njClipZ` rebuilds the matrix; Ninja
reloads it for every 3D draw): the spinning card (`fn_802297C4`, two `fn_80287B38` sites) and the
falling triangles (`fn_80101828` -> `fn_80097CA4`, only caller). Checked on Windows in both variants
(dumps): full 16:9, the gauge and compass break up where they were. `WS_VERSION` 3. The code cave is
nearly full (0x10E0 of 0x1100 bytes; room made 2026-10-10, see *Widescreen HUD at the edges*). Savestates `build/savestates/13-pre-battle.sav` / `-early.sav`
(an encounter follows within a few seconds of flying forward; which variant is random).
Checked on the Retroid too (`skies-ws8-interp-2026-10-09.apk`, 2026-10-09; `adb shell screenrecord`, decoded
with `-fps_mode passthrough`): both variants full 16:9, every frame continuous, 59.6-59.9 fps. The thin
vertical seams while the picture breaks up belong to the effect (the Windows dumps have them as well).

**Open:**
- (Fixed above.) The battle-entry shatter effect (the screen breaks into tiles that fall away) covered only the 4:3
  area, with black sides, the full 16:9 picture squeezed into it. Seen on the Retroid (state 07 runs
  into a random battle about 2 s after loading there, `build/shots-android/ws-07c.png`) and on Windows
  (walk in state 07; `C:\dev\SkiesRecomp\build\shatter\s01.sav` is just before an encounter, made with
  the f82e437-era build: see *savestates* below). Not the cause: the battle transition task
  `fn_800B0FD0` draws through `fn_800B0278` (kind 0, tiles turned with `njSin` / `njCos`) and
  `fn_800AF780` / `fn_800AF8F8` (kind 1); wrapping the first two full width (in the work-in-progress
  patch) left the shatter unchanged. The tiles fly off in perspective, so it may be a 3D effect sized for
  the original field of view; next step is the projection trace (`trace2d`) over the transition.
- Not checked in the world map, towns or ship battles.
- Three `njProjectScreen` callers are unclassified (decomp `docs/widescreen.md`).

### Widescreen HUD at the edges (2026-10-10, DOL `329c631f`, `GEAE8P-ws9`)

(The world map's flight meter and compass keep their shape too; a "stretch" reported 2026-10-17 was a
Picture change made while the world map's menu was open, *Known issues*. Since 2026-10-18 the edges can
be those of a narrower centred area, *HUD placement*.)

The HUD pieces that sit at the sides of the 4:3 picture now sit at the sides of the 16:9 one, through the
anchor table (`WS_HUD_ANCHORS` in `widescreen.c`, `WS_VERSION` 5). Found with a deeper `trace2d` (ten return
addresses; the four-deep chains all ended in the shared sprite code `fn_80005D98` <- `fn_801D5DD8`), each
checked by anchoring it alone at runtime:

| Scene | Piece | Code (task or call sites) | Move |
|---|---|---|---|
| Battle | party status panels | `fn_80022F74` | left edge |
| Battle | command wheel | `fn_80025624` | left edge |
| Battle | the wheel's lists (S-move, Magic, Item) | `fn_800240EC` | left edge, with the wheel |
| Battle | SP gauge | `fn_80027C04` | 16 px up |
| Battle | status icon columns | `BattleIconColumnDraw`, `fn_80159610` (ship battles) | right edge |
| World map | altitude gauge | `fn_8016B434` | left edge (ws13; ws9-ws12 had side -94, as far in as the compass, until the user saw that 4:3 hugs the edge) |
| World map | compass | `fn_8016C204` | right edge |
| Dungeons | minimap | `fn_800E23B0`'s calls at `0x800E26F8`-`0x800E27C0` | right edge |
| Field | party status | `FieldPartyStatusDraw` | right edge |

Text queued by anchored code moves with its clip rectangle (`TextState` +0x18): the SP gauge's "/ 8" has a
tight one and vanished when only the text moved. The anchor code moved to a second code area over MetroTRK's
message handlers (`.trkcave`, patches/game/README.md), freeing the low-memory gap (now 0xDB4 of 0x1100). Checked
on Windows (interpolation on, dual core): battle idle, S-move list, attack round (target labels, banner),
world map, dungeon minimap, a ws8 state (13-pre-battle) loaded and flown into a battle. On the Retroid (first
ws9 build, gauge at -100): same scenes, 1.00x / 59.8-59.9 fps. Not seen: the field party status and ship
battles (no states). The game dims the whole picture after about a minute without input (both test scenes);
that's the game's own. Tuning without a rebuild: the table is at `0x80002A18`, 12 bytes per entry
(since ws15: `lo`, `size` u16, `side` s8, `kind` u8, `dx` s16, `dy` s16; before: `lo`, `hi`, `side`, pad,
`dy`); `poke:` then `shot:` (reload the state first: the HUD slides).

### Ship battle HUD (2026-10-11 night, DOL `2d7b1f1d`, `GEAE8P-ws15`; spacing ws16)

ws16 (user's spacing): the ring's lists (`fn_8012E640`) dy +50, the crew / ship panels (`fn_801331AC`)
dy -24, midway between the Spirit gauge and the ring.

Found with `trace2d` at command input and in the action phase (states 17 / 17b), each piece moved alone
with a live anchor poke. Layout asked for by the user: the left column at the left edge, the command grid
in the top right corner, the enemy's HP plate below it.

| Piece | Code | Move |
|---|---|---|
| Spirit gauge, its "/17" and the bar | `fn_8012B054` (the max is queued with `TextPrintf`, now wrapped like `TextPrint`) | left edge |
| Command ring and its lists | `fn_8012C744`, `fn_8012E640` | left edge |
| Crew member and ship panels | `fn_801331AC` | left edge |
| Turn bar and command grid | `fn_801303D0`, `fn_80131734` | right edge + 175, 34 up (top right) |
| HP plates and their icons | `fn_8015887C` (plate 0, the party's ship, at x 42; enemies' at x 502, y 74 + 40 each) | by plate: the party's to the left edge, enemies' to the right edge and 88 down |
| Damage popups by the plates | `fn_8021F330`, `fn_8021F0EC` (shared code: only in ship battles) | by x: with the plate they belong to |
| Action name banner | `fn_8012F958` | stays (centred) |

Plates and popups are drawn by shared code, so the anchor can't tell them apart by caller: two stubs
(`ws_ship_plate_draw` / `_icon` at `0x80158994` / `0x8015899C`, `ws_popup_draw` at the popups' sprite
calls) note the object and jump on, and anchor kinds `WS_SHIP_PLATE` / `WS_SHIP_POPUP` pick the side
from it. The anchor entry grew a `dx` (HUD pixels beyond the edge move) for the grid. Checked on Windows:
command input, Focus and attack rounds both ways (plates and their damage numbers together), the battle
won to the results; 4:3 still identical to the unpatched game (state 02, 165 pictures). The results
screen is a centred 4:3 page like the save menu (shared with ground battles), unchanged.

### Ship battle tutorial grid (2026-10-21 night, ws30, issue #11)

Sandbowl (v0.80.0-beta): in the first ship battle the command grid is "stretched in different aspect
ratios", only in the tutorial. The Baltor state (`17-ship-battle-baltor.sav`, one A press) shows it:
while Drachma explains the grid ("At the top of the screen, a 3X3 grid will appear", three more lines),
the grid covers the same share of the screen at 16:9 as at 4:3 (x 385-830 of 1280 in both), 1.33
times too wide; at command input it's the real, anchored grid (top right) and fine. During the
explanation the grid isn't the HUD's grid: `trace2d` (chains only in the explanation, against command
input) gives `fn_800A9D24+0x1274 -> fn_800A1BA4 -> fn_801CCD90 -> fn_8000610C -> fn_80291064`, a
sprite object drawn as one textured quad. `fn_800A1BA4` has three callers, all in `fn_800A9D24`;
the one at `0x800AAF94` runs only on maps 500-599 (ship battles). The tutorial's picture is a quad
across the whole screen, so `ws_tex_poly` took it for a background and drew it full width.

ws30 sends that one call through `ws_squeezed_800A1BA4` (`fullwidth.S`, macro `SQUEEZED`, the reverse
of `FULLWIDTH`): `ws_config.full_m00` holds the squeezed scale for the call, so the picture keeps the
HUD's 4:3 shape, centred where 4:3 has it. 4:3 goes straight to the game's function. The other two
calls (outside ship battles) are unchanged. Code only, in the MetroTRK area: no variable moved
(`ws_config`, `qol_normal_speed`, `cs_shot` at their ws29 addresses in all three DOLs), and the
European and Japanese address tables got `fn_800A9D24`, `fn_800A1BA4` and the call site
(`tools/dolmatch.py`, only additions).

Checked on WSL (static module of the USA ws30 DOL, `build/ws30`): a state made on ws29 at the
explanation loads; the grid at 16:9 has the real grid's proportions (cells square, portraits
round), centred; the three following lines the same; command input after it with the anchored grid
top right as before. A poke to `full_m00` as a test does nothing (`ws_apply` writes it every frame),
and the JIT can't test a new patch from an older state (the state brings its own code back; only
the static module replaces it).

### Battle screen plane: Drachma's victory pose (2026-10-22, ws31, issue #9)

Sandbowl (v0.80.0-beta, with a video): in 16:9, Drachma's victory pose "reverts back to 4:3". The
video at 60 fps: on about half the frames of the pose the picture is the whole 16:9 scene squeezed to
0.75 of its width (landmarks 0.75 times as far apart as on the neighbouring full frames), in a box that
slides left and right with the camera shake, black around it. Vyse's and Aika's poses are fine.

Reproduced on WSL with the ws30 static module. No save had Drachma, so `shipdebug.c` gained a party
request (`'PTY' << 8 | 3` to `0x8031AA20`: the game's own join, `fn_800FF6B4`, stats from his table);
from `10-worldmap-android.sav` with Drachma joined, Encounters at 400 and flying forward, a deck battle
starts; the victory pose is the game's random pick, steered by poking its rand seed (`lbl_803469A8`,
`fn_8025ECC4`) before the last command. The projection trace has no chain only in Drachma's pose, and
logging every 2D polygon that misses an edge showed none during it either. What it does have: the pose
turns on the battle's screen copy (`lbl_8030A10C + 0x6C` bit `0x40000000`, set at `0x80056CA4` and
`0x8005B7B0`); `fn_8002FF70` draws the task list in two passes with the copy in between
(`fn_80226D14`: `GXCopyTex` with clear), and the only task drawn after it (a hook on the copy that
listed the tasks) was `fn_8003484C`. That task draws the copy back on a model in front of the camera
(unit matrix, `njTranslate` to the screen distance, rotate, scale: the shake), through `fn_801CEF30`,
`fn_801CF020` or `fn_801CF0F8` by how the model is animated (the trace: `fn_8003484C+0x158` ->
`fn_801CF0F8` -> `fn_80287B38`). The model is sized to the game's own field of view, so with the
widened one it covered the middle 4:3, the copy squeezed into it, and the clear left black around it.
Its 2D branch (no model, `fn_800333A0`) wasn't seen.

ws31 sends those three calls (`0x8003497C`, `0x80034994`, `0x800349A0`) through `UNWIDE` wrappers
(`fullwidth.S`), as the battle-entry shatters: the plane is drawn at the game's field of view and covers
the whole picture. The wrappers are in a new input section, `.trkcave_late`, which `cave.ld` places
after all other MetroTRK-area code, so no older symbol moved (the script VM's handler table holds
pointers to the cutscene stubs, and a state brings the old table back). Checked on WSL: the same state
and seeds on the ws30 and ws31 static modules, 9 shots each during the deck part of the pose: ws30 boxed
on 4, ws31 on none; the rest of the pose and the results screen as before. Retroid Pocket Nova (local
test APK `skies-recomp-ws31-2026-10-22-test.apk`, PGO from the main checkout's profile): the launcher
re-patched the imported USA disc to ws31 on its own (`7ef9451c`); the same battle state, three seeds,
9 shots across Drachma's pose: all full width. Retroid Pocket 5 (the same APK over the Aurora session's
test build; its USA disc re-patched to ws31 too): three seeds, 12 shots, all full width.

### Match screen: ultrawide and any other shape (2026-10-17 night)

The game patch takes any target aspect in `ws_config.display_aspect` (Hor+ 3D, HUD anchors as a
fraction of the width gained, full-width 2D and screen copies), so ultrawide is a frontend setting:
`[Skies] Widescreen = 2` (`recompcore-match-screen.patch`). `Presenter::OnBackbufferSet` reports the
backbuffer size to `SkiesSettings::OnScreenSize`; with Match screen a new shape queues `ApplyNow`
(always a host job: single core presents on the CPU thread), which writes the window's aspect to the
game and Dolphin's custom ratio as the window's own width and height in the **current** config layer
(this run only; `GFX.ini` keeps 16:9). Clamped to 4:3..32:9: a narrower window letterboxes at 4:3, a
wider one pillarboxes at 32:9. No window yet: 16:9. Launcher, in-game menu (Left / Right steps 16:9,
Match screen, 4:3) and Android Settings offer it.

Checked 2026-10-17 night (USA ws22, no ultrawide monitor here):
- WSL, `ws_config.display_aspect` poked to 16:9 / 21:9 (64:27) / 32:9 in ten states (field, the
  opening cutscene with dialogue, both battles, Baltor ship battle with commands, save menu, red
  alarm, world map, deck battle): 3D widens, every HUD anchor reaches the edges, menus and dialogue
  stay centred, backgrounds full width.
- WSL, Match screen in a 2560x720 (32:9) window: the battle-entry shatter, spell effects (dark and
  bright full-screen overlays), the red alarm pulse, a cold boot (logos, intro text, title, the attract
  flight) all full width. Live switching 2 -> 1 -> 0 -> 2 (`config:Dolphin.Skies.Widescreen`) and a
  state load keep the right aspect.
- Windows (release build, Vulkan, 3x): the window resized to 1920x1080, 2560x1080, 2580x1080,
  3440x1440, 1920x1200, 3200x900 and 1440x1080 (client sizes): the log's `[skies] picture WxH` follows
  each, and the picture fills the window.
- The world map's flight meter and compass keep their shape at every width (the "stretch" first
  reported was a Picture change made in the world map's menu, *Known issues*).
- Edge scenery at 32:9 only: the red alarm room shows a white wedge of level geometry at the far left,
  the world map's cloud wall edge shows, and the attract cutscene's palace has a black void past the
  right of the 21:9 area (at 3440x1440 it starts at the edge). 21:9 was clean elsewhere.
- Sharpness: the 640-wide EFB is stretched across the width; 3440 pixels wide needs about 5x for full
  horizontal detail (3x is soft).
- Android (Retroid, debug test app `skies-ws23ms-2026-10-17.apk`, ws23 modules): Match screen at the
  native 1920x1080 gives 1.778; `adb shell wm size 1080x2400` (a 20:9 phone) gives 2.222 live, the
  picture fills 2400x1080 at full speed (100%, 59.8 fps), minimap at the edge; `wm size reset` back to
  1.778. The in-game menu's Picture row shows "Match screen" and steps to the others. (Automation pad:
  `press:z=1 wait:0.6 press:z=1,start=1` opens the menu; a `tap:start` after `press:z` doesn't.) The
  branded app's Settings row (Kotlin) is compiled in but not looked at.

### Texture filtering, anisotropic filtering, scaling (2026-10-21, branch `gfx`)

Requested on Reddit (r/recomps, v0.60.0-beta thread): texture filtering options, and resolutions
"lower" than 640x528 for an original-resolution look on 16:9 (640x360 or 768x432). The Resolution
setting is Dolphin's integer EFB scale and 1x (640x528) is the GameCube's own picture, whatever the
aspect, so there is nothing lower; what blurs it on a 4K screen is the bilinear scaling to the
window. Three Graphics rows now expose Dolphin's own settings (launcher: `moderngekko-local.patch`;
in-game menu and Android: `recompcore-gfx-options.patch`), stored in `GFX.ini [Enhancements]`:

| Row | Choices | Dolphin |
|---|---|---|
| Texture filtering | Original / Smooth / Sharp | `ForceTextureFiltering` Default / Linear / Nearest (0 / 2 / 1) |
| Anisotropic filtering | Off / 2x / 4x / 8x / 16x | `MaxAnisotropy` Default / Force2x-16x (-1 / 1-4) |
| Scaling | Default / Sharp / Smooth / Bicubic | `OutputResampling` Default / SharpBilinear / AreaSampling / CatmullRom (0 / 5 / 6 / 4) |

All three apply live (Dolphin reads them each frame). Checked on Windows (a test copy at 1x in an
853x480 window, set through the automation's `config:` and screenshotted): Sharp scaling visibly
crisper than Default, Sharp texture filtering pixel-crisp textures. The in-game menu's Graphics tab
has 11 rows now (the menu shrinks to fit its tallest tab), so its three descriptions are one line.

### HUD placement: edges, 21:9, 16:9 or the original layout (2026-10-17 night, branch `hud`)

User request: on an ultrawide the HUD at the screen's edges is far apart, so the player picks where it
goes. `[Skies] Hud` (launcher, in-game menu Graphics, Android Settings): **Screen edges** (default, as
before), **21:9 area**, **16:9 area**, **Original 4:3**. The game patch reads `ws_hud_aspect`, a float
at a fixed address (`0x80002E9C`, `.ws_hud` in `cave.ld`, just below `qol_input`; free in every earlier
build, whose data ends at `0x80002BD8`, so the frontend writes it whatever the DOL): 0 = the anchors
reach the picture's edges, an aspect = they reach the edges of a centred area that wide (never wider
than the picture, so 21:9 on a 16:9 picture is the edges), < 0 = no anchor moves at all (the game's own
HUD in the middle 4:3, clipped there, as before ws9; the SP gauge keeps its own height too). In
`ws_anchor_find` the x move is computed from the HUD area's scale instead of the picture's; the
pieces keep their shape either way (the 2D squeeze is still the picture's). Not in `ws_config`:
growing it would move every later patch variable, which states from earlier builds still have at the
old addresses; `WS_VERSION` stays 10. `recompcore-hud.patch`; the launcher row is in
`moderngekko-local.patch`.

Checked on WSL 2026-10-17 night (test DOL `build/game/GEAE8P-hud`, SHA-1 `0569daa3`, module
`build/us-genhud/module-linux`, worktree `~/projects/skies-aspect`), Match screen in a 2560x720 (32:9)
window, each choice switched live (`config:Dolphin.Skies.Hud=N`): first battle (party panel, command
wheel, SP gauge), the Baltor ship battle with commands (left column, command grid top right, enemy
plate below it: the layout scales to the area, `dx` +175 included) and the world map (altitude gauge,
compass). The 4:3 choice matches the Original 4:3 picture's layout (ship battle compared). The in-game
menu's HUD row steps Screen edges -> 21:9 -> 16:9 -> 4:3, applies on close (log `[skies] HUD 21:9`)
and saves `Hud`. Android: the Settings row compiles (`compileDebugKotlin`), not run on a device;
launcher row built, not looked at. Not checked: Windows, the field party status and dungeon minimap
(right-edge anchors, same code path), the European DOL (same sources, no new game address).

### European intro in sync with its music (2026-10-21, branch `cutscene`, issue #3)

A comment on issue #3 (GLEclipseGaming): with the European disc the intro's picture and music drift
apart. The European game has no 50 Hz compensation in code (at 50 Hz it runs 5/6 speed; the port runs
it at 60 Hz, `eu-boot.md` in the research checkout), but three of its scripts were retimed for 50 Hz:
the scenes set to a streamed song, `me299a` (the intro, `m01`) and `me298a` / `me250a` (`m03`). Their
waits and motion times are `x / 1.2` (expression words `0x04000000 x 0x04000000 0x3F99999A 0x13`,
271-306 instructions each, 1016 in all), where the USA and Japanese scripts have `x`; no other script
of any release has the expression (all 258 scripts of each disc, decompressed, checked). The song files
are byte-identical on every disc, so at 60 Hz the European picture ran 1.2 times as fast as the music.
Besides those, the European scripts differ from the USA ones only in expression lengths and an extra
`Switch` block in the intro.

`patches/game/pal60.c`: the script loader `fn_801F7158` (one caller) copies the decompressed code into
its buffer with one `memcpy` (`0x801F72A8`, `redirect_sites`); `pal_script_copy` makes each 1.2 there
1.0, so the script keeps its size and `x / 1.0` is the USA's `x`. A save state made before the fix
keeps the old script until the next load. Checked on WSL (JIT, cold boot, muted, the game frame of
each camera shot of the intro, `cs_shot`): shots common to both releases come at 0.84 of the USA frame
unpatched (1 / 1.2; the whole intro 3178 frames against 3753) and at the USA frame patched (within
0.6 %, the polling step; 3775). The `m03` scenes use the same mechanism and were not played.
Again with ws29 (after ws28): WSL JIT 0.993-1.005 of the USA ws29 frames; Windows, the European
static module (LLVM, no PGO) with the MSVC runner, within about 1 % (the intro ends at 3763, USA 3752).

### European 50 Hz audio (2026-10-21, branch `pal60`, issue #3)

The rest of the European release's 50 Hz changes are in its sound, found by comparing the
European and USA `sound/` folders (the Dreamcast PAL release had the same: Riggles' 2025 60 Hz
patch re-timed its cutscenes and restored the USA sound effects). The USA and Japanese discs need
none of this.

- **Streams.** 223 of the 310 stream pairs are time-stretched (pitch kept) to 1.2 times the USA
  length: the spells' and S-moves' `KO00NN` (voice and effects pre-mixed; the action waits for
  the stream's end, *Spells and special attacks* in the decomp's `qol_cheats.md`), battle voices
  `B702NNNN`, field voices `f71000NN` and three victory lines. Fitting each European loudness
  envelope to the USA one (time scale and offset) gave 1.20 for most; 13 were re-edited (S-moves
  `KO0036`-`KO0056`, some `f71000NN`, `B7023000`, `B7024700`), longer than 1.2 times, Cutlass Fury
  (`KO0036`) with a ~2.5 s fade-out tail; the rest (25 `B702`, 54 victory lines, 2 field voices,
  the songs `m01`-`m04`) are the USA files. At 60 Hz the European Cutlass Fury stream ran 978 game frames
  (16.3 s; the USA one is 11.6 s).
- **Sound banks.** In all 279 enemy banks (`e6*.info`) and `m5100000.info`, every sequence's
  tempo is 5/6 of the USA one (the MusyX song header's initial tempo at `+0x10`, the tempo table
  at the offset in `+0xC`, `{tick, tempo}` pairs whose `0xFFFFFFFF` terminator holds a tempo too,
  and a few tempo fields elsewhere): 120 → 100, 250 → 208, 60 → 50. The `.samp` data, the field,
  voice (`stv`), crew (`cr`) and common banks and the music banks (4216 tempos) are unchanged.
  Two banks have 4 other changed bytes (`e6k006`, `e6p001`, not tempos), left as they are.
- **The game stops a stream at its header's nibble count**, not at the file's end: `PcmPrepare`
  (`fn_802159E0`) sets the descriptor's end (`+0xC`) to `(nibbles >> 1) & ~0x1F` and the refill
  (`fn_80216744`) zero-fills past it. So a shortened stream can keep its file's size.

`recompcore-pal60.patch` (`DiscIO/SkiesStreams.{h,cpp}`, `DiscIO/SkiesPal60Data.inc`): the
extracted disc registers the files `SkiesPal60Data.inc` names, and the game's first read of a sound
file starts a background pass that makes 60 Hz copies in `Cache/SkiesPal60/`, served in place of
the originals (the banks first, then the `KO` streams, then the rest):

- a stream pair is checked by its header sample counts, decoded, shortened by its factor (WSOLA:
  30 ms Hann frames at half overlap, each placed where it best continues the last within ±7.5 ms,
  picked on the sum of both channels), cut with a 0.25 s fade at the USA length for the re-edits
  (their factor matches the end of their sound to the USA's: Cutlass Fury 1.286, 11.57 s against
  11.58 s), and re-encoded with its own coefficient pairs into a copy of the original file, its
  header's sample and nibble counts (and loop end) shortened;
- a bank is checked by its size and SHA-1, and each listed tempo field (a big-endian `u32`) is
  multiplied by 1.2: 2586 of the 2772 come out as the USA value, the other 186 one off (Europe's
  rounding).

The table holds names, sample counts, factors, SHA-1 prefixes and field positions: no game data.
Same sizes, so the disc layout and save states are unchanged. A stream or bank already loaded
when its copy becomes ready keeps the European timing until the game reads it again (first run
only). The re-encoding is lossless for unchanged audio (120-131 dB against the original decode)
and ~30 dB for the shortened audio, the format's usual level. Offline check: each shortened stream
fits the USA envelope at scale 0.99-1.0 (r up to 0.999).

Checked on Windows by ear (the user, 2026-10-21; a test copy of the European game with the
streams and banks replaced by the prototype's output, `C:\dev\SkiesLang`): "converted is much
better than original eu", and with the banks too "definitely works better"; the app's own output
(this patch, WSL) in the same copy: "that was perfect". In game on WSL (JIT): Cutlass Fury's stream
818-832 frames with the prototype's files, 709-821 with the app's, against 933-1150 original.

Platforms (2026-10-21, a fresh cache each, the European disc booted without a state):

| Platform | Made | Result |
|---|---|---|
| Linux (WSL, dev runtime `build-egl`) | 18 s, speed 1.00 | the reference |
| Windows (MSVC, `tools\build-win.bat` in a copy of the branch's sources) | 53 s | banks and all 446 stream files byte-identical to Linux |
| macOS (arm64, the Mac session; patch applied last on its ws28 tree) | < 10 s, speed 1.00 | banks identical; stream names and header counts identical |
| Android (Retroid Pocket 5, test app built from this branch with the ws28 modules) | 21 s, speed 1.00 | banks identical; 434 streams identical, 12 differ after rounding (same length, loudness envelope r = 1.0000) |

On Android the copies go to the app's external cache (`Android/data/<app>/cache/SkiesPal60`).

### Cutscene shots framed for widescreen (2026-10-21, branch `cutscene`, issue #10)

Issue #10 (Renatodep): in the opening "you see the Valuan ship on the side". The cutscenes are
scripted field scenes (`field/meNNNx.sct`) with set pieces built only as wide as the 4:3 camera sees;
Hor+ widening shows what was staged out of frame or never built. Found in the opening (map 201,
`me201a.sct`, 16:9 vs 4:3 shot by shot up to the first battle, 34 shots): Fina's ship by the moon with
the Valuan ship waiting just past the right edge, and the look up at the Valuan bridge whose side decks
end in mid-air short of the edges. Other edge differences there (the pirate flag's end, the pirate
ship's bow) are real objects.

`cutscene.c` / `cutscene.S` (game patch; decomp `docs/widescreen.md` *Cutscene shots*): the script VM's
ten camera commands go through stubs that note the shot, map + FNV-1a of the command's words (the same
in the USA, European and Japanese scripts; all 53 opening keys match); a shot listed in
`cs_shot_aspect` shows no wider than its aspect: the screen distance times picture aspect / listed
aspect, through both ways the camera sets the screen (`njSetScreen`, and `njSetPerspective` from the
lens, `fn_8028BAEC`); `CS_BOTTOM` / `CS_TOP` move the screen's centre so the bottom (top) edge stays
where 4:3 has it. 4:3 never zooms. Listed: `0x90763369` (the bridge, 1.36, bottom), `0xE6946063`
(Fina's ship, 1.38). To list another: play to it, `peek` `cs_shot` (key, map, script offset, opcode),
poke `test_key` / `test_map` / `test_aspect` to try an aspect live, or hash the command's words from a
SPICE dump (`SpiceSCT`, built on Linux with a Windows.h stub, `rawWords` = the command's words).

The MetroTRK code area grows to `0x80257AF8` (`cave.ld`): ws28 + this needed 0x277C of the old 0x278C;
ws29 (with `pal60.c`) uses 0x2810 of 0x3210. The
added functions (`fn_80257074`-`fn_80257A20`) are reached only by `InitMetroTRK` (checked from every
non-MetroTRK function and data pointer; dolmatch: same range and offsets in Europe and Japan).

Checked on WSL (USA, test DOL `build/game/GEAE8P-cs1`): JIT runs from a cold boot and the static recomp
module (`build/us-gencs/module-linux`, PGO profile of `modules-llvm-pgo-gp`) at speed 1.0: both shots
zoom (1.307 / 1.288 at 16:9), the bridge keeps its decks along the bottom, the waiting ship is gone,
the next shots (Alfonso's bridge) are unzoomed from their first frame, 4:3 keeps the game's distance,
and a state from ws24 picks the stubs up (the handler table is rewritten every frame). Not checked
yet: Windows, Android, the European and Japanese games in play (they build), the rest of the game's
cutscenes.

**ws32 (2026-10-22, two more shots from #10's comments).** In the opening, Fina's ship heading for the
horizon (`0xF62BE063`, 1.55): the Valuan ship's side waits at the right edge for about 8 s before it
sweeps in. On the Moon Stone night (map 202, `me202a.sct`), after "Dinner is ready!", Aika and Vyse walk
to the lookout's ladder (`0xA5940371`, 4:3 + `CS_TOP`, so Fina's head stays in frame when she walks in):
the game removes each of them just past the 4:3 right edge, so 16:9 showed them vanish in view. Both
keys, like the first two, are once each in the USA, European and Japanese scripts (FNV-1a over every
word run of the decompressed `me201a` / `me202a`).

A longer `cs_shot_aspect` would move the stubs, and the script VM's handler table in game data points at
them: a state from ws29-ws31 brings that table back, and `cs_frame` rewrites it only at the end of the
frame. So `cs_shot_aspect` is in `.trkcave_late` now, the stubs in `.trkcave_stubs`, pinned by `cave.ld`
where ws29-ws31 had them (0x26F4 into the MetroTRK area, the same offset on every disc), with the code
before them free to shrink (only `cs_note` and `cs_frame` moved, 0x5C down). Shot lists can now grow
without moving anything older.

Checked on WSL (USA, static ws32 module `build/ws32` in the `pal60` worktree, JIT for the first look):
the horizon shot zooms 1.147 at 16:9 with no hull at the edge until the ship arrives; the lookout shot
1.334, Aika and Vyse go out past the right edge, the shots after it unzoomed; a ws31 state at the start
of the opening plays all three opening shots at their zooms (1.147, 1.307, 1.288); ws10-era states (world
map, a deck battle, the Antonio fight) load and run at speed 1.00. States for the lookout:
`build/savestates/20-pirate-isle-fina-meets-mom-ws32.sav` (card save #3 of the Retroid backup, in the
house), `21-pirate-isle-before-lookout-ws32.sav`, `22-lookout-scene-ws32.sav` (the scene's first frame).
Retroid Pocket 5 (local test APK `skies-recomp-ws32-2026-10-22-test.apk` over the Aurora session's
test build; the launcher re-patched the USA disc to ws32 on its own): the lookout shot zooms 1.334 and its
end matches WSL's frame; from the ws31 opening state the horizon shot zooms 1.147, the moon at the right
edge and no hull 40 and 117 frames in (captures timed by the game's frame count, not adb polling, which
lags by seconds). Not run: Windows, Linux, macOS (game code, the same on every platform).

### Widescreen 4:3 setting: identical to the unpatched game (2026-10-10 night, DOL `7cb9de68`, `GEAE8P-ws10`)

The runtime's `[Skies] Widescreen = 0` writes `ws_config.display_aspect = 0` (handoffs/HANDOFF-settings.md). The
first frame-by-frame A/B against the unpatched DOL (Windows, `tools/dump-ab-win.sh`, state 02 first
battle, 1x single core, every real picture dumped; two unpatched runs match byte for byte) found two
things in the ws9 DOL's 4:3 mode:
- **The SP gauge sat 8 pixels high.** Its anchor's `dy` (-16) applied at factor 1 too, where `dx`
  becomes 0. Fixed: `ws_anchor_find` finds nothing in 4:3 (no move, and no unclipped anchored text).
- **Faint differences on most edges of every frame** (at most 25 of 255, ~400 pixels per frame at a 5%
  tolerance; invisible). Bisected with four DOL variants, each without one group of redirects: only
  dropping the text hooks removed them. They were no-ops in 4:3, so the cause was earlier: the DOL's
  patch data started with the 16:9 `factor`, and the hooks read `factor`, which only `ws_apply` at the
  end of a frame recomputes from `display_aspect`. So the first frame after a load (the restore puts
  the DOL's patch data back) ran the widescreen HUD path, and in this battle that left lasting
  differences. A variant whose defaults were 4:3 matched exactly. Fixed: the defaults start at
  `factor` 1 ("nothing widened yet"; `display_aspect` stays 16:9, so `ws_apply` widens at the end of
  the first frame), `WS_VERSION` 6.
- Also: in 4:3 every wrapper in `fullwidth.S` jumps straight to the game's function without a frame
  of its own (`ORIGINAL_IN_43`; the C hooks became `*_wide` behind `GATE`s), except
  `ws_set_projection`, which in 4:3 puts the game's scissor back after every projection load. That's
  what undoes the 16:9 clip after a switch: bypassed too, the HUD stayed cut to the 16:9 HUD area.
  The runtime now applies the setting at once after a state load instead of from a host job.

Result (ws10 DOL + its LLVM+PGO module): 4:3 vs the unpatched game identical on all 165 pictures, and 16:9
identical to the ws9 play build on all 198 pictures (state 02). Live switching at 3x dual core (Windows)
and on WSL: 16:9 HUD at the edges, 4:3 the original layout, nothing clipped. In 4:3 the battle-entry
shatter (state 13, flown into an encounter) covers the whole picture. Not checked frame by frame: a
live 16:9 -> 4:3 switch mid-scene (the first-frame finding says a switch can leave invisible
differences until the scene is rebuilt) and the battle-entry path against the unpatched game (no
unpatched pre-battle state; 07 was saved by a widescreen build, and the unpatched game loads its
widened camera). The low code area now holds `0xFB0` of `0x1100` bytes.
Traps: Windows paths ignore case (dump dirs `ws10B` and `ws10b` are the same directory); a 1% CPU
overclock changes nothing here, so timing can't explain picture differences in this scene.

### Widescreen: work in progress (late 2026-10-07, committed; superseded by the section above)

The source in `patches/game/` is ahead of every built DOL (`build/game/GEAE8P-ws`, the Windows module
`build\modules-llvm-pgo-ws`, the Retroid). Built and booted only as the debug variant
(`build\game\GEAE8P-ws6t` + `build\modules-llvm-pgo-ws6t`, with `trace2d.json`): cold boot, intro, title.
Not yet checked in menus, battle or field.

- **Savestates and moved patch code.** (2026-10-11: also the retrace polling loop, see *Known issues*;
  check a state with `tools/statecheck.py`.) States are taken while the game waits for the retrace inside
  `fn_801DC420`, which the old `ws_frame_end` called, so a state held a return address into patch code.
  When a later build moved that code, loading the state returned into the middle of another function
  (Windows: "Invalid write to 0x0000a855, PC = 0x80001e54", or a hang). `ws_frame_end` is now assembly
  that calls `ws_apply` and *jumps* to `fn_801DC420`, so it returns straight to `main`; states made with
  this build onwards survive patch changes. **States made with any earlier widescreen build (including
  the user's) crash in it**; memory-card saves are unaffected.
- **Patch data in old states.** A loaded state also brings back the old build's patch data section (only
  code sections are restored, `recompcore-restore-boot-dol.patch`). Constants now live in `.text`
  (`cave.ld`), and `ws_check` resets the mutable data when `ws_config.version` isn't this build's
  (`WS_VERSION`, bump it with any layout or default change).
- **Patch-code pointers in game data (ws19, 2026-10-12).** The same goes for game data the patch
  rewrites: `redirect_pointers` puts `qol_damage` into the battle action handler table
  (`lbl_802DBB10[0]`, `0x802DBB10`), and a state brings back the address from the build that made it.
  ws17 / ws18 moved `qol_damage`, so with the user's ws15 / ws16 states (and every auto-save made from
  them) each attack jumped into `qol_ship_results` ("Invalid write to 0x0000001e, PC = 0x80255594" on
  Windows, a freeze on the Retroid). `qol_frame` writes the entry every frame now. Audited: that entry is
  the only patch-code address in game data, the patch's own data holds none, and nothing registers a
  callback. Any new `redirect_pointers` entry needs the same treatment.
- **Layout:** the data section moved to `0x80002A00` (code may run to there; `ws_config` is at the start
  of data, `patches.txt` has the address). Patches are compiled `-Os`. `gamepatch.py` leaves weak
  undefined symbols at 0, so `trace2d.c` hooks in through `trace_projection` only when it's linked.
- **Battle HUD anchors (mechanism only, table empty).** `ws_config.anchors` lists code ranges
  (`lo`, `hi`, `side` in percent of the gained width, `dy` in pixels); 2D drawn with one of them up the
  call stack (`ws_anchor_find`, PowerPC back chain) gets the projection moved and no 4:3 clip. Text is
  queued and drawn by `TextFlush`, so `TextPrint`, `TextPrintN` and `TextPrintNumber` go through
  `ws_text_anchor` (moves the node when queued) and `TextDrawNode` through `ws_text_draw_node` (draws
  moved nodes unclipped). The table can be filled at runtime with `poke:` to find and tune ranges. Still
  to find: the code that draws the battle status panel, command wheel and SP gauge (`BattleHudDraw` only
  does labels and icons). The user wants the left panels moved to the left edge and the top gauge up a
  little.
- **Anchors tested live (2026-10-09, ws8 on Windows; the mechanism is in the installed DOL).** The table is
  at `0x80002A18` (12 entries of `lo`, `hi`, `side` s8, pad, `dy` s16; poke it, e.g. the scratchpad
  `anchor.sh`). One entry over all game code (`0x80003100`-`0x80300000`, side -100) moved the whole battle
  HUD (SP gauge, status panel, command wheel) to the left edge, unclipped and drawn right, except the
  "/ 8" beside the SP number, which vanished (unexplained). In the world map (state 14) it moved the
  altitude gauge and compass, and pulled in a dark quad the game normally keeps out of sight, so anchors
  must be per element. `main` is more than 12 frames above HUD draws, so it never matches. Finding the
  per-element code by bisection (poke a range, screenshot, compare boxes against a fresh unanchored shot)
  works, but Windows screenshots of a background window take ~20 s, so it's hours per scene; found only
  `fn_80005D98` (generic rotated sprite: parts of the panel and wheel) and `fn_8000A118` (part of the
  wheel). Better: a `trace2d` build, which **doesn't fit**: code 0x1287 bytes against 0x1100 below the data
  at `0x80002A00`, and code + data 0x1742 against the gap's 0x1600. More room: the debugger-stack area
  (`0x8035D4C8`-`0x803624C8`) is heap here (OSInit starts the arena at `_stack_addr` when BI2's debug flag
  is < 2, and it's 0); MetroTRK's code (`0x802547C0`-`0x8025A094`, ~22 KB, interleaved with runtime code)
  only runs when `0x800030E8` is 2 or 3 (it's 0), a candidate after a call-graph check; or raise the arena
  start in OSInit (`0x80231D74`/`0x80231D3C`) at a small cost in heap.
- **Tracing:** `trace2d.c` now records every projection load (2D and 3D) with four game return
  addresses, hits and first/last frame; `tools/readtrace.py ADDR [reset]` prints it with names.
  On the title screen it showed the 2D: `main`'s background quad, `fn_80290A60` from `fn_801D129C`, and
  `fn_801D5DD8` (a multi-part sprite: logo, PRESS START) through `fn_80005D98` (a rotated sprite quad
  used by ~20 callers); the clouds come through the 3D path (`fn_80288DE4`).

### 60 fps: frame interpolation (prototype, 2026-10-07)

The game logic is fixed-step at 30 Hz (decomp `docs/frame_loop.md`), so 60 fps means drawing an
in-between picture, not running the game faster. `recompcore-frame-interpolation.patch` does it in the
video code, for any game: it records each frame's GX command stream, replays it with every XF matrix
and every inline vertex position halfway between this frame's and the previous frame's, and presents
that picture on the first retrace of the new frame and the real one on the second. For Skies the
positions are what matter: Ninja transforms vertices on the CPU and sends them in view space, so the XF
matrices barely change (about 2 of 2,400 loads per cutscene frame; ~4,200 draws and 24,000 vertices
per frame carry the motion). Draws are paired by vertex format, count and texture; within a kind, the
two frames' lists are aligned by their first vertices (below).

Turn it on with `frame_interpolation=true` under `[Video]` in the user directory's `config.ini`
(`build\user-ws` has it), or `RECOMP_FRAME_INTERPOLATION=1` in the environment, which wins (`2` adds
statistics every 300 frames).
`RECOMP_FRAME_INTERPOLATION_DUMP=<dir>` saves the real and in-between pictures of frames 300-305
(`_DUMP_FRAME` moves the first, `_DUMP_COUNT` sets how many; the count is XFB copies since boot, so
load a state to land on a scene). `RECOMP_FRAME_INTERPOLATION_TRACE=1` logs each frame end, replay and
retrace (which picture was shown) and the draws the alignment left unpaired.

**Title-screen cloud flicker (fixed late 2026-10-07).** The user saw the title's clouds flicker with
interpolation on (not off, not in 4:3). Presentation order was right every frame (trace); the bad
pictures were single in-between frames, every one to two seconds: the lower cloud layer is ~120 alike
draws, and when one strip scrolled out of the set, pairing the n-th draw with the n-th put every later
strip halfway to its neighbour. Draws of a kind are now aligned with a banded dynamic-programming match
on their first vertex positions (gap cost four times the typical motion), so a draw that appears or goes
is left unpaired and the rest pair correctly. Checked with 240 dumped title frames
(`build\savestates\09-title.sav`, the title in the current `GEAE8P-ws` build): the four bad in-between
pictures (in-between 10-25x farther from both neighbours than they are from each other) are gone, the
worst is now 1.21 like any frame (not yet confirmed by eye). Cost, limiter off, opening cutscene, 1x: 1.468x
before, 1.432x after (slowest second 1.40 both). Only the Windows runtime is rebuilt with it.

Checked on Windows (LLVM + PGO, widescreen game): 59.9 presented fps at speed 1.0 in the cutscene and
the first battle; 299 of 300 frames interpolated; the in-between picture of a slow camera move is as far
from each neighbour (RMSE 0.0059 / 0.0058, neighbours 0.0084 apart) and shows no tearing. Cost, limiter
off, opening cutscene: 2.07x without, 1.65x with. **Open (as of 2026-10-07):** scene cuts and particles (unpaired draws show the new frame); EFB copies the next frame reads
(the afterimage effect) see the replay's; WSL (llvmpipe). Since 2026-10-08 the replay's copies stay out of RAM and the cache
(*60 fps on Android: dual core*), so the game never reads one; the Retroid's own copy and TMEM problems are under
*Green bands at the bottom of the Valuan ladder*.

**Android (2026-10-08):** a test APK has it (`skies-ws-interp-2026-10-08.apk`, handoffs/HANDOFF-android.md), switched
on by an empty `FrameInterpolation.on` in the user directory (the runtime checks for it when the
environment variable isn't set). Retroid, limiter on: first battle full speed at 59.8 fps, opening
cutscene 0.79x (46 fps), so it doesn't fit there yet. (Superseded the same day: dual core, see
*60 fps on Android: dual core* below.)

### 60 fps on Android: dual core (2026-10-08)

**What the user saw** (three screen recordings from the Retroid, `build/android/recordings-2026-10-08/`,
single-core test build `skies-ws-interp-2026-10-08.apk`): slowdowns in the world map (79-91%), deck
battles (73-82%, 46-55% during a ring attack), the red alert room (72%) and the save menu (74%); and, in
two dialogue scenes after the first battle ("You coward!", Fina in the cell), every in-between picture
black from the top down to the bottom of the text box. Savestates of the slow spots, made on the device:
`build/savestates/android-worldmap-2026-10-08.sav`, `android-redalarm-slow-2026-10-08.sav` (Windows copies
`10-worldmap-android.sav`, `11-redalarm-slow-android.sav`). The user's memory card is backed up in
`build/android/user-save-2026-10-08/`.

**Why single core can't hold 60 fps there.** Skies is forced single-core (`GEA.ini`), so the game, its
GPU command processing and the replay share one A77; heavy scenes already use ~95% of the frame without
the replay. Adaptive replay (skip it while it doesn't fit, see *patches/README*) keeps full speed but
shows 30 fps there; it is still what single core does.

**Skies' dual-core desync, explained.** The game's end-of-frame sync, `fn_801D22CC`, sends draw-sync
token `0xB00B` and polls `GXReadDrawSync` until it comes back; if two retraces pass first it treats the
GPU as hung and re-inits the FIFO (`GXInitFifoBase` / `GXSetCPUFifo` / `GXSetGPFifo`). In dual core the GPU
thread can lag that much, and the reset pulls the FIFO from under it ("GFX FIFO: Unknown Opcode"). That
is presumably why Dolphin needed SyncGPU, later single core, for this game. Fix: a PE token read waits
for the GPU thread to work through the FIFO (`WaitForGpuToDrain`), so emulated time stands still while
the game waits and the token is current when read. Cold boots in dual core run cleanly (Windows and
Android, 90 s at 60 fps), and the standard states load, except below.

**Making the replay free in dual core** (each step measured on the Retroid, red alert room):
- First dual-core run (replay right after the XFB copy): 0.96 at 57 fps, but the CPU waited for the
  replay at the next token poll. Deferring the replay until the GPU thread idles made it worse (0.74):
  Dolphin's "sync on skip idle" made every CPU idle wait for the GPU thread's whole queue, replay
  included. With interpolation on, idle skipping now waits for the FIFO only.
- A timeline (`FrameInterpolation.timeline`) showed the rest: Skies submits its frame in the ~10 ms
  after the token, waits for draw-done, then runs ~17 ms of game logic with the GPU idle. A replay
  placed after the token sat in front of the next frame's commands, and the CPU waited for both.
- So the replay now runs on a **second EFB** (`FramebufferManager::SwapEFB`) **in slices** (96 KiB of
  stream each) whenever the GPU thread's FIFO is empty, with its own saved video state. The game's
  commands always go first. Its EFB/XFB copies go to private textures (never RAM or the cache), so a
  replay running during the next frame can't overwrite what that frame wrote. It is presented **one
  retrace later** (previous real, in-between, real), which gives it a retrace more; if it isn't done
  when due or when the next frame ends, the rest runs at once.
- The GPU thread's FIFO loop now reads up to 2 KiB per pass (it read 32 bytes, with a status update
  and atomics each time) and only runs its end-of-work steps (flush, peek cache) after work.

| Retroid, limiter on, interpolation on | speed | fps |
|---|---|---|
| single core, before (user's build) | red alert 0.72, world map 0.82, opening 0.79 | 42 / 49 / 46 |
| single core, adaptive | 0.98 / 1.01 / 1.00 | 29 / 30 / 30 |
| **dual core, sliced replay** (`skies-ws-interp-dualcore-2026-10-08m.apk`) | red alert 1.007, world map 1.005, first battle 1.003, ship field 1.005 | 59.8-59.9 |
| same, ws8 (`skies-ws8-interp-2026-10-09.apk`, 2026-10-09, 20 s each) | red alert 1.005, world map 1.008, first battle 1.003, ship field 1.005 | 59.8-59.9 |

Limiter off with the same build: red alert 1.28 (slowest second 1.235), world map 1.17 (1.137); dual core
without interpolation was 1.20 in the red alert room. Per frame there, the CPU now waits 1.1 ms for the
GPU (10.8 ms before the slices). On Windows (3x, LLVM + PGO, red alert, limiter off): dual core with
interpolation 2.11x, dual core without 2.08x, single core with 1.59x. In-between pictures checked by dump
(halfway between neighbours, same values as before the rework) and the presentation order by trace.

**Installed on the Retroid (2026-10-08):** `skies-ws-interp-dualcore-2026-10-08m.apk` (late 2026-10-08: `-08n.apk`, the same plus the state-load fix below), `CPUThread = True`
added to `files/GameSettings/GEAE8P.ini` (original in `files/Backup-2026-10-08/GEAE8P.ini.orig`, also
`build/android/bench/cfg-2026-10-08/`). `Dolphin.ini` unchanged (limiter on).

**Open:**
- **Fixed (late 2026-10-08): the state-01 load desync in dual core.** Loading `01-opening-cutscene.sav`
  into a running dual-core game hung at its first frame (3835) with "Unknown Opcode": 0xda over the
  title, 0x70 over state 02, so the GPU thread was decoding what was in RAM *before* the load. Cause:
  Dolphin loads states as a CPU-thread job with the GPU thread running again (`RunOnCPUThread` pauses,
  queues the job, unpauses). `VideoCommon_DoState` restores the command processor on the GPU thread,
  but RAM only comes later (`HW::DoState`, CPU thread), so the GPU thread went on reading the FIFO at the
  restored pointers from the old RAM. Only states saved with commands left in the FIFO hit it (01 is one);
  Windows' faster GPU thread mostly missed the window. Fix (`State.cpp`, in the interpolation patch):
  `State::DoState` holds the GPU thread (`Fifo::PauseAndLock`, which lets it finish the FIFO first) for
  the whole save or load in dual core. Checked on the Retroid (`skies-ws-interp-dualcore-2026-10-08n.apk`):
  01 loads over the title and over 02 at 59.9 fps / 1.00x, and 01, 02, 03, 04, world map, red alert, 01
  back to back in one session, pictures right, no alert. Windows: 01 -> 02 -> 01 in one session, fine.
  Dual-core saves had the same race (the GPU thread changing the FIFO registers mid-save) and are covered too.
- **FIFO overflow in dual core (user's play, 2026-10-08 17:35, `-08n` build):** "FIFO is overflowed by
  GatherPipe! CPU thread is too fast!" (`CommandProcessor::GatherPipeBursted`) in a palace/court scene
  after ~40 min of play, at 100% speed / 59.7 fps. The game stops writing at the high watermark (16 KiB
  before full) through the CP overflow interrupt (`GXOverflowHandler` suspends the writer), but the
  recomp takes interrupts only between dispatches, and one dispatch can run a whole function with its
  callees; with the GPU thread behind, the writes ran past the margin and overwrote unread commands.
  Fix (`Fifo::WaitForFifoSpace`, called per burst from `GatherPipeBursted`): when one more burst would
  overflow, the CPU thread waits until the GPU thread is back at the high watermark (2 s limit; not while
  the GPU waits on the CPU). It logs `FIFO full (… bytes): CPU thread waits for the GPU thread` (first 8
  times) to the COMMANDPROCESSOR log, which is off by default (the device's `Logger.ini` is empty), so in
  play the sign is only that the alert stays away. Installed `skies-ws-interp-dualcore-2026-10-08o.apk`; Windows/WSL runtimes rebuilt (previous
  Windows exe `moderngekko-run-1008-prefifospace.exe`). **Not yet confirmed at the spot** (no state there).
- **Black in-between pictures (user's recordings, Windows and Android, 2026-10-08 evening):** single frames
  with a black sky and black blocks while flying, and a black picture with only the HUD after closing the
  main menu. Reproduced on Windows by automation (world-map state 10, X to open the menu, B twice) with
  `RECOMP_FRAME_INTERPOLATION_DUMP`: always the first in-between after the menu, the real pictures fine.
  `RECOMP_FRAME_INTERPOLATION_T=1` (no blending) cured it, and the trace found the cause: a full-screen
  640x480 quad (backdrop/fade) at z = 100 in the first world frame and z = 0.001 in the next; the vertex
  jump test (more than half the value *plus* 50 units) let 99.999 through, and at z = 50 the quad covered
  the scene. Fix: a jump is more than half the value *or* more than 50 units, whichever is larger. A/B
  with the old exe (`moderngekko-run-1008-preefbstart.exe`): black at frame 113 before, none after.
  Also added, for correctness (none was the cause in the repro): the replay starts from the video state
  its frame started with (an EFB snapshot too, removed 10-09: it hung state loads) (the game writes only changed
  registers), and reads the vertex array data its draws read by index, copied when recorded (~1,900 of
  ~1,950 world-map draws index their arrays; ~57 KB per frame); Vulkan's `CopyRectangleFromTexture` now
  copies depth too. The mid-flight black sky (run 1, frame 346, ~20 frames before a menu opened) wasn't
  reproduced on a long flight with either build: probably the same quad, unconfirmed. Evidence pictures:
  `build/interp-black-2026-10-08/`. APK `skies-ws-interp-dualcore-2026-10-08q.apk` built, not installed.
- **Stray bands in in-between pictures while steering (user, Android and Windows, 2026-10-08 night):**
  single 16 ms frames with a translucent band or smear (bottom right over the cloud sea, behind the
  ship), every second or so, worst when steering left and right. Found in the recording only with a
  fine-grid neighbour test (`interp_vs_real`/outlier scripts, 16x9 cells); a 30 fps capture hides them.
  Cause: matrix loads were paired between frames by occurrence within their kind (address, count, next
  draw's key); when the game culls one of many alike objects (cloud strips while turning) every later
  object's matrix was blended with its neighbour's (traced: translations -124.8 vs +111.7, accepted by
  the 200-unit translation slack). Fix: loads of a kind are aligned like draws (`Align`, the dynamic
  programming that pairs draws, now shared) by the L1 distance of their matrices; an unpaired load
  snaps. Savestate at the user's spot: `build/savestates/12-streaks.sav`. A/B there (steer left/right,
  every picture dumped): 17 of 417 in-between pictures with stray content before
  (`moderngekko-run-1008-prealign.exe`), 0 after. APK `skies-ws-interp-dualcore-2026-10-08r.apk`
  installed; the user: "definitely a lot better".
- **Battle transition in-betweens (2026-10-09):** the spinning card came out torn into offset strips in
  the in-between pictures. Three causes, each A/B'd with dumps: vertices snapped or blended one by one
  (some corners of a strip jumped); then whole draws, but strips of one card snapped while others blended;
  then the alignment paired strips with neighbours (motion faster than their spacing). Now: when one draw
  jumps, every draw with the same texture snaps; kinds with equal counts pair in order unless the alignment
  costs less than half as much (scrolling clouds still align). Clean in both transition variants; the
  steering spot still 0 stray, the menu still no black frame.
- **Hang after state loads (found 2026-10-09, fixed):** the EFB start snapshot (added 10-08 night) hung the
  game right after loading `12-streaks.sav` in 2 of 3 loads (state running, 0 fps); bisected with switches
  to the snapshot (start state and vertex arrays innocent). Removed, with its Vulkan depth-copy change; the
  spare EFB starts as the last replay left it, as before. 6 of 6 loads fine. The `-08q`/`-08r` APKs have it. On the Retroid with ws8: 5 of 5 loads fine, no stray
  bands steering there, menu close is a plain fade (no lone black frame).
- **Cracks in the cliff while turning (user's recording, Retroid, 2026-10-09 evening, ws8):** flying along a
  floating island's cliff, a thin sky-blue seam opened along cliff polygon edges in every other picture (the
  in-betweens); recording `build/android/recordings-2026-10-09/`, state saved on the device at the spot
  `build/savestates/14-cliff-cracks.sav`. On Windows (steer left/right from the state, every picture dumped)
  the in-betweens tore whole blocks of rock out of place. Trace: almost every frame snapped some of the
  cliff's texture groups. The jump test looked at each coordinate alone: the cliff runs far off to the side
  of the camera (x = -1790), where its view-space z sits near zero, and turning moved z 99.6 -> 43.4, "more
  than half the value and more than 50 units", although the vertex moved 3% of its distance. One such
  vertex snapped its texture's whole group to the new frame while the cliff's other textures moved halfway.
  Fix (`VertexJumps`): the 50-unit floor grows to a tenth of the vertex's distance from the camera (a turn
  of up to ~6 degrees per frame no longer counts; the menu's z 100 -> 0.001 quad still does). A/B, same
  input: frames with a snapping group 392 -> 13 (the rest are real teleports, 370 units in y); worst
  in-between 23,101 -> 2,283 outlier pixels (texture speckle of normal motion, no seams). Unchanged:
  12-streaks steering (0 stray), menu close after X B B (no black picture), both battle transitions
  (identical stray lists); one battle-start in-between lost a dark smear. The trace now names draws
  without a partner, snapping groups and the first jumping vertex. The recording's other two oddities are
  the game's own: the diagonal white streak and the fog wall with a hard vertical edge (cloud quads) show
  in the original DOL at 4:3 without interpolation. APK `skies-ws8-interp-2026-10-09b.apk` (same module
  and DOL as ws8, runtime only), installed on the Retroid and checked there: no seams at the cliff, single-frame
  outlier pixels per frame median 205 -> 25, none over 1,000 (35 before), 59.7-59.9 fps.
- **Green bands at the bottom of the Valuan ladder (Retroid, 2026-10-10): fixed. The cloud-sea texture
  garbage on the same ladder: NOT fixed (see the end of this entry).** The pictures: single in-between frames where the
  cloud sea turned dark, gold or red, a pale rectangle over the cannons, a cloud billboard drawn white or
  black (`build/android/recordings-2026-10-10/`, state `build/savestates/15-ladder.sav`, hold down), and at
  the balcony three full-width bands of green dots at rows 0, 208 and 416 (sometimes the picture shifted
  instead). Only with interpolation on, only in-betweens, never on Windows at 1x or 3x.
  - The way there (half a day; the dead ends are worth knowing): the bands' rows are the first rows of the
    Adreno's three 208-row tiles of a 640x480 render pass, so a GPU-side race was the first reading. Vulkan
    synchronization and core validation on the device found nothing; a semaphore chain between submissions,
    full memory barriers before or after the replay's copies, a layout barrier at each EFB swap, transfer
    blits instead of copy passes, storage usage (no UBWC), the replay on the game's own EFB, whole-frame
    replays, holding the replay's copies for three frames: no change. Painting the replay's EFB magenta
    before each replay and the grab textures yellow after their copy showed the bands drawn over the paint,
    i.e. the draws making them don't read what the copy wrote. A fence wait before each replay copy
    (`WaitForGPUIdle`) made them vanish at 70-83% speed; a bare submit made them worse. Skipping ranges of
    the replay's draws (`FrameInterpolation.skip`) narrowed it to draws 3011-3012 of the frame: two
    full-screen quads sampling the second screen grab (`003a84e0`, an I8 intensity EFB copy made before
    draw 3009), i.e. the game copies the screen and draws it back two draws later.
  - The cause: the game's copies go to RAM too (`EFBToTextureEnable = False` in the game ini), so each is
    the VRAM copy pass *and* the encode pass (a render pass reading the EFB, then an image-to-buffer
    transfer). The replay's copies went to VRAM only. On this driver, without the encode pass behind it, a
    copy sampled right after shows rows it hadn't finished, barriers or not. Fix: the replay's copies run
    the same encode pass and drop the result (never RAM); `DriverDetails::BUG_EFB_COPY_NEEDS_READBACK`,
    Qualcomm Vulkan. Final build (`skies-ws9-encode-2026-10-10.apk`, sha1 75cac5b8), three 15 s ladder runs: 0, 0, 0
    green (7-44 before), single-frame outliers 1, 0, 2 (1-18 before), speed 99-101% on the overlay.
  - Also in: textures the game preloads into TMEM are recorded and re-bound like the others (the replay
    used to re-read TMEM, which the next frame had refilled by then; right on its own, not the cause), and
    `FramebufferManager::SwapEFB` ends the outgoing EFB's render pass with a layout change (a tiled GPU
    loads the next pass's tiles from memory; also right on its own). Both A/B'd on Windows: ladder and menu
    in-betweens unchanged.
  - Device debug files (user dir): `FrameInterpolation.trace` (trace to logcat, tag FrameInterpolation),
    `FrameInterpolation.dump` holding "first count" (pictures to `InterpDump/`), `FrameInterpolation.slice`
    holding the slice size in bytes, `FrameInterpolation.nodefer`, `FrameInterpolation.skip` holding
    "lo hi" (the replay leaves draws lo..hi-1 out: bisect a wrong picture; 3700 draws a frame here).
    Vulkan validation on the device: push `libVkLayer_khronos_validation.so` (Khronos android-binaries)
    into the app's data dir with `run-as`, `settings put global enable_gpu_debug_layers 1`,
    `gpu_debug_app`, `gpu_debug_layers VK_LAYER_KHRONOS_validation`, `setprop
    debug.vulkan.khronos_validation.validate_sync true`; 0.33x speed, tag VALIDATION in logcat.
  - **Cloud-sea garbage (user's play on the encode build, `build/android/recordings-2026-10-10/user-ladder-play.mp4`):
    fixed late 2026-10-10 (APK `skies-ws9-notmemload-2026-10-10.apk`).** The picture: the cloud sea below
    the ladder keeps its shape but is drawn with a dark olive-and-black texture, the same look every time,
    for one game frame: frames 125-126, 153, 206-207, 314-315, 996-997, 1021-1022, 1237, 1376-1377 of 1397.
    `tools/ladder-check/cloudfind.py` scores it (the cloud-sea region's brightness, 600x200 at 0,880 of
    1080p, frames darker than the video's median by 25) and finds exactly those eight events; over every
    ladder recording of the day it gave 0-5 events per 15 s run with the replay on (encode 0/1/4, tmemstate
    5/3/3, copytrace 2/3), **0/0/0 with `FrameInterpolation.noreplay`**, 0 without interpolation, and 0 on
    Windows even with 2 KiB slices.
    - What it was: a **real** picture is wrong, and the in-between replayed from its recording with it
      (shown by `check-ladder-skip-3500-6000-1.mp4`, where the replay left the HUD out, so in-betweens are
      the frames without the minimap: frame 635, an in-between, and 636, the real one, both have it).
      The replay's recorded stream carries the game's TMEM loads (`BPMEM_LOADTLUT1` palette loads,
      `BPMEM_PRELOAD_MODE` preloads: the cloud textures are preloaded), and a slice replayed them,
      copying what RAM holds *now* over the game's TMEM data (`s_tex_mem`). On the Retroid the replay
      takes ~10 ms of GPU-thread time spread over the FIFO's idle gaps, so slices fall inside the game's
      own frame, between its load and its bind; the bind then hashes the replaced bytes, makes a new
      entry from them, and the draw shows another texture through the clouds' mapping. On Windows the
      whole replay fits the idle gap after the frame, so no slice ever lands there.
    - Fix: TLUT loads and TMEM preloads are skipped while replaying (`BPStructs.cpp`, with the token and
      bounding-box writes). The replay never needs them: it binds the cache entries its frame's draws got
      (`FindRecordedTexture`), preloaded ones included.
    - Also in the tree from the hunt, each right on its own but not the cause: the TMEM unit state goes
      with the replay's saved video state and the cache's bound textures are put back after a slice
      instead of `TMEM::InvalidateAll()` (tested alone: 5/3/3, no change); debug switches
      `FrameInterpolation.copytrace` (the game's 640-wide copies: copy / flush / prune with hashes; showed
      the grabs are never pruned, their pending RAM writes are always tossed by the XFB copy, so the
      "hash mismatch -> RAM decode" theory was wrong), `.paintgrab` (the game's grabs yellow after the
      copy: showed the grab is drawn back over the whole screen, a bloom, not just the sea) and
      `.paintefb` (the replay's EFB magenta after its last slice).
    - Checked on the Retroid: six 15 s ladder runs (three on the test build, three on the final APK),
      0/0/0 and 0/0/0 cloud-sea events, 0 single-frame outliers, 0 green, speed 99-101% on the overlay.
      The user played the ladder on the final APK and called it resolved. Windows regression at 1x (`build\user-bt`): ladder 0 bad in-betweens of 519 at the default and at
      2 KiB slices, menu 0 of 295, 0 dark pictures, no panics (unchanged).
- **Input only with focus (Windows, 2026-10-10, user request):** the user's keys reached background test
  windows (menus opened mid-test). The Win32 window now tracks focus, so Dolphin's input gate works:
  `[Input] BackgroundInput = False` (the user dirs' setting) ignores pads and keyboard while unfocused; `True`
  is the toggle. Automation input still applies (input overrider). Previous exe `moderngekko-run-1010-prefocus.exe`.
- The black dialogue pictures weren't reproduced: state 01's dialogue never showed them (Windows, or the
  Retroid with the old build). Suspected cause: the replay's EFB copies to RAM (Skies sets
  `EFBToTextureEnable = False`), which the rework took out of RAM entirely. Needs a by-eye check at
  "You coward!" or Fina's cell.
- Deck battles with heavy effects (the 46% ring attack) have no savestate yet.
- **Desktop plays dual core too (late 2026-10-08):** `build\user-ws` (what `tools\play-ws.bat` uses) now
  has `GameSettings/GEAE8P.ini` with the idle skip and `CPUThread = True`. Checked limiter off in the red
  alert state with that setup (16:9, 1920x1080): dual core 2.09x / 124 fps, the same with `CPUThread = False`
  1.59x / 94 fps.

### 60 fps (earlier notes, 2026-10-06)

Full speed (1.00×) is the game's own 30 fps; headroom above it is spent idling by the frame limiter, so
speed work never raises the frame rate. Two routes, neither started:
- **Game-side 60 fps patch.** If the game logic is fixed-step, it runs twice as often, which needs ~2.0×
  headroom in every scene. Android has ~1.02–1.05× and the fastest desktop module (Windows LLVM) 1.4–1.6×,
  so CPU work can't get there.
- **Renderer interpolation (RT64-style).** The game stays at 30 fps and the renderer draws in-between
  frames from interpolated GX/Ninja matrices (*Hook points*). It costs GPU time, not emulated-CPU time, so
  it's the realistic route, Android included. Dolphin has nothing like it: substantial new renderer work.

First step if resumed (cheap, no device): find in the decomp whether the frame loop is fixed-step or
delta-time, which decides between the two.

## Status (2026-10-05, Linux/WSL2 + WSLg, GCC 15; C and LLVM 20 backends)

- `make tools`: builds cleanly.
- `make recompile`: ~6 minutes on 12 cores, peak ~650 MB RAM. DolRecomp warns that the DOL may
  patch executable memory and registers 103 self-modifying-code ranges to verify.
- **Plays.** Boot → SEGA / Overworks logos → title (New Game / Continue) → new-game prompts →
  opening cutscene on the Valuan ship → first battle (Attack via A) → Vyse walking around the ship
  under stick control. `fallback=0` (no interpreter fallback) and no SMC failures throughout.
- **Speed:** full speed (1.00×) in the standard test state with the LLVM state-in-memory module,
  idle skipping and 1× internal resolution; 0.565× with the default C module at 3×. See *Performance*.
- **Native Windows build: full speed at 3×** (1.01×, 1920×1580) with a clang-built C module on the RTX
  5080. An MSVC-built module runs at half speed. See *Native Windows build*.
- Not yet checked: audio by ear, memory-card saving. (The keyboard was pressed by hand on WSL and
  Windows on 2026-10-17, *Controller and keyboard*.)

## Rendering on WSL

WSL has no `/dev/dri`, so Mesa falls back to llvmpipe (software) unless told otherwise. Measured
from one savestate in the opening cutscene:

| Renderer | How | Result |
|---|---|---|
| Vulkan / llvmpipe | default | **Correct picture.** ~0.58–0.75 speed, ~11 llvmpipe threads at ~58%, ~4.3 GB RSS. Use this. |
| Vulkan / dzn (Mesa on D3D12, RTX 5080) | `VK_DRIVER_FILES=~/.local/dzn/icd.d/dzn_icd.json` | **Broken picture**: blocky clouds, unreadable text. ~0.66–0.75 speed. Segfaults (exit 139) during teardown, after the runtime's clean shutdown line. Don't use. |
| OpenGL / d3d12 (RTX 5080) | EGL-enabled runtime (`-DMODERNGEKKO_ENABLE_EGL=ON`, built in `lib/ModernGekko/build-egl`) + `--graphics OGL` + `GALLIUM_DRIVER=d3d12 MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA` | **Broken picture, worse than dzn**: most geometry, the background and the dialogue box don't draw. EGL does pick the RTX 5080 (the `DRI3 error` warnings are harmless). Segfaults on shutdown. Don't use. |

dzn's speed gain was small and its picture wrong. When checking a renderer, compare it against
llvmpipe on the same savestate, text included, before trusting any speed figure.

## Performance

`resolution=1920x1080` in `config.ini` renders at 1920×1580 (screenshot size), which is 3× the
640×528 EFB. Earlier notes called it "2×".

**Full speed is reachable** in the standard test state (the opening cutscene): **1.00×** with the
LLVM backend + state-in-memory + idle skipping, at 1× internal resolution. Measured 2026-10-05 from
`01-opening-cutscene.sav`, llvmpipe, speed averaged over 30 s, interleaved runs:

| Setup | 3× res (`1920x1080`, renders 1920×1580) | 1× res (`640x528`) |
|---|---|---|
| C backend, gcc `-O2` (starting point) | 0.565 | — |
| + idle skip, no mod hook | 0.639 | 0.80 |
| LLVM backend | 0.575 | — |
| LLVM + `--state-in-memory` | 0.645 | — |
| LLVM + state-in-memory + idle skip, no mod hook | **0.658** | **1.00** |

At 3×, the limit is llvmpipe. The CPU-GPU thread drops from 99% to ~77% and waits on 8+ llvmpipe
threads at ~65%, sharing 12 vCPUs. That's why the internal resolution matters more than anything else.

### Recipe

1. Build the tools with LLVM 20 (`llvm-20-dev` from apt) and apply the [local patches](../patches/README.md):
   ```sh
   cmake -S lib/ModernGekko -B lib/ModernGekko/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
     -DDOLRECOMP_ENABLE_LLVM=ON -DLLVM_DIR=/usr/lib/llvm-20/lib/cmake/llvm
   cmake --build lib/ModernGekko/build --target moderngekko-port moderngekko-run
   ```
2. Build the module (~8 min, separate output so the C module stays):
   ```sh
   lib/ModernGekko/build/moderngekko-port build extracted/SkiesOfArcadiaLegends \
     --backend llvm --state-in-memory --output build/modules-llvm-sim
   ```
   The port's cache key doesn't cover GXRuntime source edits. After changing GXRuntime, rebuild
   `<artifact>/module-build` with `cmake --build` and copy its `.so` over the artifact's.
3. Idle skipping: `build/user/GameSettings/GEAE8P.ini` holds `[Core] StaticRecompIdlePC = 0x80237BA8`.
   That's the SDK scheduler's spin in `SelectThread` (`lwz r0, RunQueueBits` / `cmplwi` / `beq`),
   which was 7.75% of the thread before.
4. For full speed on WSL set `resolution=640x528` in `build/user/config.ini` (1× EFB; on Windows 3× is
   fine, see *Native Windows build*), then run:
   ```sh
   MODULE=$(ls build/modules-llvm-sim/GEAE8P/*/gGEAE8P_recomp.so) tools/play.sh
   ```
   (2026-10-05 recipe. The current module is LLVM + PGO from `tools/pgo-wsl.sh build/modules-llvm-pgo-gp`:
   `MODULE=$(cat build/modules-llvm-pgo-gp/GEAE8P/active-module.txt)`. `modules-llvm-sim` predates the
   gather-pipe patches and is refused by the current runtime.)

### What didn't help (or barely)

- clang `-O3` + ThinLTO on the C backend: ~+4%. Upstream measured `-O3`/ThinLTO as neutral.
- Early-out in `ModManager::Dispatch` with no mods: ~+2%. Superseded by not installing `host_call`.
- Dual-core (`CPUThread = True`): hung on the first frame after a savestate load.
- GPU renderers under WSL: both break the picture (see *Rendering on WSL*).

### Profile, LLVM + state-in-memory (3×, before idle skip / hook removal)

The hottest guest code was `SelectThread` (idle spin, now skipped), unnamed Ninja functions around
`0x802A2200`/`0x802A4A00`/`0x802A6000`, `PSMTX*` (paired-single matrix math), and `GXSetProjection`/
`GXLoad*MtxImm`. Runtime side: GPU FIFO writes through `HookExternalWrite` → `GPFifo::Write32`
(~3.7%), `OnICacheInvalidate` (~1.9%).

Upstream context: ModernGekko `8f7a642`/`ecd3188` show CPU-limited Mario Kart: Double Dash!! scenes at
0.50–0.61× on a 5900X, and DolRecomp's LLVM work (`8993041`, `18d0add`, `c876e2b`) targets a newer
module ABI than our pin. PGO (`moderngekko-port pgo-run`) is untried; on the LLVM backend it needs
`clang-20` to match.

### CPUs without FMA3 (issue #13, 2026-10-22)

Kaito0 (#13): 5-10 fps on an Ivy Bridge CPU with a GTX 1050 Ti, where Dolphin holds 30. The x86-64
modules are built for the generic target, which has no fused multiply-add instruction, so every guest
`fmadd` / `fmadds` / `ps_madd` family instruction became a call to the C runtime's `fma()` (the
generated code's `llvm.experimental.constrained.fma`, and `ppc_fma` in the CPU runtime): about 13
million calls per game second in the opening, 6 million in the first battle. The C runtime uses the
FMA3 instruction where the CPU has one (Haswell, 2013, and later; AMD Piledriver and later) and a
software routine where it doesn't: UCRT's took 26.5 ns a call against 2.2 ns on the i7-12700K
(`_set_FMA3_enable(0)`), glibc's is similar. Dolphin's JIT avoids it by a separate multiply and add on
such CPUs (`Config::SESSION_USE_FMA`, Jit64 `fmaddXX`), which rounds differently.

Fix (`recompcore-module-fma.patch`, `module-template/host_fma.c`): the module defines `fma()` itself, so
neither the generated code nor the runtime reaches the C runtime's. With FMA3 it is the instruction.
Without, a call whose operands each have at most 26 significant bits (the low 27 stored bits clear),
are finite, and whose product's exponent is well inside the normal range, has an exact product in a
double, so `a * b + c` rounds once like the fused operation: same bits, rounding mode, flags, and
denormals-are-zero / flush-to-zero behaviour. That is 94% of the game's calls (single-precision
values, the 25-bit `ps_madd` multiplier); the rest still go to the C runtime's `fma` (`ucrtbase.dll`'s
by `GetProcAddress`, glibc's by `dlsym(RTLD_DEFAULT)`). A NaN result may name either NaN input, as the
C runtime's may; `ppc_fma` and DolRecomp's `preferPairedFMANaN` replace it with the PowerPC's choice.
No code generation change, so the PGO profiles and the codegen cache stay valid. Empty on arm64
(Android, macOS: the instruction is always there and inlined). `MODERNGEKKO_NO_FMA3=1` takes the
no-FMA3 path on any CPU, for testing.

Checks:

- `tools/fma-exact-test.c`: 80 million cases on the no-FMA3 path and 24 million on the FMA3 one
  against the instruction, all four rounding modes, DAZ/FTZ on and off: no difference in result or
  flags (host_fma.c built with GCC and with clang).
- WSL (ws32 module and game, i7-12700K, the calls that reach the C runtime slowed by 25 ns to stand in
  for a CPU without FMA3): lookout scene 0.837 before, 1.569 after (1.569 with FMA3 before); Pirate
  Isle 0.721 before, 1.322 after (1.323). With FMA3 the Linux module is 7-9% faster than before (no
  call through glibc's `fma` PLT and resolver): 1.677 / 1.438.
- Windows (v0.82 runtime, ws34 USA module relinked from the same objects, both scenes): with FMA3
  2.483 / 2.290 against 2.462 / 2.279; the no-FMA3 path forced, 2.398 / 2.210 (its exactness test and
  the 6% left to the C runtime, which on a real CPU without FMA3 still pays the software routine, 6% of
  the old fma cost). `fallback=0` in every run; the DLL imports `fmaf` (never called) but not `fma`.
- Not run: a real CPU without FMA3. The reporter's speed after the fix depends on the rest of their
  CPU and GPU; the WSL figures say the fma cost is gone, not that an Ivy Bridge reaches full speed.

## Native Windows build

Two separate limits showed up under WSL:

- **Rendering** (WSL only, now confirmed): WSL has no `/dev/dri`, so the only correct renderer is
  llvmpipe on the CPU, which caps 3× at ~0.66×. On Windows, the RTX 5080 holds 3× at full speed
  with the GPU nearly idle.
- **The game thread** (every OS): recompiled code plus Dolphin's GPU command processing run on one
  thread, because Skies is forced single-core. Single-core CPU speed and the module's code quality
  decide this. Heavier scenes (big battles, the world map) are unmeasured.

### The game window: fullscreen and display scaling (2026-10-17, after 0.10.0-beta)

`patches/recompcore-win32-fullscreen.patch` (`DolphinNoGUI/PlatformWin32.cpp`, commit `b9f0157`),
from players at 3840x2160 on 0.10.0-beta:
- **Fullscreen at start.** Upstream's `Init` had a TODO where `[Display] Fullscreen` should apply, so
  the window always opened windowed until Alt+Enter. It now goes fullscreen before the video backend
  makes its swap chain.
- **No border.** The window is made with `WS_EX_CLIENTEDGE`; fullscreen removed only the frame styles,
  so the client edge stayed as a light 3-pixel frame (client 3834x2154). Fullscreen now drops the edge
  styles and restores them when windowed; the class brush is black, not the system window colour.
- **Display scaling.** The runner wasn't DPI aware: at 150% Windows rendered a 3840x2160 monitor as
  2560x1440 and stretched the window, so the picture was soft at any internal resolution. It is now
  per-monitor DPI aware (`SetProcessDpiAwarenessContext` PMv2 in `Init`), scales the configured window
  size (96-DPI units) by the window's DPI, follows `WM_DPICHANGED` while windowed (untested: one
  monitor) and sets `render_surface_scale` to DPI / 96 so on-screen messages keep their size. Checked
  against the 0.10.0 package at 150%: window DPI 144 instead of 96, sharp menu text at the same size,
  the same windowed size on screen.
- **Linux has none of this.** `PlatformX11` already applied Fullscreen at start (upstream did that one),
  and fullscreen is a `_NET_WM_STATE_FULLSCREEN` request (`UICommon/X11Utils.cpp`): the window manager
  sizes the window, and X11 has no client-edge style to leave behind. Seen only on WSLg, not on a Linux
  desktop or a Steam Deck.

### Status (2026-10-05): full speed at 3× with a clang-built C module

Measured with `tools/bench-win.sh` (the standard-state protocol), three interleaved rounds:

| Module (C backend, idle skip, no mod hook) | 1× (`640x528`) | 3× (`1920x1080`) |
|---|---|---|
| MSVC 14.50 (`cl /O2 /fp:strict`) | 0.505 / 0.506 / 0.504 | — |
| clang 20.1.8 (`-O2 -ffp-contract=off`, ThinLTO) | 1.010 / 1.012 / 1.009 | **1.011 / 1.012 / 1.011** |
| *WSL for reference:* gcc 15 `-O2`, llvmpipe | 0.80 | 0.639 |

- **Use clang for the module on Windows.** The MSVC module is exactly half as fast. The template's
  `--toolchain auto` picks `cl` when the tools were built with MSVC, so it has to be overridden.
  The picture is identical. Why MSVC is so much slower isn't settled. A likely candidate is the template's
  `/fp:strict` for MSVC (clang and gcc only get `-ffp-contract=off`), which blocks a lot of
  optimization in float-heavy code. Untested; worth reporting upstream.
- **1.01× is the frame limiter.** Neither number shows how much headroom is left above full speed.
- **The GPU check, done before the clang test:** it renders on the RTX 5080, not the Intel UHD 770
  iGPU. Its 3D-engine work and dedicated memory are on the NVIDIA adapter, at ~1–3% GPU use while one
  thread sat at 97%. E-cores were ruled out too: pinned to the P-cores (CPUs 0–15) the MSVC module
  ran at 0.50×, and on the E-cores (16–19) at 0.24×.
- **Boots from the standard state** made on WSL. The picture is correct at 1× and 3×, dialogue
  text included. The user checked the 3× screenshot and approved it (2026-10-05). Shutdown is clean (`fallback=0`, `smc_failed=0`).
- On Windows the game sees all 20 logical processors (WSL: 12, from `.wslconfig`).

### Building on Windows

The Makefile is Unix-only, so `tools/build-win.bat` runs the same CMake steps and sets up the VS
environment itself. Needs: VS Build Tools 18 with the C++ workload (MSVC 14.50, Windows SDK
10.0.26100.0, bundled CMake + Ninja) for the tools, and LLVM's release tarball for the module.
No admin rights are needed.

1. Get the source onto a Windows path (not `\\wsl$`, which is slow and brittle for builds): clone
   with submodules and apply [`patches/`](../patches/README.md), or copy a patched WSL tree without
   `.git` and the `build/` dirs (Dolphin's CMake doesn't need Git). Copying the tree plus the disc
   over `/mnt/c` takes ~12 min.
2. Put the extracted disc (`sys/` + `files/`) at `extracted\SkiesOfArcadiaLegends`.
3. Unpack `clang+llvm-20.1.8-x86_64-pc-windows-msvc.tar.xz` (LLVM GitHub release, 940 MB, matches
   WSL's LLVM 20) with Windows `tar.exe -xf` into `C:\dev\`, or set `LLVM_ROOT`. It also has
   `lib\cmake\llvm` for a future LLVM-backend build.
4. `tools\build-win.bat configure`, then `build` (~15 min), then `module` (clang, ~10 min with ThinLTO,
   into `build\modules-clang`). `module-msvc` builds the slow MSVC module into `build\modules`. They work
   from WSL too, e.g. `cmd.exe /c "C:\dev\SkiesRecomp\tools\build-win.bat module"`. (vcvars prints
   a harmless "'vswhere.exe' is not recognized".)
5. Make `build\user` from the WSL one: `config.ini`, `GameSettings/GEAE8P.ini` (idle skip),
   `Config/GFX.ini` and `Config/Dolphin.ini` with `[DSP] Backend = Cubeb` instead of `Pulse`. No
   controller profile is needed: the game adds the keyboard layout (DirectInput names:
   `DInput/0/Keyboard Mouse`, `SPACE`, `LSHIFT`, `RETURN`) on the first start (*Controller and
   keyboard*); [`config/GCPadNew.keyboard.windows.ini`](../config/GCPadNew.keyboard.windows.ini) as
   `Config/GCPadNew.ini` still gives Dolphin's old layout.
6. `tools\run-win.bat build\run.log --load-state build\savestates\01-opening-cutscene.sav` runs the
   clang module (set `MODULE` for another) with the automation directory on. From WSL,
   `tools/drive.py` drives it (run it in `/mnt/c/dev/SkiesRecomp` with `AUTO=build/auto`; it
   translates `/mnt/c/...` screenshot and savestate paths to `C:\...`), and
   `tools/bench-win.sh NAME MODULE` runs one measurement.

The `moderngekko-run` placement rule above applies here too: run `lib\ModernGekko\build\moderngekko-run.exe`
in place, next to its `Sys\`. From WSL, set variables for cmd as `set VAR=value&& ...`. A quoted
`set "VAR=..."` reaches cmd as `\"VAR` and silently does nothing.

### Open questions

- ~~Headroom~~: measured 2026-10-06 (*Desktop headroom*): clang + PGO worst second 1.27× (1×) / 1.29× (3×)
  with the gather-pipe patches. Heavier scenes (big battles, world map) are still unmeasured.
- Whether MSVC's gap is `/fp:strict` (try `/fp:precise` in the module template).
- LLVM backend on Windows: done by cross-generating COFF objects on WSL (`tools/module-llvm-win.sh`). The exit
  crash is fixed (`dolrecomp-win64-sjlj-frame.patch`), and `build\modules-llvm-pgo-fix` (LLVM + WSL-trained
  PGO) is the fastest Windows module, worst second 2.04× (*Desktop headroom*). Windows can't train its own
  LLVM profile (its tools are built without LLVM); the WSL profile matches (same DOL and patches).
- Route B, cross-compiling from WSL with `clang-cl` like HybridHeavenRecomp: no longer needed.
- How the GPU was measured, for next time: `Get-Counter "\\GPU Engine(pid_<pid>_*)\\Utilization Percentage"`
  and `"\\GPU Process Memory(pid_<pid>_*)\\Dedicated Usage"` per adapter LUID, per-thread
  `TotalProcessorTime` deltas from `Get-Process`, and `ProcessorAffinity` for pinning.

## Android

### Status (2026-10-05): 0.89–0.98× in gameplay on the Retroid Pocket 5

*Current state (late 2026-10-06): limiter off 1.05 / 1.57 / 1.20 / 1.04, worst second 1.020, a stable 30 fps;
see *Gather-pipe stores* below. This section is the 2026-10-05 history, measured with the limiter on.*

Skies runs natively in Dolphin's own Android app (vendored, plus
[`patches/recompcore-android.patch`](../patches/recompcore-android.patch)) with the arm64 module
inside the APK. Logcat shows `StaticRecomp SC | Vulkan | HLE | Skies of Arcadia: Legends (GEAE8P)`.
The module stays mapped, so it was accepted (a rejected module is unloaded and runs on the
interpreter). Picture correct in all states checked, text included.

Standard protocol (`tools/bench-android.sh`: fresh boot, load, 16 s, mean speed over 30 s), 1×,
limiter on, one run each. Battle `02` swung 0.4–1.1× within a run (the `mtmsr` bug, found and fixed
2026-10-06), so treat ±0.03 as noise. The 01 column includes an A press in at least the last two rows
(the dialogue advanced); untouched, 01 measures 0.87–0.90:

| Build | 01 cutscene | 02 battle | 03 field | 04 field later |
|---|---|---|---|---|
| C backend (NDK clang `-O2`) | 0.529 | 0.607 | 0.533 | 0.478 |
| LLVM backend + state-in-memory (aarch64) | 0.869 | 0.717 | 0.710 | 0.720 |
| + icache early exit | 0.877 | 0.801 | 0.769 | 0.802 |
| + FIFO exception-check skip, inline REL identity | 0.914 | 0.746 | 0.801 | 0.822 |
| **+ PGO trained on the device** | **0.970** | **0.876** | **0.944** | **0.896** |
| + Dolphin built `-mno-outline-atomics` (noise) | 0.976 | 0.900 | 0.939 | 0.891 |

FPS at the last row: 29.0 / 25.8 / 27.8 / 26.3. The PGO profile was trained on these same four
states, so its gain is probably a little optimistic for the rest of the game. The game thread
sits on cpu7 (the 2.84 GHz prime core) at full clock by itself, so pinning wouldn't help.

What the profiles showed (simpleperf, `CPU-GPU thread` ≈ 88% of the process, Adreno driver ~2–4%,
so CPU-bound on one thread):
- C module: 56% game code, 32% runtime. `StaticRecompCore::Run()` alone was 11.5%, with load
  stalls on the 24 MB chunk lookup table (A77 caches are small).
- LLVM module: dispatch overhead drops (Run 2.4%). `OnICacheInvalidate` became 6%, all in its
  binary search over 5,452 chunks for `icbi`s on data buffers. Early exit when the range lies
  outside the code: 0.9%.
- `CompileExceptionCheck` ran on every gather-pipe flush and re-decoded a stale `ppc_state.pc` each
  time. It's now skipped during native bursts.
- Left after PGO (share of the runtime's ~39%): GPU FIFO writes through `HookExternalWrite` →
  `GPFifo::Write32` ~20%, `Run`/`SyncIn`/`GetPPCState` ~14%, texture hashing, icache bookkeeping.

The three runtime fixes are in [`patches/recompcore-staticrecomp-speed.patch`](../patches/recompcore-staticrecomp-speed.patch)
(shared core). Desktop runs them too since 2026-10-06 (+2–6% there, see *Desktop headroom*).

### Dips: interpreter re-entry after `mtmsr`, fixed (2026-10-06)

**Re-baseline, limiter off** (`EmulationSpeed = 0.0`, two rounds, before the fix; mean / slowest second):
01 0.870/0.857 and 0.897/0.894, 02 1.432/1.403 and 1.428/1.403, 03 0.992/**0.506** and
1.058/1.029, 04 0.876/0.859 and 0.908/0.904. cpu7 held 2.84 GHz in every sample, the hottest core stayed
≤56 °C, the GPU was ~12% busy: one thread, CPU-bound, no throttling.

- **The 2026-10-05 0.97 in 01 was not a clean number.** State 01 stops on a dialogue line that waits for A.
  That run ended two lines later, so something pressed A during it, and part of the window measured
  a lighter shot. Untouched, 01 is a flat 0.87–0.90 from boot to boot.
- **Not shaders, not thermal.** No shader-cache file was written all day (the cache is warm), and during a
  dip the Adreno driver was <2% of the game thread while cpu7 stayed at full clock. (Separate fact:
  `am force-stop` never saves `Vulkan-Pipeline-*.cache`; a clean `stop` does.)
- **Cause: the LLVM module's cold-entry path.** simpleperf with `--clockid realtime`, sliced per second
  against drive.py's trace: in a 0.44× second, `HookInstructionFallback` → `Interpreter::SingleStepInner`
  with `SyncIn`/`SyncOut`/`SetPPCStateFromGuestState` around *every* instruction was >45% of the game
  thread. An `mtmsr` that sets MSR[EE] side-exits to the next instruction; that PC was not a region leader,
  so the dispatcher re-entered through the cold path, which interprets one instruction at a time until a
  leader. That is the `rlwinm; blr` tail of every `OSRestoreInterrupts` and `OSEnableInterrupts`: 435k of
  912k fallbacks in a desktop battle run (`STATICRECOMP_FALLBACK_SAMPLES=1`). Interrupt-heavy moments
  (battle effects) call them in bursts, hence 1–2 s dips that only appear on the A77, and differ run to
  run because battle RNG differs.
- **Fix:** [`patches/dolrecomp-mtmsr-entry.patch`](../patches/dolrecomp-mtmsr-entry.patch) makes the
  instruction after `mtmsr` a native entry. Desktop battle: `hook_fb` 912k → 39k. Android, limiter on:
  02 0.999 (min 0.951), 03 1.011 (min 0.986), 01 0.875 (min 0.850), 04 0.840 (min 0.824), no dips. First
  battle passes from load, which dipped to 0.44–0.55 every time before, stayed ≥1.0 while the battle ran.
- **Retrained and A/B'd (interleaved, limiter off, two rounds; runs with screen presses dropped).** The
  first post-fix module ran with a stale profile (14 functions with changed control flow lost it), so the
  device profile was retrained on the fixed module (same four states, walking in 03/04;
  `build/android/pgo/skies-android-mtmsr.profdata`, 0 mismatches). Old best → new, mean (slowest second):
  01 0.881/0.876 → 0.901/0.902 (0.888), 02 1.423/1.448 → 1.472 (1.438), 03 1.059 → 1.076/1.067 (1.050),
  04 0.891/0.894 → 0.904/0.910 (0.897): +1.5–2.6% everywhere, no second more than 0.015 below its mean.
  Limiter on: 01 0.909 (0.884), 04 0.897 (0.878). APK `build/android/apk/skies-dolphin-debug-mtmsr-pgo.apk`.
- **Desktop (WSL LLVM, PGO retrained with the fix, `build/modules-llvm-pgo-v2`; superseded late 2026-10-06 by
  `build/modules-llvm-pgo-gp`, which has this fix and the gather-pipe patches):**
  fallbacks ~10× lower (01 548k → 55k, 03 682k → 54k); speed equal to −2–4% (04), within WSL's llvmpipe
  ceiling and noise (runs overlapped the device A/B). One v2 battle run took a different course (mean 2.3)
  with 1.07M fallbacks: a second fallback source in some battle outcome, not yet sampled.
- What is left are the remaining 39k fallbacks: lazy FPU switching (the first FP instruction after a thread
  switch traps; `rfi` lands mid-block, e.g. `GXSetProjection`, 2.2k×9) plus small groups. Same fix
  pattern (make the FP-check PC an entry) if they ever matter.
- **The user was playing on the device during some runs** (screen taps to advance screens and actions;
  01 and 02 of the second post-fix round, and likely the 2026-10-05 01 runs). `tools/android-sampler.sh` now logs
  input and `bench-android.sh` warns per state; treat a warned state as void, and ask before benchmarking.

### Gather-pipe stores: stable 30 fps (2026-10-06, late)

**Fresh profile of the fixed build** (simpleperf, 01 and 04, limiter off; `build/android/bench/prof-mtmsr-pgo/`):
the game thread is 100% busy; module ~50%, `libmain.so` ~42%, Adreno driver ~5%. Within it, by callchain
(01 / 04): gather-pipe stores through `HookExternalWrite` → `GPFifo::Write32` and out-of-line
`System::GetPPCState()` ~11% (per store) plus ~4% per 32-byte burst (`UpdateGatherPipe`,
`GatherPipeBursted`); GPU emulation on the CPU thread (`RunGpuOnCpu`, inside `CoreTiming::Advance`) 15–17%,
of it `VertexManagerBase::Flush` 7–8%; `OnICacheInvalidate` 3.5% (all from `dcbf`/`dcbst` lines in guest
`0x80232E00`); `Run()` self 2.6%, a fifth of it the load from the 24 MB lookup table.

Three changes, each A/B'd interleaved (two rounds, limiter off, no presses):
1. **Runtime: inline gather-pipe hook** (`recompcore-staticrecomp-speed.patch`): the store, the pointer bump
   and `UpdateGatherPipe` per burst, with the MMU path moved out of line so the hot entry has a small prologue;
   plus `OnICacheInvalidate` without `ResolveNativeAddress` when there are no RELs. Module unchanged.
   01 0.907/0.895 → 0.971/0.976, 04 0.905/0.909 → 0.960/0.960 (+6–8%).
2. **Module: gather-pipe stores in generated code** (`dolrecomp-gather-pipe.patch`,
   `recompcore-gxruntime-gather-pipe.patch`): a store that misses RAM and hits page `0xCC008000` appends to
   GPFifo's buffer through `CPUState::gather_pipe_ptr_ref` and calls `gather_pipe_flush` once per 32 bytes;
   no indirect call, no state flush/reload per store. Changes most functions' CFG, so the device profile was
   retrained (`pgo/skies-android-gp.profdata`, 0 mismatches). Against (1): 01 0.948/0.965 → 1.053/1.041,
   04 0.975/0.962 → 1.031/1.038.
3. **`DOLRECOMP_LLVM_CPU=cortex-a77`** (`dolrecomp-llvm-cpu.patch`): +1–2% in 01/04, −1% in 02/03; it raises
   the worst second (the binding one is 01/04), so it stays.

**Result** (all four states, two rounds; mean, slowest second in parentheses), gather + PGO → + A77:
01 1.040/1.011 (1.001) → 1.053/1.042 (1.033); 02 1.60/1.59 → 1.59/1.56 (1.521); 03 1.22/1.21 → 1.19/1.21
(1.169); 04 1.032/1.040 → 1.040/1.048 (1.020). **Every second ≥1.00 in every state**, so the parity target
is met, but with ~2% to spare in 01/04 against desktop's 13% (WSL) to 27% (Windows clang). Pictures match
the pre-change shots.
Installed: `build/android/apk/skies-dolphin-debug-gp-pgo-a77.apk` (module `jniLibs-gp-pgouse-a77/`).

Run-to-run spread between sessions is ±2–3% in 01 (gpfast 0.948–0.976 on the same build, full clock, cooler
device on the slower run), so compare builds only within one interleaved A/B.

### Automation and savestates on Android

`jni/Automation.cpp` (in the Android patch) is moderngekko-run's automation directory, using
ModernGekko's own `tools/automation_protocol.cpp` parser. It runs when
`files/Automation/` exists. `tools/drive.py` drives it over adb when `ADB_AUTO` is set (pushes
savestates, pulls screenshots). `tools/android-boot.sh` restarts the game and waits for
`state=running`. Pad input goes through Dolphin's InputOverrider, and clearing it hands control
back to the built-in pad. The **desktop savestates load on arm64**, so all three platforms use the
same four states.

### Savestates need the same disc layout (2026-10-05)

The desktop savestates load on arm64 (x86-64 → arm64 works). But Android first booted the RVZ,
and then every scene transition (the end of the first battle, room changes) raised
"Invalid read/write from 0x…, PC = 0x80005b6c / 0x80109dcc", with either module. The cause is
that desktop boots the **extracted** disc (`sys/` + `files/`), which Dolphin serves as a
DirectoryBlob with its own FST layout. The game caches file offsets in RAM, so a state made on
that layout reads wrong data when the disc underneath is the real image. The fix (and the rule):
**every platform boots the extracted folder**. On Android it's pushed to
`files/Games/SkiesOfArcadiaLegends/` (1.4 GB, 5,557 files, checked by total size and the md5 of
`sys/`), with `ISOPath0` pointing at its `sys/`. The RVZ moved to `files/Discs/`, out of the game
list. A state made from the RVZ would need the RVZ everywhere.

Loading one state over another also raised the alert once while still on the RVZ. That's the
same cause, not a separate bug.

### Building and running

The exact commands are in [handoffs/HANDOFF-android.md](handoffs/HANDOFF-android.md) *Status*. In short:
1. `DOLRECOMP_LLVM_CPU=cortex-a77 DOLRECOMP_LLVM_TARGET=aarch64-unknown-linux-android24 dolrecomp
   --backend=llvm --state-in-memory --targets=aarch64 [--profile-use build/android/pgo/skies-android-gp.profdata] …`
   (~4 min), with all of `patches/` applied. Copy `main.dol` into
   `generated/`, then configure `module-template` with the NDK toolchain file
   (`-DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24`) and link. It links as
   `gGEAE8P_recomp.so`; strip it into `jniLibs-*/arm64-v8a/libgGEAE8P_recomp.so`.
2. PGO: the same with `--profile-generate skies-%m.profraw`, linked with `-fprofile-generate`. The
   hook points `LLVM_PROFILE_FILE` at `files/PGO/`, and the counters are written when the game
   stops (drive.py `stop`). Train on the four states (walking in 03/04), pull, then
   `llvm-profdata-20 merge` (LLVM 20 reads the NDK 21 runtime's raw profiles). Retrain whenever DolRecomp
   changes control flow: functions with a changed CFG silently lose their profile (`hash mismatch`
   warnings in the dolrecomp log).
3. `./gradlew :app:assembleDebug -Pandroid.injected.build.abi=arm64-v8a -PrecompJniLibs=DIR
   -PrecompNativeFlags=-mno-outline-atomics`, then `adb install -r -t`.

### Open questions

- **Parity target (2026-10-06): a stable 30 fps**, i.e. the slowest second ≥1.00× in all four states
  with the limiter off. **Met** (late 2026-10-06, *Gather-pipe stores*): worst second 1.020, but ~2% to spare
  in 01/04 where desktop has 13–29%. Levers left, by size: the per-burst path (`UpdateGatherPipe` →
  `GatherPipeBursted`, ~4%), the `dcbf`/`dcbst` invalidation chain (~3%; skip it when the line holds no
  fallback-JIT block, as Dolphin's JIT does with `GetBlockBitSet()`), a compact lookup table for `Run()`
  (chunks are contiguous and 32-byte aligned: 24 MB → ~350 KB), broader PGO training (the profile is trained
  on the four benchmark states, so the margin elsewhere is unmeasured).
  **Expected size (estimate, 2026-10-06):** the percentages are profile shares, not gains; a fix removes
  part of each. Realistically 2–3% (burst path) + 2–3% (`dcbf`/`dcbst`) + ~0.5% (`Run()` table), i.e.
  ~4–7% together: 01/04's worst second from ~1.02–1.03 to ~1.06–1.09. PGO broadening is ±1–2% on these
  states; its point is the untrained scenes. Desktop would gain less (Windows clang 1–3%, WSL
  llvmpipe-bound). None of it raises the frame rate (*60 fps*); it buys margin and, with the limiter on,
  idle time per frame (heat, battery). **Cheaper first:** play a big battle and the world map on the
  device; if nothing drops below full speed, these are polish. **Done 2026-10-07: nothing dropped** (see
  below), so these levers are polish now.
- **Hand check on the Retroid (user, 2026-10-07, installed `skies-dolphin-debug-gp-pgo-a77.apk`, limiter
  on):** played from the opening through many battles up to the world map, then flew around it: a smooth
  29–30 fps the whole time, no hitches. Memory-card save, then reload after
  restarting: works. Built-in pad: works for everything tried. Audio: sounded okay, not yet checked in depth.
- The remaining interpreter fallbacks: lazy-FPU re-entries (~2%) and an unexplained 1.07M in one WSL
  battle run that took a different course (*Dips*).
- **Vulkan pipeline cache and app kills:** Dolphin writes `cache/Shaders/Vulkan-Pipeline-*.cache` only
  on a clean shutdown. Benchmarks force-stop the app, and players often swipe it away, so pipelines
  compiled since the last clean exit are rebuilt by the driver on the next boot. It didn't cause the
  dips; whether it causes first-use hitches for players is untested (none noticed in the 2026-10-07 hand
  check, but that session may not have followed a swipe-away).
- Stock Dolphin's JIT on the same device, for comparison.
- Left from the hand check: audio in depth (the user will test more) and the game after a
  swipe-away (pipeline cache).

## macOS

Apple Silicon, macOS 13+, since 2026-10-18 (HANDOFF-macos). The same generated code paths as Android
(aarch64, LLVM, state in memory), as Mach-O for `arm64-apple-macosx13.0` with `apple-m1` tuning; the
runtime is ModernGekko's upstream macOS support (Cocoa window `DolphinNoGUI/PlatformMacos.mm`, Metal,
Cubeb, SDL gamepads, the Quartz keyboard) plus `moderngekko-macos.patch` / `recompcore-macos.patch`.
The release is an app bundle (`tools/build-release-macos.sh`): programs and modules in
`Contents/MacOS`, `Sys` and the DOL patches in `Contents/Resources`; data in
`~/Library/Application Support/Skies of Arcadia Legends`. Building and the toolchain without admin
rights: BUILDING *macOS*.

- **Speed (headroom, 2026-10-19, M4 Mac mini, `tools/bench-mac.sh`):** standard protocol, limiter off, fullscreen,
  one warmup + two interleaved rounds. Raw: `build/bench-mac-2026-10-19b.txt`. Mean / worst second:

  | Profile | 01 | 02 battle | 03 | 04 |
  |---|---|---|---|---|
  | 1× | 2.90 / 2.85 | 4.41 / 4.29 | 3.90 / 3.49 | 3.88 / 2.95 |
  | 4× (2560×1440) + 2× MSAA | 2.84 / 2.81 | 4.38 / 4.27 | 4.07 / 3.90 | 4.09 / 4.01 |
  | 4× + 2× MSAA + 60 fps | 2.67 / 2.63 | 4.17 / 4.07 | 3.89 / 3.78 | 3.69 / 3.62 |

  Extras at 1×: 07 red alarm 4.13 (4.05), 10 world map 3.13 (3.04). 1× 03 round 1 (3.72) and 04 round 1 (one
  2.95 second) ran below round 2; state 03's camera varies. Measured on a bench copy of the app with the two fixes below added by hand; the fixes are in
  `recompcore-macos.patch` since (the rebuilt app, `v0.50.0-beta-fix`, measured 2.93 / 4.44 / 4.12 / 4.10× at 1×
  with no hint in its config). The published v0.50 macOS app has neither.
- **Fixed bug: Dolphin's `Sys` wasn't found in the app** (`recompcore-macos.patch`, `FileUtil.cpp`). ModernGekko forces `LINUX_LOCAL_DEV` ON, so `SYSDATA_DIR` is
  `Sys` and the `__APPLE__` branch of `CreateSysDirectoryPath` looks in `<app>.app/Sys/`, not
  `Contents/Resources/Sys`. Without it: no `GameSettings/GEAE8P.ini` (idle skip, `CPUThread = True`), no IPL fonts
  ("Windows-1252 fonts" warning), no DSP coefficients. States from other platforms then fail to load silently
  (OSD only: "Could not find the DSP polyphase resampling coefficients used by the savestate"; the runner boots the
  disc instead). Now `Contents/Resources/Sys`, or `Sys` beside a runner outside a bundle.
- **Fixed bug: a Bluetooth pad cut the speed to a fifth** (`recompcore-macos.patch`, `SDL.cpp`). With the DualSense Edge over Bluetooth (Apple's GameController driver),
  SDL3's `SDL_WaitEvent` on Dolphin's SDL hotplug thread re-polls the joysticks about every 1 ms and holds the joystick
  lock ~72% of the time; the CPU-GPU thread blocks on it in `SDL_UpdateGamepads`. `[SDL_Hints]
  SDL_AUTO_UPDATE_JOYSTICKS = 0` in `Dolphin.ini`: 01 at 1× from 0.60× to 2.90× (Sys fixed in both). The game thread's
  own `SDL_UpdateGamepads` still polls and detects pads; a real-pad play check is still open. `--headless` (no input)
  isn't affected.
- A window behind others, or a run started from a shell while the user works, is throttled: bench fullscreen.
- **Page size:** Apple Silicon has 16 KB pages; nothing here assumes 4 KB (guest RAM is plain heap,
  SMC goes through icbi/dcbf hooks, Android already links with 16 KB max pages).
- **Input:** the keyboard is Dolphin's Quartz source, which reads the HID key state, so the window
  drops game key events itself (no beep, no F1–F12 state shortcuts while Skies owns the keyboard).
  With `[Input] BackgroundInput = False` a lost focus (another app, macOS's Game Overlay on the PS
  button) closes the input gate; `PlatformMacos.mm` logs `[macos] game window lost/gained focus`.
- **Keyboard without Input Monitoring (fixed 2026-10-19):** without the permission
  (`CGPreflightListenEventAccess()` false) `CGEventSourceKeyState` reports no real key, but key code 0
  (`A`) as held whenever the app is in front (probe: W and A held by hand never showed; code 0 always
  did, with focus only). `A` is stick-Left in the Skies layout, so the menu's Left never registered
  (stick X 27) and the prompts flipped back to the keyboard (`SkiesInput` counts any pressed key).
  Without the permission the Quartz keys now come from the window's KeyDown / KeyUp / FlagsChanged
  events (`ciface::Quartz::KeyEvent`, released on focus loss); with it, the HID state as before.
- **Quitting** from the Dock goes through `applicationShouldTerminate:` (stop, cancel), not
  `terminate:` → `exit()`.
- **CI (2026-10-19):** validate.yml's `macos` job (`macos-15`, Xcode 26 selected: 16.4's libc++
  has no `std::jthread`) links both modules from the `macos us` / `macos eu` codegen jobs'
  arm64 Mach-O output and packages the app zip; release.yml ships it with the other packages and
  checks it the same way (no game files; both `.dylib` modules by name). The dev build of `6d54f6f`
  (game patch ws25), made the same way on the Mac, loads the standard states and ran 2.93 / 4.42 /
  3.23 / 2.90× (01–04, 1×, no SDL hint in the config): 01/02 match the table above. 04 ran just as low
  for the v0.50-based build in the same session (back to back: 2.92 / 2.94 vs 2.91 / 2.91), so that's
  spread between sessions, not the dev changes; 03's camera varies (see the table's notes).
  The runner image has Homebrew's zstd, lz4, lzo and libusb, which Dolphin's `USE_SYSTEM_LIBS=AUTO`
  linked by path, so the first CI zips (`127b9a1`, `b62186d`) stopped in dyld on a Mac without
  Homebrew; `87ba4f3` builds Dolphin's own (`USE_SYSTEM_*=OFF`) and the package step refuses any
  `/opt/homebrew` or `/usr/local` link. That CI zip ran 3.05 / 4.63× (01 / 02, 1×) on the M4, and the
  v0.60.0-beta candidate's (`1e7e586`) Field speed 2x held 1.00x at 60 fps.

## Known issues

- **Second code review, section D (2026-10-22):** a cloud review of v0.82's code
  (docs/handoffs/HANDOFF-review.md, section D: 10 Medium, 21 Low; no remote-exploitable issue). Fixed:
  D1 camera control on the European and Japanese discs (ws33, *Camera control*), D6 a use after free in
  the 60 fps replay when its second EFB can't be made, D5 the auto-save deleting a good save before a
  failed write, D7 / D8 the Android launcher's save, backup, driver and Remove actions racing the game,
  D2 texture packs writing outside their folder, D3 / D4 a damaged Dreamcast driver crashing or
  halving the speed (`recompcore-review-d.patch`; D2-D4 on the local branch `review` until pushed),
  D9 the Japanese Continue writing one word past its card work (ws34, *Japanese Continue's card work*).
  Not fixed: D10 (netplay with differing gameplay options; the project doesn't use netplay). The Lows
  D11-D26 and D28-D31 (HANDOFF-review, each item's notes; `recompcore-review-d.patch` for the
  RecompCore ones): Android picker results and the driver guard, the Dreamcast import's names and
  size, bank files read off the audio thread, cheaper and latched 60 Hz copies with a fallback to the
  disc's file, the converters stopped at shutdown, a texture-pack size cap and safe replacement, menu
  clamps and locking, the GPU drain wait (1 s), pinned CI image, signing-script pipefail; D27 a quit
  from the Dock, a logout or a shutdown with the game running is answered "quit" after the runtime has
  shut down (it used to report "refused to quit", aborting a logout; checked on the Mac).

- **macOS: Left didn't register in the in-game settings menu (fixed 2026-10-19):** D-pad and stick
  Left did nothing in the Skies menu while Right / Up / Down worked. Without Input Monitoring macOS
  reports key code 0 (`A`, stick-Left in the Skies layout) as held while the app is in front, so the
  stick read full left and the menu, which acts on new presses only, never saw Left; the same phantom
  key flipped the button prompts to the keyboard. The keys now come from the window's own events then
  (*macOS*, `recompcore-macos.patch`); user-confirmed.

- **Dead controls after closing the in-game menu while holding the stick (fixed 2026-10-18, every
  platform):** the menu's Closing state held back all input until the sticks were centred too, so a
  player walking on as the menu closed got nothing, keyboard included, until letting go. Only the
  menu's buttons wait now (`recompcore-menu-close.patch`).

- **macOS: quitting from the Dock aborted (fixed 2026-10-18):** AppKit's `terminate:` called `exit()`
  with the emulation running and cubeb's logger thread aborted in its destructor;
  `recompcore-macos.patch` stops the platform instead.

- **No usable Vulkan on a PC (found 2026-10-13 in Windows Sandbox, fixed):** Dolphin put up "Failed to
  create Vulkan device", then a white window that never started. Now `Core::VideoBackendFailed()`
  (`recompcore-skies-settings.patch`, set where the EmuThread's video init fails, so closing the
  window during the boot's shader compile doesn't count) makes `Runtime::Run` return
  `RuntimeErrorCode::VideoBackendFailed`; the runner, when the backend came from `config.ini` and was
  Vulkan, saves `backend=OGL` and starts itself again with `--graphics OGL` (Windows: a child it waits
  for, so the launcher still waits for the game; Linux: `exec`). If that fails too, a message box
  says to update the graphics driver. Checked: Windows with `VK_ICD_FILENAMES` pointing at nothing ->
  restart, OpenGL draws the title intro at 30 fps (real GPU); Linux (WSL) the same path; Sandbox has
  neither API, so there it ends at the message box. The branded Windows release is also a GUI program
  now (`WIN32_EXECUTABLE` when `MODERNGEKKO_REQUIRED_DISC_ID` is set): no console window beside the
  launcher or the game; dev builds stay console programs for their scripts.

- **The world map's flight meter and compass keep the old shape after a Picture change made while the
  world map's own menu is open (found 2026-10-17 as "widescreen stretches them", re-diagnosed the same
  night):** in normal play they keep their shape in every picture: at the same picture height the
  meter is ~36 px wide per 480 and the compass ~111x123 in 4:3, 16:9 and 32:9 alike, the same as the
  unpatched game (WSL, `10-worldmap-android.sav`, the unpatched DOL and module beside the `hud` build).
  The Windows shots behind the first report were both taken with the Map / To Bridge / Save menu
  (Start) open: its 16:9 one is right, its 4:3 one has the 16:9 squeeze. Reproduced on WSL: open that
  menu at 16:9, switch Picture to 4:3, and the meter and compass stay squeezed (the other way round:
  stretched) until the menu closes. While it's open those two are replayed from display lists recorded
  before the change (the projection load is baked into the list, *frame_loop.md*), while the 3D and
  the menu panels are recorded again each frame. Transient and cosmetic; a fix would have to rewrite
  the projection in the recorded lists or make the game record them again. The HD textures merge was
  held for the "stretch", which normal play doesn't have.

- **A battle can freeze when the battle speed changes during a round (fixed 2026-10-21, `recompcore-overclock-timebase.patch`):**
  the cause was the emulated-CPU overclock, not the speed change. Every spell and S-move plays one
  pre-mixed stereo stream from the disc, `SOUND/KO00NN_L/R.dsp` (NN = the skill id: 0-35 the spells,
  2.5-13 s; 36-59 the S-moves, 10-33 s; Cutlass Fury is `KO0036`, 11.6 s), and the action's last step
  waits for that stream to end. With the overclock at 1.5 or more (battle speed 3x = 1.5, 4x = 2.0;
  field speed 3x = 2.0) the stream started, went silent within a few seconds and never ended: MusyX's
  `streamInfo` (`0x8032C730`) kept `last` at 0, so the game's refill callback (`fn_802163FC`) was never
  called again, the two channels in `lbl_80314B80` stayed "playing" (state 4) and the battle waited for
  good. The whole DSP mix went quiet with it (audio dumps, `[DSP] DumpAudio`). Measured on the Windows
  test copy with Cutlass Fury (`smove-us.sav`): overclock 1.1 and 1.25 fine, 1.5 and 2.0 froze, also at
  battle speed 1x; battle speed 4x with the overclock off played it; **Dolphin's JIT
  (`MODERNGEKKO_STATICRECOMP=0`) with overclock 2.0 played it**. The static recomp core advanced the
  guest timebase by the module's cycle charges inside a native burst, and those are CPU cycles, which
  run faster than the emulated clock under an overclock: `mftb` ran ahead, then jumped back at the next
  `SyncIn` (which takes Dolphin's `GetFakeTimeBase`). The patch advances it by charge / overclock, as
  Dolphin's `GetTicks()` counts it. With it, 3x and 4x play the whole stream and the battle goes on (and
  1x is unchanged: the animation ends 10.0 s after the stream starts, 10.1 s before). The user's frozen
  state from the Retroid (`18-retroid-battle-softlock.sav`) is this: `KO0036` stuck in both channels,
  Antonio's special (unit 4, type 0xC) waiting. Such a state stays frozen (the stall is in it). Retroid
  Pocket 5 (test app, the Japanese game, `jp-03-worldmap-battle-start.sav`, ws27 APK): with overclock
  2.0 forced at 1x, Cutlass Fury's stream played its ~11 s and the battle went on. Tools:
  `sndrun.py` / `stall.py` style runs (load, commands, `[DSP] DumpAudio` on, game state logged with the
  retrace count).
- **Before (2026-10-11):** the user
  hit it on the Retroid (Antonio fight, several Z taps, the 2x step-down kicking in): the round stops
  with a damage number on screen while the game keeps running at 60 fps. Reproduced on Windows from the
  user's state at the fight's start (`build/savestates/19-retroid-antonio-start.sav`; the frozen one is
  `18-retroid-battle-softlock.sav`): 5 of 12 first rounds froze with 5 Z taps during the round, with
  Invincible on or off; none froze with the speed left alone (single trials at 1x / 2x / 4x). The freeze
  is in the game's state (stays after a reload; still frozen after a minute); every battle task stays on
  its step; no DVD read is pending (`executing` 0, empty queue). Not separated yet: the emulated-CPU
  overclock switched at 3x / 4x (and by the step-down) vs the 60 fps interpolation stood down / back up
  (a batch to split them was cancelled by the user). Advice meanwhile: change speed while choosing
  commands. Tools: `trial.sh` / `trial2.sh` style runs (load, Z taps, two captures 8 s apart: a frozen
  round changes ~2 % of the picture, a live one 17-44 %).

- **"Compiling shaders" at full length on every Android start (fixed 2026-10-11, verified on the
  Retroid: pipeline cache 48 bytes -> 3.1 MB after a minute of play, shorter compile at the next start):** the game waits at boot until every ubershader pipeline is built
  (`WaitForShadersBeforeStarting`). The shader code is cached across runs (`Vulkan-uber-*`,
  `-specialized-*` in the app's external cache dir, `Android/data/<app>/cache/Shaders`), but the
  expensive driver step only skips when `Vulkan-Pipeline-*.cache` has data, and Dolphin wrote that file
  only at a clean emulation shutdown. Leaving an Android app with Home / the recents list kills the
  process without one, and Skies Recomp hides Dolphin's Exit, so its pipeline cache stayed a 48-byte
  header (the Dolphin Debug app had 6.6 MB from one clean exit). `recompcore-vulkan-pipeline-cache-save.patch`
  saves it in the background at most every 30 s while new pipelines appear, via a temp file + rename.
  Windows check: saved 30 s into a run, intact after `taskkill /F`. A new cache set (`2C4DFF40` ->
  `2C4DFF41`) also appears when settings that change the shader host config (e.g. MSAA) change; that
  set compiles once.

- **Only the face buttons worked in Skies Recomp (Android), fixed 2026-10-11:** the user found the
  in-game menu ignoring the D-pad and L / R. A temporary log in `SkiesMenu::FilterOpen` showed the pad
  reaching the menu with buttons only: `stick=128,128`, `trig=0,0` and no D-pad bits whatever was
  pressed (so the game itself had no stick either). The app's `Config/GCPadNew.ini` was Dolphin's
  bundled `Sys/Profiles/GCPad/SDL Gamepad.ini` mapping (Z changed to `Button L1`): `Left Y+`, `Pad N`,
  `Trigger L`, names Android's backend doesn't have; only `Button A` / `Start` style names matched. The
  Dolphin Debug app, mapped by hand in Dolphin's controller screen long ago, uses `Axis 1-` etc. and
  worked. The branded app hides that screen, so a player could never fix it. Fix: `SkiesNative.
  ensurePadProfile()` (`jni/SkiesImport.cpp`, called from `SkiesLauncherActivity.installDefaults` at
  every start) rewrites the stick, C-stick, triggers, D-pad and rumble of `[GCPad1]` to the standard
  Android layout of the first gamepad with an `Axis 0+` input when the main stick isn't on `Axis`
  names; buttons and a player's own Android mapping are left alone; Dolphin reloads the file at every
  boot (`Core.cpp`: `Pad::LoadConfig`). The user's device was repaired by hand first (the debug app's
  mapping copied in; the old file kept as `Config/GCPadNew.ini.sdl-backup`). **Automatic fix verified
  on the Retroid (21:12):** with the SDL profile put back, starting the app logged "mapped the stick,
  D-pad and triggers of Android/1/Retroid Pocket Controller" and wrote the same mapping as the working
  one (buttons, Z on L1 kept); the next start left the file byte-identical and logged nothing. Still
  open: a second Android controller (only the Retroid's axis ids are known to match the standard
  layout).
  Desktop is unaffected (SDL names are right for SDL).

- **The Baltor savestates aren't from the story** (2026-10-11): no save of ours reaches the first ship
  battle, so `tools/shipbattle-state-win.sh` makes them from the world map with the debug build
  (`patches/game/shipdebug.json`): it gives the party the Little Jack and its two cannons and changes the
  map to `me500a`. The battle itself plays and ends normally; the party is Vyse and Aika (the story's
  lead-up isn't played). Other ship battles: untested.
- **WSL "hangs compiling shaders" can be the whole VM out of memory** (2026-10-11): the ws16 smoke test
  died at shader compile three times. The kernel log (`journalctl -k | grep -i oom`) showed a global OOM,
  not the run's `MemoryMax`: the game had ~2.2 GB while idle Gradle / Kotlin daemons (~2.6 GB), a dozen
  Claude sessions and RAM-backed `/tmp` (~2.9 GB) filled the 12 GB VM, and one run started without
  `tools/play.sh`'s cap took WSL down. Same build after a restart: boots in ~5 s, Baltor 1.00x, peak
  ~4.2 GB. Fixes: `tools/android-ws-build.sh` stops the daemons; `.wslconfig` now `memory=16GB`. Always
  start the runtime through `tools/play.sh`.
- **Save states from ws12-ws14 can hang in ws15 and later** (2026-10-11): those builds called patch
  code (`qol_retrace_count`) from the end-of-frame loop that polls the retrace count, where the game
  spends much of each frame, so a state made then holds a PC in patch code that later builds moved
  (Baltor state 17b hung in ws15: PC `0x80255720`). ws15 moves that work to `qol_frame`, run once
  before the loop, so new states don't; `tools/statecheck.py STATE DOL` tells whether a state is caught
  in patch code. Memory-card saves are unaffected. The player's Retroid states (ws13) may be hit.
- **Invincible shows late in the status box** (2026-10-11, user report): turned on while commands are
  chosen, HP / MP are full in memory at once but the party status box keeps the old numbers until the
  round plays out (the Spirit gauge fills at once). What that box is drawn from during command input
  isn't known; writing the battle HUD panel didn't change it. Knocked-out members aren't revived.
- **A new release build refuses an older patched game root** (2026-10-11): the release runner only
  patches the original `main.dol`; a game root made by an earlier release (the user's
  `%LOCALAPPDATA%\SkiesRecomp\games\GEAE8P`, ws10) fails the SHA-256 check, so the disc has to be
  picked again (Select Disc refreshes `sys/` and keeps saves, states and settings). Since 0.10 the
  launcher and the runner tell the player so (*Launcher and release*). Doing it without the disc would
  need the old patches' outputs as accepted inputs (or a kept original, as on Android).
- **An auto-save costs a hitch of a few frames on the Retroid** (2026-10-11): one second at 0.94-0.99x
  when it saves (the state is captured on the CPU thread; compression and writing run on Dolphin's
  own thread). None measurable on Windows. Every 10 minutes by default.
- **Dolphin's save / load messages show file names** ("Saved State to state-skies-slot1.sav"): fine
  for now, a shipped build would want its own wording.
- **Not checked yet**: the real controller's L3 / R3 press (automation can't press it), MP refill after
  casting magic with Invincible on, music at 2x-4x battle speed by ear, the menu on Windows after the
  2026-10-11 pulse fix (built, not looked at: Dolphin screenshots leave out the OSD), the launcher's
  Continue button clicked by hand (its list and `--load-state` were checked; for a card save, the
  launcher writing `AutoLoadCard` is unclicked, the game-side load was checked with the key set by
  hand), the card auto-load on Android, auto-save in real play.
- **The Retroid tops out near 2.5x battle speed** (2026-10-11): 3x / 4x ran at 0.85x / 0.65x emulation, so
  the runtime steps such a battle down to 2x (message on screen); every battle starts at the setting again.
- **A lone Z tap reaches the game late**: on release where the camera can move, 150 ms after the press
  elsewhere (held back to catch Z+Start and the camera hold). Nothing in the game needs a held Z.
- (Fixed 2026-10-11: the branded app imports the disc itself, *Launcher and release*.) **Android had no disc
  import**: the patched DOL was pushed with adb and had to match the APK's module (handoffs/HANDOFF-android.md). Skies of Arcadia Legends is a single disc (`GEAE8P`, disc number 0, ~1.4 GB of
  files; the Dreamcast original had two), so one disc ID is all the launcher needs.
- (Closed 2026-10-11: the user sees it no more.) **Deck cloud billboard glitch on the Retroid** (user's phone video, 2026-10-10): a cloud billboard over
  the cannons drawn white, black or purple for a frame with interpolation on. Never reproduced or scored.
  Probably the TMEM-load cause fixed late 2026-10-10 (those textures are preloaded too).

- **Picture in the top-left two-thirds of the window** (seen once, 2026-10-07): a test window started
  from WSL with `cmd.exe` (Windows display scaling 150%) drew the game at 1536 x 871 inside a
  2304 x 1306 window, the rest black; the window reports DPI-unaware. The user's earlier window filled
  normally. Cause not looked into (window resize or maximise with a DPI-unaware window, most likely).
  Probably gone since `b9f0157` (*The game window: fullscreen and display scaling*): 1536/2304 and
  871/1306 are both 1/1.5, the 150% scale, and the runner is per-monitor DPI aware now. Not retried with
  the 2026-10-07 setup.

- `--headless` aborts in Dolphin's ImGui OSD ("No current context"): a ModernGekko bug, not the game.
- `--graphics OGL` needs the EGL build and then renders wrong (see *Rendering on WSL*).
- **Windows exit crashes: both fixed (2026-10-07).** Two unrelated bugs, found with a PDB build of the
  runtime (`lib\ModernGekko\build-pdb`, `/Z7` + `/DEBUG`) and `lldb.exe` from the clang tarball (it needs
  `python310.dll`: the Python 3.10 embeddable zip unpacked at `C:\dev\python310-embed`, on `PATH` with
  `PYTHONHOME` set).
  - **LLVM module: `abort()` (`0xc0000409`) before the shutdown line.** Not random: every stop in states
    01/02 crashed (12 of 12), none in 03/04 (0 of 11). The CPU-GPU thread throws from `timing.join()` in
    `CPUManager::Run` (`CPU.cpp:216`, the Play Time Tracker thread): `_Thrd_join` fails, the
    `std::system_error` leaves the thread function, `std::terminate`. MSVC keeps that `std::thread` in
    **xmm6** across the CPU loop, and xmm6 came back as garbage (a heap pointer and a return address). A
    canary trampoline around `m_module->dispatch` (xmm6–15 preset, checked after) showed every native
    entry that escapes through DolRecomp's `eh.sjlj` path returns xmm6–15 shuffled. Cause: the entry
    wrapper stores `llvm.frameaddress(0)` in the jump buffer, `eh.sjlj.longjmp` reloads `rbp` from it, but
    a Win64 frame pointer sits at an offset into the frame (`rbp = rsp + 0x80`), so the epilogue reloaded
    the callee-saved XMM registers 0x80 too low. Fixed in `patches/dolrecomp-win64-sjlj-frame.patch`
    (store `rbp` itself on Windows targets). Linux x86-64 objects are byte-identical; SysV has no
    callee-saved XMM registers, which is why WSL never crashed. After the fix: 40 of 40 clean
    automation stops (20 each in 01 and 02), plus the focused X-button and Alt+F4 runs below.
    `patches/dolrecomp-coff-uwtable.patch` adds `.pdata`/`.xdata` to the module so debuggers and dumps
    can walk module frames; it's not part of the fix (the crash reproduced with it).
  - **Window callback access violation (`0xc0000005` at `moderngekko-run.exe+0x68392`, then
    `0xc000041d`): fixed by
    `recompcore-win32-exit.patch`.** It happens whenever the window has keyboard focus at exit:
    `DestroyWindow` sends `WM_KILLFOCUS`, whose handler wrote Config after the runtime had shut it down.
    Earlier tests never reproduced it because a process launched from WSL can't take the foreground on its
    own; `tools/focus-win.ps1` forces it (`AttachThreadInput` + `SetForegroundWindow`) and checks
    `GetForegroundWindow`. Focused A/B, state 01, `tools/exit-test-win.sh` with `STOP=close|altf4` /
    `FOCUS=1`: the pre-fix runtime (`moderngekko-run-prefocus.exe`) crashed **30 of 30** (X button, Alt+F4
    and automation stop alike, exit `0xc000041d`); the current runtime exited cleanly **45 of 45** with
    the clang + PGO module and **20 of 20** with the fixed LLVM module. Since 2026-10-07 the window
    procedure also hands every message to `DefWindowProc` once the window is detached (the F1/F11/Esc/
    Alt+Enter/`WM_CLOSE` handlers dereferenced the platform unchecked).
  - **Memory-card saves survive an exit on Windows** (2026-10-07). From `build/savestates/06-save-menu.sav`
    (made on WSL from the user's Android save: the save-point menu on the Valuan battleship), the LLVM + PGO
    module saved to file #02 (`A`, `Down`, `A`, `A`): Dolphin wrote `8P-GEAE-SA_LEGENDS.001.gci` at once, the
    run exited with code 0, and a WSL cold boot with a copy of that card listed and loaded file #02. File #01
    stayed byte-identical. `05-save-point.sav` is the same room before stepping onto the save point.
- ~~**Windows cold boot hangs**~~ (2026-10-06; **fixed 2026-10-07**): not a hang and not cold-boot specific.
  Every launch spent ~27 s on Windows (~10 s on WSL) at ~28 MB, one core busy, before the first log line,
  with or without `--load-state`: `InspectGame` read and SHA-256-hashed all of `files/` (1.4 GB), once in
  `moderngekko-run` and again in `Runtime::Create`, for an assets hash only a pinned release build reads.
  The 2026-10-06 run was killed at ~30 s, just before the window would have opened. `moderngekko-local.patch`
  skips the hash there: time to `state=running` ~28 s → ~2 s on Windows, 12.4 s → 2.7 s on WSL; a Windows
  cold boot reaches the opening text crawl at 30 fps, full speed, exit 0. The `build-pdb` debug runtime
  predates the fix. Before rebuilding: `moderngekko-run-1007-prehash(.exe)` in both build dirs.
- `moderngekko-port` doesn't pass a symbol map to DolRecomp, so the build doesn't use the decomp's
  names yet. `tools/export_map.py` writes them in DolRecomp's `--map` format for when it does (or for
  running DolRecomp by hand).

## Hook points

See `../SkiesOfArcadiaLegends-Decomp/docs/ninja.md`, `docs/widescreen.md`, `docs/frame_loop.md`,
`docs/camera.md` and `docs/qol_cheats.md`: projection setup (`njSetAspect`, `njSetScreen`, `njClipZ`) and the
2D layer for widescreen (done, *Widescreen*), the frame loop for 60 fps (done in the video code, *60 fps*; Ninja
transforms vertices on the CPU, so game-side matrix tagging wouldn't catch the motion), the camera manager's
view build for camera control (done, `camera.c`), the encounter roll, battle rewards, end-of-frame waits and
damage function for the options (done, `qol.c`), and MusyX's `hw*` layer for native audio. Game patches go in
`patches/game/` (C built into the DOL by `tools/gamepatch.py`); `patches/game/trace2d.c` records which code draws
2D, for finding the next thing to fix.
