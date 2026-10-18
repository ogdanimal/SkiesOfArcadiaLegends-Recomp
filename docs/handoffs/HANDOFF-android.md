# Handoff: Android build of the Skies recomp (2026-10-05, updated 2026-10-17)

## European PGO profile (2026-10-17 night: in progress, paused by the user after step 1)

**Progress.** Step 1 partly done: `eu-05-pirate-base-ws23`, `eu-07-red-alarm-ws23` and
`eu-10-worldmap-ws23` are saved in `build/savestates/` (each loaded from the ws22 state, 4 s, loader
state 5, game mode 6; screenshots `build/android/bench/statemake-eu-*.png`). Still to make:
`eu-02-worldmap-battle`. Step 3 is built but **not installed**:
`build/android/apk/skies-ws23pgo-gen-2026-10-17.apk` (instrumented European module + the ws23 USA
module, staged in `build/android/jniLibs-ws23pgo-gen-src/`; outputs `llvm-gen-ws23pgo-GEAP8P`,
`module-ws23pgo-GEAP8P`, log `build/android/ws23pgo-gen-build.out`). The debug app still runs the
plain ws23 APK; `files/PGO/` still holds four old USA `.profraw` files (move them aside before
training). The Retroid was lent to the HD texture session (`hdtex`) during the pause. **It installed
`skies-hdtex-2026-10-17.apk` (plain ws23 modules from `jniLibs-ws23`, data kept) over the debug app:
reinstall the ws23 test APK before going on.** It left the DOLs (ws23), `files/PGO/`, the card saves
and `stay_on_while_plugged_in` (still `2`: set it back to `0` at the end) alone; its texture pack is
removed and `GFX.ini HiresTextures = False`. The Downloads folder holds the two pack `.7z` files.
**Later the same night the Match screen check installed `skies-ws23ms-2026-10-17.apk`** (the `aspect`
branch, plain ws23 modules from `jniLibs-ws23`, data kept) over it: still reinstall the ws23 test APK
(or any build of current `dev`) before going on. That check left the screen size reset (`wm size`
shows no override), `[Skies] Widescreen = 1` and the app stopped (development-notes *Match screen*).

Why: the European game runs 0.930 (55 fps, two runs) in `eu-17-ship-battle-baltor` against 1.003 for
the USA one on the same APK (status below). The USA module has the device profile; the European one
has none (profiles are keyed by fixed address chunks, so the USA one can't be reused). Train on
**ws23** (`b1493b4`); a later game patch change costs only the changed functions (`hash mismatch`).

Setup already on the Retroid's debug test app (`org.dolphinemu.dolphinemu.debug`): ws23 APK, ws23
DOLs in `files/Games/SkiesOfArcadiaLegends` and `files/Games/GEAP8P` (`ISOPath1`; the app's game list
must have been opened once after adding it), the European card saves (`8P-GEAP-SA_LEGENDS.000-002`
in `GC/USA/Card A`). Boot it with `GAME=GEAP8P tools/android-boot.sh` (`SERIAL=<the device's serial>`,
`ADB_AUTO=/sdcard/Android/data/org.dolphinemu.dolphinemu.debug/files/Automation` for drive.py).
European addresses for peeks: game mode `0x8034ACEC` (6 field, 7 battle), battle state `0x8034AA94`
(4+ running), resident loader state `0x8034AF50` (5 = loaded).

1. **States, on ws23.** Load `eu-05-pirate-base`, `eu-07-red-alarm`, `eu-10-worldmap` (ws22-made: wait
   ~3 s for the resident files, then save them again as `eu-*-ws23.sav` so training doesn't start with
   the repair; **done**). Add a battle: from `eu-10-worldmap-ws23` hold `main_y=1` in 120-frame steps until the mode
   is 7 and the battle state 4+ (2-4 steps), wait 3 s, save `eu-02-worldmap-battle.sav`. Optional: an
   opening state (no card save: title, New Game, into the opening cutscene). Keep
   `eu-17-ship-battle-baltor`. Five or six states, like the USA set (01-04 + red alert + world map).
2. **Baseline** with the current APK (no European profile): `tools/bench-android.sh ws23eu-base
   eu-...` with `GAME=GEAP8P` exported (bench boots fresh per state). Two rounds (device noise: never
   one run).
3. **Instrumented European module.** As *Retraining* further down this file, with
   `GAME_ID=GEAP8P` and the ws23 European DOL: `dolrecomp ... --profile-generate 'skies-%m.profraw'`
   into `build/android/llvm-gen-ws23pgo-GEAP8P` (copy `main.dol` into `generated/`), cmake with
   `-DGAME_ID=GEAP8P -DCMAKE_SHARED_LINKER_FLAGS="-fprofile-generate -Wl,-z,max-page-size=16384"`,
   ninja, strip to `libgGEAP8P_recomp.so`. Put it beside the ws23 USA module in a new folder
   (`build/android/jniLibs-ws23pgo-gen-src/arm64-v8a/`, copy `libgGEAE8P_recomp.so` from
   `jniLibs-ws23-mods`) and build the APK with `NAME=ws23pgo-gen JNILIBS=build/android/jniLibs-ws23pgo-gen-src`
   (`JNILIBS` must not be the script's own `jniLibs-NAME`, here `jniLibs-ws23pgo-gen`: it deletes that
   first). Install with `adb install -r -t`.
4. **Train.** Move old `files/PGO/*.profraw` aside. Per state: `GAME=GEAP8P tools/android-boot.sh`,
   `drive.py load:... wait:16`, then `speed:30` (cutscene, battles) or walking for the field states
   (`hold:main_y=1:150 hold:main_x=1:150 hold:main_y=-1:150 hold:main_x=-1:150 speed:20`; world map:
   fly), then `stop` (writes one `.profraw` per state). Pull `files/PGO/`, then
   `llvm-profdata-20 merge -o build/android/pgo/skies-android-eu.profdata <profraws>`.
5. **Profiled build.** `NAME=ws23 tools/android-ws-build.sh` picks up `build/android/pgo/skies-android-eu.profdata`
   for the European module by itself (USA keeps `skies-android-gp.profdata`). Check
   `build/android/llvm-gen-ws23-GEAP8P.log` for `hash mismatch` (should be few). Install, then bench
   the same states two rounds: the target is >= 1.00 everywhere, like the USA set.
6. **Ship it.** Copy the profile to the private secrets repo as `pgo/android-eu.profdata` (the user's
   call; `tools/ci-codegen.sh` uses it when present, else no PGO), then a validate run. A desktop
   European profile (`tools/pgo-wsl.sh` on the European DOL -> `pgo/windows-eu.profdata`) is separate.

Gotchas: WSL's `/tmp` is a RAM tmpfs that other sessions fill; Gradle then fails with "No space left
on device": build with `JAVA_TOOL_OPTIONS=-Djava.io.tmpdir=$HOME/.cache/gradle-tmp
GRADLE_OPTS=-Dorg.gradle.daemon=false`. The Retroid's swipe lock screen stops boots: set
`settings put global stay_on_while_plugged_in 2` for the session and back to `0` after
(`android-boot.sh` dismisses the keyguard). `[Skies] AutoLoadCard = N` in `Config/Dolphin.ini` loads
card file N at boot once (the runtime clears it). Only Windows' `adb.exe`, never WSL's `adb`. The user
plays on the Retroid: ask before using it.

## Status (2026-10-17, night): data folder (branch `datadir`, merged into `dev` 2026-10-18)

Settings → Disc → Data folder (HANDOFF-datadir, development notes *Data folder*,
`patches/recompcore-data-folder.patch`). Tested on the Retroid with a **separate test app**,
`tools/android-ws-build.sh BRAND=1 TEST_ID=1` → `com.skiesofarcadia.recomp.test`: the plain branded
debug build is `com.skiesofarcadia.recomp`, the release app's ID with another signature, so it must
never be installed over the user's release app. The test app was uninstalled afterwards; the device is
as before (0.9.1 release app untouched). Lessons: `uiautomator dump` fails on the immersive launcher
("null root node"), so tap by coordinates from `screencap`; take All files access back with
`appops set --uid PKG MANAGE_EXTERNAL_STORAGE default` (the package mode alone leaves the uid mode
`allow`); writes in shared storage go through MediaProvider (warnings like "Database update failed
while renaming"), so a moved folder is slower to copy into than app storage. Dolphin's
`DocumentProvider` root follows the user directory, so it shows the moved folder. Untested: an SD card
(none in the device).

## Status (2026-10-17, late night): HD textures on the Retroid (branch `hdtex`)

The texture pack import checked in the debug app (HANDOFF-hdtextures *Android / Retroid*): two fixes
came out of it, both in `recompcore-hd-textures.patch`. mGBA (built only in the Android app) links an
older LZMA SDK with the same names, so the import's `.7z` reader called mGBA's and the app was killed
for memory: our copy now has `Skies7z_` names. The stock Adreno driver samples BC7 as noise:
`DriverDetails::BUG_BROKEN_BPTC` skips BC7 (Vulkan, Qualcomm, Android). PNG pack: first battle 1.003x.
Tool note: `simpleperf record --app <pkg> -e page-faults -g` works on a debuggable app without root
(`debuggerd -b` needs root here).

## Status (2026-10-17, late): v0.10.0-beta

- **Release APK** (HANDOFF-release *Released*): both games' modules (`libgGEAE8P_recomp.so`,
  `libgGEAP8P_recomp.so`, built by CI with `PREGEN` / `PREGEN_EU`) and both `.dolpatch` files; the
  import picks the region from the disc ID and keeps one game at a time (`jni/SkiesImport.cpp`, NOTES
  *The European release*). Signed with the 0.9.1 key, so it installs over 0.9.1; the app repatches the
  imported 0.9.1 DOL (ws20) to ws22 from its `main.dol.orig`.
- **60 fps on ws22 with the existing `android.profdata`: passed** (debug test app,
  `build/android/apk/skies-ws22-2026-10-17.apk`, `tools/bench-android.sh`, two rounds: red alert
  1.004 / 1.004, world map 1.007 / 1.006, first battle 1.003 / 1.003, ship field 1.004 / 1.004,
  59.8-59.9 fps; `build/android/bench/ws22-60fps-r{1,2}`).
- **European speed (2026-10-17 evening): misses 60 fps without a profile.** `skies-ws22eu-2026-10-17.apk`
  on the debug test app, European game in `files/Games/GEAP8P` (ISOPath1 in `Config/Dolphin.ini`;
  `Dolphin.ini.pre-eu` is the old one; open the app's game list once so it rescans), boot link
  `dolphinemu://app/play/0/GEAP8P`: `eu-17-ship-battle-baltor` 0.930 / 0.930 (55.3 fps, two fresh
  boots), the USA `17-ship-battle-baltor` on the same APK 1.003 (`build/android/bench/ws22eu-r{1,2}`).
  Picture right. Next: a European Android profile (`pgo/android-eu.profdata`, trained on the device).
  Only one European state exists, so training needs more European states first. Still untried on a
  device: Settings → Sound → Import a Dreamcast disc (CHD) and a European disc import (release app).
- **ws23 (2026-10-17 night):** the debug test app runs `skies-ws23-2026-10-17.apk` with the ws23 DOLs in
  both game folders (`sys/main.dol.ws22` beside them). European states `eu-05-pirate-base`,
  `eu-07-red-alarm`, `eu-10-worldmap` (card saves 3 / 1 / 2 through `[Skies] AutoLoadCard`, one shot)
  were made under ws22 and carry the empty resident table; ws23 repairs it in ~1 s after loading
  (development-notes *Memory card auto-load*). `tools/android-ws-build.sh` with `JNILIBS` set to its
  own `jniLibs-NAME` deletes it before copying: use another folder.
- **The Retroid** runs the **0.9.1 release app** with the user's data (0.10.0-beta not installed
  there yet; ask before updating it) and the debug test app with the ws22 APK.
