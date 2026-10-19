# Handoff: the macOS port (Apple Silicon)

**Status (2026-10-19): on `dev` (`6d54f6f` the port and its fixes, `04bc86d` CI); not in a release
yet.** Builds, boots and plays both discs at full speed; benchmarked (NOTES *macOS*); the Sys path,
Bluetooth pad and keyboard bugs are fixed; CI builds the app zip (validate.yml `macos`, first run
39341292040) and release.yml ships it with the next tag. Worked on from a Mac session (Apple Silicon,
macOS 26.3, Command Line Tools only, no Homebrew) in the main checkout
`~/Projects/SkiesOfArcadiaLegends-Recomp`, with answers from the Windows/WSL session. Build
steps: BUILDING *macOS*; patches: `patches/README.md` (`moderngekko-macos`, `recompcore-macos`,
`recompcore-menu-close`, `dolrecomp-macho-sections`); developer notes: NOTES *macOS*. Open: the PS button's Game Overlay, a player check of the launcher,
Developer ID signing / notarization, and a macOS PGO profile (*Open* below).

## What it is

- `Skies of Arcadia Legends.app` (arm64, macOS 13+), in `build/SkiesOfArcadiaLegends-<version>-macos.zip`
  (`tools/build-release-macos.sh package`): the launcher (`SkiesOfArcadiaLegends`), the runner
  (`skies-run`) and both modules (`gGEAE8P_recomp.dylib`, `gGEAP8P_recomp.dylib`) in `Contents/MacOS`;
  `Sys`, `patches/*.dolpatch`, the launcher background and `CREDITS.md` in `Contents/Resources`
  (Dolphin's macOS `GetSysDirectory` is `<bundle>/Contents/Resources/Sys`). Signed ad hoc (Apple
  Silicon runs nothing unsigned), not notarized: Gatekeeper asks once (README *Getting Started*).
- Data folder `~/Library/Application Support/Skies of Arcadia Legends` (`tools/user_directory.cpp`).
- Graphics: **Metal** by default, OpenGL as the fallback (`DEFAULT_GRAPHICS_BACKEND`); no Vulkan
  (MoltenVK needs the full Xcode). Audio: Cubeb.
- Codegen: `TARGETS=macos tools/ci-codegen.sh` → `DOLRECOMP_LLVM_TARGET=arm64-apple-macosx13.0`,
  `DOLRECOMP_LLVM_CPU=apple-m1`, `--targets=aarch64`; no PGO (no `pgo/macos.profdata` yet).
  Modules: `tools/module-macos.sh`.

## Checked (2026-10-18)

Through `skies-run --automation-dir` (`tools/drive.py`) with a scratch `HOME`, and by the user:

| Check | Result |
|---|---|
| Import (`--extract`), USA and European RVZ | ~4 s each, DOL patched and pinned |
| Boot → intro → title → new game, USA | correct picture (Metal, 16:9, 2560x1440), text readable |
| Speed, 30 fps / 60 fps (interpolation), limiter on | 1.00x at 29.97 / 59.92 fps, USA and EU |
| Save state save + load (`save:` / `load:`) | works, keeps running at 1.00x |
| Module | `fallback=0`, `smc_failed=0` over 470M native calls |
| Launcher → game (user) | imports the disc, starts the game; profile gets the Skies layout + `SDL/0/DualSense Edge Wireless Controller` |
| Quit from the Dock / Apple Event quit | clean exit (was an abort, below) |

Not measured: headroom with the limiter off (`EmulationSpeed = 0`). A window behind other windows runs
slower (~0.67x, Metal drawables throttled for occluded windows): automation runs need the game window
visible.

## Found and fixed

- **Mach-O section names** (`dolrecomp-macho-sections.patch`): DolRecomp's cold FP fix-ups asked for
  ELF section `.text.unlikely.<name>`; LLVM refuses that for Mach-O (18 USA chunks failed).
- **Launcher link** (`moderngekko-macos.patch`): `Common::LoadPNG` didn't link; libc++ tags
  `UniqueBuffer`'s deleter with the exceptions setting (`[abi:nqn…]` in `-fno-exceptions` libcommon vs
  `[abi:nqe…]`). The background is decoded with `SDL_LoadPNG` on macOS.
- **Bundle layout**: `SDL_GetBasePath` is `Contents/Resources` in a bundle; the launcher finds its
  sibling programs with `_NSGetExecutablePath` (`ProgramDirectory`), the runner reads its data from
  `Contents/Resources` (`DataDirectory`) and loads the module beside itself.
- **Keyboard** (`recompcore-macos.patch`): Quartz key names in `SkiesBindings` (`Up Arrow`,
  `Left Control`, `Tilde`; Insert has none), default device `Quartz/0/Keyboard & Mouse`. The window
  doesn't pass key events without Command on while Skies owns the keyboard (they beeped and reached
  the menu's F1–F12 state shortcuts); the Quartz source reads the HID state, so the game still sees them.
