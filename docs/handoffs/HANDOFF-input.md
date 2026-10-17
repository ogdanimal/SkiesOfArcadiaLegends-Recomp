# Handoff: keyboard, remapping and controller handling (input)

**Status (2026-10-17): shipped in v0.10.0-beta** (release commit `61ac70e`; on `dev` as `9e32e7f`
keyboard + remapping and `e1fdaf5` the Retroid fixes). Worked on in the worktree
`~/projects/skies-input` (branch `input`); other sessions shared the main checkout (DC music / EU
disc, release), see *Coordination*. Player text: README / release/README *Controls*; developer
summary: NOTES *Keyboard and button mapping* (running and testing: *Controller and keyboard*). Open: only checks that need hardware (*Not checked yet*).

## What it does

- **The keyboard plays** on Windows and Linux, alongside a controller or on its own. Default layout
  (`Core/SkiesBindings.cpp`): W A S D stick, Space / Left Shift = A / B, F / R = X / Y, Tab = Z,
  Q / E = L / R, Enter = Start, I J K L = C-stick, arrows = D-pad, **Esc = the settings menu**.
- **Every button and key can be changed**: launcher **Settings → Controls** (table: GameCube input,
  controller button, keyboard key; click, then press; right-click / Delete / pad X unbinds; two
  resets) and the in-game menu **Controls → Button mapping** (same table; A changes, X clears, B back;
  uses Dolphin's `InputDetector`, so it works on Android too). The controller's A and B can be
  changed but not cleared (both places): without them a controller, on Android often the only
  input, can't work the menu any more (found on the Retroid).
- **Any controller works without setup** (desktop): controller 1 moves to an SDL gamepad when its own
  device isn't connected, or when another gamepad is pressed while it has been idle 750 ms
  (`SkiesInput::Follow`, OSD "Controller: <name>"). Meant for the player report "controller doesn't
  work through Steam" (cause not confirmed; a profile naming a device the game doesn't see is one way
  it happens). Android keeps the profile's device (`ensurePadProfile` sets it up).
- **Controller 1 is "Always Connected"** (Dolphin's option, set by the runtime and in new launcher
  profiles): without it Dolphin reports no controller at all, keyboard included, while the profile's
  device is unplugged. This was why the keyboard did nothing in the first test.
- **Hotkeys** in `GCPadNew.ini [SkiesHotkeys]`: `Menu` (controller Back / Android Select, Esc) and
  `Invincible` (L3 / R3; was hard-wired to the device's stick clicks). `Version = 1` once the
  keyboard layout has been added (a profile from 0.9.x gets it once, at the next launcher or game
  start; a key the player unbinds stays unbound). Android without a keyboard stays at `Version = 0`
  until one is connected.
- The menu's hints name the keyboard's keys while the keyboard is the one in use; the Button mapping
  row's description names Esc only when a keyboard is there (`SkiesInput::HasKeyboard`).
- The game window's **developer keys are off in Skies** (`SkiesInput::OwnsKeyboard`): Windows Space
  fast-forward, F1 save state, Escape quits / leaves fullscreen; X11 F1-F12 states, F10 pause,
  Ctrl+Esc quit. F11 and Alt+Enter switch fullscreen on both.
- The launcher no longer refuses to start without a controller.
- **Old profiles upgrade once** (launcher `Upgrade`, runtime `UpdateKeyboard`): a 0.9.x launcher
  profile gets the keyboard layout and the hotkeys; Dolphin's own keyboard profile (the keyboard as
  `Device`, bare keys) keeps its keys (they now name the keyboard) and gets the controller layout,
  and the launcher shows it as no controller rather than "Virtual core pointer (not connected)".

## Where

- `patches/recompcore-input.patch` (last in `recompcore.series`, after `skies-dcmusic-eu`): new
  `Core/SkiesBindings.{h,cpp}`, `Core/SkiesInput.{h,cpp}`; `Core/CMakeLists.txt`;
  `VideoCommon/SkiesMenu.cpp`; `DolphinNoGUI/PlatformWin32.cpp`, `PlatformX11.cpp`.
