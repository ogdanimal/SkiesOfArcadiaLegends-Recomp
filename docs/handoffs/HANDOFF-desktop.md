# Handoff: desktop builds of the Skies recomp (2026-10-05, updated 2026-10-17)

Goal: bring the Linux/WSL and Windows builds up to the Android build's level of speed work (LLVM
backend, PGO, `patches/recompcore-staticrecomp-speed.patch`), measure real desktop headroom with the
frame limiter off, and record it in the development-notes.md *Platforms* table. Companion to
[HANDOFF-android.md](HANDOFF-android.md). The top *Status* and *State of the tree* were checked against
both trees late on 2026-10-06; older sections are kept as history and say so where they're superseded.

## Status (2026-10-17, night): fullscreen, display scaling and the Steam Controller (after 0.10.0-beta)

Two local commits on `dev` for the next release (players' reports on 0.10.0-beta):
- `b9f0157`, `patches/recompcore-win32-fullscreen.patch` (`DolphinNoGUI/PlatformWin32.cpp`): the
  launcher's Screen = Fullscreen is applied when the window is made (upstream left a TODO, so the game
  always opened windowed until Alt+Enter); fullscreen drops `WS_EX_CLIENTEDGE` (it drew a light
  3-pixel frame: client 3834x2154 on a 3840x2160 monitor); the class brush is black; the runner is
  per-monitor DPI aware (at 150% scaling Windows had rendered a 3840x2160 monitor as 2560x1440 and
  stretched it), with the configured window size scaled by the window's DPI, `WM_DPICHANGED` handled
  and `render_surface_scale` = DPI / 96 for on-screen messages. Checked at 3840x2160 / 150% against
  the 0.10.0 package: boots fullscreen, client 3840x2160, window DPI 144 (was 96), sharp menu text at
  the same size, windowed outer size unchanged (2334x1401). `WM_DPICHANGED` untested (one monitor).
  The user saw the border again late 2026-10-17 in the 0.10.0 package (expected: fix not released);
  the build to compare against is `C:\dev\SkiesInput\lib\ModernGekko\build-release\Skies of Arcadia Legends.exe`.
  The user then checked the fixed runner fullscreen on the 4K monitor by eye: no border (window,
  client and DWM frame all 0,0-3840,2160, style 0x14000000, no ex style).
  Linux doesn't need the patch: `PlatformX11` applies Fullscreen at start already and asks the window
  manager for `_NET_WM_STATE_FULLSCREEN`, with no edge style; seen only on WSLg (development-notes
  *The game window*).
- `0e3a0cf`, controllers (HANDOFF-input *Steam Controller*): launcher names pads as the game does,
  pad navigation on the home menu, profiles without controller bindings filled.