- **Input (0.10.0-beta; HANDOFF-input.md, NOTES *Keyboard and button mapping*):** Select opens and closes
  the in-game menu (the `[SkiesHotkeys] Menu` hotkey, beside Z+Start); **Controls → Button mapping** in
  that menu changes any button (Dolphin's `InputDetector`, so Android names; shown as "Left stick up",
  "L2"); Invincible is the `Button L3 | Button R3` hotkey. The game adds `Options/Always Connected = True`
  and `[SkiesHotkeys]` to the profile on its first start; `ensurePadProfile` (projects-ac's, every launcher
  start) still fills the buttons, sticks and Z, and the two don't touch each other's keys. The
  controller's A and B can be changed but not cleared (otherwise nothing could work the menu). An
  Android keyboard (USB / Bluetooth) gets the keyboard layout when the game sees one (an Android device
  with letter keys, not "Virtual"); not tested, the Retroid has none. Checked on the Retroid in the
  debug test app with real controller events (`adb shell sendevent /dev/input/event8 ...`; adb's
  `input keyevent` comes from the "Virtual" device and doesn't count): stick, Select, rebind + reset, L3.

## Status (2026-10-17): release APK fixes, controller Z, Back up saves

All in `recompcore-android-lifecycle.patch` (the last patch then; it now also covers `SkiesImport.cpp`'s
`ensurePadProfile` and `SkiesSettingsPanel.kt`) or in the build scripts. Release story: HANDOFF-release
*Released*.
- **Release APK was test-only** (players: "App not installed"): `tools/android-ws-build.sh` passes
  `-Pandroid.injected.build.abi=arm64-v8a`, and AGP marks any APK built with an injected ABI
  `android:testOnly="true"`; only `adb install -t` installs those. Now also
  `-Pandroid.injected.testOnly=false` (checked on the merged release manifest), and `tools/sign-apk.sh`
  refuses a test-only APK. `tools/sign-apk.sh` also reads the signer from build-tools 37's apksigner
  (`V3.0 Signer:` instead of `Signer #1`).
- **Controllers had no Z**: Dolphin's bundled `SDL Gamepad.ini` maps Z to `Shoulder R`, an SDL name;
  Android's inputs are `Button R1` / `Button L1`. `ensurePadProfile` (every start) now puts Z on
  `Button R1` (the right bumper, as on desktop) when Z is empty or a `Shoulder` name, and keeps a Z the
  player mapped (the Retroid's `Button L1`). On a fresh install there's no `GCPadNew.ini` yet (Dolphin
  keeps its defaults in memory and writes the file only when settings are saved), and the old code wrote
  only the sticks, D-pad and triggers, so the face buttons loaded empty: it now writes A / B / X / Y /
  Start too when missing. Retroid: fresh install -> full profile with Z on R1; a planted `Shoulder R` ->
  R1; the user's L1 kept.
- **Back up saves** (Settings → Saves): the memory card saves (every region's `Card A`) and the save
  states (not `lastState.sav`) go into one `.zip` through `ActivityResultContracts.CreateDocument`, each
  entry dated as its file. **Import a save** recognises a `.zip` (`PK\3\4`) and imports each file in it
  through the normal import (content-detected, so names don't matter), oldest first: imported states are
  dated at import time, so the newest one stays the one Continue loads. Retroid: 7 card saves + 4 states
  backed up, wiped, imported: 11 of 11, byte-identical, newest import = the 08:27 auto-save. Before this
  there was no way for a player to keep saves through an uninstall (app storage is closed to file
  managers, and Dolphin's Export User Data is hidden in the Skies build).
- Restoring app data by hand: `adb pull -a` / `adb push -a`, which keep file times (Continue picks the
  newest file). The release build isn't debuggable, so its internal storage (`run-as`) can't be backed
  up; it only holds touch-overlay positions and first-run flags.
- Building Android in the shared checkout picks up other sessions' working-tree changes (2026-10-17: the
  DC-music session's audio code; it broke arm64 twice, `regparm` and a hex escape, both fixed by them).

## Status (2026-10-16): background / foreground tested; three fixes (`recompcore-android-lifecycle.patch`)

Tested on the Retroid (state saved first, user OK): Home / app switcher / screen off and back, in a menu,
the opening cutscene and a battle, 60 s away, six fast toggles, Home during boot, Android killing the
process in the background (restored from Dolphin's `temp.sav`), the Z+Start menu open (it doesn't pause
the core), audio (AudioTrack paused, then started again). All of those resumed at ~59.9 fps. Broken, now
fixed and checked on the device with `skies-recomp-ws20-bgfix-2026-10-16.apk` (Gradle-only build on the
ws20 module, installed over the app, data kept):
- **Pause (Back menu), Home, back: black game area** until Resume. Nothing presents while paused, so
  the new surface stayed empty. `MainAndroid.cpp` SurfaceChanged: when paused in dual core, an
  `AsyncRequests` event makes the GPU thread `Present()` the held XFB again. (Dolphin's "Saved State to
  temp.sav" OSD from `onSaveInstanceState` then stays on that still picture until Resume.)
- **Exit Game after a process kill + restore: second boot, then SIGSEGV** in `CreateAndroidSurfaceKHR`
  (upstream `EmulationFragment.kt`: `if (loadPreviousTemporaryState)` lacked an `else`).
- **Task-clearing relaunch** (`FLAG_ACTIVITY_CLEAR_TASK`, as some frontends do) destroys EmulationActivity
  but the game keeps running, paused; `launch()` dropped every request (the session holds
  `ignoreLaunchRequests`). The launcher now shows Continue "Game in progress" (`resumeSession`), and
  Start Game / Change Disc / Import a save stop the session first (`afterGameClosed`).
Test helper: `cycle.sh NAME home|recents|screen [seconds]` (scratchpad, not kept): logcat, status.txt,
two screenshots compared. Return to the app with the LAUNCHER intent + `-f 0x10200000` (EmulationActivity
isn't exported, so `am start -n` on it fails). Uncommitted at time of writing.

## Decision (2026-10-11, user agreed): keep Dolphin's hardware layer; own app shell before release

The Android app is a real recompilation port (the game's code runs from `libgGEAE8P_recomp.so`, not
Dolphin's CPU emulation) on Dolphin's hardware code (`libmain.so`: video to Vulkan, DSP audio, DVD,
EXI), inside Dolphin's Android app with its UI hidden. N64 recomps replace the OS layer instead
(ultramodern + RT64); for GameCube, nearly every static recomp (DolRecomp / ModernGekko projects,
RecompCore, dolbundler) is Dolphin-based, and only gcn-recomp (own runtime, OpenXR headset APK) and a
Windows-only Wind Waker project write their own hardware layer. So: **don't replace the hardware layer**
unless a concrete problem needs it. **Before release:** the app's own shell: its own package / class
names, Dolphin's unused screens and resources removed, no "Dolphin" left where a player can see it.

## Status (2026-10-12, evening): ws20 on the Retroid

- `build/android/apk/skies-recomp-ws20-2026-10-12.apk` (versionCode 410419) installed 20:20 with the
  user's OK, saves md5-identical before / after (backup before ws19:
  `C:\dev\SkiesRecomp\build\backup\retroid-before-ws19-2026-10-12`). Game patch ws20: the battle
  freeze from older states fixed (ws19) and Continue loading a card save directly (ws20). User: "all
  good on retroid". Left on the device from an earlier session: `files/Automation` in the app's
  external data folder (the user can delete it).

## Status (2026-10-12, morning): Import a save installed and checked on the Retroid

- **Installed (user: "You can test directly on Android"):** `build/android/apk/skies-recomp-ws16-saveimport-2026-10-12.apk`
  (versionCode 409742, debug-signed, same app as below, saves kept: `adb install -r -t`). Built with the
  Gradle-only rebuild (module unchanged, `-PskiesBrand=$A/brand-ws16`).
- **What it adds:** Settings, Saves: "Import a save" (prompt, then the system picker; the picked document is
  copied to the app's cache and imported by `SkiesNative.importSave`, the desktop launcher's
  `tools/save_import.cpp`), "Your saves" (card files, state count) and the result line. Refused while
  `NativeLibrary.IsRunning()`. NOTES *Importing saves*.
- **Checked on the device:** backup first (`C:\dev\SkiesRecomp\build\backup\retroid-before-saveimport-test-2026-10-12`:
  GC, StateSaves); a European save refused; a card image imported files 2 and 3 and Continue booted into
  file 2 by itself (59.9 fps); a copy of slot 1 imported as a state and started from Continue; with the
  pad, A on "Choose a save file..." opens the prompt and A again cancels. Then the test files removed:
  the user's file 1 and states md5-identical to the backup, Continue back on their 22:13 auto-save.
- **Device tips:** the picker's Downloads view goes by the media index and doesn't list files pushed with
  adb: open the device root ("Retroid Pocket 5") -> Download. The lock screen only needs A (no PIN).
  Settings, Saves at 1920x1080: Saves tab 986,268, Choose a save file 738,541, the prompt's Choose a file
  1371,766, Close 1684,158. In touch mode the prompt's focused Cancel isn't drawn highlighted (pad A still
  takes it).

## Status (2026-10-12, after midnight): "Skies of Arcadia Legends" installed, saves moved

- **Installed (user OK):** the branded test build `build/android/apk/skies-recomp-ws16-2026-10-12b.apk`
  (`com.skiesofarcadia.recomp`, "Skies of Arcadia Legends", dev build 409209, **debug-signed**; the
  release-signed APK needs the key's password, so it comes from CI or the user's terminal, and replacing
  this one needs an uninstall: back up and restore the saves again). The old app (previous ID, "Skies
  Recomp") is **still installed, untouched**, until the user says to remove it.
- **Saves moved:** backup first (`C:\dev\SkiesRecomp\build\backup\retroid-before-rename-2026-10-12`:
  GC, StateSaves, Config, GameSettings, 22 files); then on the device `Config`, `GC`, `StateSaves`,
  `GameSettings`, `Games` (the imported disc, 1.3 GB, so no re-import) and both shader caches
  (`files/Shaders`, `cache/Shaders`) copied from the old app's folder: 5,592 files, md5 identical. The
  device's `cp` doesn't keep file times, and Continue goes by them (it showed slot 1 at the copy time):
  the times were copied over with `touch -r`, and Continue shows the 22:13 auto-save as before.
- **Checked on the device:** home screen; Settings with the user's values, all seven tabs, Graphics
  Renderer (Vulkan) and GPU driver (shown: the loader works on the Adreno 650), About (Retroid Pocket 5,
  SM8250, Android 13, driver 0.746.0), Licences; Continue boots the auto-save at 59.9 fps; the Back menu
  has Pause / Touch Controls / Exit Game, and Exit Game returns to the launcher. Device tips: the lock
  screen comes back fast, so wake, `wm dismiss-keyguard` and act in one adb call; `input tap` works on
  the launcher (1920x1080: Continue 290,540, Settings 290,894, About tab 1288,268).
- Not tried yet: a real driver package, the Renderer switch in this app (OpenGL ES itself was checked in
  the dev app, NOTES *Slower devices and GPU drivers*).

## Status (2026-10-11, late): release prep, app ID renamed

- **App ID is now `com.skiesofarcadia.recomp`** (user's choice, matching their other ports' IDs; the
  old one carried a personal domain). Not built yet. The Retroid still has the branded app under the old
  ID: the next branded build installs as a second app. Before uninstalling the old one, copy its user
  dir (`Android/data/<old ID>/files`: `Config`, `GC`, `StateSaves`) into the new app's, with the user's
  OK; the backup from 10-11 is `C:\dev\SkiesRecomp\build\backup\retroid-saves-2026-10-11`.
- **Builds strip local paths:** `tools/android-ws-build.sh` passes `-ffile-prefix-map` (the 10-11
  APK's `libmain.so` carried the build machine's home path in ~420 source-file strings). The Windows
  release builds in `C:\dev`, which holds no user name (scanned clean).

## Status (2026-10-11, ~22:15): card auto-load installed; battle freeze tabled

- **Installed 22:15 (user OK; versionCode 409088):** `build/android/apk/skies-recomp-ws16-autoload-2026-10-11.apk` (Continue
  from a memory card save: the launcher sets `[Skies] AutoLoadCard`, the runtime drives the title /
  load screens; NOTES *Memory card auto-load*; checked on Windows; on the Retroid waiting for the
  user's first Continue). Also drops the temporary `SkiesMenuDbg` input log of the tabs build.
- **Battle freeze (open, tabled by the user 10-11):** the user changed battle speed several times during the Antonio fight (the
  2x step-down kicking in too) and the round froze with a damage number on screen. Their slot 1 is at
  the fight's start (copied to `build/savestates/19-retroid-antonio-start.sav`; the frozen moment,
  saved over automation, is `18-retroid-battle-softlock.sav`). Reproduced on Windows (NOTES *Known
  issues*). The user's own advice for now: change speed while choosing commands.

## Status (2026-10-11, ~20:45): Skies Recomp controls fixed, menu like the launchers, own version / icon

**Installed now:** `build/android/apk/skies-recomp-ws16-tabs-2026-10-11.apk` (versionCode 409003,
20:44, user OK). It still carries a temporary input log (`SkiesMenuDbg` in logcat, from
`SkiesMenu::FilterOpen`); the source and the patches don't (removed after this build), so the next
build drops it.

- **Controls:** in Skies Recomp only the face buttons worked: the app's `GCPadNew.ini` had Dolphin's
  SDL-style gamepad defaults (`Left Y+`, `Pad N`, `Trigger L`), which Android's backend doesn't name
  (NOTES *Known issues*). The user's profile was repaired by copying Dolphin Debug's `[GCPad1]` (backup
  `Config/GCPadNew.ini.sdl-backup` on the device). The app now fixes it on every start:
  `SkiesNative.ensurePadProfile()` (standard Android gamepad axes for the first gamepad with a stick;
  buttons and an Android mapping left alone). **Verified 21:12:** from the SDL backup the app rewrote the
  profile to the working mapping (logcat `SkiesLauncher`), and left it untouched on the next start.
  The device keeps `GCPadNew.ini.sdl-backup` and `GCPadNew.ini.good-backup` (safe to delete). Not yet:
  a second controller.
- **In-game menu:** the launchers' look (tabs, descriptions; `SkiesMenu.cpp`); L / R switch tabs, and
  Up from the first setting reaches the tab bar like the launchers. Device: automation check of D-pad
  / R passed; user 10-11: the menu is "perfect" (nothing left).
- **Version / icon:** the branded build sets its own `versionCode` (minutes since 2026-01-01) and
  `versionName` "Skies Recomp build N", so each build is a newer version: with Dolphin's fixed 43011 the
  Retroid's launcher (Radikal, `com.radikal.gamelauncher`) kept the old icon. Icon from Reckoner's logo
  (`release/android-icon/`, copied into `build/android/brand-ws16/res`). The user: it shows "in most
  places but the app drawer": Radikal's drawer shows the app's **banner** (`android:banner`, Dolphin's
  `banner_tv`), not its icon. The branded build now has its own banner
  (`release/android-icon/drawable-xhdpi/skies_banner.png`, 640x360: the launcher art cropped 16:9 from
  the right, a navy fade, the logo; made by `tools/make-skies-icon.sh`; manifest placeholder
  `appBanner`). Installed 20:58 (user OK): `skies-recomp-ws16-banner-2026-10-11.apk` (versionCode 409009, no input
  log); the user's states were copied to `C:\dev\SkiesRecomp\build\backup\retroid-saves-2026-10-11`
  first and were still on the device after the update (the app's memory card folder was empty). Radikal's
  drawer kept the old dolphin picture even then (its own cache); `adb shell am force-stop
  com.radikal.gamelauncher` (with the user's OK; it restarts on Home, nothing lost) fixed it: the logo
  everywhere now (user, 10-11).
- **User check after the controls fix + tab bar build:** "it works great now".
- **Shader compile at every start:** the Vulkan pipeline cache was only saved at a clean shutdown,
  which Android never gives (NOTES *Known issues*). `recompcore-vulkan-pipeline-cache-save.patch` saves
  it while running. `skies-recomp-ws16-pcache-2026-10-11.apk` (versionCode 409024) installed 21:09
  (user OK); **verified on the Retroid:** after a minute of play and a close from recents,
  `Vulkan-Pipeline-2C4DFF41.cache` was 3.1 MB (was 48 bytes) and the user saw the shorter compile at
  the next start ("it worked").
- **Full builds now:** `NAME=ws16 tools/android-ws-build.sh` (dev app `org.dolphinemu.dolphinemu.debug`,
  push the ws16 DOL by hand) or `BRAND=1 NAME=ws16 tools/android-ws-build.sh` (Skies Recomp,
  `build/android/apk/skies-recomp-ws16-<date>.apk`; copies `release/android-icon` into the brand dir
  when it exists; stops the Gradle / Kotlin daemons afterwards). The "Build and install" block further
  down is the ws13-era version of the dev-app flow.
- **Gradle-only rebuild** (module unchanged), what this session used each time:
  `cd lib/ModernGekko/vendor/dolphin/Source/Android && ANDROID_HOME=~/Android/Sdk ./gradlew
  :app:assembleDebug -Pandroid.injected.build.abi=arm64-v8a -PrecompJniLibs=$A/jniLibs-ws16
  "-PrecompNativeFlags=-mno-outline-atomics -ffile-prefix-map=$R/=" -PskiesBrand=$A/brand-ws16; ./gradlew --stop` with
  `A=build/android` (absolute) and `R` the repo root (the prefix map keeps local paths out of
  `libmain.so`), then `adb install -r -t` (the debug APK is test-only). Don't
  `pkill -f KotlinCompileDaemon` from a shell whose command line contains that text (it kills itself:
  exit 144).
- **Device etiquette:** the user plays on the Retroid while we work: ask before installing, injecting
  input or editing its files; never revert a setting change on it without asking (10-11: their own
  Battle speed / Encounters changes were mistaken for a test's and reverted, then restored).

### Before (2026-10-11, late night): device on ws16, branded app with disc import installed

**Settings in the branded app (installed 20:00, `skies-recomp-ws16-settings-2026-10-11.apk`):** the home
menu has **Settings**, a window like the desktop launcher's (`skies/SkiesSettingsPanel.kt`): rounded
card over the dimmed home screen, tabs Graphics (Picture, Frame rate, Resolution = EFB scale, Anti-
aliasing, V-Sync, Show FPS) / Gameplay / Sound / Controls (dead zone, rumble) / Saves (Auto-save) /
Disc, label + value + one-line description, A- / A+ (`[Skies] LauncherTextSize`), Close. Values go to the
in-game menu's keys through `NativeConfig` (base layer, saved at once); the pad profile through new JNI
`SkiesNative.getPadOption` / `setPadOptions` (`GCPadNew.ini [GCPad1]`, only an existing profile: it
appears after the first boot; Dolphin reloads the file at every boot). Choices open in place (Android's
Spinner popup ignores the pad's A), A picks, B / Back closes the list or the window, left / right on a
value steps through it. Checked on the device: opens from the pad, reads the real values (FrameMode 0,
Widescreen 1, resolution unset = Native), and the user's own changes in it (Battle speed 2x, Encounters
1/2) landed in `Dolphin.ini`. Built with Gradle only (module unchanged): the script's `BRAND=1` flags by
hand. **Full screen** (20:1x, `skies-recomp-ws16-fullscreen-2026-10-11.apk`, installed with the user's OK):
immersive like EmulationActivity (bars hidden, swipe shows them, re-applied on focus after the file
picker), cutout mode short edges, the art cropped to the whole display (`maximumWindowMetrics`).
Credits: background art I_LIKE_SONIC, app icon art Reckoner (README *Credits*, `release/CREDITS.md`).

**Installed 2026-10-11 19:52 (user OK; `adb install -r -t`, the debug APK is test-only):**
`skies-recomp-ws16-2026-10-11.apk`, rebuilt at 19:21 with the
desktop launcher's home screen (the cover art from `release/launcher-background.png`, bundled as
`assets/skies-background.png` by `BRAND=1 tools/android-ws-build.sh` and cropped to the screen's shape
from the left; a scrim; gold "Recompiled" subtitle over the title; Continue / Start Game / Change Disc at
the bottom left as text items with a gold focus highlight). The earlier import was kept: the app opens
on Start Game / Change Disc (screen capture checked). Still shows Android's status and navigation bars
(not immersive).

`skies-ws16-2026-10-11.apk` (dev app, ws16 DOL pushed; ws13 DOL backup `main.dol.fa7b1af1`) and
`skies-recomp-ws16-2026-10-11.apk` (branded "Skies Recomp", under the app ID it had before the 10-11 rename, built with
`BRAND=1 NAME=ws16 tools/android-ws-build.sh`; patch `recompcore-skies-android-app.patch`) are installed.
The branded app's first run picks the disc image (pad A opens the system picker), extracts it, applies the
bundled `skies-widescreen.dolpatch` and checks the DOL's SHA-256 in JNI (`SkiesImport.cpp`) before Play
shows: RVZ import ~11 s. An update with a new patch re-patches from the kept original; a ws15 import
(no original kept) needs a fresh import. Continue boots the newest state: Baltor at 1.01x. The build
script now stops the Gradle / Kotlin daemons afterwards (they held ~2.6 GB and helped starve WSL into
a global OOM on 10-11). Not done: the launcher icon (needs the user's logo file), dead zone / rumble on
the device's pad.

### Before ws16 (2026-10-11, night)


ws14 / ws15 (QOL options in ship battles, frontend `InShipBattle` + the 3x / 4x overclock, the ship
battle HUD at the edges, the polling-loop savestate fix; NOTES *The player build*) are checked on Windows
only. Android needs `NAME=ws15 tools/android-ws-build.sh` (the APK carries the new `SkiesMenu.cpp`) and
the ws15 DOL pushed with it; ask before installing. The device's own ws13 save states may hang in ws15
(caught in the old polling-loop hook): pull them and check with `tools/statecheck.py` first. Note for the overclock:
the Retroid tops out near 2.5x, so its step-down to 2x also drops the overclock.


**Installed (2026-10-11, late evening, user away with tests and installs OK):**
`build/android/apk/skies-ws13c-states-2026-10-11.apk`: save states in the menu (Save state / Load state /
Auto-save rows; Android had no player-usable states before), checked on the device: save in a battle, load
from the world map, the 18-row menu fits with its hint per row, auto-save costs one second at 0.94-0.99x.
StateSaves cleaned and the app stopped after the tests. Before it (user OK):
`build/android/apk/skies-ws13b-2026-10-11.apk` + DOL `fa7b1af1` (ws13:
camera, QOL options, Invincible, and the world map altitude gauge at the left edge like 4:3; the APK also has
the menu fix: the 15-row menu no longer pulses between two sizes at 1080p, HANDOFF-settings.md). Checked: on
the world map (`10-worldmap-android`) the gauge is at the edge at 59.85 fps / 1.005x; the menu's highlight bar
holds one size over 242 recorded frames and D-pad selection works. `skies-ws13-2026-10-11.apk` (same DOL,
before the menu fix) is superseded. To go back to ws12: push `files/Backup-2026-10-10/main.dol.7ad26f22` and
reinstall `skies-ws12b-2026-10-11.apk`, together. Older DOLs are backed up beside it (`.54afdf92` ws11 with
`skies-ws11-2026-10-11.apk`, `.7cb9de68` ws10, `.329c631f` ws9, `.a4f9ef7c` ws8, `.97adf61a`).

Debugging on the device that worked for the menu: `adb shell screenrecord --time-limit 4` + measuring an
element per frame (`ffmpeg -vf format=gray,crop=...` strips), and temporary `__android_log_print` lines read
with `adb logcat -d -s TAG` (remove them before the build that's kept). `adb exec-out screencap -p` includes
the OSD; `drive.py shot:` doesn't.

Build and install (always ask the user before installing: they play on the device):

```sh
NAME=ws13 tools/android-ws-build.sh        # module from build/game/GEAE8P-ws13 + APK (build/android/apk/skies-ws13-<date>.apk)
# runtime-only change, same DOL: gradle with -PrecompJniLibs=build/android/jniLibs-ws13 (see the script's last step)
adb install -r -t skies-ws13-<date>.apk
adb push build/game/GEAE8P-ws13/sys/main.dol \
  /sdcard/Android/data/org.dolphinemu.dolphinemu.debug/files/Games/SkiesOfArcadiaLegends/sys/main.dol
tools/android-boot.sh                       # then ADB_AUTO=... tools/drive.py (peek works over adb now)
```

The APK's module and the DOL on the device must come from the same build. All settings are in the in-game
menu (hold Z / L1, press Start) and in `files/Config/Dolphin.ini [Skies]`; the old `FrameInterpolation.on`
file is only read when `FrameMode` isn't set. Checked on the device: the 15-row menu fits at 1080p; camera
tilt / zoom / orbit; battle speed 2x at 1.00x (3x / 4x reach only ~2.6x / 2.5x, so the runtime steps them
down to 2x); Invincible keeps HP full (turned on mid-turn, the status box shows it only after the round,
same on every platform: HANDOFF-settings.md); 59.9 fps at 1.00x in the field and on the world map. Not checked:
the Retroid's L3 / R3 (the user), MP after casting with Invincible, ship battles. The battery gauge read 1% once (2026-10-11) and the kernel
then isolated the big cores (24% speed): check `cpu-isolate*` before blaming a build for low speed.

## Status (2026-10-11, night): camera control build installed (superseded)

**Installed (user OK):** `build/android/apk/skies-ws11-2026-10-11.apk` + DOL `54afdf92` (ws11, camera
control; HANDOFF-settings.md). To go back: push `files/Backup-2026-10-10/main.dol.7cb9de68` and reinstall
`skies-ws10-menu2-2026-10-11.apk`, together. The battery gauge reads normally again (94%, no isolated
cores); 59.9 fps at 1.00x on the world map and in the ship cabin.

## Status (2026-10-11): in-game menu build installed (superseded)

**Installed (user OK):** `build/android/apk/skies-ws10-menu2-2026-10-11.apk`, same DOL `7cb9de68` and
module as the settings build below, plus the Z+Start in-game menu (HANDOFF-settings.md). To go back:
`skies-ws10-settings-2026-10-10.apk`. That session the device ran at 23-24% because the kernel's battery
gauge read 1% and the limiter isolated the big cores (see HANDOFF-settings.md); recheck speed when it reads
normally.

## Status (2026-10-10, late night): settings build installed on the Retroid and checked (superseded)

**Installed (user OK):** `build/android/apk/skies-ws10-settings-2026-10-10.apk` + DOL `7cb9de68` (ws10).
Backups on the device in `files/Backup-2026-10-10/`: `main.dol.329c631f`, `Dolphin.ini.presettings`. To go
back: push `main.dol.329c631f` and reinstall `skies-ws9-notmemload-2026-10-10.apk`, together. Checked: first
boot wrote `[Skies] FrameMode = 1` (from `FrameInterpolation.on`) and `Widescreen = 1`; battle (state 02)
16:9 -> 4:3 -> 16:9 correct (`build/shots-android/settings/`), `FrameMode` 0 -> 30 fps, 1 -> 59.9 fps at
1.00x; ladder (state 15) three standard runs (`devladder.sh`, `PFX=settings`): 0 outliers, 0 green, 0
dark events each; a run switching both settings six times each while climbing (replay slices running,
dual core): no errors, 0 outliers, 0 green, all "dark" frames are the 4:3 pillarbox bar over the scorer's
corner; 1.00x afterwards. `drive.py peek:` isn't available over adb.

Original build notes:
Built: `build/android/apk/skies-ws10-settings-2026-10-10.apk`
(module `jniLibs-ws10`, made by a ws10 copy of `tools/android-ws-build.sh`) with the `[Skies]` settings
backend and automation `set_config` (HANDOFF-settings.md). Push the ws10 DOL with it:
`build/game/GEAE8P-ws10/sys/main.dol` (SHA-1 `7cb9de68`) to `files/Games/SkiesOfArcadiaLegends/sys/main.dol`
(keep the installed `329c631f` as a backup). Checks to run then: first boot writes `[Skies]` with
`FrameMode = 1` (from `FrameInterpolation.on`); live `config:Dolphin.Skies.Widescreen=0/1` and
`FrameMode=0/1` in dual core while a replay slice runs; the ladder (state 15) stays clean. Everything
below still describes the installed build.

## Status (2026-10-10, night): cloud-sea garbage fixed; installed on the Retroid (superseded)

**Installed on the Retroid:** DOL `329c631f` (unchanged) + `build/android/apk/skies-ws9-notmemload-2026-10-10.apk`
(runtime only). The replay no longer executes the game's TLUT loads and TMEM preloads from its recorded
stream (NOTES *Green bands at the bottom of the Valuan ladder ... / Cloud-sea garbage*): a slice inside the
game's frame used to copy today's RAM over the game's TMEM data between its load and its bind, and the
game's own cloud-sea draw got a texture decoded from the wrong bytes (one real picture plus the
in-between replayed from it). Device: six 15 s ladder runs, 0/0/0 + 0/0/0 events (0-5 per run before), 0 outliers, 0 green, 99-101% speed. Windows: see HANDOFF-desktop.md. **The user checked the ladder by eye on this build and called it resolved.** The deck
billboard glitch: user saw none (10-11), closed. To go back: `skies-ws9-encode-2026-10-10.apk`.
Also in: TMEM unit state saved with the replay's video state and the game's bound textures restored
after each slice (harmless, not the cause), and debug switches `FrameInterpolation.copytrace` /
`.paintgrab` / `.paintefb` (NOTES). Scorers: `tools/ladder-check/cloudfind.py` (video), `clouddump.py`
(Windows dumps). Trick that settled real-vs-in-between: a `FrameInterpolation.skip` range that drops the
HUD from the replay marks in-betweens in a recording. The day's `*-exp-*.apk` can be deleted.

**Committed:** everything through this fix is in `a9a5418` (repo files, patches, `tools/ladder-check/`);
the submodule stays pinned with the patches applied, as always.

**Open, in order:**
1. The cloud billboard glitch from the user's phone video on the deck (white / black / purple quad over
   the cannons): likely the same cause, since the billboard textures are preloaded too; needs the user's
   eyes on the deck with this build, or a recording to score.
2. Black in-between pictures in dialogue scenes ("You coward!", Fina's cell): suspected fixed when the
   replay's copies left RAM (NOTES *60 fps on Android: dual core*), never confirmed by eye; no savestate.
3. Deck battles with heavy effects (the 46% ring attack): no savestate yet; make one during that fight,
   then check speed and in-betweens there.
4. Longer hand play in dual core; audio in depth; the pipeline cache after swiping the app away. (Disc 2 was
   listed here too; Skies of Arcadia Legends is a single disc, 2026-10-11.)

## Status (2026-10-10, late): cloud-sea garbage fix built, not installed; green bands fixed (superseded)

**Built, NOT installed:** `build/android/apk/skies-ws9-tmemstate-2026-10-10.apk` (runtime only, same DOL
`329c631f`; `libmain.so` differs from the encode APK). It carries the fix for the cloud sea below the
ladder drawn as a dark olive-and-black garbage texture for one game frame (NOTES *Green bands at the
bottom of the Valuan ladder ... / Cloud-sea garbage*): a replay slice no longer invalidates TMEM for the
game's draws; the game's TMEM unit state travels with the replay's saved video state and its bound
textures are put back after each slice. Evidence for the cause: `tools/ladder-check/cloudfind.py` finds
0-4 events per 15 s ladder run on every replay build of the day and 0/0/0 with `FrameInterpolation.noreplay`,
and a two-retrace event means a real picture is wrong. Windows (regression only, it never showed the
garbage): see HANDOFF-desktop.md. **To check:** install with `adb install -r -t` (the game restarts: ask
the user first), then at least three `tools/ladder-check/devladder.sh` runs scored with `cloudfind.py`
(0-4 per run before), plus the user's eyes on the ladder. To go back: `skies-ws9-encode-2026-10-10.apk`.

## Status (2026-10-10, evening): green bands fixed; cloud-sea garbage frames on the ladder still open (superseded)

**Installed on the Retroid:** DOL `329c631f` (unchanged) + `build/android/apk/skies-ws9-encode-2026-10-10.apk`
(runtime only). The replay's EFB copies now run the game's encode pass to RAM too and drop the result,
on Qualcomm Vulkan (`DriverDetails::BUG_EFB_COPY_NEEDS_READBACK`); without it the game's full-screen
quads two draws after the screen grab sampled rows the copy hadn't finished (NOTES *Texture garbage in
in-betweens ... and green bands*, with the whole route there). Also in: TMEM-preloaded textures recorded
for the replay, a layout barrier at each EFB swap, device debug files `FrameInterpolation.slice` /
`.nodefer` / `.skip`. Checked (sha1 75cac5b8): three 15 s ladder runs (state 15, hold down, `screenrecord`): 0, 0, 0 green
frames (7-44 before), single-frame garbage outliers 1, 0, 2 (1-18 before), speed 99-101% on the overlay.
Left running on the device with state 15 loaded for the user's own check.
To go back: `skies-ws9-hud-tex3-2026-10-10.apk`. The day's experiment APKs (`*-exp-*.apk`) are in
`build/android/apk/` and can be deleted. The session's test scripts (device run + scoring: `devladder.sh`, `colorfind.py`,
`outlier.py`, `bisect.sh`) are copied to `tools/ladder-check/` with a README.

**Still open, seen by the user on this build:** the cloud sea below the ladder turns into a dark blocky
garbage texture for two frames, ~8 times in 23 s (`recordings-2026-10-10/user-ladder-play.mp4`, frames
125-126 etc.; NOTES has the list and how to score it — the session's single-frame scorer misses it).
The automated numbers above only cover green bands and single-frame outliers.

**Next:** that glitch (fix built late 2026-10-10, see the top status); the cloud billboard glitch from the
user's phone video on the deck; then commit (nothing from 10-09 evening on is committed: see HANDOFF-desktop.md).

## Status (2026-10-10): HUD at the wide edges installed; ladder glitches open (superseded)

**Installed on the Retroid:** DOL `329c631f` (`GEAE8P-ws9`: HUD anchors, NOTES *Widescreen HUD at the edges*) +
`build/android/apk/skies-ws9-hud-tex3-2026-10-10.apk` (runtime with the replay's recorded textures, content
locks on held copies, snapshots of partially updated textures). Old DOLs in `files/Backup-2026-10-10/`
(`main.dol.97adf61a` first ws9, `main.dol.a4f9ef7c` ws8). Memory card backed up twice
(`build/android/user-save-2026-10-10/`, `-10-b/`, Windows copies in `C:\dev\SkiesRecomp\build\`). HUD-only APK
(old runtime, same DOL): `skies-ws9-hud-2026-10-10.apk`.

**Open: ladder glitches on the device (user's recording, state `build/savestates/15-ladder.sav`, hold down):**
1. Single in-between frames with garbage clouds / a pale rectangle: reproduced on Windows only with
   `RECOMP_FRAME_INTERPOLATION_SLICE=2048` and fixed there by the recorded textures (36 -> 0), but the device
   still shows 2-4 per 15 s run with every build.
2. Green bands at the bottom of the ladder (balcony): whole in-between frames with three full-width
   bands of YUV-zero green over the picture (sometimes shifted). Only with interpolation on (0 with
   `FrameInterpolation.on` removed), device only, very variable (7 and 44 in two runs of the same build;
   the old runtime 29, the tex build 2-13), so single-run comparisons say nothing. The interpolator's own
   pictures dump clean on the device (`FrameInterpolation.dump` = "first count" -> `InterpDump/`), and every
   retrace gets one of them (trace: `FrameInterpolation.trace` -> logcat tag FrameInterpolation). Each frame
   copies the EFB into its own XFB buffer as a texture (scratch) before the XFB copy. Suspects: the
   presenter's XFB container path (RAM decode of zeros = green, VRAM copies stitched on top) reaching the
   screen, render-target pool reuse (FindMatchingTextureFromPool reuses render targets at once), or the
   game's strips sampling the grab through TMEM preloads. Needs many runs per hypothesis.

## Status (2026-10-09, evening): cliff cracks fixed; -09b installed and checked on the device (superseded)

The user's recording (`build/android/recordings-2026-10-09/`) showed seams opening along a floating
island's cliff in the in-between pictures while turning. Cause: the interpolation's jump test snapped
cliff texture groups (a coordinate near zero far off to the side counted as a jump); fixed with a
distance-scaled floor (NOTES *60 fps on Android: dual core*, Open). Checked on Windows by A/B with every
picture dumped (`build/savestates/14-cliff-cracks.sav`, saved on the device at the spot), no regression in
12-streaks, menu close or the transitions. Built: `build/android/apk/skies-ws8-interp-2026-10-09b.apk`
(runtime only; same module and DOL `a4f9ef7c` as ws8, so no DOL push). **Installed** (user OK) and checked:
state 14 loaded, steering past the cliff both ways, screen recorded (`recordings-2026-10-09/check-cliff-09b.mp4`):
no seams by eye, single-frame outlier pixels median 205 -> 25 per frame and no frame over 1,000 (35 before),
59.7-59.9 fps, no `Alert`. To go back: reinstall `skies-ws8-interp-2026-10-09.apk`. Windows `moderngekko-run.exe` has the fix (previous: `moderngekko-run-1009-ws8.exe`);
WSL runtime not rebuilt. The streak and hard-edged fog wall in the same recording are the game's own.

## Status (2026-10-09): battle transitions fixed; ws8 installed and checked on the device

**Installed 2026-10-09 (user OK):** `skies-ws8-interp-2026-10-09.apk` + DOL `a4f9ef7c` on the Retroid; old DOL in
`files/Backup-2026-10-09/main.dol.85176f05`. Checked on the device with `adb shell screenrecord` (~58-60 fps
capture; decode with `-fps_mode passthrough` or ffmpeg duplicates frames) and the 16x9 neighbour test:
- Both transitions (rectangle cards from `13-pre-battle-early`, spinning card from `13-pre-battle`, flying
  forward): full 16:9, every frame continuous, no torn or doubled strips, 59.6-59.9 fps throughout. The thin
  vertical seams while the picture breaks up are the effect itself (also in the Windows ws8 dumps).
- `12-streaks` loaded 5x: never hung (59.8 fps each). Steering left/right: no stray bands (worst cell excess ~13,
  spread evenly over many frames, i.e. texture/encoder noise).
- Main menu open/close (X, B B) twice: closing is menu fade -> 2 black frames -> world map fade-in, no isolated
  black frame between bright ones.
- Speed, limiter on, 20 s: red alert 1.005, world map 1.008, first battle 1.003, ship field 1.005 (59.8-59.9 fps).
  No `Alert` in logcat.
Still open: longer hand play incl. dialogue scenes (black dialogue in-betweens), deck battle ring attack state.


**Build:** `build/android/apk/skies-ws8-interp-2026-10-09.apk` (module `jniLibs-ws8`, built by a copy of
`tools/android-ws-build.sh` pointed at `build/game/GEAE8P-ws8`) only works with its DOL
`build/game/GEAE8P-ws/sys/main.dol` (SHA-1 `a4f9ef7c…`, the same as `GEAE8P-ws8`) in the device's
`files/Games/SkiesOfArcadiaLegends/sys/main.dol`. To go back: reinstall `skies-ws-interp-dualcore-2026-10-08r.apk`
and push `files/Backup-2026-10-09/main.dol.85176f05` back, together.

What ws8 brings (NOTES *Widescreen* and *60 fps on Android: dual core*, Open): both battle-entry
transitions (spinning card, falling triangles) at full 16:9 (they were 3D drawn at the widened field of
view: `UNWIDE` wrappers); no torn in-between pictures in them (texture-group snapping, in-order pairing
of equal counts); and the hang after state loads that `-08q`/`-08r` have (EFB start snapshot, removed).
Verified on Windows with every picture dumped and on the device (above). The DOL also carries the committed
but never shipped widescreen work-in-progress (HUD anchor mechanism, empty table; WS_VERSION 3).

**Next:**
1. Longer hand play by the user: transitions, menus, dialogue (the old black dialogue in-betweens at
   "You coward!" / Fina's cell are still unverified); watch `adb logcat | grep Alert`.
2. A deck-battle savestate with the ring attack, and speed numbers there.
3. Rebuild the WSL module for DOL `a4f9ef7c` (HANDOFF-desktop.md).
4. Game-patch code cave nearly full (0x10E0 of 0x1100 bytes): plan room before the HUD layout work.
5. Move the scratchpad analysis scripts into `tools/` (the 2026-10-09 work is committed).

## Status (2026-10-08, late): state-01 load desync fixed; desktop plays dual core (superseded)

Installed on the Retroid: `build/android/apk/skies-ws-interp-dualcore-2026-10-08n.apk` (the `-08m` build
plus the fix; settings unchanged). The state-01 load desync was Dolphin loading states with the GPU
thread running: the command processor came back (GPU thread) before RAM (CPU thread), and the GPU thread
read the FIFO from the old RAM. `State::DoState` now holds the GPU thread for the whole state in dual
core (NOTES *60 fps on Android: dual core*, Open). Every standard state, the world map and red alert
load back to back at 59.9 fps. Windows and WSL runtimes rebuilt with it; desktop play (`build\user-ws`)
switched to dual core with the idle skip. Commits: `5020658` (dual-core work), then this fix.

**Night (2026-10-08):** two more interpolation fixes, both A/B-verified on Windows with every
picture dumped (NOTES *60 fps on Android: dual core*, Open): black in-between after leaving the menu (a
full-screen quad's depth jump was blended; jump test tightened) and stray bands while steering (matrix
loads paired by occurrence; now aligned like draws). Also: the replay starts from its frame's EFB and
video state and reads the vertex arrays copied at record time. Installed:
`skies-ws-interp-dualcore-2026-10-08r.apk` (user: "definitely a lot better"). Open: the garbage frame at
the start of a battle transition (unverified), the transition itself still 4:3.

**Later (2026-10-08 evening):** the user hit "FIFO is overflowed by GatherPipe" in play (palace/court
scene; screenshot `build/android/screens/error-2026-10-08.png`). Fixed by `Fifo::WaitForFifoSpace`
(NOTES *60 fps on Android: dual core*, Open), installed as `skies-ws-interp-dualcore-2026-10-08o.apk`,
uncommitted, not yet confirmed at the spot: watch logcat for `Alert` (the `FIFO full` line needs the COMMANDPROCESSOR log type on).

**Next, in order:**
1. By-eye check of the black dialogue in-betweens ("You coward!", Fina's cell): needs the user to play
   there; if they persist, save a state (`ADB_AUTO=… tools/drive.py save:…`) and dump frames on Windows
   (`RECOMP_FRAME_INTERPOLATION_DUMP`, user dir with `CPUThread = True`).
2. A deck-battle state with the heavy ring attack (the user's 46% spot), then limiter-off numbers.
3. Longer play in dual core (any "Unknown Opcode" dialog: logcat `grep Alert`).

## Status (2026-10-08, later; superseded by the status above): 60 fps at full speed in dual core

The Retroid runs `build/android/apk/skies-ws-interp-dualcore-2026-10-08m.apk` with `CPUThread = True` in
`files/GameSettings/GEAE8P.ini` (original: `files/Backup-2026-10-08/GEAE8P.ini.orig`) and
`FrameInterpolation.on`. Red alert room 1.007 / world map 1.005 / first battle 1.003 / ship field 1.005 at
59.8-59.9 fps (limiter on); limiter off, red alert 1.28 and world map 1.17. How: Skies' dual-core desync
fixed (a PE token read waits for the GPU thread; the game resets the FIFO after two retraces of waiting),
and the in-between pictures drawn in the GPU thread's idle time, in slices, on a second EFB, presented one
retrace later. Details and the measurements: NOTES *60 fps on Android: dual core*; patch row in
patches/README.md. **Open:** loading the old `01-opening-cutscene` state into a running dual-core game
desyncs on Android (others load); the black dialogue in-betweens the user recorded need a by-eye check;
no deck-battle state yet. Debug switches (files in the user directory): `FrameInterpolation.stats`
(per-frame timing to logcat, tag `FrameInterpolation`), `.timeline`, `.noreplay`, `.norecord`.
Rebuild: the app only (module `jniLibs-ws` unchanged), the gradle line under *Status (2026-10-06)*.

**Next, in order:**
1. By-eye check of the black dialogue in-betweens ("You coward!", Fina's cell) on the new build; if they
   persist, save a state there (`ADB_AUTO=… tools/drive.py save:…`) and dump frames on Windows
   (`RECOMP_FRAME_INTERPOLATION_DUMP`), with `CPUThread = True` like the device.
2. A deck-battle state with the heavy ring attack (the user's 46% spot), then limiter-off numbers.
3. The state-01 load desync in dual core (Android only; `Unknown Opcode 0xda` at the same video-buffer
   offset every time; not interpolation, not the restore patch, not `PauseAndLock`'s timeout).
4. Longer play in dual core (any "Unknown Opcode" dialog: logcat `grep Alert`).
5. Desktop parity: decide whether desktop plays dual core too (`GameSettings/GEAE8P.ini` with
   `CPUThread = True` in `build\user-ws`, which also lacks the idle skip), and commit the work (nothing
   from 2026-10-08's dual-core session is committed yet).

## Status (2026-10-08): widescreen + 60 fps test build installed (superseded by the status above)

For the user's testing: `build/android/apk/skies-ws-interp-2026-10-08.apk` (same widescreen module and
DOL `85176f05` as before, runtime with frame interpolation including the cloud-alignment fix). On
Android, interpolation is switched on by an empty file `files/FrameInterpolation.on` in the app's user
directory (created on the device; delete it to go back to 30 fps; `RECOMP_FRAME_INTERPOLATION` still
wins on desktop). Measured with the limiter on: first battle 1.003 mean / 0.988 slowest second at
59.8 fps; **opening cutscene only 0.79 (46 fps)**: interpolation costs more than the cutscene's
headroom. The previous APK is `skies-ws-2026-10-07.apk`.

## Status (2026-10-07, late night): widescreen fix installed

The Retroid runs the fixed widescreen build: `build/android/apk/skies-ws-2026-10-07.apk` (rebuilt by
`tools/android-ws-build.sh`) with `build/game/GEAE8P-ws/sys/main.dol` (SHA-1 `85176f05…`), installed with the user's
OK. The clip build it replaced: `files/Backup-2026-10-07/main.dol.clip-52e799c7` on the device and
`skies-ws-2026-10-07-clip.apk` here. Checked on the device: red alert room (07) full width, save and camp menus
clipped, script fade full width, cutscene 1.008 mean / 0.986 slowest second (limiter on, unchanged). New open item:
the battle-entry shatter effect is still 4:3 (NOTES *Widescreen*). The device was left running the opening
cutscene state.

## Status (2026-10-07, later): widescreen installed

The Retroid now runs the widescreen build (NOTES *Widescreen*): `build/android/apk/skies-ws-2026-10-07.apk`,
the patched `main.dol` in `files/Games/SkiesOfArcadiaLegends/sys/`, and 16:9 in `Config/GFX.ini`. Updated
the same evening with the 4:3 clip for menus (DOL SHA-1 `52e799c7…`, module rebuilt); after a game-patch
change, push the new DOL and install the new APK together (the module must match the DOL). Originals in `files/Backup-2026-10-07/`; the previous APK is
`skies-dolphin-debug-gp-pgo-a77.apk`. Recipe: `tools/android-ws-build.sh` (module + APK from
`build/game/GEAE8P-ws/sys/main.dol`). Frame interpolation (60 fps) isn't in this APK (see the 2026-10-08 status for the one that has it) and probably
can't afford its ~20% on this device.

## Next phase (from 2026-10-06): a stable 30 fps on Android, at parity with desktop

**Target.** Skies renders 30 fps on real hardware, so full speed (1.00× = 60 VI/s) *is* a solid 30 fps.
"Stable" uses the desktop definition: with the frame limiter off, the **slowest one-second sample is
≥1.00× in all four standard states** (01 cutscene, 02 battle, 03 field, 04 field later). Desktop meets
it with room: worst second 1.13× (WSL LLVM+PGO) and 1.27× / 1.29× (Windows clang+PGO at 1× / 3×), measured
late 2026-10-06 with every patch (NOTES *Desktop headroom*).

**Where Android started** (2026-10-05, LLVM + state-in-memory + device PGO + speed patch, limiter
*on*, means): 0.97 / 0.88 / 0.94 / 0.90×, plus 1–2 s dips to 0.34–0.5× in every state. Two problems:
the mean, and the dips. (The 0.97 in 01 later turned out to include an A press; see below.)

**Where it is now** (late 2026-10-06, limiter off, mean / slowest second over two rounds): 01 1.05 / 1.033,
02 1.57 / 1.521, 03 1.20 / 1.169, 04 1.04 / 1.020. **Target met: every second ≥1.00 in all four states.**
The margin is thin in 01/04 (~2%; desktop has 13–29%). Details: NOTES *Android → Gather-pipe stores*.

**Progress (late 2026-10-06): step 3 done for the target.** A fresh simpleperf profile of the fixed build
showed gather-pipe stores as the largest overhead (~11% of the game thread per store, ~4% per burst). Three
changes, each A/B'd: an inline gather-pipe runtime hook (+6–8%), gather-pipe stores emitted in module code
with a retrained device profile (+7–9% more), and `DOLRECOMP_LLVM_CPU=cortex-a77` (+1–2% in 01/04).
Installed: `build/android/apk/skies-dolphin-debug-gp-pgo-a77.apk` (module `jniLibs-gp-pgouse-a77/`, profile
`pgo/skies-android-gp.profdata`). New patches: `recompcore-gxruntime-gather-pipe.patch`,
`dolrecomp-gather-pipe.patch`, `dolrecomp-llvm-cpu.patch`; `recompcore-staticrecomp-speed.patch` updated.
- **Desktop rebuilt (late 2026-10-06).** WSL LLVM + PGO and Windows clang + PGO are re-linked, retrained and
  re-measured with all patches (HANDOFF-desktop.md *Status*, NOTES *Desktop headroom*). The desktop runtime
  needed the gather-pipe fields in ModernGekko's own `cpu_state.h` as well (now in `moderngekko-local.patch`).
  Windows clang gained +3–11%; the C backend's gather fast path works.
- **Next, in order:** (a) desktop: done (above); (b) more Android margin: the per-burst path (~4%), the
  `dcbf`/`dcbst` invalidation chain (~3%; skip lines with no fallback-JIT block via `GetBlockBitSet()`), a
  compact `Run()` lookup table (~0.5%); (c) broader PGO training (battles, world map) with a held-out state;
  (d) sample the second fallback source; (e) the hand check on the device (step 4): **done 2026-10-07**
  except audio in depth: smooth 29–30 fps through many battles and the world map, saves survive a
  restart, pad works (NOTES *Android → Open questions*).
- **How much (b) is worth** (estimate): those percentages are profile shares; a fix removes part of each,
  so ~4–7% together, taking the 01/04 worst second from ~1.02 to ~1.06–1.09. That's margin (heavier scenes,
  a warm device) and idle time with the limiter on (heat, battery), **not a higher frame rate**: 1.00× is
  the game's 30 fps (NOTES *60 fps*). Suggested before (b): ask the user to play a big battle and the world
  map on the device. If nothing drops below full speed, (b) is polish; if something does, add those scenes
  to (c). **Result (2026-10-07): nothing dropped, so (b) and (c) are polish.** Started on 2026-10-06 and stopped there: the hook code was read (`HookCacheControl`,
  `OnICacheInvalidate`), nothing changed.
- `tools/android-boot.sh` now sends `KEYCODE_WAKEUP` first: a sleeping device never reached
  `state=running` (a training run timed out on it).

**Progress (2026-10-06, evening): steps 1–2 done, dips fixed, device PGO retrained.** Details in
NOTES *Android → Dips*. Commits `f1c3e52`, `847877c` (and the docs pass after them).
- Re-baselined limiter-off (two rounds). The 2026-10-05 0.97 in 01 was contaminated (an A press advanced
  the dialogue). Untouched 01 is 0.87–0.90; 04 0.86–0.91; 03 ~1.0–1.06; battle 1.43.
- The dips were the LLVM module re-entering after `mtmsr` through the per-instruction interpreter path,
  not shaders or thermals. Fixed by `patches/dolrecomp-mtmsr-entry.patch`. The first fixed build
  (`skies-dolphin-debug-mtmsr.apk`, old profile) is superseded by the retrained one below.
- Device PGO retrained on the fixed module and A/B'd: +1.5–2.6% in every state over the 2026-10-05 best.
  Installed: `build/android/apk/skies-dolphin-debug-mtmsr-pgo.apk` (module `jniLibs-mtmsr-pgouse/`,
  profile `pgo/skies-android-mtmsr.profdata`). Limiter off: 01 0.90, 02 1.47, 03 1.07, 04 0.91; slowest
  seconds 0.888 / 1.438 / 1.050 / 0.897.
- **Next:** (a) step 3 for the 01/04 mean: ~11% to reach a worst second ≥1.00 (CPU-bound on cpu7 at full
  clock); (b) sample the second fallback source seen in one WSL battle run (1.07M, battle took a different
  course; `STATICRECOMP_FALLBACK_SAMPLES=1`); (c) WSL: `build/modules-llvm-pgo-v2` (fix + retrained) is
  built but not active (superseded late 2026-10-06 by `build/modules-llvm-pgo-gp`); the Windows LLVM module
  doesn't have the fix yet. The fix is in the LLVM backend;
  whether the C backend (Windows' clang module) has a similar re-entry cost wasn't checked. (d) the
  stock-JIT reference from step 1.
- **Ask before benchmarking: the user plays on the device between runs.** `bench-android.sh` force-stops
  and restarts the game. Check its `*.input` warnings: a press during a run voids that state's number.
- Tooling added: `tools/android-sampler.sh` (clocks, temperatures, input during each state),
  drive.py `wall=` in `SPEED_TRACE`, `STATICRECOMP_FALLBACK_SAMPLES=1` (top interpreter-fallback PCs at
  shutdown). Per-second profiles: `simpleperf record --app … -e cpu-clock --clockid realtime --call-graph
  fp`, then host `simpleperf report --filter-file` (needs `CLOCK realtime`) with the unstripped
  `libmain.so` from `app/build/intermediates/cxx/RelWithDebInfo/*/obj/arm64-v8a/` via `--symdir`.
- **DolRecomp trap:** after editing it, re-run CMake in `lib/ModernGekko/build` (the object-cache key's
  source digest is taken at configure time), then a plain `ninja` (`ninja dolrecomp` alone leaves
  `build/dolrecomp`, the copy `moderngekko-port` runs, stale). See patches/README.md.
- The device's `Config/Dolphin.ini` is back to limiter on; the limiter-on copy is
  `Config/Dolphin.ini.limiter-on.bak` (also `build/android/bench/Dolphin.ini.orig`).

**Steps, in order:**

1. **Re-baseline the desktop way.** Set `[Core] EmulationSpeed = 0.0` in the device's
   `files/Config/Dolphin.ini` (back it up; restore it for play), then `tools/bench-android.sh` with two
   interleaved rounds. `drive.py speed:30` already prints `min=` (the slowest second). Add the
   numbers to NOTES *Platforms* with a worst-second column. Also measure stock Dolphin's JIT on the
   same states once, as a reference point.
   **Done 2026-10-06**, except the stock-JIT reference (not measured). Stock Dolphin would need its own
   copy of the extracted disc (Android 13 keeps apps out of each other's `Android/data`), and whether
   our savestates load in a different Dolphin version is untested.
2. **Find what the dips are.** Run `SPEED_TRACE=1` with a screenshot on each slow second, and logcat.
   The prime suspect is shaders: the JNI hook forces `AsynchronousUberShaders` +
   `WaitForShadersBeforeStarting` (same as desktop), and ubershaders are heavy on the Adreno 650.
   Test by running each state twice in one boot: if the second pass has no dips, it's shader
   compilation. Then try `ShaderCompilationMode = Synchronous` with a warm cache, or ship a
   pre-warmed cache. Rule out disc reads (extracted folder on `/sdcard`) and thermal throttling
   (watch `cpu7` frequency in `/sys/devices/system/cpu/cpu7/cpufreq/scaling_cur_freq` during a dip).
   **Done 2026-10-06: none of those.** No shader-cache writes, Adreno <2% in a dip, cpu7 at 2.84 GHz
   throughout, ≤57 °C. It was the LLVM module re-entering after `mtmsr` through the interpreter
   (NOTES *Dips*); fixed. The first-boot-of-the-day 0.51 second in 03 may be cold disc reads through
   `/sdcard` (FUSE); not reproduced on later boots.
3. **Raise the mean, biggest lever first** (from the simpleperf profiles in NOTES *Android*; those are
   from 2026-10-05, before the `mtmsr` fix, so take a fresh profile of 01/04 first). **Done late 2026-10-06
   for the target** (the gather-pipe path and the cortex-a77 tuning below; NOTES *Gather-pipe stores*). The
   other items are margin.
   - **GPU FIFO write path, ~8% of total.** Every gather-pipe write from module code calls
     `HookExternalWrite` → `GPFifo::Write32`. A direct write path (as Dolphin's JIT does with
     `optimizeGatherPipe`) needs DolRecomp/GXRuntime work; it helps desktop too. **Done** (the gather-pipe
     patches); the per-burst part (~4%) is left.
   - **`StaticRecompCore::Run()`**: the 24 MB chunk lookup table misses the A77's caches; plus
     out-of-line `GetPPCState()` / `SyncIn`. Run + SyncIn + GetPPCState ≈ 14% of runtime time.
   - **A cortex-a77 target profile in DolRecomp** (currently `aarch64` = `generic`, `+v8a`; profiles
     live in `src/backend/llvm/target.cpp`). Expected +2–4%. **Done** as an override,
     `DOLRECOMP_LLVM_CPU=cortex-a77` (`dolrecomp-llvm-cpu.patch`): +1–2% in 01/04.
   - **Broader PGO training**: the profile was trained on exactly the four benchmark states (retrained
     2026-10-06 on the same four). Train on more scenes (battles, world map), and hold one state out to
     check overfitting.
   - `Window.setSustainedPerformanceMode(true)` / Android game mode (melee-pc's idea): low priority,
     step 2 found no clock drops.
4. **Verify on device by hand** before calling it done: built-in pad, audio by ear, a memory-card
   save that survives a restart, a heavier scene (big battle, world map). The user has been playing on
   the device (2026-10-06, touchscreen and pad); ask them for each item rather than assuming.
   **Done 2026-10-07 except audio in depth:** played from the opening through many battles up to the
   world map, then flew around it: a smooth 29–30 fps the whole time, no hitches. Memory-card save, then reload after
   restarting: works. Built-in pad: works for everything tried. Audio: sounded okay, not yet checked in depth.

**Desktop findings that matter here:**
- The `moderngekko-port pgo-run` LLVM bug (NOTES, `moderngekko-local.patch`) does **not** affect
  Android: the Android module is built by calling `dolrecomp --profile-use` directly.
- An x86-64 LLVM-backend IR profile might apply to arm64 too (same DOL, same options); untested.
  Check with `--profile-use build/modules-llvm-pgo-gp/GEAE8P/pgo/merged.profdata` (WSL, trained late
  2026-10-06 with the same patches) on the Android build and count mismatch warnings. If it holds, train
  once on desktop (fast) and reuse.
- Speed-patch gains were 2–6% on desktop vs 4–14% on Android; the runtime-side overheads hurt the A77
  much more, so runtime work (FIFO path, `Run()`) pays off most on Android.
- The `mtmsr` fix applies to every LLVM-backend module. Desktop absorbed those fallbacks (only one
  0.65 second seen in a WSL battle run); the active WSL module (`build/modules-llvm-pgo-gp`) has it, the
  Windows LLVM module would need regenerating.
- **Any `CPUState` change touches three headers:** GXRuntime `core/cpu.h`, DolRecomp `src/cpu/cpu.h`, and
  ModernGekko's `include/moderngekko/cpu_state.h` (desktop runtime only, so Android never notices). Missing
  the third made the desktop runtime refuse every rebuilt module.

## Status (2026-10-06): current build and how to rebuild it

The current best build is the **LLVM module with gather-pipe stores, a device profile trained on it, and
`cortex-a77` codegen**: `build/android/apk/skies-dolphin-debug-gp-pgo-a77.apk` (installed), module
`build/android/jniLibs-gp-pgouse-a77/`, profile `build/android/pgo/skies-android-gp.profdata`. Kept for A/B:
`skies-dolphin-debug-gp-pgo.apk` (generic CPU), `-mtmsr-pgo-gpfast.apk` (runtime hook only, old module),
`-mtmsr-pgo.apk` (before today's step 3). Modules linked before the gather-pipe patches only load in APKs
built before them (`cpu_state_size`). All builds and outputs stay in gitignored `build/android/`. Results: development-notes.md *Android*.

- **Tools first:** apply all of `patches/` (in the README's order), then a plain
  `ninja` in `lib/ModernGekko/build` (re-run `cmake .` there after editing DolRecomp). See
  patches/README.md for why `ninja dolrecomp` alone isn't enough.
- **Module (LLVM, PGO):** with `A=$PWD/build/android`, `V=$PWD/lib/ModernGekko/vendor/dolphin`,
  `NDK=~/Android/Sdk/ndk/29.0.14206865`, LLVM 20 first on `PATH`:
  ```sh
  DOLRECOMP_LLVM_CPU=cortex-a77 DOLRECOMP_LLVM_TARGET=aarch64-unknown-linux-android24 lib/ModernGekko/build/dolrecomp -j12 \
    --backend=llvm --state-in-memory --targets=aarch64 --profile-use $A/pgo/skies-android-gp.profdata \
    --cpu gekko --gamecube extracted/SkiesOfArcadiaLegends/sys/main.dol $A/llvm-gen-X     # ~4 min
  cp extracted/SkiesOfArcadiaLegends/sys/main.dol $A/llvm-gen-X/generated/
  cmake -G Ninja -S $V/module-template -B $A/module-X \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-24 -DGENERATED_DIR=$A/llvm-gen-X/generated -DGXRUNTIME_DIR=$V/GXRuntime \
    -DCHASSIS_ABI_DIR=$V/Source/Core/Core/PowerPC/StaticRecomp -DGAME_ID=GEAE8P -DCMAKE_BUILD_TYPE=Release \
    -DRECOMPCORE_MODULE_ENABLE_IPO=ON -DRECOMPCORE_MODULE_OPT_LEVEL=2 -DCMAKE_C_FLAGS=-mtune=cortex-a77 \
    -DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384
  ninja -C $A/module-X
  $NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip --strip-debug \
    -o $A/jniLibs-X/arm64-v8a/libgGEAE8P_recomp.so $A/module-X/gGEAE8P_recomp.so
  ```
  The linked file is `gGEAE8P_recomp.so`; the APK needs the `lib` prefix. Check the dolrecomp log for
  `hash mismatch` warnings: each one is a function that lost its profile.
- **Retraining:** the same with `--profile-generate 'skies-%m.profraw'` instead of `--profile-use`, and
  `-DCMAKE_SHARED_LINKER_FLAGS="-fprofile-generate -Wl,-z,max-page-size=16384"`. Install that APK, move
  old `files/PGO/*.profraw` aside, then per state: `tools/android-boot.sh`, `drive.py load:… wait:16`,
  then `speed:30` (01/02) or `hold:main_y=1:150 hold:main_x=1:150 hold:main_y=-1:150
  hold:main_x=-1:150 speed:20` (03/04), then `stop` (writes one `.profraw` per state). Pull
  `files/PGO/`, `llvm-profdata merge` (LLVM 20 reads the NDK's raw profiles).
- **App:** vendored Dolphin + `patches/recompcore-staticrecomp-speed.patch` + `patches/recompcore-android.patch`.
  ```sh
  cd lib/ModernGekko/vendor/dolphin/Source/Android && ANDROID_HOME=~/Android/Sdk ./gradlew :app:assembleDebug \
    -Pandroid.injected.build.abi=arm64-v8a -PrecompJniLibs=$A/jniLibs-X \
    -PrecompNativeFlags=-mno-outline-atomics
  ```
  The APK is `app/build/intermediates/apk/debug/app-debug.apk`; copy it to `$A/apk/` under a
  descriptive name. It's test-only, so install with `adb install -r -t` (keeps app data). Package
  `org.dolphinemu.dolphinemu.debug`, alongside stock Dolphin. Check `unzip -l` shows the module
  (~110 MB): a missing module still builds a working, module-less APK.
- **On-device setup** (`/sdcard/Android/data/org.dolphinemu.dolphinemu.debug/files/`): the
  **extracted** disc in `Games/SkiesOfArcadiaLegends/` (not the RVZ, which is parked in `Discs/`;
  see NOTES *Savestates need the same disc layout*). `Config/Dolphin.ini` has `ISOPath0` = its
  `sys/`, Vulkan, analytics off, limiter on (the play setting). `Config/Dolphin.ini.limiter-on.bak` is a
  copy of it; for headroom runs add `EmulationSpeed = 0.0` under `[Core]` and restore the copy after.
  `GameSettings/GEAE8P.ini` holds the idle skip, `GFX.ini` the speed overlay, and `Automation/` turns
  on the automation directory. `PGO-old/` holds the 2026-10-05 raw profiles.
- **Measuring:** ask the user first (they play on the device). `tools/bench-android.sh NAME [STATE…]`
  (fresh boot, load, 16 s, `speed:30`) writes `build/android/bench/NAME/`: `results.txt`, per state
  the `SPEED_TRACE` log (with `wall=`), a screenshot, `*.sampler.txt` (clocks, GPU busy, temperatures;
  the device clock ran 1.6 s ahead of WSL's) and `*.input` (touch/button presses, with a WARNING on
  stderr: that state's number is void). Two interleaved rounds per build. Ad hoc:
  `tools/android-boot.sh`, then `ADB_AUTO=…/files/Automation tools/drive.py load:… speed:30`.
- **Profiling a slow second:** `adb shell simpleperf record --app org.dolphinemu.dolphinemu.debug -e
  cpu-clock --clockid realtime -f 1000 --call-graph fp --duration 52 -o /data/local/tmp/x.data`
  (hardware `cpu-cycles` refuses `--clockid`), pull it, then the NDK's host `simpleperf report
  --filter-file F` with `CLOCK realtime` + `GLOBAL_BEGIN/END` ns lines and `--symdir` holding the
  unstripped `libmain.so` (`app/build/intermediates/cxx/RelWithDebInfo/*/obj/arm64-v8a/`). Callchain
  addresses it reports are return addresses minus 2, so the call instruction is at the reported
  address minus 2.
- **Don't** use `adb shell input tap` for dialogs without checking the result (a tap meant for "No"
  on the analytics dialog hit "Yes"). Dolphin alerts block the emulation thread, and the user
  sees them on the device: check `adb logcat | grep Alert`. And don't `pkill -f` a pattern from inside
  `adb shell`: it matches its own shell and kills it.

## Goal

Get Skies of Arcadia Legends (GEAE8P) booting natively on Android (arm64) and reach the opening
cutscene on the Retroid Pocket 5. Then keep **Linux/WSL, Windows and Android at parity**: same
pins and patches, the same generated code, the same test state and measurement protocol, and one
results table.

## Where things stand

| Platform | Module | Opening cutscene (standard state) | Picture |
|---|---|---|---|
| Linux/WSL | LLVM backend + state-in-memory | 1.00× at 1× (llvmpipe caps 3× at ~0.66×) | correct (llvmpipe) |
| Linux/WSL | C backend, gcc 15 `-O2` | 0.80× at 1× | correct |
| Windows | C backend, clang 20.1.8 | **1.01× at 1× and at 3×** (frame limiter) | correct; user approved 3× |
| Windows | C backend, MSVC | 0.505× (don't use) | correct |
| Android (Retroid Pocket 5) | LLVM + state-in-memory + device PGO, speed patch | 0.97× in 01, 0.89–0.94× in 02–04 (standard protocol) | correct, text readable |

This table is the 2026-10-05 snapshot (limiter on). **The live table is development-notes.md *Platforms***
(limiter off, with the slowest second): desktop worst second 1.13× (WSL) / 1.27× (Windows clang), Android
1.020× (stable 30 fps in all four states).
All with idle skip (`StaticRecompIdlePC = 0x80237BA8`) and no mods loaded. Details are in
[development-notes.md](../development-notes.md): *Performance*, *Native Windows build*, *Android*. Everything is committed on
branch `skies-windows-android` (not pushed). Build outputs stay in gitignored `build/`. Windows copy at
`C:\dev\SkiesRecomp` (not a git checkout; docs, patches and tools are copied over).

## The device

**Retroid Pocket 5**. Android 13 (SDK 33), arm64-v8a, Snapdragon 865
(`kona`): 1× 2.84 GHz + 3× 2.42 GHz Cortex-A77 + 4× 1.8 GHz A55 (`nproc` in adb shell says 6;
cpuset). Adreno 650, Qualcomm driver V@0746.0 (2023-09-27). 7.5 GB RAM, 48 GB free on
`/sdcard`, 1080×1920 panel, built-in controller.

Already installed on it: stock Dolphin (`org.dolphinemu.dolphinemu`) and the user's
other recomps (`com.goemon64.recomp`, `com.hybridheaven.recomp`, `com.quest64.recomp`).

**adb goes through Windows.** WSL can't see USB, so use
Windows' `adb.exe` (WinGet's Google.PlatformTools, on the WSL PATH; the tools default to it, `SERIAL=`
picks a device when several are attached) with `wslpath -w` paths. Don't start WSL's `/usr/bin/adb`. On 2026-10-05 Windows adb saw no device
after a WSL adb server had been started. `adb kill-server` on both sides plus a fresh Windows server fixed
it, though the user also reset the authorization at the same moment, so the cause isn't proven.
The device is in debugging-only USB mode (`sys.usb.config=adb`). Windows lists it as
"KONA-HDK" (VID_18D1 PID_4EE7, WinUSB), not as an "ADB Interface", and that works.
Goemon64Recomp's memory has more device lore (`device-install-method`, `drive-device-ui-with-adb-input`,
`goemon-adreno-618-630-vulkan-crash`, `android-custom-vulkan-driver-loading`).

## Toolchain on this machine

| Have | Path / version | Notes |
|---|---|---|
| Android SDK | `~/Android/Sdk` (`ANDROID_HOME` unset) | cmdline-tools `latest`, platform-tools |
| NDK | 27.1.12297006, **29.0.14206865** | 27.1: HybridHeaven/Quest64/Goemon. 29: Dolphin's app and the Skies module |
| Platforms / build-tools | android-34, **android-36** / 33.0.1, 34.0.0, **36.0.0** | |
| SDK CMake | 3.22.1 | |
| JDK | OpenJDK 17.0.20 | |

**Dolphin's Android app pins newer versions** (`vendor/dolphin/Source/Android`): `ndkVersion =
"29.0.14206865"`, `compileSdk = 36`, `minSdk = 24`, AGP 9.1.0, Kotlin 2.3.20, Gradle 9.4.1
(the wrapper downloads it), Java 17. Installed 2026-10-05 (no admin needed):

```sh
~/Android/Sdk/cmdline-tools/latest/bin/sdkmanager "ndk;29.0.14206865" "platforms;android-36" "build-tools;36.0.0"
```

AGP 9.1 runs on JDK 17 (checked 2026-10-05), so JDK 21 isn't needed.

## What exists upstream for Android: nothing specific

ModernGekko (pin `edd460a`; upstream `a2e4e2b` adds 8 commits, none Android), DolRecomp
(`1bec355`) and RecompCore have **no Android work**. ModernGekko's only Android mention is
`if(NOT ANDROID)` around the touch `InputOverrider`. This is greenfield. The facts that make it
look feasible:

- **The static-recomp core is part of the vendored Dolphin core** (`Source/Core/Core/PowerPC/StaticRecomp/`).
  `CPUCore::StaticRecomp = 6` is in `AvailableCPUCores()` on every architecture, ARM64 included.
- **The module is set by one call**: `JitInterface::SetStaticRecompModuleSource(StaticRecompModuleSource::Dynamic(path))`.
  ModernGekko's `src/runtime/dolphin_runtime.cpp:~695` makes it before boot, leaving `host_call`
  unset when no mods are loaded (our local patch). Nothing in Dolphin's Android JNI makes that call yet.
- **Dolphin's full Android app is vendored**: `vendor/dolphin/Source/Android` (Kotlin UI, `jni/` with
  `MainAndroid.cpp`, input, `GpuDriver.cpp` for custom Turnip drivers). Its CMake entry is
  `vendor/dolphin/CMakeLists.txt`, i.e. plain Dolphin, without ModernGekko's `src/` and `tools/`.
- **The module ABI has arm64 paths**: DolRecomp `src/backend/module_abi.c` handles
  `__aarch64__ && __linux__`, and GXRuntime has `__aarch64__` paths
  (`cpu_interpreter_float.c`). The x86 bits (`immintrin.h`, the x86-64-v3 dispatch in
  `module_export.c`) are `#if`-guarded. (It compiled for arm64 unchanged, C and LLVM, on 2026-10-05.)
- **The generated C is platform-neutral.** Reuse WSL's
  `build/modules/GEAE8P/<hash>/dolrecomp-output/generated/` (171 chunks) instead of re-running DolRecomp.

## How other GameCube ports did Android (researched 2026-10-05)

Clones were read at the commits below. None of them has a **device-tested** static recomp on
Android, so nothing here is a speed precedent for us.

| Port | Kind | Android stack | Run on a device? |
|---|---|---|---|
| [Wind Waker Recomp](https://github.com/flintstrike-nz/Wind-Waker-Recomp) (BlueWake fork, `85e6650`) | static recomp: RecompCore + DolRecomp, our lineage | own SDL3 host (`runtime/host`) + GXRuntime + **Aurora → Dawn → Vulkan 1.1**, Kotlin shell, NDK 27.2 pinned | **No.** "Built and packaged, not yet run on a device" (`docs/ANDROID.md`) |
| [fzero-gx-recomp](https://github.com/Pipyakas/fzero-gx-recomp) (`1bc613b`) | static recomp: DolRecomp + ModernGekko/GXRuntime | `android/CMakeLists.txt` builds `gx_null.c` + `os_win32.c`, `# TODO: replace gx_null…, add gradle wrapper` | **No.** It's a stub. Ignore the README's Android claim |
| [melee-pc](https://github.com/999sian/melee-pc) (`e833835`) | decomp (doldecomp/melee) | Aurora (WebGPU/Dawn, Vulkan 1.1) + SDL3 | yes, with a frame-pacing overhaul |
| [Dusklight](https://github.com/TwilitRealm/dusklight) (Twilight Princess), Open Nectar (Pikmin) | decomp | Aurora + SDL3 (Dusklight GLES then Vulkan; Open Nectar GLES 3.0) | yes |

**BlueWake is the only real precedent, and its module packaging matches our plan.** The game is
built with the NDK into `libgGZLE01_recomp.so` and shipped in `lib/arm64-v8a` with
`jniLibs.useLegacyPackaging = true` plus `extractNativeLibs="true"`. The Kotlin side resolves
`applicationInfo.nativeLibraryDir/libgGZLE01_recomp.so` (`DataPaths.kt:26`) and passes it in via
`Os.setenv` before `libmain.so` loads. That's our step 3 with a different host. Its security is
sound: `allowBackup="false"` (the disc image is in private storage), the game activity isn't
exported, the FileProvider only exposes `cache/shared/` for log sharing, there are no storage or
network permissions, the keystore password comes only from env or a 0600 file, and the APK is
refused inside the repo.

**Why we still don't copy BlueWake's architecture:**

- **Picture parity.** It renders through Aurora (Dawn/WebGPU), a different GX implementation
  from the Dolphin VideoCommon Vulkan backend our Linux and Windows builds use. Aurora is in our
  tree (`vendor/dolphin/GXRuntime/graphics/aurora`, `GXRUNTIME_ENABLE_AURORA`), but switching
  Android alone breaks "same picture on three platforms". We'd be taking on an untested
  renderer for Skies on top of an untested platform.
- **Unproven.** Nobody has run it. Its own "what to check first" begins with "whether Aurora got
  a Vulkan device and a surface".
- **Disc handling is worse.** It copies the whole 1.5 GB ISO into private storage, rejects
  RVZ/GCZ/WIA/CISO, and retries failed picker copies. Dolphin's app reads the image through the
  content URI in place and handles compressed formats. Copying the
  ~1.4 GB disc would cost the Retroid the space twice for nothing. (This note first said Skies
  was two discs; that's the Dreamcast original. Legends on GameCube is one disc, corrected
  2026-10-11.)
- **No custom GPU driver path.** Dawn uses the system Vulkan driver. Dolphin's
  `jni/GpuDriver.cpp` can load Turnip, our fallback if the Adreno 650 blob misrenders (Goemon's
  lesson).

**Worth taking from BlueWake:**

- **Game-thread stack.** BlueWake's one SDL change raises the game thread's stack to 16 MB. On
  Android the default `std::thread` stack is about 1 MB (desktop glibc's is 8 MB). Our
  route runs the CPU on Dolphin's `s_emu_thread = std::thread(EmuThread…)` (`Core/Core/Core.cpp:231`,
  single-core). Windows (also 1 MB by default) runs Skies fine, so it may not bite. If it
  crashes deep in recompiled code, start the emu thread with a bigger stack via
  `pthread_attr_setstacksize`.
- **Code generation flags.** `-O2 -march=armv8-a -mtune=<big core>`, never SVE (Qualcomm doesn't
  implement it). For the SD865, use `-mtune=cortex-a77`.
- **PGO matters on mobile.** BlueWake's iOS build only reaches about 30 FPS with profiles, and
  NDK clang can't read Apple-LLVM profiles. Plan on a device-side training run (an instrumented
  module on the Retroid) if the speed test comes in short.
- **Smaller habits.** `-Wl,-z,max-page-size=16384` (16 KB pages, harmless on Android 13),
  `-ffile-prefix-map` (keeps home paths out of the `.so`), a pinned NDK and build-tools, and
  a provenance file in the APK.

**melee-pc's Android manifest is worse; don't copy it.** It requests
`MANAGE_EXTERNAL_STORAGE` (all-files access), legacy external storage, `INTERNET` and multicast,
sets `allowBackup="true"`, and its launcher activity has an exported `VIEW` filter for any
`file:`/`content:` URI with `*/*`. Any app can then hand it a crafted image to parse. Its one
useful idea: `Window.setSustainedPerformanceMode(true)` when supported, plus a
`game_mode_config`. Both are worth trying for the single-thread clock problem.

**Our route inherits some bad stock-Dolphin defaults too.** The vendored manifest has
`allowBackup="true"` and `requestLegacyExternalStorage="true"`, and asks for `RECORD_AUDIO`,
`INTERNET` and the TV EPG permissions. It also has exported `MainActivity`, `TvMainActivity`
and `AppLinkActivity`. For our private build (separate `applicationId`), set
`allowBackup="false"`, because saves and possibly disc data land in app storage. Drop the
permissions we don't use too. Keep these changes in `patches/` like everything else.

## Recommended route

**Host the module in Dolphin's own Android app** (the vendored one), adding a small JNI hook.
The alternative is porting `moderngekko-run`'s frontend to Android. That would need a new
`DolphinNoGUI` platform plus surface, input, audio and storage work that Dolphin's app already
does.

1. **Build the module for arm64.** Configure `vendor/dolphin/module-template` exactly as
   `moderngekko-port` does (see `tools/moderngekko_port.cpp`, `configure` string ~line 800), plus
   `-DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a
   -DANDROID_PLATFORM=android-24`, and `GENERATED_DIR` pointing at the generated C above. Expect to fix
   arm64/Android compile issues in GXRuntime or `module_export.c`. Fixes go in `patches/`.
   **Done:** it compiled unchanged (NDK 29, `-mtune=cortex-a77`, `-Wl,-z,max-page-size=16384`).
2. **Build stock Dolphin's Android app from the vendored tree unmodified** (`./gradlew assembleDebug` in
   `Source/Android`), with `applicationIdSuffix` or a new `applicationId` so it doesn't replace the
   installed `org.dolphinemu.dolphinemu`. This proves the toolchain before any recomp change.
   **Done, together with step 3.** The hook does nothing without a module, so one build covered
   both. The debug variant's own `.debug` suffix keeps it apart from stock Dolphin.
3. **Add the hook.** Before boot, call `SetStaticRecompModuleSource(Dynamic(<nativeLibraryDir>/libgGEAE8P_recomp.so))`,
   force `CPUCore = 6`, and make sure `Sys/GameSettings/GEA.ini` (forces single-core) and the
   user `GameSettings/GEAE8P.ini` (idle skip) apply. **Ship the module inside the APK
   (`jniLibs/arm64-v8a`)**: Android won't `dlopen` code from writable app storage (W^X since API 29),
   while `nativeLibraryDir` is allowed. Set `useLegacyPackaging`/`extractNativeLibs` so the
   file exists on disk. Apply the manifest hardening from *How other GameCube ports did Android*
   (`allowBackup="false"`, unused permissions dropped).
   **Done:** `patches/recompcore-android.patch`. The settings come from the stock `GEA.ini` and the pushed
   `GEAE8P.ini`. Only `allowBackup` is hardened so far. Unused permissions (`RECORD_AUDIO`, TV
   EPG, `INTERNET`) haven't been dropped yet.
4. **Boot.** The disc comes from the user (`sys/` + `files/` extraction, or Dolphin's own ISO/RVZ
   loading, since the app has its disc reader). Push it under `/sdcard/Download/…` and pick it in
   the app. Never commit or redistribute the module or the APK: the module is recompiled game code.
   **Done:** it boots. Instead of picking the disc, the RVZ was moved into the app's own
   `files/Games/` and `ISOPath0` set, so no SAF grant or UI taps were needed (see Status).
5. **Bring over the automation directory** (`tools/automation_protocol.cpp`): compile it into the
   app, pointed at an app-scoped dir. That way `tools/drive.py`, the standard state and the
   benchmark protocol work over `adb push`/`pull`. This is what makes parity testable.
   **Done:** `jni/Automation.cpp` (opt-in via `files/Automation/`), `drive.py` with `ADB_AUTO`,
   `tools/android-boot.sh` and `tools/bench-android.sh`. Debug APKs only since 2026-10-12
   (`DOLPHIN_ANDROID_AUTOMATION_BUILD`, set by the debug build type): a release APK has none of it.

Stop after step 4 if it boots, report, and ask before step 5. (All five steps were done on
2026-10-05. Step 5 came with the user's request to test real gameplay from savestates.)

## Expectations and risks

- **Speed is the big unknown.** Skies is forced single-core, so speed tracks single-thread
  performance and code quality. A Cortex-A77 at 2.84 GHz is roughly 40–50% of an i7-12700K P-core
  (inference from public benchmarks, not measured). The Windows clang C module sits at the frame
  limiter, so its headroom is unknown. **Measure desktop headroom first** (frame limiter off, same
  state) to predict Android. Likely needs the LLVM backend on aarch64 (DolRecomp can emit it,
  but that's untested here), PGO, and maybe pinning the game thread to the prime core (cpu7).
  **Measured 2026-10-05 (standard protocol):** C module 0.48–0.61×. LLVM + device PGO + speed patch
  0.97× (01), 0.89–0.94× (02–04). The game thread already runs on cpu7 at 2.84 GHz, so pinning
  doesn't help. **Desktop headroom was measured 2026-10-06** (NOTES *Desktop headroom*: worst second
  ≥1.15×; 1.13× WSL / 1.27× Windows clang after the late 2026-10-06 rebuild), and Android limiter-off on
  2026-10-06 (0.90 / 1.47 / 1.07 / 0.91 with the `mtmsr` fix; worst second 1.020 after the gather-pipe work).
- **Baseline for free:** stock Dolphin is installed on the Retroid. Running Skies there (JIT,
  single-core per `GEA.ini`) gives a JIT speed to compare against. Not run yet (see step 1).
- **GPU:** Adreno 650 on the Qualcomm driver. Goemon hit Adreno blob bugs (618/630) that Turnip
  avoided, and Dolphin's app can load custom drivers (`jni/GpuDriver.cpp`). Check the picture
  against the Windows 3× shot, text included. The user is the final judge.
- **Savestates across architectures:** resolved. The desktop states load on arm64, **provided the
  app boots the extracted disc folder**. On the RVZ, every scene transition raised "Invalid
  read/write" (development-notes.md *Savestates need the same disc layout*).
- **Storage:** `chmod` is ignored on app-scoped storage (Goemon lesson), and use `/sdcard/Download`
  for pushes. A `mv` from there into `/sdcard/Android/data/<pkg>/files/` is instant and gives
  the file the app's uid. Uninstalling the app deletes that copy. Back up any on-device data before risky operations, and checksum pushed files.

## Parity, once Android boots

- **Pins and patches:** one set of submodule pins plus `patches/*.patch` for all three. Any
  platform fix goes into `patches/` and is re-applied everywhere.
- **Disc:** every platform boots the **extracted folder** (`sys/` + `files/`), because the shared
  savestates carry its file layout.
- **Generated code:** one DolRecomp run per DOL and backend. The C output is shared as is. LLVM
  objects are per target (`--targets` / `DOLRECOMP_LLVM_TARGET`) but come from the same DOL and options.
  As of late 2026-10-06: WSL LLVM + state-in-memory + PGO, Windows clang C + PGO, Android LLVM + state-in-memory
  + PGO + `cortex-a77`, all built with every patch in `patches/` (the `mtmsr` fix applies to the two LLVM
  ones). The Windows LLVM module is the exception: old, refused by the current runtime.
- **Module compiler:** standardize on clang (WSL `clang-20`, Windows LLVM 20.1.8, NDK clang),
  because MSVC halved the speed. On WSL, clang beats gcc on the C backend (0.89× vs 0.85× in 01,
  NOTES *Platforms*).
- **Config:** same `GEAE8P.ini` (idle skip) everywhere, and `DSPThread = True` on all three.
  Resolution per platform. Desktop benchmarks run muted (`[DSP] Volume = 0`), while Android plays
  audio (its own `AudioTrack` thread, ~3.6% of a core). Keyboard profiles
  exist for X11 and Windows, and Android uses Dolphin's controller mapping for the built-in pad (since
  2026-10-17 the game adds its own keyboard layout: NOTES *Controller and keyboard*).
- **Measurement:** the same four states (`build/savestates/01`–`04`, made on WSL), 16 s load,
  30 s `speed` average, plus a picture check with text: `tools/play.sh` + `drive.py`,
  `tools/bench-win.sh`, `tools/bench-android.sh`. Three interleaved rounds is the target; Android
  and the late 2026-10-06 desktop re-measure use two (the 2026-10-05 Android numbers were single runs).
  State 03's camera ends somewhere different each run, so its numbers spread more than the others.
- **One table** in development-notes.md (a *Platforms* section) with module, resolution, speed and picture per
  platform, updated with every change.
- **Hand-checked on Android (2026-10-07):** built-in pad, memory-card save across a restart, many
  battles and the world map at a smooth 29–30 fps, no hitches. Audio sounded okay; an in-depth check is
  pending. **Still untested on desktop:** audio by ear, a pad pressed by hand (the keyboard: checked on WSL and Windows 2026-10-17, HANDOFF-input.md), heavier scenes. The user has played on Windows
  (a solid 30 fps, 2026-10-06) but didn't check those item by item. Memory-card saving on Windows was verified
  2026-10-07 (scripted save, clean exit, loaded on a fresh boot; NOTES *Known issues*).