- `patches/moderngekko-local.patch`: new `tools/launcher_controls.{hpp,cpp}`; `tools/moderngekko_launcher.cpp`
  (controller section, `select_controller` / `ensure_controller`, the event hook); `CMakeLists.txt`.
- Docs: README / release/README *Controls*, NOTES *Keyboard and button mapping* and *Controller and
  keyboard* (and *Controls the recomp adds*, *The in-game menu*, *Launcher and release*), `patches/README.md` rows, HANDOFF-settings /
  -android / -desktop status lines.
- Bindings format: each GameCube input's expression is `<controller terms> | <keyboard terms> | <other>`;
  controller terms are unqualified (they follow controller 1's device), keyboard terms name the
  keyboard (`` `DInput/0/Keyboard Mouse:SPACE` ``, `` `XInput2/0/Virtual core pointer:space` ``, an
  Android keyboard's device). Hand-written terms are kept. Triggers: a controller binding also goes to
  `Triggers/L-Analog` when it's an axis. Bare control names are parsed as Dolphin's lexer does (a
  letter, then letters, digits, `_`; not a function name such as `minus`).
- Key names: `KeyNames()` holds each key's SDL3 scancode name (the launcher), DirectInput, X11 keysym
  and Android name, all checked against Dolphin's tables (`NamedKeys.h`, X11 keysyms, Android
  `KEYCODE_NAMES`, `SDLGamepad.h`); Android's forward delete is "Delete".

## Checked

**WSL** (release build `lib/ModernGekko/build-release-linux`; ws20 before the merge, ws22 after):
- Launcher: Controls table with the defaults; a fresh user dir gets the default profile (keyboard
  only, no `Device`); rebinding A's key to J by a real key press wrote
  `` `XInput2/0/Virtual core pointer:J` ``; an old X11 keyboard profile (bare `X`, `Return`,
  `Shift_L`) shown as no controller, keys kept and qualified, both layouts and the hotkeys added.
- Game (state 05): `[skies] controller 1: ; keyboard: XInput2/0/Virtual core pointer`; D walks Vyse;
  Esc opens and closes the menu; keyboard hints; Button mapping rebinds by key press. After the merge
  with the DC-music / EU work (ws22 USA game and module): walking, Esc menu, Sound and Controls tabs.

**Windows** (`C:\dev\SkiesInput`, a copy of the worktree; real key presses through `SendInput` with
scan codes, only while the game window is in the foreground; a 0.9.x profile naming the missing pad
`SDL/0/Xbox Wireless Controller`, the suspected Steam case):
- Before the merge (`build\input-test.ps1`, ws20): keyboard layout added with the DirectInput names,
  `Always Connected = True`; D / A walk; Esc opens (no quit) and closes the menu; Space (1.5 s)
  doesn't fast-forward.
- On the release code (`affabd9`, `REGIONS=us eu` configure, ws22 pins; `build\input-test22.ps1`,
  `build\game\GEAE8P-ws22` = ws20's files + the ws22 DOL, module
  `C:\dev\SkiesRecomp\build\modules-llvm-pgo-ws22b`, user dir `build\user-input22`): the same, plus E
  to the Controls tab and Space into Button mapping (DirectInput names); 59.9 fps.

**Retroid** (debug test app `org.dolphinemu.dolphinemu.debug` only, an APK built from dev around
projects-48's ws22 module: `NAME=input JNILIBS=<main>/build/android/jniLibs-ws22 tools/android-ws-build.sh`;
state 05; afterwards 48's APK and the original `GCPadNew.ini` were put back, the release app untouched):
- First start: `Options/Always Connected = True`, `[SkiesHotkeys] Menu = Select`, `Invincible =
  Button L3 | Button R3`, `Version = 0`. The stick walks Vyse; Select opens / closes the menu; Button
  mapping shows the Android names as labels (`Axis 1-` = Left stick up); A → Y rebinds and saves, Y then
  works as A; reset controller buttons (Z back to R1); B back; L3 toggles Invincible.
- Found there and fixed (rebuilt and re-checked on the device): a cleared controller A left no way to
  work the menu; the mapping row had Left / Right arrows; its description named Esc without a keyboard.
  "A: reset" on the reset rows was built, not looked at.

**CI:** validate runs 39058090044 (`028a2aa`), 39067299057 (`1477216`) and 39068695991 (`61ac70e`)
passed on Windows, Linux and Android; the release run 39070491053 published v0.10.0-beta.

## Not checked yet (needs hardware)

- A physical gamepad on desktop: button capture in the launcher and in game, controller 1 moving to
  the pad pressed (two pads), the "Controller: <name>" message. WSL can't see USB pads; on Windows any
  Xbox / PlayStation pad will do.
- Steam Input (the original report): the game started from Steam with a controller through Steam's
  virtual pad.
- An Android keyboard (USB / Bluetooth): it should get the keyboard layout on connect (`FindKeyboard`
  takes an Android device with letter keys, not "Virtual"). The Retroid has none.
- Known limit, not a bug: the Android launcher's Settings → Controls has no mapping table (the in-game
  page covers it); `ensurePadProfile` (projects-ac's) is unchanged.

## Testing notes

- `drive.py` automation feeds the GameCube pad directly and bypasses the bindings: test bindings with
  real events.
- WSLg: SDL ignores `xdotool --window` key events (synthetic); real key presses (`xdotool keydown`)
  go to the focused window, so check `xdotool getwindowfocus` before each one: other sessions' game
  windows take focus. No Xvfb installed.
- Windows: `C:\dev\SkiesInput` is a copy of the worktree (Externals robocopied from
  `C:\dev\SkiesRecomp`); sync with rsync (excluding `build/`, `build-*/`, `.git`, Externals), then
  `set REGIONS=us eu && tools\build-release-win.bat configure` and `build`. The scripts check the
  foreground window before every key, so a key never lands in another window.
- Retroid: `adb shell sendevent /dev/input/event8 TYPE CODE VALUE` + `sendevent ... 0 0 0` (the "Retroid
  Pocket Controller", writable by the shell's `input` group): BTN_SOUTH 304, EAST 305, NORTH 307, WEST
  308, TL2 / TR2 312 / 313, SELECT 314, START 315, THUMBL / THUMBR 317 / 318, D-pad 544-547, `ABS_X` 0,
  `ABS_Y` 1, `ABS_HAT0X` 16, `ABS_HAT0Y` 17. adb's `input keyevent` comes from the "Virtual" device,
  which the game treats as neither controller nor keyboard. Menu taps need ~0.2 s holds; a long hold
  on the D-pad wraps past the end of the mapping list. Device use: claim it with the other sessions
  first ("claiming Retroid for X", wait for no objection, "Retroid free" after); back up
  (`adb pull -a`) before touching the release app.
- A release build refuses a game that isn't the pinned ws22 DOL: reconfigure an older build folder
  with `REGIONS="us eu" tools/build-release-linux.sh configure` and test with
  `build/game/GEAE8P-ws22` + `build/us-gen22/module-linux/gGEAE8P_recomp.so` (main checkout).
- Release builds log NOTICE only; `Config/Logger.ini` with `WriteToFile = True` and `CORE = True`
  puts `[skies] ...` lines in `Logs/dolphin.log`.
- The worktree's submodules and Dolphin Externals (with their nested submodules: `linkernsbypass`,
  `sanitizers-cmake`, `googletest`) are git worktrees of the main checkout's; symlinks break
  `git status` and Android's CMake.
- `pkill -f PATTERN` must not have PATTERN elsewhere in the same command line (it kills the shell).

## Coordination (history, 2026-10-17)

- Merged on top of projects-48's DC-music / EU commit `c131c82`: `input` last in the series, both
  patches regenerated (`moderngekko-local.patch` = their launcher hunks plus mine), six
  `SkiesMenu.cpp` hunks placed by hand next to their Music / Language rows and `SkiesMusic::Poll`.
- projects-ac (release) owns `ensurePadProfile` and the Android Saves tab; README *Controls* was
  handed to this work after `b527364`.