- **Quit crash**: a quit from outside the window's menu ran `terminate:` → `exit()` with emulation
  running; cubeb's async logger thread aborted in its static destructor (crash report 13:34).
  `applicationShouldTerminate:` now stops the platform and cancels AppKit's termination.
- **Dead controls after the in-game menu** (`recompcore-menu-close.patch`, every platform): the
  Closing state waited for the sticks too, so a player still holding the stick got no input at all
  (keyboard included) until letting go. Now only A, B, X, Y, Z, Start and L / R wait.
- Scripts for bash 3.2 and BSD tools: `tools/apply-patches.sh` / `ci-codegen.sh` (empty arrays under
  `set -u`, `realpath -m`, `nproc`, `sha256sum`).

## Open

1. ~~**Left doesn't register in the in-game settings menu**~~ **Fixed 2026-10-19** (`recompcore-macos.patch`,
   user-confirmed): without Input Monitoring, `CGEventSourceKeyState` shows no real key as held but
   reports key code 0 (`A`, bound to stick-Left) as held while the app is in front; the stick sat at
   27 and the menu's new-press logic never saw Left. The same phantom key kept the button prompts
   flipping to the keyboard. The Quartz keyboard now takes its keys from the window's own events when
   `CGPreflightListenEventAccess()` is false (NOTES *macOS*). The temporary logging is removed.
2. **PS button** opens macOS's Game Overlay (log: "Game Overlay Presented"; the game resigns active and
   with `BackgroundInput = False` ignores input until it's dismissed). SDL disables system gestures for
   bound buttons on open and re-enables them on close. README says to avoid it; a real fix would be
   to keep the gestures disabled for the session.
3. ~~**Input Monitoring**~~ not needed since 2026-10-19 (item 1); README's tips still say where to allow it.
4. **Launcher by hand**: Select Disc / Change Disc, Settings tabs, Controls remapping, the data folder
   move (SDL's folder picker), Continue from a card save — the user started the game from it once.
5. **Release**: CI builds it since 2026-10-19 (validate.yml `macos` job on `macos-15`: codegen makes
   `codegen/macos{,-eu}` on Linux, the job links the modules and packages the app; release.yml ships
   the zip and checks it like the other packages). No Developer ID signing or notarization (backlog
   *macOS release*). PGO profile for macOS not trained (`--profile-generate` run on the Mac, like
   Android's device run; trying `pgo/android.profdata`, the same aarch64 code, is a cheap first step).
7. **Two speed bugs found by the 2026-10-19 benchmark (NOTES *macOS*), fixed in `recompcore-macos.patch`
   (build `v0.50.0-beta-fix`, not released; the user played with the pad after the fix, item 1):**
   Dolphin's `Sys` resolves to `<app>.app/Sys/` (`LINUX_LOCAL_DEV`), so the app runs without the game INI
   (idle skip, dual core), fonts and DSP coefficients; and a Bluetooth pad halves the speed (SDL hotplug thread
   holds the joystick lock; `SDL_AUTO_UPDATE_JOYSTICKS = 0` fixes it, real-pad check open). With both worked
   around: 2.90 / 4.41 / 3.90 / 3.88× (01–04, 1×), worst second 2.85×; 2.63× worst at 4× + 60 fps.
6. Dolphin's menu items still set `NSEventModifierFlagFunction`, which AppKit ignores with a log line
   per item at start (the shortcuts become Command-F1… instead); harmless.

## Running on the Mac

- Commits from the Mac use the ogdanimal noreply identity (global git config since 2026-10-19).
  Pushing workflow changes needs the `workflow` scope on gh's token (`gh auth refresh -s workflow`,
  done 2026-10-19).

- Toolchain without admin rights: `python3 -m pip install --user cmake ninja`; LLVM 20.1.8 in
  `~/.local/opt/LLVM-20.1.8-macOS-ARM64`; a static zstd from Dolphin's Externals in `~/.local/opt/zstd`
  (BUILDING *macOS*). DolRecomp: `build/dolrecomp`. Codegen: `codegen/macos{,-eu}`. Modules:
  `build/module-macos{,-eu}`. Release build: `lib/ModernGekko/build-release-macos`.
- The game's DOLs come from the private secrets repo cloned beside the checkout; disc images stay
  outside every repo.
- Automation: `HOME=<scratch> ".../Contents/MacOS/skies-run" --game <user>/games/GEAE8P --user-dir <user>
  --automation-dir <dir>`, then `AUTO=<dir> python3 -I tools/drive.py …`. `config:Dolphin.Skies.FrameMode=1`
  turns 60 fps on live. Dolphin's log: `Config/Logger.ini` (`WriteToFile = True`, `Verbosity = 2`,
  `[Logs] CORE = True`) → `Logs/dolphin.log`; the user's data folder has it on now.
- The terminal has no Screen Recording permission: use `shot:` through automation, not `screencapture`.
