# Handoff: keyboard, remapping and controller handling (input)

Branch `input`, worktree `~/projects/skies-input`, committed locally on top of `dev` (not pushed:
the user decides when; the next release is one bigger release). Other sessions work in the main
checkout at the same time (DC music / EU disc, release); see *Coordination*.

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
- **Any controller works without setup**: controller 1 moves to an SDL gamepad when its own device
  isn't connected, or when another gamepad is pressed while it has been idle 750 ms (desktop only;
  `SkiesInput::Follow`). Meant for the player report "controller doesn't work through Steam" (cause
  not confirmed; a profile naming a device the game doesn't see is one way it happens).
- **Controller 1 is "Always Connected"** (Dolphin's option, set by the runtime and in new launcher
  profiles): without it Dolphin reports no controller at all, keyboard included, while the profile's
  device is unplugged. This was why the keyboard did nothing in the first test.
- **Hotkeys** in `GCPadNew.ini [SkiesHotkeys]`: `Menu` (controller Back / Android Select, Esc) and
  `Invincible` (L3 / R3; was hard-wired to the device's stick clicks). `Version = 1` once the
  keyboard layout has been added (a profile from 0.9.x gets it once, at the next launcher or game
  start; a key the player unbinds stays unbound).
- The menu's hints name the keyboard's keys while the keyboard is the one in use.
- The game window's **developer keys are off in Skies** (`SkiesInput::OwnsKeyboard`): Windows Space
  fast-forward, F1 save state, Escape quits / leaves fullscreen; X11 F1-F12 states, F10 pause,
  Ctrl+Esc quit. F11 and Alt+Enter switch fullscreen on both.
- The launcher no longer refuses to start without a controller.
- **Old profiles upgrade once** (launcher `Upgrade`, runtime `UpdateKeyboard`): a 0.9.x launcher
  profile gets the keyboard layout and the hotkeys; Dolphin's own keyboard profile (the keyboard as
  `Device`, bare keys) keeps its keys (they now name the keyboard) and gets the controller layout,
  and the launcher shows it as no controller rather than "Virtual core pointer (not connected)".

## Where

- `patches/recompcore-input.patch` (last in `recompcore.series`): new `Core/SkiesBindings.{h,cpp}`,
  `Core/SkiesInput.{h,cpp}`; `Core/CMakeLists.txt`; `VideoCommon/SkiesMenu.cpp`;
  `DolphinNoGUI/PlatformWin32.cpp`, `PlatformX11.cpp`.
- `patches/moderngekko-local.patch`: new `tools/launcher_controls.{hpp,cpp}`; `tools/moderngekko_launcher.cpp`
  (controller section, `select_controller` / `ensure_controller`, the event hook); `CMakeLists.txt`.
- `README.md` / `release/README.md` *Controls*; `patches/README.md` rows.
- Bindings format: each GameCube input's expression is `<controller terms> | <keyboard terms> | <other>`;
  controller terms are unqualified (they follow controller 1's device), keyboard terms name the
  keyboard (`` `DInput/0/Keyboard Mouse:SPACE` ``, `` `XInput2/0/Virtual core pointer:space` ``, an
  Android keyboard's device). Hand-written terms are kept. Triggers: a controller binding also goes to
  `Triggers/L-Analog` when it's an axis.

## Checked (2026-10-17, WSL, release build `lib/ModernGekko/build-release-linux`, ws20 game, state 05)

- Launcher: Controls tab table with the defaults; a fresh user dir gets the default profile (keyboard
  only, no `Device`); rebinding A's key to J by a real key press (XTEST) wrote
  `` `XInput2/0/Virtual core pointer:J` ``.
- Game: `[skies] controller 1: ; keyboard: XInput2/0/Virtual core pointer` at start; D walks Vyse
  right (after the Always Connected fix; background input off, window focused); Esc opens and closes
  the menu; keyboard hints (`Q / E: tab ... J: change ... Left Shift or Esc: close`); Controls →
  Button mapping draws, rebinding A's key back to Space by a key press saved it.
- An old X11 keyboard profile (bare `X`, `Return`, `Shift_L`) through the game and the launcher: the
  keys kept and qualified, the rest of both layouts added, the hotkeys, `Version = 1`.
- Every key name in `KeyNames()` against Dolphin's tables (DirectInput `NamedKeys.h`, X11 keysyms,
  Android `KEYCODE_NAMES`, SDL3 scancode names) and the controller names against `SDLGamepad.h`;
  fixed Android's Delete (Dolphin calls `KEYCODE_FORWARD_DEL` "Delete"). Bare control names are
  parsed as Dolphin's lexer does (a letter, then letters, digits, `_`; not a function name such as
  `minus`): `Shift_L` written bare was missed before.
- `tools/check-patches.sh`: "patches reproduce the working copies". The worktree's submodules and
  Dolphin Externals are now git worktrees of the main checkout's (they were symlinks, which
  `git status` refuses).

## Checked (2026-10-17, Windows, `C:\dev\SkiesInput` release `skies-run.exe`, ws20, state 05)