Game patch **ws23** since `b1493b4` (the Continue fix, development-notes *Memory card auto-load*):
checked on the Retroid only; no desktop module or release build has it yet. Before the next release,
a Windows check: Continue from a card save, then a battle (a release build configured after the
commit pins ws23 through the dolpatch's `output-sha256`).

Test tree: `C:\dev\SkiesInput` (`lib\ModernGekko\build-release`, ws22 pins) has both; build only
`moderngekko-run` / `moderngekko-launcher` there from a `.bat` that calls `vcvars64.bat` (a quoted path
in `cmd.exe /c` from WSL fails). Scripts in its `build\`: `fs-test.ps1` (fullscreen at boot + Alt+Enter,
window and client rects, edge pixels), `dpi-test.ps1` (window DPI, fullscreen, menu screenshots,
`-ToggleFirst` for a build that boots windowed), the released package for comparison in
`build\rel0100`. `C:\dev\SkiesRecomp` hasn't been synced with these patches.

## Status (2026-10-17): keyboard and remapping (0.10.0-beta)

The keyboard plays in both release builds, every button and key can be changed (launcher Settings →
Controls, in-game Controls → Button mapping), Esc opens the menu, and controller 1 follows the SDL
gamepad in use (HANDOFF-input.md, NOTES *Keyboard and button mapping*). In Skies the game window's developer
keys are off (`DolphinNoGUI/PlatformWin32.cpp` / `PlatformX11.cpp`, `SkiesInput::OwnsKeyboard`): Windows
Space fast-forward, F1 save state and Escape-quits, X11 F1-F12 states, F10 pause and Ctrl+Esc; F11 and
Alt+Enter switch fullscreen. Windows checked with real key presses on the release code (`affabd9`).

## Status (2026-10-12, morning): Import a save in both release launchers

> **Since then (2026-10-17):** the current release is **v0.10.0-beta** (HANDOFF-release *Released*):
> game patch ws22, both regions' modules in the Windows zip and the Linux tar (`REGIONS="us eu"`), the
> Dreamcast music option (HANDOFF-dcmusic-eu), keyboard and button mapping (HANDOFF-input), and the
> update message for an earlier release's import (NOTES *Launcher and release*). Current desktop
> builds and paths: NOTES *Running*. The sections below are history.

- Settings, Saves: **Import a save** (NOTES *Importing saves*; `tools/save_import.cpp`, also used by
  Android). Windows `lib\ModernGekko\build-release` and Linux `build-release-linux` rebuilt; the Windows
  tree got `CMakeLists.txt`, `tools/moderngekko_launcher.cpp`, `tools/save_import.cpp/.hpp`, the two
  regenerated patches and the READMEs copied from WSL (the only differences were this work). Both packages
  rebuilt with it and the new player README (*Bringing your saves*): zip SHA-256 `33aef308...`, tarball
  `6fa985c0...`. Tests: `tools/save-import-test.py` (WSL launcher) and the same files through the Windows
  exe with a scratch `LOCALAPPDATA` (`--import-save`; a `.bat`, since `cmd.exe /c` mangles the quoted
  exe path with spaces). The dev play build (`lib\ModernGekko\build`) has no launcher change to pick up.

## Status (2026-10-12, after midnight): release builds for Windows and Linux (HANDOFF-release.md)

- **Windows release** (`lib\ModernGekko\build-release`, `tools\build-release-win.bat`): launcher now
  `Skies of Arcadia Legends.exe` (the old `SkiesRecomp.exe` is still in the folder, stale), user folder
  `%LOCALAPPDATA%\Skies of Arcadia Legends` (the user's `SkiesRecomp` folder was copied there with
  robocopy, `default-game.txt` re-pointed, the old one kept as a backup). `package` makes
  `build\SkiesOfArcadiaLegends-v0.9.0-windows.zip`; import checked from the unpacked zip. The `.bat`
  files skip the local vcvars when MSVC is already set up (CI). Module: `build\modules-llvm-pgo-ws16`.
- **Linux release** (`tools/build-release-linux.sh`, `lib/ModernGekko/build-release-linux`): launcher
  `SkiesOfArcadiaLegends`, `skies-run`, user folder `~/.local/share/Skies of Arcadia Legends`, package
  `build/SkiesOfArcadiaLegends-v0.9.0-linux.tar.gz` with `build/modules-llvm-ws16` (no PGO). Checked on
  WSLg: import, home screen, Settings, boot. Captures: `xdotool search --name Skies` + `import -window`.
  Its `skies-run` is the newest WSL runtime; `build-egl` (the dev runtime) is older.
- **In every runtime since 2026-10-12:** the one-time 60 fps prompt (NOTES *Slower devices and GPU
  drivers*): **WSL automation / benchmark user dirs at 60 fps need `[Skies] SlowFrameRateNoted = True`**,
  or the menu opens mid-run. OpenGL shader link failures are logged instead of shown as alerts.
- Both packages were rebuilt after each change (zip SHA-256 `5b4640d8` / tarball `f1f14252` then;
  superseded by the save import rebuild above).

## Status (2026-10-11, ~22:15): play build and release on the ws16 DOL; new launcher and menu

**Continue / card auto-load:** `lib\ModernGekko\build` (play build) and `build-release` (launcher +
`skies-run.exe`) are rebuilt with it: Continue takes the newest memory card save when it is newer than
the newest state and the runtime loads it through the title screen (NOTES *Memory card auto-load*).
Test user dir `build\user-cardtest` (the Retroid's card save in `GC\USA\Card A`, started by
`build\cardtest.bat`: set `[Skies] AutoLoadCard = 1` in its `Config\Dolphin.ini`, cold boot): in the
game at the save point in ~33 s with no input. **Open bug:** a battle can freeze when the speed changes
during a round (NOTES *Known issues*; states `build\savestates\18-retroid-battle-softlock.sav`,
`19-retroid-antonio-start.sav`). The dev user dir `build\user-ws` was left at BattleSpeed 2,
Invincible 0 by those trials.

**In-game menu:** restyled like the launchers (tabs, descriptions; L / R or Up to the tab bar);
`lib\ModernGekko\build` (play build) and `build-release` (`skies-run.exe`) rebuilt with it and checked
in the dev play build (window captures with `tools/capture-window-win.ps1`). **Icon:** both release exes
embed Reckoner's logo (`-DMODERNGEKKO_WINDOWS_ICON`, `release/skies.ico`). WSL's `build-egl` runtime
isn't rebuilt with the new menu yet.

**Launcher:** the release's `SkiesRecomp.exe` has the art home screen and the tabbed Settings window
(NOTES *Launcher and release*, HANDOFF-settings.md). `tools\build-release-win.bat build` copies
`release\launcher-background.png` beside it. The Windows tree got the new `moderngekko_launcher.cpp`
by copy (as before, no .git there); only the release build was rebuilt.

**ws16:** ship battle lists and panels moved down (user's spacing), menu rows Stick dead zone and Rumble;
`tools\play-ws.bat` (`GEAE8P-ws16`, `modules-llvm-pgo-ws16`) and the release (pinned DOL SHA-256
`7b4c416c`, run from the original DOL in `build\game\reltest-ws16`: patched to `66da66de`, 1.000x,
59.94 fps) are on it. WSL: `build/modules-llvm-ws16` with the `build-egl` runtime, Baltor 17b at 1.00x.


**ws15:** the ship battle HUD at the edges (grid top right, enemy plate below it) and the polling-loop
savestate fix; `tools\play-ws.bat` and the release (packaged with `modules-llvm-pgo-ws15`, run from the
original DOL in `build\game\reltest-ws15`: patched to `2d7b1f1d`, 1.000x) are on it. The Baltor states
17 / 17b were remade with `GEAE8P-ws15-shipdbg` (the ws14 ones were caught in patch code and hang in
ws15). Debug builds `GEAE8P-ws14-trace` (trace2d) and `-ws14-shipdbg` / `-ws13-shipdbg` are safe to
delete.

**ws14 (2026-10-11, night):** battle speed, Invincible and the rewards now reach ship-versus-ship battles
(NOTES *The player build*, decomp `docs/qol_cheats.md` *Ship battles*); `tools\play-ws.bat` and the
release (rebuilt, packaged with `modules-llvm-pgo-ws14`, run from the original DOL in
`build\game\reltest-ws14`: patched to `7a73cd6e`, Baltor state at 1.000x) are on ws14; the dev runner has
the frontend side (`InShipBattle`, the 3x / 4x overclock). Ship battle states `17-ship-battle-baltor` /
`17b-...-commands` (made with `tools/shipbattle-state-win.sh` and the debug build
`GEAE8P-ws14-shipdbg` + `modules-llvm-pgo-ws14-shipdbg`). The earlier unused ws14 experiment was
overwritten; `GEAE8P-ws13-shipdbg` + its module are a first debug build whose states don't load
elsewhere (safe to delete).

Before (ws13): `tools\play-ws.bat` ran `build\game\GEAE8P-ws13` (DOL `fa7b1af1`, SHA-256 `fe464ed3`: widescreen, camera
control, QOL options, Invincible, the world map altitude gauge at the left edge like 4:3;
`patches/game/README.md`) with `build\modules-llvm-pgo-ws13` directly (no more copies into `GEAE8P-ws` /
`modules-llvm-pgo-ws`, which still hold ws10). The dev runner `lib\ModernGekko\build\moderngekko-run.exe`
and the branded release in `lib\ModernGekko\build-release` (`SkiesRecomp.exe`, `skies-run.exe`;
`tools\build-release-win.bat`, pinned to ws13, patch `release\skies-widescreen.dolpatch`) have the whole
in-game menu (with the 2026-10-11 fix that stops it pulsing when it has to shrink, and since the late
evening save states and auto-save; HANDOFF-settings.md; the release launcher has Continue),
camera control, the options, the L3 / R3 toggle and the resolution write-back to `config.ini` (NOTES *The
player build*). The release runs end to end from the original DOL (patched to ws13 at start, gauge at the
edge, 59.9 fps at 1.00x; ws12 checks: 2x battle speed and Invincible). It refuses any other DOL, including an
older patched one: the user's `%LOCALAPPDATA%\SkiesRecomp\games\GEAE8P` holds ws10, so re-import the disc
there. A new game build is tested with the dev runner first. Desktop speed is unchanged: 3x and 4x battle
speed hold 1.00x on this PC (92 / 122 game frames per second). Previous builds kept: `GEAE8P-ws12` +
`modules-llvm-pgo-ws12`, `GEAE8P-ws11` + `modules-llvm-pgo-ws11`, `GEAE8P-ws10` + `modules-llvm-pgo-ws10`;
`build\game\reltest-ws13` a release test root (both safe to delete; the game folders' `files` are
junctions). WSL: runtime in `lib/ModernGekko/build-egl` current (menu fix, save states), newest module
`build/modules-llvm-ws10`. Save-state test: `tools/states-test-win.sh` (states in `build\user-sstest`).

Test tools since 2026-10-10: `tools/dump-ab-win.sh`, `tools/button-probe-win.sh`, `tools/camera-test-win.sh`,
`tools/qol-test-win.sh` (NOTES *Test tools and savestates*); each run makes its own `build\user-*` /
`build\auto-*` folders afresh (safe to delete between runs). Old test output that can go (none of it is used):
`build\auto-*`, `build\dump-*`, `build\user-ab-*`, the experiment builds `build\game\GEAE8P-ws10{A,B,C,D,def43}`,
`-ws4`..`-ws8t`, `-wst`, `-ws-trace` and their `build\modules-llvm-pgo-*` (the game folders' `files` are
junctions: remove those with `rmdir` from cmd, never a recursive delete), the older APKs in `build\`, and the 2026-10-11 investigation output `build\inv-ram\` (three 24 MB RAM dumps),
`build\inv*.png`, `build\gauge-*.png`, `build\reltest-ws13.png`, `build\rel-continue.sav` (a copied auto-save for the
release `--load-state` check).
Keep `build\user-bt` (the test scripts copy it), `build\user-ws` (play), the savestates.

## Status (2026-10-10, late night): play build on the ws10 DOL; runtime has the settings backend (superseded)

`tools\play-ws.bat` runs DOL `7cb9de68` (`GEAE8P-ws10`: the 4:3 setting is now identical to the
unpatched game, 16:9 unchanged from ws9; NOTES *Widescreen 4:3 setting*) with `build\modules-llvm-pgo-ws`
(copied from `modules-llvm-pgo-ws10`). Previous: `main.dol.329c631f`, `modules-llvm-pgo-ws9`.
`moderngekko-run.exe` has the `[Skies]` settings backend and `set_config` (HANDOFF-settings.md); previous
exe `moderngekko-run-1010-presettings.exe`. WSL: runtime rebuilt, module `build/modules-llvm-ws10`.
New tool: `tools/dump-ab-win.sh` (frame-by-frame A/B of two runs from a state). Test leftovers that can
go: `build\modules-llvm-pgo-ws10{A,B,C,D,def43}`, `build\game\GEAE8P-ws10{A,B,C,D,def43}` (their `files` are
junctions: remove those with `rmdir` from cmd, never a recursive delete), `build\dump-ab-*`,
`build\user-ab-*`, `build\auto-ab-*`.

## Status (2026-10-10, night): runtime rebuilt with the TMEM-load fix; regression-checked at 1x (superseded)

`moderngekko-run.exe` has the Retroid cloud-sea fix (NOTES *Cloud-sea garbage*: the replay skips the game's
TLUT loads and TMEM preloads) plus the TMEM-state save/restore and the debug switches. Previous exes:
`moderngekko-run-1010-tmemstate.exe` (TMEM state only), `-1010-encode.exe` (before both). WSL runtime
rebuilt with the TMEM-state change only. Windows never showed the garbage (`build\dump-base2k`, 2 KiB
slices: 0 dark real pictures of 521), so the Windows runs are a regression check: ladder (state 15, 1x) 0 bad in-betweens of 519 at the default slice and 0 of 519 at 2 KiB slices, main menu 0 of 295, 0 dark cloud-sea pictures, no panics (`build\dump-final`, `dump-final2k`, `dump-menu-final`). The
Windows tree got the four changed files and the regenerated patch (21 sections, reverse-applies cleanly).
All of it is committed in `a9a5418` (the repo side; the Windows tree is not git and was synced by hand).

## Status (2026-10-10, evening): runtime has the Retroid ladder fix; Windows unchanged in effect (superseded)

`moderngekko-run.exe` rebuilt with the day's interpolation changes (NOTES *Texture garbage in in-betweens
... and green bands*): TMEM-preloaded textures recorded for the replay, the EFB swap's layout barrier, the
replay's copies running the encode pass on Qualcomm only (not on Windows), the file switches. A/B on
Windows at 1x (`build\user-bt`, state 15 ladder and the main menu, every picture dumped and scored):
no change against the previous exe (final exe: ladder 0 bad in-betweens of 519, menu 0 of 299, no panics). Earlier exes kept: `moderngekko-run-1010-pretmem.exe` (before all of
it), `-1010-tmem.exe`, `-1010-prefix.exe`. The Windows tree got every touched vendor file copied over;
`patches/recompcore-frame-interpolation.patch` regenerated (20 sections) and copied too.

## Status (2026-10-10): play build is ws9 (HUD at the edges); input needs focus (superseded)

`tools\play-ws.bat` runs DOL `329c631f` (`GEAE8P-ws9`: HUD anchors, NOTES *Widescreen HUD at the edges*) with
`build\modules-llvm-pgo-ws` (copied from `modules-llvm-pgo-ws9`). Previous: `main.dol.a4f9ef7c`,
`modules-llvm-pgo-ws-a4f9ef7c`. `moderngekko-run.exe` has: the replay's recorded textures with content
locks and snapshots (NOTES *60 fps on Android: dual core*, Open), the Win32 focus tracking (pads and keyboard
ignored while unfocused; `[Input] BackgroundInput = True` turns that off; automation input still applies),
and the trace/dump file switches. Earlier exes: `-1010-pretex`, `-1010-prefocus`, `-1009-ws8`. WSL runtime
and WSL module not rebuilt (WSL module still from an old DOL). Game-patch tooling: second code area over
MetroTRK (`.trkcave`, patches/game/README.md), `trace2d` ten deep with its table in MetroTRK's buffers
(`tools/readtrace.py 0x80319078 [reset] [2d]`), debug DOL `GEAE8P-ws8t` + `modules-llvm-ws8t` (Windows game
roots need a `files` junction: `mklink /J`). Test windows: the user's input no longer reaches them.

## Status (2026-10-09): play build switched to the ws8 DOL; transition and interpolation fixes (superseded)

`tools\play-ws.bat` now runs DOL `a4f9ef7c` (`build\game\GEAE8P-ws\sys\main.dol`, copied from
`GEAE8P-ws8`) with its module (`build\modules-llvm-pgo-ws`, copied from `modules-llvm-pgo-ws8`; built
with `DOL=build/game/GEAE8P-ws8/sys/main.dol tools/module-llvm-win.sh llvm-pgo-ws8 --profile-use
build/modules-llvm-pgo-gp/GEAE8P/pgo/merged.profdata`). Previous: `main.dol.85176f05`,
`modules-llvm-pgo-ws-85176f05`. Runtime `moderngekko-run.exe` has the interpolation fixes and the
state-load hang fix; earlier exes kept (`-1008-prestatefix`, `-prefifospace`, `-preefbstart`,
`-prealign`, `-presnap`, `-1009-snap6`). WSL runtime rebuilt; the WSL module (`build/modules-llvm-ws`)
is still from the old DOL (mismatched chunks run in the interpreter: rebuild it before playing on WSL).
Test tooling used tonight (scratchpad scripts, worth turning into `tools/`): fly-until-battle with
dumps, a fine-grid in-between vs real scorer, a stray-content counter. Test user dirs `build\user-flk`
(1280x720) and `build\user-bt` (640x528).
Android: the same DOL and fixes installed on the Retroid and checked there (2026-10-09, HANDOFF-android.md).

## Status (2026-10-08, late): desktop plays dual core; state-load fix (superseded)

`build\user-ws` (`tools\play-ws.bat`) now has `GameSettings/GEAE8P.ini` with the idle skip and
`CPUThread = True`, like the Retroid: red alert, limiter off, 2.09x / 124 fps (single core 1.59x). The
Windows tree was an hour behind (interpolation statistics cleanup, `gamepatch.py`, two tools): synced, and
`moderngekko-run.exe` rebuilt with the dual-core state-load fix (`State.cpp`, HANDOFF-android.md);
previous exe `moderngekko-run-1008-prestatefix.exe`. WSL runtime rebuilt too. To go back to single core,
delete the `CPUThread` line.

## Status (2026-10-08): interpolation reworked for dual core (Android); desktop still single core (superseded by the status above)

`recompcore-frame-interpolation.patch` was reworked for Android (HANDOFF-android.md, NOTES *60 fps on
Android: dual core*): the replay draws on a second EFB with private copies, in slices in dual core, and a
PE token read waits for the GPU thread, which fixes Skies' dual-core desync. Windows runtime rebuilt with
it (`moderngekko-run.exe`; previous `moderngekko-run-1008-predefer.exe`, `-preopt.exe`,
`-preadaptive.exe`); WSL runtime rebuilt too. Windows numbers (3x, red alert state, limiter off): dual
core + interpolation 2.11x, single core + interpolation 1.59x. Desktop play still uses single core (adaptive
replay keeps full speed there). To try dual core: `CPUThread = True` under `[Core]` in the user
directory's `GameSettings/GEAE8P.ini` (test dirs `build\user-ws-dualfree` / `user-ws-dual1x` have it).
Note: `build\user-ws` (what `tools\play-ws.bat` uses) has no `GameSettings/GEAE8P.ini`, so no idle skip.

## Status (2026-10-07, after midnight): interpolation cloud flicker fixed; widescreen HUD and shatter in progress (superseded: shatter fixed 10-09, HUD layout still open)

- **Fixed: title-screen cloud flicker with 60 fps interpolation** (`recompcore-frame-interpolation.patch`;
  NOTES *60 fps*). Draws of a kind are now aligned between frames instead of paired n-th to n-th.
  Only the **Windows** runtime is rebuilt (`lib\ModernGekko\build\moderngekko-run.exe`; the one before is
  `moderngekko-run-1007-predumpframe.exe`). WSL runtime not rebuilt. Waiting for the user's by-eye check.
  The Windows exe lacks the later `FrameInterpolation.on` switch (Android's; source synced, relink failed
  while the exe was running: run `tools\build-win.bat build` once it's closed).
- **Android test build with widescreen + 60 fps installed** (2026-10-08, HANDOFF-android.md): battle full
  speed at 59.8 fps, opening cutscene 0.79x.
- **Game patch source is ahead of every build** (NOTES *Widescreen: work in progress*): tail-call
  `ws_frame_end` (savestates survive patch changes from now on), versioned patch data, HUD anchor
  mechanism (table empty), `-Os`, data at `0x80002A00`, new `trace2d`. Only the debug build
  (`GEAE8P-ws6t` / `modules-llvm-pgo-ws6t`) was booted. Savestates from earlier widescreen builds crash
  in it; tell the user before shipping.
- **Open, in order:**
  1. Battle HUD layout (user request): find the code that draws the status panel, command wheel and SP
     gauge (trace a battle with `trace2d` + `tools/readtrace.py`, e.g. state 02), fill
     `ws_config.anchors` (left panels `side = -100`, gauge `dy` a little negative), tune with `poke:`, then
     make them the defaults in `widescreen.c` and bump `WS_VERSION`.
  2. Battle-entry shatter still 4:3: not `fn_800B0278` / `fn_800AF780` (wrapped, no change). Trace the
     transition (3D and 2D) to find it. A pre-encounter state that loads in the new build is needed: walk
     in state 07 (Windows, ~1-2 s of `hold:main_y=1`) with the new build and save one.
  3. Then check menus, battle, field and text in the new build, build the play DOL + modules (Windows,
     WSL, Android), and ask before installing on the Retroid.
- **Test instances on Windows:** run `moderngekko-run.exe` directly with its own `--automation-dir`
  (`build\autoN`) and a copied user dir (`build\user-ws-test`), so they don't touch the user's session.
  The user watches the screen: say when a test window will pop up. States used: `build\savestates\09-title.sav`
  (title, current `GEAE8P-ws` build), `08-title-ws.sav` (actually the attract-mode demo battle).

## Status (2026-10-07, late night): widescreen regression fixed, script fades full width (superseded: ws8 builds)

- **Fixed:** the 4:3 clip no longer cuts the red alert room. `ws_set_projection` now writes the matching
  scissor on every projection load instead of remembering whether the clip was on (the display lists put the
  GPU's order out of step with the CPU's). Menus still clip. NOTES *Widescreen* has the account and the checks.
  The UiObj-level rework the earlier status proposed isn't needed for this.
- **Script fades** (`ScreenFadeDraw`) now cover the side bars (`ws_full_80210390` in `fullwidth.S`).
- **Builds:** Windows `build\modules-llvm-pgo-ws` and `build\game\GEAE8P-ws` are the new ones (`tools\play-ws.bat`
  unchanged); the previous module is kept as `build\modules-llvm-pgo-ws-f82e437`. WSL `build/game/GEAE8P-ws` is the
  new DOL and `build/modules-llvm-ws` is rebuilt from it (the older one, from 92d1c29's DOL, is
  `build/modules-llvm-ws-92d1c29`). Android: installed on the Retroid and checked (HANDOFF-android.md).
- **New open item:** the battle-entry shatter effect only covers the 4:3 area (seen on the Retroid; NOTES *Widescreen*).
- **Trap:** a module only tests the DOL it was built from. A WSL run of the clip DOL with the older module looked
  correct, most likely because the module's compiled callers skipped the new code.

## Status (2026-10-07, night): the 4:3 clip broke widescreen in places; decomp menu research (superseded: fixed, see above)

- **Widescreen regression:** the 4:3 clip for parked menu UI (f82e437) cuts the 3D scene to 4:3 in the Valuan
  ship's red alert room (savestate `build/savestates/07-valuan-red-alarm.sav`; checked: the pre-clip build renders
  it right). Details and the likely mechanism in NOTES *Widescreen*. **Not fixed yet**, and the Retroid install
  still has the clip. Next step: replace the scissor with a UiObj-level fix using the decomp's menu research.
- **Decomp (`../SkiesOfArcadiaLegends-Decomp`):** menus, HUD and the 2D layer are mapped in `docs/ui.md`
  (every menu element is a UiObj moved to parked positions: camp menu x = -348 / 668, save/load -340 / -330 /
  -240 / 676, shops -260 / 648). First game code from C: `drawstate.c` linked, `uiobj.c` 40/44, `text.c` 28/33.
  The game compiler is GC/1.3.2. About 160 game functions and globals are newly named in its `symbols.txt`,
  which `tools/export_map.py` exports, so module function names change on the next rebuild (addresses don't;
  game patch specs use addresses and are unaffected).
- **Fix that likely helps the Android widescreen picture too:** the script screen fade (`ScreenFadeDraw`,
  0x80210390) draws 640 x 480 through Ninja's own primitive, which the patch doesn't hook, so fades leave the
  wide side bars unfaded (decomp `docs/ui.md`). Not changed yet.

## Status (2026-10-07, evening): widescreen and 60 fps

- **Widescreen** is a game patch built into the DOL (`tools/gamepatch.py`, `patches/game/widescreen.*`; NOTES
  *Widescreen*, decomp `docs/widescreen.md`): Hor+ 3D, the HUD at its original shape, full-screen 2D kept full
  width, battle labels on their characters, and squeezed 2D clipped to 4:3 (menus park UI off-screen). It
  needs a module built from `build/game/GEAE8P-ws/sys/main.dol` (Windows: `build\modules-llvm-pgo-ws`, WSL:
  `build/modules-llvm-ws`, no PGO) and Dolphin set to an exact 16:9 picture (`build\user-ws`, `build/user-ws`).
  Checked on Windows, WSL and the Retroid (installed there; HANDOFF-android.md).
- **60 fps** is `recompcore-frame-interpolation.patch` (NOTES *60 fps*): replays each frame with matrices and
  inline vertex positions halfway to the previous frame's and presents it in between. `frame_interpolation=true`
  in `config.ini` (on in `build\user-ws`); 59.9 fps at full speed on Windows, ~20% of the headroom. Not on
  Android.
- **Play:** `tools\play-ws.bat` (Windows, both features). The plain launchers still run the unpatched game.
- **New runtime patch:** `recompcore-restore-boot-dol.patch` (state loads put the boot DOL's code back, so
  savestates from before a game patch work). Both runtimes rebuilt with every patch; copies kept:
  `moderngekko-run-1007-preinterp.exe`, `-prerestore.exe`, `-prehash.exe`.
- **Tools:** `tools/drive.py peek:` / `poke:` / `pokef:` (guest memory), `GAME=` for both launchers, `DOL=` for
  `tools/module-llvm-win.sh`. To read a Dolphin dialog before closing it, a UI Automation script works
  (`[System.Windows.Automation.AutomationElement]` over the process's windows); don't kill one unread.
- **Open:** widescreen in places no savestate reaches (world map, towns, ship battles: the user is play-testing);
  HUD at the far sides of battle (labels clip at the 4:3 edge); interpolation at scene cuts and for particles;
  three unclassified `njProjectScreen` callers (decomp `docs/widescreen.md`).

## Status (2026-10-07, later): launchers default to LLVM + PGO; the cold-boot "hang" was startup hashing

- `tools/play.sh` defaults to WSL `build/modules-llvm-pgo-gp`, `tools/run-win.bat` to Windows
  `build\modules-llvm-pgo-fix` (both checked: module loads, clean shutdown). `MODULE=` still overrides.
- **Windows cold boot fixed.** Every launch hashed the whole 1.4 GB `files/` directory twice before the first
  log line (~27 s on Windows, ~10 s on WSL); the earlier cold boot was killed just before its window appeared.
  `moderngekko-local.patch` skips that hash unless the build pins it: ~2 s to `state=running` on Windows, 2.7 s
  on WSL. Both runtimes rebuilt; cold boot reaches the intro at full speed (NOTES *Known issues*).
- **Upstream reports: the user decided not to file them** (2026-10-07; drafts kept in `upstream-reports/` as a record). DolRecomp
  Win64 sjlj frame (+ unwind tables), ModernGekko Win32 exit (RecompCore has issues disabled), ModernGekko
  startup hashing. All three bugs are still in upstream heads as of 2026-10-07. Upstream DolRecomp has
  since merged its own `mtmsr` region-leader fix (`ac3deea`, PR #28), equivalent to our
  `dolrecomp-mtmsr-entry.patch`; drop ours if the pin is ever moved past it.

- **Upstream update: skipped by the user for now** (2026-10-07). Checked: ModernGekko master `a2e4e2b` pins
  RecompCore `cf8ceb81b4` (only a DolRecomp repin over ours) and DolRecomp `71ce7f9` (9 commits; `40637c4`
  rewrites much of the LLVM backend, `CPUState` unchanged). On those, `llvm-cpu`, `win64-sjlj-frame`, `coff-uwtable`
  apply cleanly; `mtmsr-entry` fails (upstream `ac3deea` replaces it: drop); `gather-pipe` fails only in
  `CMakeLists.txt`; `moderngekko-local` fails in the port files (upstream has `6645667`: drop those hunks). Cost:
  regenerate all modules, retrain PGO, re-bench, re-run Windows exit tests and the Android check. Revisit only if
  upstream ships something we need.

## Status (2026-10-07): Windows exit crashes fixed, Windows LLVM + PGO is the fastest module

- **Both exit crashes are fixed and explained** (NOTES *Known issues*):
  - LLVM `abort` = DolRecomp's `eh.sjlj` entry wrapper put `llvm.frameaddress` (a Win64 frame pointer sits
    0x80 above it) in the jump buffer, so every cold escape restored xmm6–15 from the wrong slots; MSVC kept
    the Play Time Tracker `std::thread` in xmm6, `join()` threw at exit. `patches/dolrecomp-win64-sjlj-frame.patch`.
    40/40 clean automation stops in 01/02 (before: 12/12 crashed). Linux objects byte-identical.
  - `+0x68392` access violation = `WM_KILLFOCUS` after Config shutdown, **only with real keyboard focus**
    (WSL-launched windows never had it). Focused A/B: pre-fix runtime 0/30 clean, current 45/45 (clang) and
    20/20 (LLVM) over X button, Alt+F4 and automation stop. `recompcore-win32-exit.patch` now also routes
    every message to `DefWindowProc` while the window is detached.
- **Windows LLVM + PGO** (`build\modules-llvm-pgo-fix\gGEAE8P_recomp.dll`, WSL profile): worst second 2.04×
  in all four states at 1× and 3×, +37–88% over clang + PGO (same session). It's the module to play with now;
  since 2026-10-07 it's `tools/run-win.bat`'s default (`tools/play.sh` defaults to WSL `build/modules-llvm-pgo-gp`).
- **Memory card:** an in-game save on Windows survives the exit and loads on a fresh boot (NOTES *Known issues*).
  The user's Android save is backed up read-only at `build/android/user-save-2026-10-07/` (never write to the
  device copy). New states `05-save-point` / `06-save-menu` (both trees).
- **New tools:** `tools/exit-test-win.sh` (N stop/exit cycles; `STOP=close|altf4`, `FOCUS=1`),
  `tools/focus-win.ps1` (forces and verifies the foreground). Debug kit: `lib\ModernGekko\build-pdb` (runtime
  with a PDB; diagnostics reverted), `lldb.exe` with `C:\dev\python310-embed` on `PATH` + `PYTHONHOME`;
  symbolize offline with `llvm-symbolizer --obj=...exe` (lldb doesn't load the PDB). Runtime copies kept for
  A/B: `moderngekko-run-prefocus.exe` (pre-exit-patch), `moderngekko-run-1006.exe` (before today's WndProc guard).
- **Open:** upstream reports (DolRecomp: Win64 sjlj frame + COFF unwind tables; RecompCore: the Win32 exit
  path); Windows cold-boot hang (untouched); the Android LLVM module doesn't use the x86 sjlj path, so nothing to
  port there.

## Status (2026-10-06, late): desktop rebuilt with the gather-pipe patches

- **Active modules now:** WSL `build/modules-llvm-pgo-gp` (LLVM + PGO, all patches including `mtmsr`;
  path in its `GEAE8P/active-module.txt`); Windows `build\modules-clang-pgo\use-build\gGEAE8P_recomp.dll`
  (clang + PGO). Both runtimes are rebuilt. **Every other desktop module is rejected at load** (CPU state
  size mismatch, exit 1), including the launchers' defaults and the Windows LLVM module. Pass `MODULE=`.
- **Numbers** (NOTES *Desktop headroom → With the gather-pipe patches*): Windows clang +3–11% (1×) / +6–10%
  (3×), worst second 1.27 (1×) / 1.29 (3×); WSL +2% (01) / +5% (04), worst second 1.13. Pictures correct,
  all Windows runs exit 0.
- **Fix needed on the way:** the desktop runtime checks `cpu_state_size` against ModernGekko's own
  `include/moderngekko/cpu_state.h`, which the Android work never touched (Android doesn't use it). Added the
  three fields there; folded into `patches/moderngekko-local.patch`.
- Windows tree synced: the patched sources, `patches/`, docs and `tools/` (`.bat` kept CRLF). Before the
  sync it had only the older patch set, nothing local.
- **hook_fb** (interpreter fallbacks per benchmark run, ~46 s): WSL LLVM 50–59k; Windows clang C 113–118M
  (before: ~85M per run, but the faster runs now cover more game time, so not like for like). The C
  module's source is still unsampled (`STATICRECOMP_FALLBACK_SAMPLES=1`; the Windows runtime has it now).
- **What's left and how much it's worth** (estimate): the Android margin items (burst path, `dcbf`/`dcbst`,
  `Run()` table) would give Windows clang ~1–3% and WSL little. None raises the frame rate: full speed is
  the game's 30 fps (NOTES *60 fps*).
- **Open:** a Windows LLVM module with the `mtmsr` fix and the gather-pipe patches (`tools/module-llvm-win.sh`;
  still blocked on its exit crash); re-link the default (non-PGO) modules if they're still wanted; sample
  the C module's fallbacks; heavy scenes by hand.

## Status (2026-10-06): plan steps 1, 2 and 4 done; full speed with no dips on both desktops

- Tooling committed (`7aaa5c4`). Benchmark session done: results and protocol in development-notes.md
  *Platforms* and *Desktop headroom*. Raw lines: `build/bench-desktop.txt` (states 01/03, three rounds +
  warmup) and `build/bench-states.txt` (all four states, two rounds, with the slowest second).
- **Goal met on desktop:** with the limiter off, the slowest second in every standard state is
  ≥1.15× (WSL LLVM+PGO at 1×; Windows clang+PGO at 1× and 3×), so the game holds a locked 60 VI/s.
  (Superseded: after the late 2026-10-06 rebuild, 1.13× WSL and 1.27× / 1.29× Windows; see the top *Status*.)
  Use these two modules. Windows LLVM is faster still (worst second 1.38×) but crashes on exit.
- Bench scripts now print `min=`/`max=` (slowest/fastest second) and the exit code.
- Step 3 (Windows exit crashes) is still open; see NOTES *Known issues*. `recompcore-win32-exit.patch`
  fixes the focus handler (it re-enabled the frame limiter on focus loss), but focused-window tests
  didn't reproduce the `+0x68392` crash before the fix either, so that cause is unproven. Next step there:
  a runtime build with a PDB (`/Zi` + `/DEBUG` in Release) and `lldb.exe` from the clang tarball, or
  WER LocalDumps, to get a backtrace of each crash. The Windows runtime is rebuilt with the patch; the
  pre-fix binary is `moderngekko-run-prefocus.exe`.
- **First play session (2026-10-06):** the user played on Windows (clang+PGO, 3×, limiter on, from
  `01-opening-cutscene.sav`) at a solid 30 fps, which is the game's native rate (full speed = 60 VI/s =
  30 fps). Clean exit (code 0). `build\user\config.ini` is now `resolution=1920x1080` (3×).
  60 fps (game patch or renderer interpolation) was discussed and tabled by the user (done 2026-10-07: top *Status*).
- **New: Windows cold boot hangs** without `--load-state` (NOTES *Known issues*); launch from a state.
- Android parity (stable 30 fps) is the next phase: HANDOFF-android.md *Next phase*.
- **2026-10-06 evening, from the Android work: an LLVM-backend fix that applies to desktop too.**
  `patches/dolrecomp-mtmsr-entry.patch`: after an EE-setting `mtmsr`, LLVM modules re-entered through
  the per-instruction interpreter path (the tail of every `OSRestoreInterrupts`/`OSEnableInterrupts`),
  95% of their `hook_fb`. That caused Android's dips; desktop mostly absorbed it, though an old-module
  WSL battle run showed one 0.65 second. With the fix and a WSL PGO retrain (`build/modules-llvm-pgo-v2`):
  fallbacks ~10× lower, speed equal to −2–4% (04) on WSL, i.e. within llvmpipe's ceiling and noise
  (raw: `build/bench-mtmsr-ab.txt`, `build/bench-pgov2-ab.txt`). **Not active yet**: decide after a
  Windows LLVM run, where the GPU isn't the limit. The Windows LLVM module (cross-generated) needs
  regenerating to get the fix; the C backend wasn't checked.
- **New diagnostic:** `STATICRECOMP_FALLBACK_SAMPLES=1` prints the top 48 interpreter-fallback PCs at
  shutdown (in `recompcore-staticrecomp-speed.patch`; both runtimes have it since late 2026-10-06).
- **One unexplained run:** a v2 battle run took a different course (mean 2.3×) with 1.07M fallbacks,
  10× the usual. Sample it with the diagnostic.

## State of the tree

**Runtimes** (select with `RUNTIME=` for `tools/play.sh` / `tools/run-win.bat`):

| Platform | Current (all patches, `CPUState` 3560 bytes) | Older binaries (`CPUState` 3536) |
|---|---|---|
| WSL | `lib/ModernGekko/build/moderngekko-run` (rebuilt late 2026-10-06) | `lib/ModernGekko/build/moderngekko-run.prespeed` (also `.orig`, `.fastpath`) |
| Windows | `lib\ModernGekko\build\moderngekko-run.exe` (rebuilt late 2026-10-06) | `lib\ModernGekko\build\moderngekko-run-prespeed.exe`, `-prefocus.exe` |

**A runtime only loads modules with its own `CPUState` size.** The current runtimes take only modules
linked after the gather-pipe patches; the older binaries take only older modules. So a runtime-only A/B
across that change is no longer possible, and the old binaries are only good with old modules.
The speed patch itself touches only runtime files (`JitInterface.cpp`, `StaticRecompCore*.cpp/.h`).

**Modules** (WSL paths are `build/<dir>/GEAE8P/<hash>/gGEAE8P_recomp.so`, read `active-module.txt`):

Only the two **bold** rows load in the current runtimes; every other module is from before the gather-pipe
patches and is refused ("CPU state size mismatch").

| Platform | Module | Where |
|---|---|---|
| WSL | **LLVM + state-in-memory + PGO, all patches (`mtmsr`, gather-pipe), retrained late 2026-10-06** | **`build/modules-llvm-pgo-gp`** |
| Windows | **C, clang + PGO, all patches, retrained late 2026-10-06** | **`build\modules-clang-pgo\use-build\gGEAE8P_recomp.dll`** (`pgo-run`'s own copy is under `build\modules-clang-pgo\GEAE8P\`) |
| WSL | C, gcc 15 | `build/modules` |
| WSL | C, clang-20 (new) | `build/modules-clang` (`build/modules-clang-O3` also exists) |
| WSL | LLVM + state-in-memory | `build/modules-llvm-sim` (`build/modules-llvm` is LLVM without state-in-memory) |
| WSL | LLVM + state-in-memory + PGO (new) | `build/modules-llvm-pgo` (built 22:10, profile 21:56) |
| WSL | LLVM + state-in-memory + PGO, `mtmsr` fix, retrained 2026-10-06 | `build/modules-llvm-pgo-v2` (superseded by `-gp`) |
| WSL | same fix, old profile (14 functions unprofiled; test only, deletable) | `build/modules-llvm-pgo-mtmsr/module-build/gGEAE8P_recomp.so` |
| Windows | C, clang 20.1.8 | `build\modules-clang\GEAE8P\<hash>\gGEAE8P_recomp.dll` |
| Windows | C, clang + PGO (2026-10-05, retrained 22:39) | `build\modules-clang-pgo-pre-gp\use-build\gGEAE8P_recomp.dll` |
| Windows | LLVM + state-in-memory (new, cross-generated from WSL) | `build\modules-llvm-sim\gGEAE8P_recomp.dll` |

Windows tree: `C:\dev\SkiesRecomp` (`/mnt/c/dev/SkiesRecomp`), driven from WSL by `tools/bench-win.sh`,
`tools/pgo-win.sh`, `tools/module-llvm-win.sh`. It isn't a git checkout: after a patch change, copy the
patched files over (compare first; it has had nothing local of its own), plus `patches/`, the docs and
`tools/`. Keep the `.bat` files CRLF (`.gitattributes` asks for it, but WSL's working copies are LF). Rebuild
with `tools\build-win.bat build`; that also relinks `moderngekko-port.exe`, which fails while a port
command is running, so build just the `moderngekko-run` target then.

**Benchmark user dir:** `build/user-bench` (and the Windows copy) has `EmulationSpeed = 0.0`
(limiter off), `DSPThread = True`, `Volume = 0`, Vulkan, 1× resolution. `tools/bench-wsl.sh` and
`tools/bench-win.sh` implement the standard protocol: load state, 16 s, mean of 30 one-second
`speed=` samples, screenshot, clean stop.

**Committed:** the tooling and the patch fix in `7aaa5c4`, the numbers in `034b8f7`; the `mtmsr` fix,
Android tools and docs in `f1c3e52` / `847877c`; the gather-pipe work in `5c9985f`; the desktop rebuild and
the `cpu_state.h` fix in `5a2142d` and the docs pass after it (branch `skies-windows-android`, not pushed).
The `lib/ModernGekko` submodule has all of `patches/` applied (uncommitted by design; the patches record it).

## Verified facts

- **PGO tool bug, fixed locally.** The template's `moderngekko-port` set PGO through variables that the
  pinned DolRecomp ignores, so LLVM-backend chunks were neither instrumented nor optimized. The fix in
  `patches/moderngekko-local.patch` (21:39) passes `--profile-generate GEAE8P-%m.profraw` /
  `--profile-use <merged>` on the dolrecomp command line. Proof it works: the WSL profile trained
  after the fix has 11015 functions (Android's has 10910); before the fix it would have had only the
  template's C helpers. Worth reporting upstream.
- **Windows clang+PGO: retrained, but four-state coverage is unconfirmed.** (2026-10-05. The late
  2026-10-06 retrain had all four states in the tree and ran at 0.82 / 0.71 / 0.59 / 0.54×, distinct per
  state where the failed-load run was flat, so the loads very likely happened; no screenshot taken.) The first Windows
  training only covered state 01: states 02–04 were not in the Windows tree, so the `drive.py` loads
  failed silently (the runtime does not log loads). They were copied in at 22:16 and the retrain ran
  at 0.76 / 0.81 / 0.77 / 0.75× (the first run: 0.48 / 0.50 / 0.50 / 0.49×, still in
  `build/pgo-win-run.out`, which is stale). `Total functions: 1083` counts instrumented functions,
  not executed ones (the state-01-only profile had the same number), so it says nothing about
  coverage. Only one raw profile survives (`pgo/raw/GEAE8P-1732.profraw`, 22:20) and there is no
  Dolphin log dir, so confirm loads directly: a `drive.py load:… shot:…` on the Windows tree.
  `tools/module-win.bat` passes only `-Wno-profile-instr-unprofiled` (no `-Werror`); the check made
  was zero out-of-date or mismatch warnings in `build/pgo-clang-use.log`. The "pgo-run failed" line
  is the exit crash of the instrumented run, after the profile is flushed; `pgo-win.sh` continues.
- **Training speeds under instrumentation** (not benchmarks): WSL LLVM 0.47 / 0.70 / 0.26 / 0.20× in
  states 01–04; Windows clang 0.76 / 0.81 / 0.77 / 0.75×. Late 2026-10-06 (the two platforms trained at the
  same time, so lower): WSL 0.18 / 0.96 / 0.19 / 0.16×, Windows 0.82 / 0.71 / 0.59 / 0.54×.
- **Two different Windows exit crashes** (Windows Application event log, Event ID 1000):
  - LLVM module (22:10:52, 22:14:02): `0xc0000409` (fast-fail, `abort()`) in `ucrtbase.dll`
    offset `0xa527e`, before the `[staticrecomp] shutdown: …` line, so likely before any PGO profile
    flush. This blocks a Windows LLVM+PGO module.
  - Access violation `0xc0000005` at `moderngekko-run.exe+0x68392` (then `0xc000041d` a second
    later): 21:53:19 after the instrumented clang run, **and already at 18:28:28 after the plain clang
    warmup run**, before any PGO work. So it is a runtime exit bug the C module can hit too, not an
    instrumentation bug.
  - `LiveKernelEvent 141` (a GPU hang report) entries appear about 2 s *after* each crash: the driver
    tearing down the dead process, not the cause.
  - `lldb.exe` in the clang tarball (`C:\dev\clang+llvm-20.1.8-x86_64-pc-windows-msvc\bin`) can
    give a backtrace for both.
- **Interpreter fallback counter.** The shutdown line's `hook_fb=` is
  `m_hook_fallback_instructions`: instructions the module handed to the interpreter because they are
  not modeled (cache ops, DMA `mtspr`, …; `StaticRecompCore_Hooks.cpp` `HookInstructionFallback`).
  In similar windows (~11–12 G cycles):

  | Module | native | hook_fb |
  |---|---|---|
  | Windows clang C | 541,143,322 | 79,102,068 |
  | WSL LLVM + state-in-memory | 103,216,355 | 524,818 |

  `native` is `m_native_dispatches`, whose granularity differs by backend, so that column is not
  comparable. `hook_fb` is the same counter in the same runtime function and the cycle windows are
  similar, so the raw gap is real, but it compares Windows-C with WSL-LLVM and cannot separate backend
  from platform. The benchmark session prints a shutdown line for every module (except the crashing
  Windows LLVM one), so the same-platform C-vs-LLVM comparison falls out of the session logs for free.
  Read those before attributing the speed gap to it. See *Improvements* 1.
  **2026-10-06:** for the LLVM module, 95% of `hook_fb` turned out to be the `mtmsr` re-entry (now
  fixed, see *Status*); the count also includes cache ops that take the hook's fast path. The C
  module's 79M is still unexplained: run it once with `STATICRECOMP_FALLBACK_SAMPLES=1`.

## Environment notes

- `pgo-wsl.sh` needs LLVM 20 on `PATH` (the LLVM that DolRecomp links; `/usr/bin/clang` is 21):
  `apt-get install clang-20 libclang-rt-20-dev` (1:20.1.8-2ubuntu8) was done on 2026-10-05.
- DolRecomp `--targets` accepts `host, x86-64-v2, x86-64-v3, aarch64, aarch64-a57`. Every desktop
  LLVM module so far is plain `x86_64-pc-linux-gnu` / `x86_64-pc-windows-msvc`; `x86-64-v3` (AVX2)
  is untried.
- (2026-10-05) The other session's benchmark script was `scratchpad/bench-desktop.sh` in its scratchpad;
  it ran the matrix below into `build/bench-desktop.txt`. The late 2026-10-06 re-measure used a similar
  scratchpad script; its protocol is in NOTES *Desktop headroom*, raw lines in `build/bench-states-gp.txt`.

## Plan (adjusted; steps 1, 2 and 4 done 2026-10-06, step 3 open)

1. **Commit** the tooling and the local patch fix (one commit, no numbers yet).
2. **Benchmark session.** States 01 (cutscene) and 03 (field), three *interleaved* rounds, WSL and
   Windows run one after the other, never concurrently. Configurations:
   - Current runtime × every module above (WSL 4, Windows 3).
   - Pre-speed runtime × **one** module per platform only (WSL LLVM-sim, Windows clang). The patch is
     runtime-only, so more pairs just repeat the measurement. 54 runs at about 70 s each ≈ 65 min
     (the full cross product would be 84 runs, closer to 100 min).
   - Discard a warmup run per module (shader cache). Report median and min–max of the three rounds.
   - **Headline headroom = Windows, state 03.** The field is static; the cutscene's 30 s window
     covers different content at different speeds. WSL numbers are module-vs-module only: with the
     limiter off, llvmpipe's software rasterization grows with frame rate and steals cores from the
     game thread. If the WSL modules cluster, that is the llvmpipe ceiling. For a CPU-only upper
     bound on WSL, run with Dolphin's Null video backend (the runtime already knows how to set it,
     `lib/ModernGekko/src/runtime/dolphin_runtime.cpp` near line 664); it gives no screenshot, so keep
     one Vulkan run per module for the picture check.
3. **Fix the Windows LLVM exit crash** (the `ucrtbase` fast-fail). It gates Windows LLVM+PGO, which
   Android's data says is the best combination. The `+0x68392` access violation is a separate bug that
   also hits the plain clang module; fix it too, since it is what users would see. Then build Windows
   LLVM+PGO
   (`tools/module-llvm-win.sh NAME --profile-use <profdata>`) and add it to the table.
4. **Update development-notes.md *Platforms*** from the session (mark it provisional if step 3 is still open) and
   commit the numbers.

## Improvements, biggest lever first

1. **Chase `hook_fb` before more compiler tuning.** Add a per-opcode histogram to
   `HookInstructionFallback` and print it at shutdown. Either model the top offenders in the C backend
   or make LLVM the only shipped backend. Cheaper than another PGO round, probably worth more.
   **Half done (2026-10-06):** the per-PC histogram exists (`STATICRECOMP_FALLBACK_SAMPLES`), and the
   LLVM side's main source is fixed. Left: the C module's breakdown, and on LLVM the lazy-FPU
   re-entries (~2%) and the battle outlier.
2. **Benchmark a fixed workload.** Record a Dolphin input movie (`.dtm`) through the cutscene, the
   first battle and some field walking, and measure wall time to finish it with the limiter off.
   The movie code is in the vendored Dolphin; single-core mode keeps it deterministic; the same movie
   runs on all three platforms. It also becomes the PGO training set, fixing the current overlap
   where the profile is trained on exactly the four states it is tested on (hold out at least one
   state until then).
3. **Reuse one profile across platforms.** The LLVM backend's IR profile is keyed on function names
   and control-flow hashes from the same DOL and options, so an x86-64 profile should apply to arm64
   and back. Test: build a WSL module with `--profile-use build/android/pgo/skies-android-gp.profdata`
   (the current device profile; the WSL one is `build/modules-llvm-pgo-gp/GEAE8P/pgo/merged.profdata`, same
   patches) and read the mismatch count. If it works, train once on desktop (fast, limiter off) and ship the
   profile to the Retroid.
4. **Matrix script with a CSV.** One row per run: platform, runtime, module hash (from the manifest
   path), state, round, mean, min, max, exit code. Interleave rounds, do the warmup, and generate the
   NOTES table from the CSV so provenance and spread survive.
5. **Pin the game thread.** The i7-12700K is hybrid (8P+4E). Unpinned, the thread can drift to an
   E-core or share a core with llvmpipe, and that noise is indistinguishable from a module
   difference. `taskset` on WSL, `ProcessorAffinity` on Windows (`build/affinity.ps1` exists).
   For users, at least high priority.
6. **Merge all raw profiles.** `pgo-win.sh` takes only the newest `.profraw`; merge every file from
   the session, weighted if one state matters more.
7. **Report spread** (median, min–max) in the table. The Android battle state used to swing
   0.4–1.1× within a run (that was the `mtmsr` bug, fixed); a bare mean still hides whether 0.03 is real.
8. **Desktop hotspot profile.** Android's simpleperf breakdown drove the speed patch; desktop has no
   equivalent yet. Use Windows Performance Recorder on the Windows build (or `perf` on WSL if the
   kernel allows) with the LLVM module to see whether the runtime/game-code split matches Android.

## Still open

- Audio by ear, memory-card saving, a physical pad by hand (button capture in the launcher, controller 1
  moving to the pad pressed, Steam Input; the keyboard was checked on WSL and Windows 2026-10-17,
  HANDOFF-input.md), heavy scenes (big battles, world map) on desktop. Android passed all but an in-depth audio check by hand on 2026-10-07 (HANDOFF-android.md
  step 4); its world map and battles held 30 fps with less headroom than desktop has, so desktop heavy
  scenes are very likely fine too.
- Whether `moderngekko-run.fastpath` / `.orig` on WSL still matter; delete if not, to avoid confusing
  `RUNTIME=` comparisons.
- ~~Making `build/modules-llvm-pgo-v2` the WSL module~~: superseded by `build/modules-llvm-pgo-gp` (active).
  The Windows LLVM module with the `mtmsr` and gather-pipe patches is still open (blocked by its exit crash).
- WSL numbers on 2026-10-06 ran ~5–10% below the 2026-10-06 morning table (04: ~1.06–1.10 vs 1.18),
  for both old and new modules; host load or llvmpipe variance. Compare within a session only. Late
  2026-10-06 the same showed between rounds: 04 1.19 in round 1, 1.31 in round 2.
- ~~The launchers' default modules predate the gather-pipe patches~~: done 2026-10-07, `tools/play.sh` now
  defaults to `build/modules-llvm-pgo-gp` and `tools/run-win.bat` to `build\modules-llvm-pgo-fix`.