- `build\input-test.ps1`: real key presses (scan codes through `SendInput`, only while the game
  window is in the foreground), screenshots in `build\input-test\`. User dir `build\user-input`
  (a copy of `user-bt` with a 0.9.x launcher profile naming `SDL/0/Xbox Wireless Controller`, not
  connected: the suspected Steam case).
- Log: `keyboard layout added for DInput/0/Keyboard Mouse`; the saved profile has the DirectInput
  names (`SPACE`, `LSHIFT`, `RETURN`, `ESCAPE`, ...) and `Always Connected = True`.
- D walks Vyse, A walks back; Esc opens the menu (the game no longer quits), Esc closes it; Space
  (1.5 s) doesn't fast-forward (59.9 fps); the game still running at the end.

## Checked (2026-10-17, Retroid, debug test app)

- `org.dolphinemu.dolphinemu.debug` only (projects-48's ws22 module in an APK built from dev:
  `NAME=input JNILIBS=<main>/build/android/jniLibs-ws22 tools/android-ws-build.sh`), state 05;
  the release app untouched. Afterwards 48's APK and the original `GCPadNew.ini` were put back.
- Real controller input: `adb shell sendevent /dev/input/event8 ...` (the "Retroid Pocket
  Controller", writable by the shell's `input` group; BTN_SOUTH 304, EAST 305, NORTH 307, WEST 308,
  TR2 313, SELECT 314, THUMBL 317, ABS_X 0, ABS_HAT0Y 17). `drive.py` automation would bypass the
  bindings.
- First start: `Options/Always Connected = True`, `[SkiesHotkeys] Menu = Select`, `Invincible =
  Button L3 | Button R3`, `Version = 0` (no keyboard). The stick walks Vyse; Select opens / closes the
  menu; Button mapping shows the Android names as labels (`Axis 1-` = Left stick up); A → Y rebinds
  and saves, Y then works as A; reset controller buttons (Z back to R1); B back; L3 toggles Invincible.
- Fixed after it (rebuilt and re-checked there): the controller's A / B no longer clearable, no
  Left / Right arrows on the Button mapping row, its description names Esc only with a keyboard,
  "A: reset" on the reset rows (this last one built, not looked at).

## Not checked yet

- Controller capture in game and in the launcher (no gamepad on WSL), the follow logic with real
  pads, Steam Input.
- Android keyboards (the Retroid has none; `FindKeyboard` ignores the "Virtual" device that adb's
  `input keyevent` uses).
- The Android launcher's Settings → Controls has no mapping table (the in-game page covers it);
  `ensurePadProfile` (projects-ac's) untouched.

## Testing notes

- WSLg: SDL ignores `xdotool --window` key events (synthetic); real key presses (`xdotool keydown`)
  go to the focused window, so check `xdotool getwindowfocus` before each one: other sessions' game
  windows take focus (scratchpad `key.sh` did this). No Xvfb installed. Windows: `C:\dev\SkiesInput`
  is a copy of this worktree (Externals from `C:\dev\SkiesRecomp`); copy changed files, then
  `tools\build-release-win.bat build`.
- Release builds log NOTICE only; `Config/Logger.ini` with `WriteToFile = True` and `CORE = True`
  puts `[skies] ...` lines in `Logs/dolphin.log`.
- `pkill -f PATTERN` must not have PATTERN elsewhere in the same command line (it kills the shell).

## Coordination (2026-10-17)

- projects-48 (DC music, EU disc): also edits `SkiesMenu.cpp` (Sound / Gameplay rows, RegionAddress)
  and the launcher (Sound tab, a region table), uncommitted in the main checkout; game patch ws21 /
  ws22 reserved for them (ws23 for this work if ever needed, ask first). The hunks don't overlap.
- projects-48 added `recompcore-skies-dcmusic-eu.patch` after `android-lifecycle` in the main
  checkout's series (uncommitted, 19 Dolphin files incl. `SkiesMenu.cpp`, `SkiesSettings*`,
  `SkiesSettingsPanel.kt`, `SkiesLauncherActivity.kt`, `jni/SkiesImport.cpp`) and changed
  `moderngekko-local.patch` (`release_games.hpp`, disc-ID lookup, Settings → Sound). **Merged** on top of their `c131c82`: `input` is last in the series,
  both patches regenerated (`moderngekko-local.patch` = their launcher hunks plus mine),
  check-patches passes; the six `SkiesMenu.cpp` hunks next to their Music / Language rows and
  `SkiesMusic::Poll` were placed by hand. Checked after the merge on WSL (ws22 USA game and module,
  state 05): D walks, Esc opens the menu, Sound and Controls tabs both there, keyboard hints. Not
  re-run on Windows after the merge (`C:\dev\SkiesInput` is still the pre-merge copy).
- A release build now refuses a game that isn't the pinned ws22 DOL: reconfigure a build folder with
  `REGIONS="us eu" tools/build-release-linux.sh configure` and test with
  `build/game/GEAE8P-ws22` + `build/us-gen22/module-linux/gGEAE8P_recomp.so` (main checkout).
- projects-ac (release): owns `ensurePadProfile` and the Android Saves tab; README *Controls* was handed
  over after `b527364`.
