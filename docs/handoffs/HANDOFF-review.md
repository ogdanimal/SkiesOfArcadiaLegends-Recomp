# Handoff: code review findings (2026-10-12; second review 2026-10-22, section D)

A bug + security review of the project's own code: the applied patches in the three submodules
(`git diff` in `lib/ModernGekko`, `lib/ModernGekko/vendor/dolphin`, and the nested
`lib/ModernGekko/vendor/dolphin/DolRecomp`; ~3,100 lines), `patches/game/`, `tools/` and
`.github/workflows/`. Five reviewers, one per area; every High/Medium below was then re-read and
confirmed in the code by a second pass. Nothing was built, run or changed, and no device was touched.

**Overall:** no remote-exploitable issue, no command injection, no secret leaks in CI, no unpinned
actions, and the save / DOL atomicity work holds up. What's left is (a) three places where a file the
*player downloads* (a disc image, a card save, a save state) isn't checked enough, (b) crash and
data-loss paths in the shipping flows, and (c) hygiene. Paths are relative to the repo root;
`RC/` = `lib/ModernGekko/vendor/dolphin/Source/`, `MG/` = `lib/ModernGekko/`. Line numbers are from
the working tree on 2026-10-12 morning (commit `35d1a4d`).

Tick the box and note the commit when done. Anything that touches a submodule file goes through the
matching `patches/*.patch` (`tools/regen-recompcore-patch.sh`, `patches/README.md`).

**Second review (2026-10-22): section D**, open. The code written since this review (the Japanese
disc, Uncensored, the Dreamcast music's streams and reverb, the European disc at 60 Hz, HD textures
and nested archives, the data folder, romhacks, language, state folders, field speed, battle
specials, the overclock timebase, macOS, Android orientation). Still no remote-exploitable issue; the
only untrusted inputs are files the player downloads. Its own additions to *Checked and OK* are at
the end.

**Status after v0.10.0-beta (2026-10-17):** B7 is closed (the repo went public; environments `ci` / `release` and the rulesets `main` / `release tags` exist, validate.yml's jobs run in `ci` and `android-sign` in `release`; HANDOFF-release *Status*). Still open: the state header's triple read.

**Status (2026-10-12, evening):** A1-A9 done (`0afcabc`); B1-B6, B8-B14 done (`1d4856b`, `b0f0ca1`);
B7 waits on your GitHub plan; section C done (`89cdb47`; the four game-patch items in ws18) except the state header's
triple read. Device checks: all done on the Retroid 2026-10-12 (B11, B12 below), as are
the C batch's Android changes (permissions removed, DocumentProvider, import copy, DOL hash cache:
all in the ws18 branded debug APK `build/android/apk/skies-recomp-ws18-2026-10-12.apk`, **installed on
the Retroid 2026-10-12 13:20** (user OK; `adb install -r -t`, versionCode 409977; backup first:
`C:\dev\SkiesRecomp\build\backup\retroid-before-ws18-2026-10-12`). Checked there without input: the
app requests only VIBRATE (+ AndroidX's own); first launch re-patched the ws17 import to ws18
(`0fcdb786` -> `39dd1e6c`), so the user folder is still found without the storage permissions;
Continue offers the Oct 11 22:13 auto-save; the card save and all five states md5-identical before /
after. A1 checked there too (below). Then (user OK to drive the device, 13:2x-13:3x): the auto-save boots
at 59.86 fps (device driver), B12 and B11 checked (below), and Turnip v26.0.0 R8 runs it at 59.86 fps.
The game's exit auto-save pruned the user's oldest auto-save (Oct 11 21:54): put back from the backup
with its original time. Left on the device: Turnip installed and selected; the user answered "Keep this GPU driver?" with
Keep (10-12) and says it plays the same as before; `Turnip_v26.0.0_R8.zip` in Download; the test zips and `Automation/` removed. Windows tree synced
through the ws18 commit (release + dev builds rebuilt, `build\game\GEAE8P-ws18`,
`build\modules-llvm-pgo-ws18`).

## A. Fix before v0.9.0

- [x] **A1 High: the GPU-driver crash safeguard never runs after a crash.**
  `RC/Android/app/src/main/java/org/dolphinemu/dolphinemu/skies/SkiesLauncherActivity.kt:145-178`,
  `skies/SkiesDriverGuard.kt:78-86`. `checkDriver()` is called only from `onResume`, and only when
  `DirectoryInitialization.areDolphinDirectoriesReady()`. After a crash the process is gone, the
  launcher cold-starts, `onCreate` starts dir-init asynchronously, so the first `onResume` skips the
  check; the `AfterDirectoryInitializationRunner` callback (145-148) only does `installDefaults()` +
  `refresh()`. Tapping Start then runs `onLaunch()`, which rewrites `skies-driver-boot-pending` with
  the same driver: a Turnip build that crashes at boot crashes every time. The check only fires on a
  later *warm* `onResume` (back from a picker / dialog). **Fix:** call `checkDriver()` from the
  dir-init callback after `refresh()` (or make `check()` independent of Dolphin init: it only needs
  `filesDir` and the `DriverLibName` setting).
  **Done 2026-10-12 (`0afcabc`):** `checkDriver()` now also runs from the directory-init callback
  (after `installDefaults()` + the home screen), which is where a cold start after a crash lands;
  it keeps the dialog it shows and skips while one is up, so the callback and `onResume` can't stack
  two. Compiled (APK); not run on a device: the test needs a crash marker in the app's private
  `filesDir`, which this session wasn't allowed to write. To check by hand on the debug build:
  `adb shell "run-as com.skiesofarcadia.recomp sh -c 'echo x > files/skies-driver-boot-pending'"`,
  `adb shell am force-stop com.skiesofarcadia.recomp`, open the app: the "Back on the device's own
  GPU driver" dialog should show at once (before the fix, only after leaving and returning).
  **Checked on the Retroid 2026-10-12 13:3x (ws18 debug APK, the user asked for the recipe):** the
  dialog showed on the cold start, straight away, and the marker was gone after. It named the driver
  "x" plus a line break (the marker's text from `echo`; real markers hold the name without one):
  `SkiesDriverGuard.check` now trims it (source + patch; not rebuilt, cosmetic).

- [x] **A2 Medium: disc import trusts the FST's file names (path traversal), all three platforms.**
  `RC/Android/jni/SkiesImport.cpp:117-121` and `MG/tools/moderngekko_launcher.cpp:~690-729` both call
  upstream `DiscIO::ExportDirectory` (`RC/Core/DiscIO/DiscExtractor.cpp:101-136`), which builds
  `export_root + file_info.GetName()` from the raw FST string: nothing rejects `..`, `/` or `\`, nothing
  checks the result stays under `staging/files`. The DOL SHA-256 check runs *after* the whole
  extraction, so a crafted image only needs the 6-byte ID `GEAE8P` (rev 0) to write app-writable
  files anywhere (`Config/*.ini`, `GameSettings`, saves, the `Automation/` dir). Related:
  `ExportDirectory` returns `void` and only `ERROR_LOG`s a failed file, so disk-full / a damaged RVZ
  gives a "successful" import with missing files that is then reused on every launch (the launcher's
  "Using existing extraction" path). The release builds pin only the disc ID and the DOL hash
  (`tools/build-release-linux.sh:21-22` passes neither `REQUIRED_REL_SHA256` nor
  `REQUIRED_ASSETS_SHA256`; Android checks nothing but the DOL), so a substituted `_Main.rel` or asset
  passes too. **Fix:** pre-walk the FST and fail on any name that is empty, `.`, `..`, or contains
  `/` or `\`; check each export path stays under staging; count/size-check the exported files
  against the FST (or pin the assets hash) before the final rename.
  **Done 2026-10-12 (`0afcabc`):** new shared `MG/tools/disc_extract.cpp/.hpp` (`moderngekko-local.patch`,
  built into the launcher and the Android `main` library). `CheckDiscFileNames` walks the whole FST
  before anything is written: empty / `.` / `..`, `/ \ :` or control characters, Windows device
  names, a trailing dot or space, or a case-insensitive duplicate refuses the image; depth and entry
  count are capped. `ExportDiscFiles` replaces `DiscIO::ExportDirectory`, writes with Dolphin's
  `ExportFile` and fails on the first file that can't be written or isn't its FST size. Android now
  also checks `filesystem->IsValid()`. Checked on WSL with the Linux release launcher's `--extract`:
  crafted GEAE8P images with `../evil.bin`, a `..` folder, `a.bin` + `A.BIN` and `nul.txt` are all
  refused with nothing written; the real RVZ extracts 5552 files to the ws17 DOL. The same on Windows
  with the MSVC release build (`build-release-win.bat configure` + `build`, scratch `LOCALAPPDATA`),
  plus a name with backslashes (`a\..\..\evil.bin`). Not done: pinning
  `REQUIRED_ASSETS_SHA256` (hashing 1.4 GB at every launcher start, the old startup freeze); a
  `_Main.rel` pin doesn't apply (Skies has no REL). Retroid (ws17 debug APK, 2026-10-12): the
  `../evil.bin` image picked through the system picker is refused with the message, the install
  untouched and no `.importing` left; the real RVZ re-imports (5552 files, ws17 DOL).

- [x] **A3 Medium: stale ship-battle pointer, write through freed memory.** `patches/game/qol.c:140-153`.
  `qol_ship_battle()` gates on `SHIP_BATTLE_DATA` (`lbl_8034727C`), which the game sets in
  `fn_801453D0` and never clears after freeing (our own
  `../SkiesOfArcadiaLegends-Decomp/docs/qol_cheats.md:176`). From the second ship battle on, the
  pointer is non-null from map load (mode 6, maps 500-599) until the new allocation; with Invincible
  on, `qol_refill_ship` reads `*(stale+0x20)` and writes `ship+0x18 = ship+0x14` through reused heap
  (or faults). Battle speed treats those frames as battle too. **Fix:** a patch-owned "live" flag set
  when `fn_801453D0` runs and cleared at `fn_8012A26C`'s teardown; gate on that, not the pointer.
  **Done 2026-10-12 (`0afcabc`), DOL ws17 (`910d9504` / SHA-256 `0fcdb786`):** `qol_ship_live`, set by
  `qol_ship_alloc` (new hook at `0x8015C6DC`, `fn_8015C6C8`'s `bl fn_801453D0`) and by
  `qol_ship_phase`, cleared right after `qol_ship_results`' call of `fn_80144E60` (which frees the
  data at `0x801453B8`; the only free of it); `qol_ship_battle()` requires it. In `.late_bss`
  (`cave.ld`) so every ws16 variable keeps its address (checked in `patches.txt`); a ws16 state
  made mid ship battle reads 0 until the battle's next phase. `release/skies-widescreen.dolpatch`
  regenerated (checked with `applydolpatch.py`); `build-release-win.bat` now reads the hash from it.
  Disassembly checked (`patch.elf`). Run on Windows (dev runner, `modules-llvm-pgo-ws17`, Invincible on,
  Baltor state 17b from ws15; `tools/qol-test-win.sh ... ship`): flag 0 after the load, 1 from the
  first phase, ship held at 10000 through the battle, won normally (map 205, Vyse EXP 231 as in the
  ws14 check), flag 0 after the results with the freed pointer still set (`80e86220`); a second
  `me500a` battle reallocated at the same address and set the flag again (the stale window between
  the map load and the allocation is too short to catch by polling). A second harness run fled the
  battle instead (map 90): the flag cleared all the same.

- [x] **A4 Medium: spare-EFB allocation failure, null deref on the GPU thread.**
  `RC/Core/VideoCommon/FramebufferManager.cpp:316-345`. `CreateSpareEFB` calls `PanicAlertFmt` when
  `CreateEFBFramebuffer` fails but still sets `m_spare_efb_created = true` with a half-null set;
  `SwapEFB` then exchanges it in and `BindEFBFramebuffer()` dereferences null. The spare doubles EFB
  memory; the menu offers 6x + MSAA, which Adreno / Mali can refuse. Also reachable on a
  resolution / MSAA change with interpolation on. **Fix:** return `bool`; on failure disable
  interpolation stickily (`FrameInterpolator::Shutdown()`) and never swap.
  **Done 2026-10-12 (`0afcabc`):** `CreateSpareEFB` / `SwapEFB` return `bool`; a failed spare is
  dropped (never swapped in), remembered until the EFB is recreated (a resolution / MSAA change
  retries), and `RunJob` then shuts the interpolator down, sets it off and shows "60 fps is off: not
  enough graphics memory at this resolution" (OSD). The swap back can't fail (the spare exists by
  then). `recompcore-frame-interpolation.patch`. Compiled (Linux release runner, Windows MSVC release
  runner); the failure path itself wasn't forced.

- [x] **A5 Medium: no block-count bound on an imported card save.** `MG/tools/save_import.cpp:314-327`,
  `193-235`. Dolphin's loader silently skips a `.gci` with `m_block_count > 2043`
  (`RC/Core/Core/HW/GCMemcard/GCMemcardDirectory.cpp:82-93`); the importer has no such check, so a
  2100-block GEAE8P GCI is written, the working same-identity save is deleted (kept only as `.bak`),
  the player sees "Imported file N. Continue loads it", and the game never sees it. GCS is worse:
  `ReadSavefileInternalGCS` derives `block_count` from the file size and truncates to `u16`, so a
  512 MiB + 0x150 GCS gets `m_block_count = 0` and is read whole into memory on the UI thread
  (`moderngekko_launcher.cpp:1258` runs `ImportSave` synchronously). The comment in
  `save_import.hpp:14` ("checked as strictly as the code that will load it") isn't true yet.
  **Fix:** before `ImportCardSaves`, require `1 <= blocks.size() == dir_entry.m_block_count <= 2043`;
  ideally also the game's block count and the `SA_LEGENDS.NNN` name.
  **Done 2026-10-12 (`0afcabc`):** `CardSaveProblem` refuses a save with 0 or more than 2043 blocks or
  a block list that doesn't match its count, and, with the new `SaveImportGame::save_blocks` /
  `save_name_prefix` (Skies: 3, `SA_LEGENDS.`; desktop `SaveGame()`, Android `SKIES_SAVES`), any
  other size or name; a card image with one such save is refused whole. A single-save file over
  `0x150 + 2043 * 0x2000` bytes is refused before Dolphin reads it (the GCS u16 truncation). Covered
  by new cases in `tools/save-import-test.py` (24 cases, 0 failures on WSL; the 512 MiB GCS is
  refused in 0.00 s).

- [x] **A6 Medium: save-state import validates 48 header bytes, then makes it the Continue target.**
  `MG/tools/save_import.cpp:366-418`. `uncompressed_size` (u64), `payload_offset` and the LZ4 block
  chain are never looked at. Dolphin's `DecompressLZ4` (`RC/Core/Core/StateFile.cpp:~249`) does
  `raw_buffer.reset(uncompressed_size)` → `bad_alloc` for a huge value, so a header-valid state with
  `uncompressed_size = 2^60` is copied in, dated newest, and every Continue crashes until the player
  deletes it by hand. (Beyond that DoS: a well-formed hostile state is deserialised into guest RAM /
  CPU regs / recomp state by upstream `PointerWrap` code with little validation; not traced further,
  an open risk rather than a confirmed bug.) **Fix:** bound `uncompressed_size` (fixed for this
  game, give it a margin), walk the LZ4 block lengths against the file size; consider not
  auto-selecting an imported state for Continue.
  **Done 2026-10-12 (`0afcabc`):** the importer now refuses `uncompressed_size` of 0 or over 1 GiB (Skies
  states unpack to 93-108 MB), walks the LZ4 chunk chain (each length positive and inside the file,
  the last ending exactly at the end) and refuses an unpacked size over 255 times the packed bytes
  (LZ4's limit); an uncompressed state needs payload after its offset. Real states (WSL and
  Retroid) still import; 2^60, a cut chain and a zero-length chunk are refused (save-import-test).
  Imported states stay Continue's target (the user's 10-12 design). Open, as before: a well-formed
  hostile state is still deserialised by upstream `PointerWrap` code.

- [x] **A7 Medium: a failed disc import can destroy a working install.**
  `RC/Android/jni/SkiesImport.cpp:127-130`; `MG/tools/moderngekko_launcher.cpp:656-670, 727-729`.
  Both paths do `fs::remove_all(game_root)` *then* `fs::rename(staging, game_root)`: a rename failure
  leaves no game. The desktop refresh path (656-670) also writes `ExportSystemData` straight into the
  live `output/sys` (boot.bin, apploader, an unpatched main.dol) before the patch is verified, so a
  second `ApplyBundledDolPatch` failure (a different disc picked) leaves a foreign unpatched DOL in a
  previously working install. Android's failure returns (97-113) also leave `.importing` behind and
  the status text set. No free-space check anywhere. **Fix:** rename the old folder aside, rename the
  new one in, then delete the old; stage the refresh in a copy; check free space up front.
  **Done 2026-10-12 (`0afcabc`):** `ReplaceDirectory` (disc_extract) renames the old folder to
  `<name>.old`, renames the new one in, renames the old one back if that fails, then deletes it; a run
  killed between the renames is finished by `RecoverReplacedDirectory` (desktop, at import). Desktop's
  earlier-release refresh exports `sys/` into `<id>.refresh`, patches it there and swaps it in, so a
  wrong disc leaves the live `sys/` untouched. Both platforms check free space first (the FST's
  total + 64 MiB). Android removes `.importing` and clears the status on every failure. Checked on
  WSL: a ws16 `main.dol` planted in an import is refreshed to ws17 with `files/` kept; the same with
  a wrong GEAE8P image fails and leaves `sys/` byte-identical; no `.refresh` / `.old` left. Retroid:
  updating over the ws16 app re-patched the old import from `main.dol.orig` to ws17 at first start;
  two re-imports over a working install left no `.importing` / `.old`.

- [x] **A8 Medium: Android Activity recreation during an import.**
  `SkiesLauncherActivity.kt:68,172-178,561-588`; manifest entry for `SkiesLauncherActivity`
  (`RC/Android/app/src/main/AndroidManifest.xml:76-83`) has no `configChanges`/`screenOrientation`.
  `importing` lives in the Activity; rotation / dark-mode / locale / multi-window recreates it with
  `importing = false`, `onCreate` runs `refresh()` (dir-init already done): after Change Disc `mainDol`
  still exists so Start / Continue is offered while the native thread is about to `remove_all`; after
  a first import a second import would `remove_all(staging)` under the first. `s_status` is shared;
  the old Activity leaks until the thread ends and gets `handler.post` after `onDestroy`.
  **Fix:** `android:configChanges="orientation|screenSize|keyboardHidden|uiMode|locale"` plus a
  process-level `@Volatile` flag (or a `std::atomic<bool>` in `importDisc`).
  **Done 2026-10-12 (`0afcabc`):** the import state is process-wide now (`importRunning` / `importResult`
  in the Activity's companion, set from the tap to the thread's end; native `s_importing` refuses a
  second `importDisc`), the thread holds no Activity, and each instance polls and shows the result,
  so a recreated launcher shows the import's progress instead of Start / Change Disc. Pad
  connects no longer recreate it (`configChanges="keyboard|keyboardHidden|navigation"`); rotation
  still does, to re-crop the art, which is now safe. Retroid: a display-density change 2 s into an
  import recreated the launcher, which showed the import's progress (not Start / Change Disc); the
  import finished normally and the next instance showed the menu.

- [x] **A9 Medium: Dolphin's full Settings is still reachable from the Back menu.**
  `RC/Android/app/src/main/java/org/dolphinemu/dolphinemu/activities/EmulationActivity.kt:448-453,799-889`.
  Back menu → Touch Controls → "Choose controller" → neutral button "More controller settings"
  (line 888) opens `SettingsActivity.launch(this, MenuTag.SETTINGS)`: CPU core, GFX backend, paths.
  `menu_overlay_controls_gc.xml` still includes `menu_emulation_choose_controller`. A second path:
  `performLaunchChecks` (1135-1160) opens `CONFIG_PATHS` / `CONFIG_WII` on an invalid path setting.
  HANDOFF-release says these were removed. **Fix:** in brand mode drop that menu item (or the neutral
  button) and skip the `isPathEmptyOrValid` dialogs.
  **Done 2026-10-12 (`0afcabc`):** in the branded app "Choose controller" has no "More controller
  settings" button, `MENU_ACTION_SETTINGS` does nothing, and `performLaunchChecks` skips the
  invalid-path dialogs (they open `CONFIG_PATHS` / `CONFIG_WII`). Retroid: Back menu, Touch Controls,
  Choose Controller shows only OK.

## B. Worth fixing soon

- [x] **B1 Low-Medium: `.dolpatch` run offsets can wrap, out-of-bounds write.**
  `MG/tools/dol_patch.cpp:226-261`. `run.offset` is u64 from hex; the ordering check (231,
  `run.offset < back.offset + back.bytes.size()`) and the bounds check (241) both add without an
  overflow guard, and only the *last* run is bounds-checked. Run 1 at `fffffffffffffff0` with 0x20
  bytes wraps to end 0x10; run 2 at 0x20 passes; `std::copy` (259) writes at
  `bytes.begin() + 0xfffffffffffffff0` before the output SHA (261) can reject it. Trust boundary is
  the install directory's patch file. **Fix:** per run, `offset <= output_size &&
  bytes.size() <= output_size - offset`. (`tools/applydolpatch.py` is fine: hash check before write.)
  **Done 2026-10-12 (`b0f0ca1`):** every run is checked after parsing: `offset >= previous end`, `offset <= output_size`,
  `size <= output_size - offset` (no addition on an untrusted offset). A harness built with ASan on the
  committed patch overflows the heap on the review's example; on the fix the good patch applies and the
  wrapping run, a non-last run past the end, out-of-order and overlapping runs are all refused.

- [x] **B2 Medium (Windows): path encoding mixes ANSI and UTF-8.** (2026-10-17: the user-directory
  lookup is now `tools/user_directory.cpp`, which reads `LOCALAPPDATA` with `_wgetenv` and the
  pointer file as UTF-8; development notes *Data folder*.)
  `MG/tools/moderngekko_launcher.cpp:2397-2406, 2420, 2476` (also 130-134, 152-160, 293-320, 585,
  791, 808, 1100, 1417) and the runner (`moderngekko_run.cpp` `config.game_root = value("--game")`,
  `DefaultUserDirectory`). `fs::path::string()` is the ANSI code page on Windows and is passed to
  `SDL_CreateProcess` (`--game`, `--user-dir`, runner path), `SDL_IOFromFile`, Dolphin's
  `IniFile::Load/Save`, `DiscIO::CreateVolume`, the SDL file dialog; `std::getenv("USERPROFILE"/
  "LOCALAPPDATA")` is narrow too. `--load-state` already goes through `PathText` (UTF-8), so one
  launch mixes two encodings. A user named "José" or "Мария" gets mojibake: runner can't find the
  game, Settings writes go to a different `Config` than the runner reads. **Fix:** an
  `activeCodePage=UTF-8` manifest on launcher and runner (simplest), or `_wgetenv` /
  `SHGetKnownFolderPath` + `u8string()` at every boundary.
  **Done 2026-10-12 (`b0f0ca1`):** `MG/tools/utf8.manifest` (`activeCodePage` UTF-8, Windows 10 1903+) is linked into the
  launcher and runner (CMake merges it with the linker's manifest; checked in both MSVC exes).
  `--import-save` with `LOCALAPPDATA=...\José Мария`: the committed Windows package fails ("Could not
  create the save folder: The filename, directory name, or volume label syntax is incorrect"); the
  new build writes the save under `José Мария\Skies of Arcadia Legends\GC\USA\Card A`.

- [x] **B3 Low-Medium: `config.ini` written in place, now from the render thread.**
  `MG/tools/frontend_config.cpp:248-286`; the new callback at `moderngekko_run.cpp:~250` /
  `src/runtime/dolphin_runtime.cpp:688-701`. `SaveConfig` truncates and rewrites with no temp file
  and returns true without checking the stream; a kill mid-write leaves a truncated file,
  `LoadConfig` says "missing resolution", the launcher shows an error and `return 2`, and only
  recreates the file when it is *absent*. An in-game resolution change is the new trigger. Also:
  `SaveConfig(dir, resolution, fps, controller)` (284-286) falls back to `config = {}` when the
  existing file doesn't parse (a typo'd netplay address), silently resetting backend / fullscreen /
  interpolation / nickname on the next UI save; `fs::exists(path)` at 97 is the throwing overload;
  the callback mutates the unsynchronised `reported_efb_scale` and isn't limited to the menu's
  scales (0 or >12 writes `0x0` / `8320x6864`, which `LoadConfig` then rejects). **Fix:** write
  `config.ini.tmp` + rename, check the stream, treat unparsable as missing; make
  `reported_efb_scale` atomic, skip unsupported scales, defer the write off the render thread.
  **Done 2026-10-12 (`b0f0ca1`):** `SaveConfig` writes `config.ini.tmp`, checks the stream after `close`, renames over;
  `LoadConfig`'s `exists` takes an `error_code`; the four-argument `SaveConfig` refuses (with the parse
  error) instead of resetting when `config.ini` exists but doesn't parse; `reported_efb_scale` is
  `std::atomic<int>` (`exchange`); the runner writes back only scales 1-12. Not done: moving the
  write off the render thread (a few hundred bytes on a menu change; a detached writer thread
  would outlive shutdown).

- [x] **B4 Low: `ini.Load()` result ignored before `ini.Save()`.** `moderngekko_launcher.cpp:130-170`
  (`WriteDolphinOption`, `WritePadOptions`). A transient lock (antivirus) on `Dolphin.ini` / `GFX.ini`
  makes `Save` rewrite it with one key, wiping the player's other `[Skies]` / DSP / GFX settings.
  **Fix:** when the file exists and `Load` fails, abort with an error.
  **Done 2026-10-12 (`b0f0ca1`):** `WriteDolphinOption` returns "can't read ...; try again" when `Load` fails on a file that
  exists. `WritePadOptions` already refused on a failed `Load`.

- [x] **B5 Low: `SkiesSettings::Apply` runs on the host thread, not the CPU thread.**
  `RC/Core/Core/SkiesSettings.cpp:110-122`. `Core::QueueHostJob` is drained by `HostDispatchJobs` in
  the platform main loop (`PlatformWin32.cpp:311`); the comment says "lands on the CPU thread". The
  aligned `Write_U32` is benign; the plain `bool s_applying` isn't: a change on another thread can read
  it mid-apply and drop its own `Apply`, losing a setting until the next change. **Fix:**
  `Core::RunOnCPUThread` / `CPUThreadGuard` for the RAM write; make `s_applying` atomic.
  **Done 2026-10-12 (`b0f0ca1`):** `s_applying` is `thread_local`, so only the applying thread's own config callbacks are
  skipped and a change from another thread still applies; `ApplyOnCPUThread` renamed `ApplyNow` with
  the comments saying host thread / CPU thread. The RAM write stays a host job: `RunOnCPUThread` from
  the render thread (where the in-game menu changes settings) could wait on a CPU thread that waits
  on the GPU in dual core.

- [x] **B6 Low: Skies INI values reach guest memory unclamped.** `RC/Core/VideoCommon/SkiesMenu.cpp:845-848`
  (`Write()`), `RC/Core/Core/Config/SkiesSettings.cpp:9-12`, `patches/game/qol.c:84-92` (`qol_scale`).
  `Encounters`, `Exp`, `Gold`, `MagicExp` are cast straight to `u32`; only battle speed is clamped
  (849). A hand-edited or downloaded `Dolphin.ini` with `-1` or `2000000000` gives negative or
  overflowed rewards (`value / 100 * percent`). At the menu's own values it can't overflow.
  **Fix:** clamp to the menu's min/max at the read; `percent = clamp(percent, 0, 1000)` in `qol_scale`.
  **Done 2026-10-12 (`b0f0ca1`):** `SkiesMenu::Write` clamps the four percentages to 0..`ENCOUNTER_RATES.back()` /
  `REWARD_RATES.back()` (400) before the write. `qol.c` unchanged: the host write is its only input,
  and a change would mean ws18 and new DOL hashes everywhere. Windows (`qol-test-win.sh ... rewards
  Exp=2000000000 Gold=-1 MagicExp=2000000000`): guest input reads 400 / 0 / 400, Vyse gets 12 EXP and
  8+16 magic EXP (the 4x numbers in NOTES), 0 gold.

- [x] **B7 Medium (CI): the "tag must be on main" guard lives in the tagged commit's own workflow.**
  **Done 2026-10-17** (at going public): `tools/setup-github-protection.sh` made the environments and
  rulesets below, the secrets moved into them, and the jobs name their environment.
  `.github/workflows/release.yml:40-46`, `validate.yml:30-35`. A push-tag run uses the workflow file
  *at the tag*, so a tag on a non-main commit can carry a `release.yml` with the guard removed;
  `validate.yml` also has `workflow_dispatch` (any ref) + push to `dev`, and its jobs read the secrets
  repo PAT, the artifact key and the signing key. Anyone with write access gets the DOL, PAT and
  keystore. The merge-base logic itself is sound. **Fix:** put the secret-using jobs behind a GitHub
  Environment (`release`) with a deployment branch/tag rule and a required reviewer; add a `v*`
  tag-protection ruleset.
  **Not done, your call (2026-10-12):** the fix needs GitHub Environments, which a *private* repo
  only gets on GitHub Pro / Team (Free has them for public repos), plus a tag ruleset (same rule).
  Today the threat isn't there: you are the repo's only collaborator (admin) and nothing is pushed
  yet. When the repo goes public, or on a paid plan: create environments `ci` (deployment branches
  `main`, `dev` and tags `v*`) and `release` (required reviewer: you; deployment tags `v*`), move
  `SOAL_SECRETS_REPO_WITH_PAT` and `SOAL_ARTIFACT_KEY` into `ci` and the four `RELEASE_*` into
  `release` (2026-10-17: signing is its own `android-sign` job, so a release waits for one approval,
  right before signing; `tools/setup-github-protection.sh` + `tools/set-release-secrets.sh
  --environments` do all of this), delete the repo-level copies, then add `environment: ci` to validate.yml's
  codegen / windows / linux / android jobs and `environment: release` to `android-sign`, and a ruleset that
  restricts creating `v*` tags to admins. Do the workflow line first only on a plan that has
  environments.

- [x] **B8 Medium (CI): the publish job's "no game data" check hashes only the original DOL.**
  `release.yml:97-113`. Name-based (extensions, `generated/`, `chunks/`) plus the hash `87c240ac`; the
  *patched* DOL `7b4c416c` (what the launcher produces) isn't checked there although
  `tools/ci-codegen.sh:35` checks both; nested archives aren't scanned; the recompiled module
  (`gGEAE8P_recomp.dll` / `libgGEAE8P_recomp.so`) is translated game code and matches nothing, which is
  by design but should be an explicit allow-list. The check does stop the upload when it fires (own
  step before `gh release create`, `-e` + `pipefail`). **Fix:** add `7b4c416c`, scan nested
  archives, allow-list the module by name.
  **Done 2026-10-12 (`1d4856b`):** the publish job sparse-checks-out `release/skies-widescreen.dolpatch`
  and refuses both its `input-sha256` (checked to be the original) and its `output-sha256` (the
  patched DOL, so no hash to keep in step); archives inside the packages are unpacked three levels
  deep (zip / apk / jar / aar) and any tar / 7z / rar fails the check; the recompiled module must be
  present, by name. Tested locally on the real Windows zip + ws17 APK (passes) and on the same zip
  with the patched DOL renamed `game.bin` inside a nested `extras.zip` (refused).

- [x] **B9 Low (CI): unpinned toolchain downloads.** `validate.yml:70-74, 88` (`wget llvm.sh | sudo
  bash` as root before the secrets are fetched; `LLVM_VERSION: 20.1.8` labels whatever apt.llvm.org
  ships for 20), `:128-131` (Windows LLVM tarball, no checksum, cached and used as the compiler),
  `choco install ninja` unpinned. **Fix:** pin / SHA-256-verify the script and tarball; install the
  exact version.
  **Done 2026-10-12 (`b0f0ca1`):** the codegen job runs on `ubuntu-26.04` and installs Ubuntu's own
  `llvm-20-dev=1:20.1.8-2ubuntu8` (archive-signed; the same build this machine made the shipped
  codegen with; apt fails rather than substitutes) and checks `llvm-config-20 --version` =
  `LLVM_VERSION`; no `llvm.sh`. The Windows tarball is checked against GitHub's published digest
  (`LLVM_WINDOWS_SHA256`, `f229769f…`) before unpacking. The unpinned `choco install ninja` is gone:
  the windows-2025 image ships Ninja 1.13.2. Checked against the package index and the runner-image
  manifests only; CI hasn't run.

- [x] **B10 Low: keystore and APK signer checks can't fail.** `tools/make-release-key.sh:40-42`:
  `keytool … | grep … | head -2 || {…}` without `pipefail` tests only `head`, so the "did not verify"
  branch never fires and secrets get uploaded from an unverified keystore. `tools/android-ws-build.sh:92`:
  `apksigner verify --print-certs | grep -q "CN=Skies…"` loses apksigner's exit status and matches a
  CN string, not the digest. **Fix:** run `keytool -list` alone and test its status; check
  apksigner's status and compare the signer SHA-256 to a pinned value.
  **Done 2026-10-12 (`1d4856b`):** `make-release-key.sh` tests keytool's own status (and now pins the
  certificate digest in `release/android-signer.sha256`); `android-ws-build.sh` (release mode) needs
  apksigner's status, exactly one signer, and that signer's SHA-256 equal to the keystore's (read
  with the build's password), plus the pin when the file exists. Tested with the debug key on the
  ws17 debug APK (match) and a throwaway key (mismatch). The existing release key has no pin file
  yet (the cert is encrypted in the keystore, so it needs your password): `keytool -list -v
  -keystore ~/skies-release-key/release.jks -alias skiesrecomp-release | sed -n 's/^[[:space:]]*SHA256:
  //p' | head -1 | tr -d : | tr A-F a-f > release/android-signer.sha256`, then commit it.

- [x] **B11 Low: Android GPU-driver install error handling.** `SkiesLauncherActivity.kt:264-283`,
  `RC/Android/app/src/main/java/org/dolphinemu/dolphinemu/utils/GpuDriverHelper.kt:49-59,80-125`.
  `GpuDriverMetadata.deserialize` throws `IllegalArgumentException` on `[]` or
  `{"schemaVersion":{}}`; callers catch only `SerializationException`, so `installDriver` skips
  `uninstallDriver()` and the broken `meta.json` stays; line 271 (`getInstalledDriverMetadata()`) is
  outside the try in the worker thread and kills the app, after which `customName()` (Settings /
  About) and `deviceInfo` crash every time. `installDriver` uninstalls first, so any failure leaves
  `DriverLibName` + the KEPT file naming a driver that is gone (VulkanLoader falls back silently, UI
  shows the stale name). No size cap on the zip extraction (zip bomb fills `filesDir`); `libraryName`
  unvalidated (nit: the package already supplies native code). **Fix:** catch `Exception` in both
  places, `uninstallDriver()` + `SkiesDriverGuard.useSystemDriver()` on any failure, cap the extracted
  size, require `libraryName` to match `[A-Za-z0-9._-]+`.
  **Done 2026-10-12 (`b0f0ca1`):** `GpuDriverHelper`: both `deserialize` calls catch `Exception`; `installDriver` refuses a
  `libraryName` that isn't `[A-Za-z0-9._-]+`, starts with `.`, or isn't a file in the package;
  `ZipUtils.unzip(stream, dir, maxBytes)` stops past 256 MiB. `SkiesLauncherActivity.installDriver`
  reads the metadata inside the `try` and calls `SkiesDriverGuard.useSystemDriver` on any failure.
  Kotlin compiles (`:app:compileDebugKotlin`); not run on a device.

  **Checked on the Retroid 2026-10-12 (ws18 debug APK):** three packages made for it, each refused with
  "That isn't a GPU driver package this app can use (InvalidMetadata)", nothing left in
  `files/GPUDrivers/Extracted`, `DriverLibName` still empty, no crash: `meta.json` = `[]`, a
  `libraryName` of `../libevil.so`, a `libraryName` not in the zip. Then Turnip v26.0.0 R8
  (K11MCH1/AdrenoToolsDrivers, SHA-256 matching GitHub's digest): "GPU driver Mesa Turnip driver
  v26.0.0 - R8 Vulkan 1.4.335 installed", `hook_impl` loaded it (Mesa in logcat), shaders rebuilt
  (391), the auto-save at 59.86 fps (speed 1.006), and the clean exit asked "Keep this GPU driver?"
  with its name.
- [x] **B12 Low: PENDING driver marker written before the launch is confirmed.**
  `SkiesLauncherActivity.kt:555-559` + `EmulationActivity.kt:1113-1160`. `launch()` calls `onLaunch()`
  (writes PENDING) and sets `pendingStatePath` before `EmulationActivity.launch`; if that is dropped
  (`ignoreLaunchRequests` on a double tap, or "Yes" on the invalid-path dialog) the marker stays and a
  later warm `onResume` wrongly reverts a good driver with the "didn't close normally" dialog.
  **Fix:** write PENDING from the `launchWithoutChecks` path, or clear it (and `pendingStatePath`) on
  a dropped launch.
  **Done 2026-10-12 (`b0f0ca1`):** `EmulationActivity.launch` takes an optional `beforeStart`, run in `launchWithoutChecks`
  after `ignoreLaunchRequests` is set; the launcher's `onLaunch` + `pendingStatePath` moved into it.
  Compiles; not run on a device.
  **Checked on the Retroid 2026-10-12:** with a driver selected (a test name, then Turnip), a double tap
  on Continue started one game; the marker held the driver's name while it ran; Exit Game removed it
  and the launcher asked "Keep this GPU driver?" (not the crash revert).

- [x] **B13 Low-Medium: `ViSwap` lets a stale picture override a blanked screen.**
  `RC/Core/VideoCommon/Present.cpp:~177-186`. `FetchXFB` empties `m_xfb_entry` / `m_xfb_rect` when
  the VI reports `fb_width == 0 || fb_height == 0` (the game reconfiguring VI); `ViSwap` still calls
  `PictureForRetrace(xfb_addr, …)`, which matches the old `Shown` and installs a real picture with a
  0x0 rect and forces a present: a stale frame instead of black, 0/0 in the aspect maths. Smaller
  variant: a picture sized for the previous VI height when the height changes between shown frames
  (one stretched frame). **Fix:** only consult `PictureForRetrace` when `fb_width != 0 && fb_height
  != 0` (or skip when `m_xfb_entry` is null).
  **Done 2026-10-12 (`b0f0ca1`):** `ViSwap` asks `PictureForRetrace` only when `fb_width` and `fb_height` are non-zero
  (hand-edited hunk in `recompcore-frame-interpolation.patch`). The smaller variant (one stretched
  frame on a VI height change) is left. Builds; not reproduced (needs the game to blank the VI with
  interpolation on).

- [x] **B14 Low: a second save import destroys the first `.pre-import.bak`.**
  `MG/tools/save_import.cpp:141` (`copy_options::overwrite_existing`), `201-209`. The backup name is
  fixed per file: import A over the original, then B over A, and the original's backup is gone while
  the UI says the replaced save "is kept". **Fix:** `.pre-import.N.bak` (numbered or timestamped), or
  refuse to overwrite an existing backup. Document in NOTES *Importing saves* either way.
  **Done 2026-10-12 (`b0f0ca1`):** `FreeBackupPath`: `<name>.pre-import.bak`, then `.pre-import.2.bak` and up (999 at most);
  NOTES *Importing saves* and `release/README.md` say so. New case in `tools/save-import-test.py`
  (a third import keeps both copies): 25 cases, 0 failures on the Linux release launcher.

## C. Lower / hygiene

Launcher and runtime (`MG/tools/…`, `MG/src/runtime/…`):
- `moderngekko_launcher.cpp:2172-2183`: `WriteDolphinOption(AUTO_LOAD_CARD, …)` result ignored and
  netplay Host / Join never writes 0, so a stale non-zero `AutoLoadCard` can make the next boot
  auto-load a card save although a state was chosen.
  **Fixed 2026-10-12 (`89cdb47`):** the launcher decides `AutoLoadCard` once, at the launch (Continue from a card sets
  its file, everything else 0, netplay included), and refuses to launch when the write fails. WSLg
  test with a stand-in runner: a stale `2` became `0` for a state; `2` for a card Continue.
- `moderngekko_launcher.cpp:1072-1110, 2404`: Continue takes `launcher_savestates.front()` from
  `StateSaves` without a game check (the code's own comment notes it). Harmless in the branded
  per-game user dir; filter on the `state-skies-` / `recovery-skies-` prefixes anyway.
  **Fixed 2026-10-12 (`89cdb47`):** for Skies, Continue takes the newest `state-skies-` / `recovery-skies-` state, by its
  index in the list. WSLg: with a newer `zzz-foreign.sav` beside `state-skies-slot1.sav`, Continue
  showed and booted Slot 1.
- `moderngekko_launcher.cpp:777-785, ~312` (`SiblingRunner` / `SiblingExecutable`): when the sibling
  exe is missing it falls back to a bare file name, which `CreateProcess` resolves through the
  working directory and PATH. Fail instead.
  **Fixed 2026-10-12 (`89cdb47`):** always the full path beside the launcher; the launcher's folder now comes from
  `SDL_GetBasePath()` (argv[0] is wrong when started through PATH). WSLg: the stand-in runner
  beside the test launcher was the one started.
- `moderngekko_launcher.cpp:1040-1070` (`FindDiscImages`): recursive depth-4 scan of `Documents` on
  the UI thread before the first frame, even when a game is already imported; a OneDrive-redirected
  Documents freezes the window. Run it lazily on Select Disc, or on a worker thread.
  **Fixed 2026-10-12 (`89cdb47`):** only while there's no playable game, and at most 2 s.
- `moderngekko_launcher.cpp:1253-1260, ~1307`: SDL dialog callbacks get pointers to stack locals
  (`dialog`, `save_dialog`) that are never invalidated; quitting with the OS dialog open is a
  use-after-free on exit.
  **Fixed 2026-10-12 (`89cdb47`):** both dialog states are heap-allocated and never freed.
- `moderngekko_launcher.cpp:344-371` (`WriteDefaultGame`): non-atomic write, stream state unchecked,
  `.string()` encoding (B2).
  **Fixed 2026-10-12 (`89cdb47`):** `.tmp` + rename, stream checked (encoding: B2's manifest).
- `moderngekko_launcher.cpp:130-146` (`ReadDolphinOption`): `static_cast<int>(std::stod(text))` is UB
  out of range (`1e99`), accepts `nan`. Clamp first.
  **Fixed 2026-10-12 (`89cdb47`):** values outside ±1e9 and nan read as unset.
- `moderngekko_launcher.cpp:2438-2446`: the log is opened with `"w"`, so a relaunch overwrites the
  crash log the player would send. Rotate to `.old`.
  **Fixed 2026-10-12 (`89cdb47`):** the previous log is renamed `.old` first (WSLg: checked).
- `moderngekko_launcher.cpp` `ExtractDisc` (`output = games_directory / disc_id`): `disc_id` is only
  length-checked; a generic build with no `MODERNGEKKO_REQUIRED_DISC_ID` would pass `../..x` to
  `remove_all`. Require alphanumeric. **Fixed 2026-10-12 with A2.**
- Automation protocol (`MG/tools/automation_protocol.cpp:425`, `src/runtime/dolphin_runtime.cpp:349-362`,
  `RC/Android/jni/Automation.cpp:243-290`, `jni/CMakeLists.txt:55-62`): it is a directory of command
  files, not a socket, and the launcher never passes `--automation-dir`, so nothing is exposed on
  desktop. On Android it is compiled into the release APK whenever `automation_protocol.cpp` exists
  and is enabled by creating `<User>/Automation/`, which the Files app can do through the
  `DocumentProvider`; `ResolveControlPath` accepts absolute paths (`read_memory` dumps guest RAM
  anywhere the app can write, `write_memory` pokes it), `set_config` has no key allow-list and writes
  config from a non-main thread. Build it only in debug / behind a Gradle flag; reject absolute
  paths and `..`; allow-list `set_config` keys.
  **Fixed 2026-10-12 (`89cdb47`):** Android builds it only for the debug build type
  (`DOLPHIN_ANDROID_AUTOMATION_BUILD`, `app/build.gradle.kts`); the release configure has no
  `Automation.cpp`. Path and key rules not added: `tools/drive.py` sends absolute paths, and on
  desktop the protocol needs `--automation-dir`, which only a local user can pass.

Android (`RC/Android/…`):
- `features/DocumentProvider.kt:176-192, 222-227`: `isChildDocument` uses bare `startsWith` (so
  `root/Config2` is a child of `root/Config`); `documentIdToPath` doesn't canonicalise, `..` escapes
  the root. Gated by `MANAGE_DOCUMENTS` (system / Files app only), so not remote. The root is the
  whole user dir: the Files app can delete or overwrite the extracted disc, saves and `Config/`.
  Canonicalise + prefix-check; compare `parent + "/"`.
  **Fixed 2026-10-12 (`89cdb47`):** ids resolve canonically inside the user folder (else `FileNotFoundException`),
  `isChildDocument` compares whole segments, new / renamed names can't hold `/`, `.`, `..` or NUL.
  The root stays the whole user folder (players copy saves through it). Compiles; not run on a
  device.
- `AndroidManifest.xml:25-33, 59-63`: the branded app still declares INTERNET, RECORD_AUDIO, EPG and
  legacy-storage permissions it doesn't use, and `profileable shell=true`.
  **Fixed 2026-10-12 (`89cdb47`):** `app/src/skies/AndroidManifest.xml`, merged for the branded build only, removes
  INTERNET, ACCESS_NETWORK_STATE, RECORD_AUDIO, both storage and both EPG permissions, sets
  `requestLegacyExternalStorage` / `preserveLegacyExternalStorage` false (so Dolphin can't pick the
  shared `dolphin-emu` folder on Android 10 and older) and drops `<profileable>`. The branded debug
  APK lists only VIBRATE (+ AndroidX's own); the release manifest the same. **Needs a device run**
  before the release: nothing in the branded app should need the network, but it hasn't been played.
- `SkiesLauncherActivity.kt:441-450`: `refresh()` hashes `main.dol` and may `repatch` on the UI
  thread at every `onResume` (ANR on slow storage).
  **Fixed 2026-10-12 (`89cdb47`):** the hash is cached by the DOL's size and time. A repatch after an app update still
  runs on the UI thread, once.
- `SkiesLauncherActivity.kt:236-259` (`importSave`): copies the picked document to
  `cacheDir/save-import` with no size limit; fixed file name races if started twice.
  **Fixed 2026-10-12 (`89cdb47`):** a temp file of its own per import, at most 1 GiB + 1 MiB (the largest state the
  import accepts).
- `jni/SkiesImport.cpp:176-190` (`setPadOptions`): `GetObjectArrayElement` results not null-checked,
  pending Java exception ignored (callers are internal; robustness only). `Repatch` line 64 uses
  the throwing `fs::exists` (**fixed 2026-10-12 with A7**).
  **Fixed 2026-10-12 (`89cdb47`):** `setPadOptions` returns false on a null array / element or a pending exception.

Core / video (`RC/Core/…`, DolRecomp):
- `FrameInterpolator.h:56-70`, `.cpp:~1100-1135`: `s_enabled` / `s_replaying` are plain globals
  written on the GPU thread (`ApplyPendingEnabled`) and read on the CPU thread (`CoreTiming::Idle`,
  the `ScopeGuard` in `WaitForGpuToDrain`); `ReadEnabled()` runs lazily on whichever thread asks
  first and also writes `s_verbose` / `s_test_*`. Formal race; practical effect a one-frame
  disagreement. `std::atomic<s8>` / `std::atomic<bool>`, relaxed; call `ReadEnabled()` once at init.
  **Fixed 2026-10-12 (`89cdb47`):** `s_enabled` / `s_replaying` are atomics read relaxed. `ReadEnabled` still runs
  lazily (its other flags are statistics / test switches). Windows, opening cutscene: 59.94 fps,
  898 of 900 frames interpolated.
- `FrameInterpolator.cpp:~275-290` (`ReplayFits`): reads `GetTimeSleeping()` and
  `CoreTiming::GetTicks()` (CPU-thread state) on the GPU thread before the dual-core early return;
  results discarded in dual core. Move the return up.
  **Fixed 2026-10-12 (`89cdb47`):** moved up.
- `FrameInterpolator.cpp`: `s_current.stream`, `array_data`, `textures` grow unbounded while no XFB
  copy with a non-zero stride arrives (`s_recording` stays true). Cap (e.g. 64 MiB) and stop recording
  until the next `EndFrame`.
  **Fixed 2026-10-12 (`89cdb47`):** 64 MiB (stream + array data): the frame is dropped and recording waits for the
  next frame end (an empty frame is never replayed). A normal frame records about 1 MB.
- `FrameInterpolator.cpp:~1678` (`OnReplayPrimitive`): `data + copy.data_offset - copy.lo` can point
  before the vector when `lo > data_offset` (UB; accesses stay in range in practice). Use `uintptr_t`
  arithmetic or a per-array virtual base.
  **Fixed 2026-10-12 (`89cdb47`):** computed through `uintptr_t`.
- `StaticRecompCore.cpp:~396` (`RestoreBootDolCode`), `HW/DVD/DVDThread.cpp` (`ReadBootDOL`):
  `DiscIO::GetBootDOLSize` adds `offset + size` in u32 uncapped; `std::vector<u8> dol(*size)` up to
  ~4 GiB → uncaught `bad_alloc` on the CPU thread with a corrupt / modded disc. Cap at ~16 MiB. (The
  later `fits()` checks bound the memcpy correctly.)
  **Fixed 2026-10-12 (`89cdb47`):** `DVDThread::ReadBootDOL` refuses a size over 24 MiB (the console's RAM).
- `DolphinNoGUI/PlatformWin32.cpp` `EndFastForward`: restores the speed saved when Space went down,
  so a menu speed change while Space is held is undone. Harmless otherwise.
  **Fixed 2026-10-12 (`89cdb47`):** it restores only while the speed is still fast-forward's.

Game patches (`patches/game/`):
**Fixed 2026-10-12 late (game patch ws18, DOL SHA-1 `6a401968`, SHA-256 `39dd1e6c`):** see each
item. All 19 data symbols keep their ws17 addresses (`llvm-nm` on both `patch.elf`), so ws17 states load
as they are; code area 0x10A0 of 0x1100. `release/skies-widescreen.dolpatch` regenerated (reproduces the
DOL with `applydolpatch.py`); both release builds reconfigured (they require `39dd1e6c`); Linux
release: a fresh import gives ws18 and a ws17 import is brought to ws18. Windows module
`modules-llvm-pgo-ws18`: frame A/B against ws17 at 16:9, every common picture identical (first battle
199, Baltor ship battle 194, state 13's flight 98); state 07 walked into a random battle: the shatter
covers the whole 16:9 picture; camera test (tilt, zoom, turn, the ceiling limit) as before; ship QOL
test (Invincible, ship at 10000, `qol_ship_live` 0 after the battle, 1 in the next); opening cutscene
59.94 fps, 898 of 900 frames interpolated. Android: `skies-recomp-ws18-2026-10-12.apk` built, not
installed. `build/shatter/s01.sav` (an f82e437-era state) stalls right after its load on ws17 and ws18
alike: not usable any more.
- `camera.c:92-96`: `highest = eye[1] + rise - look_at[1]` can go negative when the old eye sits
  below the look-at by more than `rise`; with a negative `sin_pitch` the distance goes negative and
  the eye flips to the far side. Arithmetic confirmed, in-game trigger not traced. `if (highest < 0)
  highest = 0;`.
  **Fixed (ws18):** not by clamping to 0 (that puts the eye on the look-at point); when `highest <= 0`
  the camera keeps the game's own tilt for that frame.
- `widescreen.c:147-156` (`ws_check`): a version mismatch resets config / scissor / text state but
  not `unwide_depth` / `unwide_saved`; a state restored with a non-zero depth leaves the unwide
  wrappers inactive.
  **Fixed (ws18):** `ws_check` resets the depth (`unwide_reset`, a function so no data moves), and
  `ws_unwide_end` never takes it below 0 (an end whose begin came before the reset would otherwise
  leave every later pair off).
- `widescreen.c:281` (`in_ram`): accepts 0x81800000-0x81FFFFFF (past the 24 MB) and unaligned
  pointers while walking the back chain; only matters with a corrupt chain.
  **Fixed (ws18):** word-aligned and below 0x81800000 (the same in `trace2d.c`).
- `shipdebug.c:24` and `trace2d.c:29` both use `0x80319078`; README says either combines with
  `widescreen.json`, but using both together overwrites each other's word (debug only).
  **Fixed:** the ship request is the buffer's last word, `0x8031AA24` (`tools/shipbattle-state-win.sh`
  and the README follow); not part of the release DOL.

Tools:
- `tools/gamepatch.py:225`: `shutil.copytree(dirname(args.dol), sys_out)` copies the whole parent of
  `--dol` (`--dol ~/Downloads/main.dol` copies all of Downloads). Require the parent to be `sys` or
  copy the known files. `:117`: duplicate `symbols` across specs silently override.
  **Fixed 2026-10-12 (`89cdb47`):** `--dol` must sit in a `sys/` folder; a symbol defined twice with different values
  stops the build. `widescreen.json` still builds ws17 byte for byte (`0fcdb786`).
- `tools/save-import-test.py:18`: `shutil.rmtree(work, ignore_errors=True)` on argv[4] with no
  guard. Refuse a non-empty dir without a marker.
  **Fixed 2026-10-12 (`89cdb47`):** refuses a non-empty folder without its `.save-import-test` marker.
- `tools/ci-codegen.sh:24`: `tail -30` of the DolRecomp log into the Actions log on failure can carry
  game-derived addresses / instruction text (public once the repo is). Print the exit status or
  grep error lines only.
  **Fixed 2026-10-12 (`89cdb47`):** prints the exit status and the error-line count only. The `find | grep -q`
  checks write their listings to files first.
- `validate.yml:97-108`: the PAT is in `git clone` argv (visible in `ps`; log is masked); if the `cp`
  after the clone fails, `$RUNNER_TEMP/s` (`.git/config` holds the token) survives because the
  `always()` step only removes `secrets/`. Add `rm -rf "$RUNNER_TEMP/s"`; pass the token via
  `GIT_ASKPASS` / `http.extraheader`. `tools/set-release-secrets.sh:27` has the same argv pattern locally.
  **Fixed 2026-10-12 (`89cdb47`):** the token goes to git as an `http.extraheader` through `GIT_CONFIG_*` environment
  variables (not argv, not `.git/config`), masked; the always-step removes `$RUNNER_TEMP/s` too.
  Same header in `set-release-secrets.sh`. Checked locally: a clone with the header leaves no token
  in the clone, and a wrong token is refused (so the header is what authenticates).
- `validate.yml:114-116` (Encrypt) and `:225-230` (Android Decrypt): default `bash -e`, no
  `pipefail`, so a failed `tar` still encrypts a truncated stream (fails a later job, not this one).
  `find … | grep -q` under `pipefail` (`release.yml:109`, `ci-codegen.sh:37`) can SIGPIPE `find` into a
  false "not found"; very unlikely with tiny output.
  **Fixed 2026-10-12 (`89cdb47`):** `shell: bash` (= `-eo pipefail`) on Encrypt and the Android Decrypt; the
  `find | grep -q` cases fixed above (`release.yml` already writes listings to files).
- `tools/build-release-win.bat:18`: `MODERNGEKKO_REQUIRED_DOL_SHA256` hard-coded to `7b4c416c…`;
  `build-release-linux.sh:22` derives it from the `.dolpatch`. Derive it the same way. **Fixed
  2026-10-12 with A3** (`findstr` on the patch's `output-sha256` line; not run on Windows yet).
- `scripts/check-build.sh:78`: predictable `/tmp/moderngekko-build-check-$$`; use `mktemp -d`.
  **Fixed 2026-10-12 (`89cdb47`):** `mktemp -d`.
- The workflow never uses `RELEASE_KEY_PASSWORD`; `android-ws-build.sh:80` reuses the store password
  as the key password.
  **Fixed 2026-10-12 (`89cdb47`):** CI passes `RELEASE_KEY_PASSWORD` as `ORG_GRADLE_PROJECT_keypass`;
  `android-ws-build.sh` uses it when set, else the store password.
- `tools/drive.py:59,76,102,125`: `ADB_AUTO` and basenames interpolated into `adb shell` strings
  (local dev tool, own device only).
  **Fixed 2026-10-12 (`89cdb47`):** quoted with `shlex.quote`.
- `MG/tools/save_import.cpp`: `FileNumber` (52-58) overflows `int` on a 20+-digit suffix (display
  only); `std::ranges::sort(files)` (304) sorts "file 10" before "file 2"; the `ImportCardImage`
  message calls any 0x2000-multiple ≥ 512 KiB "the size of a memory card image" although `Open`
  accepts only the six real sizes; the state header is read three times (438-441, 383, 412) so check
  and copy see different reads if the file changes under a desktop picker; `32 + version_length +
  extended.size()` (385) can wrap in u32 for `version_length > 0xFFFFFFDF` with no practical effect
  (the read fails). `dol_patch.cpp` `LoadManifest` rejects a manifest with a trailing blank line.
  **Fixed 2026-10-12 (`89cdb47`):** `FileNumber` stops past 5 digits; imported files listed by number;
  the card-image message says "looks like a whole memory card"; `LoadManifest` skips blank lines.
  Left: the state header read three times (a desktop picker file changing between reads only gets
  refused or imported as it is then); the u32 sum was already 64-bit.

## D. Second review (2026-10-22)

The same kind of review on `dev` `8bf2116` (v0.82.0-beta's version strings; the code is that of
`1125b1b`): every patch applied to fresh submodules (`tools/apply-patches.sh`) in a scratch worktree,
six reviewers (desktop launcher and file imports; Dreamcast music, Uncensored and the European
streams; core, video and DolRecomp; settings menu, input and platforms; the Android app; game
patches, tools and CI). Every Medium below was then re-read in the code; the Lows are as the
reviewers traced them. Nothing was built, run or changed, and no device was touched. Same
abbreviations and the same rule for submodule files as above; line numbers are from the patched
working copies of `8bf2116`.

### Medium

- [x] **D1 Medium: camera control is inert on the European and Japanese discs.** FIXED 2026-10-22, game
  patch ws33: the suffix dropped and `gamepatch.py`'s pattern made suffix-tolerant (development notes,
  *Camera control*). Only the patch's tilt, zoom and orbit were dead; the C-stick turn is the game's own.
  `patches/game/camera.c:54` (`#define FIELD_CAMERA_EYE 0x80302F94u`), `tools/gamepatch.py:42`.
  `ADDRESS` (`\b(fn_|lbl_|0x)(80[0-9A-Fa-f]{6})\b`) needs a word boundary after the eighth digit, so
  the `u` suffix hides this literal from the region translation: the EU and JP DOLs compare against
  the USA address (the tables map it to `0x80306614` and `0x80302A14`). `cam_view` (`camera.c:124-131`)
  calls `cam_orbit` only when `manager[MANAGER_EYE_SOURCE] == FIELD_CAMERA_EYE`, which never holds
  there: hold Z + C-stick / R / L does nothing, while the hook itself is installed (so the frontend's
  hook check passes). The only suffixed `0x80…` literal that is a table entry (the others are the
  `0x80000000u` RAM-range checks). `HALF` (line 43) has the same gap. **Fix:** drop the `u`; make the
  regex suffix-tolerant (`(?<![\w.])(fn_|lbl_|0x)(80[0-9A-Fa-f]{6})(?![0-9A-Fa-f])`, the same for
  `HALF`) and have the translation fail on any untranslated `0x80xxxxxx` left that isn't an identity
  entry; new EU / JP `.dolpatch` files, camera checked on both discs.

- [ ] **D2 Medium: a texture pack can write outside its import folder (zip-slip).**
  `RC/Core/VideoCommon/TexturePackImport.cpp:101-116` (`InGameFolder`), `254-277` (`Target`),
  `279-299` (`Begin`), `350-362` (`Commit`); desktop and Android (`RC/Android/jni/SkiesImport.cpp`).
  `Target` checks only the file name (`IsTexture`: `tex1_*.png` / `.dds`); the folders before it are
  taken as they are, from `InGameFolder` (everything after `GEA/`) or, for a pack without one, the
  whole entry path. `Begin` then does `File::CreateFullPath` + `IOFile(m_temp + relative, "wb")`, and
  `Commit` renames `m_temp + relative` to `target + "/" + relative`. An entry
  `GEA/../../../../Config/tex1_x.png` (zip or 7z; minizip's own path cleaning is only in the
  extract-to-file call, which isn't used, and 7z names come raw from `SzArEx_GetFileNameUtf16`)
  creates folders and writes or truncates a `tex1_*` file anywhere the app can write: the user folder,
  app data, shared storage with All files access. Nested archives (`ImportArchives`) go through the
  same `Target`. Not code execution (the name is constrained), but a write outside the folder from a
  downloaded file; in a debug APK it can reach `Automation/commands/` (D15). **Fix:** reject an entry
  with an empty, `.` or `..` segment, `:`, `\` (after the `/` conversion), a control character or a
  leading `/`; `NameProblem` in `MG/tools/disc_extract.cpp:35-56` is the rule to reuse.

- [ ] **D3 Medium: a damaged `BONITO.DRV` crashes the game at every start (Dreamcast music on).**
  `RC/Core/AudioCommon/DCMusic.cpp:105-106`. `image_size = min(Read32(d + 8), drv.size()) - 0x20`
  wraps in u32 when the driver header's size word is below 0x20, and
  `lib/HighlyTheoretical/Core/dcsound.c:429-440` (`dcsound_upload_to_ram`) masks only the destination,
  so it reads `src[i]` for ~4 GB past the file's buffer. `Load` checks only "SDRV" and at least
  0x120 bytes; the import (`RC/Core/AudioCommon/DCMusicImport.cpp:424-446`) copies any
  `BONITO.DRV`. A damaged or crafted Dreamcast disc → SIGSEGV at every boot until the import is
  removed. The reviewer's harness (`drv_size.cpp`, not kept) printed `image_size = 0xFFFFFFF0` for a
  288-byte buffer. **Fix:** refuse `Read32(d + 8) < 0x20`, `> drv.size()` or `> 2 MB` in `Load`, and
  check the "SDRV" / "SMLT" magic and sizes at import, so a bad disc is refused there.

- [ ] **D4 Medium: a failed Dreamcast music start is retried every frame.**
  `RC/Core/Core/SkiesMusic.cpp:442-444` (`if (want && !s_running) s_running = DCMusic::Start();`),
  `RC/Core/AudioCommon/DCMusic.cpp:496-526`. Nothing remembers the failure: an `ARESNEW.MLT` without
  "SMLT", a `BONITO.DRV` whose work-table pointers are ≥ 0x200000 (`DCMusic.cpp:114`), or D3's fixed
  refusal → every poll reads both files, allocates and clears the ~8.5 MB chip state, uploads the
  driver and runs 0.25 s of ARM emulation (`Render(nullptr, AICA_RATE / 4)`, ~9 ms on desktop,
  ~17 ms on the Retroid) before failing again. The game runs at about half speed with no message for
  as long as the option is on. **Fix:** latch the failure until the option is changed or the import
  changes, and say so in the Dreamcast tab.

- [ ] **D5 Medium: the auto-save deletes the oldest auto-save before the new one exists.**
  `RC/Core/VideoCommon/SkiesMenu.cpp:1334-1345` (`autosave::Save`). `PruneAutomatic(directory,
  AUTO_KEEP - 1, ...)` runs first; `State::SaveAs` only queues the compress-and-write on Dolphin's
  dump thread, and a failed write (disk full, folder not writable) shows only an OSD line. Disk full
  with three auto-saves → the oldest goes, the new one isn't written: two left, and every later
  interval prunes (a no-op at two) and fails again. A crash between the prune and the write loses one
  too. **Fix:** prune after the write has succeeded (State's after-save callback, or keep
  `AUTO_KEEP` and prune the surplus after `SaveAs`).

- [ ] **D6 Medium: the frame interpolator can read freed memory when the second EFB can't be made.**
  `RC/Core/VideoCommon/FrameInterpolator.cpp:1897-1933` (`PictureForRetrace`), `1462-1468`
  (`RunJob`), `1880-1882` (`Shutdown`). `PictureForRetrace` keeps `current = &s_shown[i]` and, on the
  second retrace, calls `FinishJob()` → `RunJob` → `SwapEFB()` fails (high resolution + MSAA on a
  phone GPU, the case the code handles) → `Shutdown()` → `s_shown.clear()` → then reads
  `current->interp` / `current->real` (line 1933) through the dangling pointer: a crash or a garbage
  picture. Reached when the GPU thread is a frame behind (`PullEvents` before FIFO work: a pipeline
  compile hitch). `EndFrame`'s own leading `FinishJob()` (1602) survives it but then pushes a stale
  `Shown` into the disabled interpolator. **Fix:** don't shut down inside `RunJob`; set a flag there
  and apply it (`Shutdown` + `s_enabled = 0`) at a safe point (`ApplyPendingEnabled`, start of
  `EndFrame`), or re-find the frame after `FinishJob()` and return `{}` if it's gone.

- [ ] **D7 Medium (Android): save import, backup restore and export, and driver install have no busy
  state.** `RC/Android/app/src/main/java/org/dolphinemu/dolphinemu/skies/SkiesLauncherActivity.kt:386-420`
  (`importSave`), `449-468` (`importBackupEntries`), `484-521` (`exportSaves`), `523-550`
  (`installDriver`). Unlike the disc, texture, music imports and the data move, they never set
  `importRunning`; "never while the game runs" (the comment at 364-366) is checked only before the
  picker and once before the import. A large backup restore (the zip copied first, up to 1 GiB, then
  up to 400 entries one by one) looks idle; one Back (`panel.back()` → `close()`) gives the live home
  screen, Continue boots the game, and `GCMemcardDirectory` reads `GC/USA/Card A` while card files are
  still being written and renamed there: the later ones are missing from the session and can be lost
  at its card flush. The same window lets Back up saves zip files the booting game rewrites, and a
  driver install re-extract the driver folder Start is loading. `save_import.cpp` has no core check of
  its own. **Fix:** the `importRunning` state (with `showImporting()`) for all four, and re-check
  `NativeLibrary.IsRunning()` before each entry of the backup loop.

- [ ] **D8 Medium (Android): the Dreamcast and texture-pack Remove buttons don't wait for the game to
  close.** `RC/Android/app/src/main/java/org/dolphinemu/dolphinemu/skies/SkiesSettingsPanel.kt:470-473`
  (Dreamcast), `426-430` (textures) → `RC/Android/jni/SkiesImport.cpp:576-584`. Every other
  file-replacing action goes through `afterGameClosed {}`. With a game left running without its screen
  (the "Game in progress" case, `SkiesLauncherActivity.kt:752-768`), Remove deletes `DreamcastMusic/`
  (with `STREAMS/`) and `DreamcastUncensor/` under it; the next read of a file
  `SkiesUncensor::Redirect` / `SkiesStreams::Redirect` still points at fails (`DVDThread.cpp:322-323`
  empties the buffer → "The disc could not be read", lines 294-300) and the session is lost. Removing
  a multi-GB pack runs `DeleteDirRecursively` on the main thread (ANR). **Fix:** `afterGameClosed {}`
  and a worker thread for both; also have `DiscContent::Read` (`RC/Core/DiscIO/DirectoryBlob.cpp:98-103`)
  fall back to the disc's own file when a redirected one can't be opened (covers D19's cache too).

- [ ] **D9 Medium, plausible: Continue on the Japanese disc may write past the card task's work area.**
  `patches/game/autoload.c:97` (`dl_work = fn_801E1C7C(0x270)`), against `patches/game/README.md:60-61`:
  the Japanese `fn_801A4070` also probes `EA_LEGEND_EXTRA #00`-`#02` at mount (`fn_801A3BB4`) "into the
  task's work +0x270", i.e. from the end of this allocation on. The documented JP Continue tests pass,
  likely the allocator's rounding slack (0x270 isn't 32-byte aligned). Not checked against the
  decomp. **Fix:** allocate the size the Japanese load screen's own task uses (0x300 covers the three
  probe results by any reading); a heap pointer, so no layout or state change.

- [ ] **D10 Medium, plausible: netplay desyncs when the players' gameplay options differ.**
  `RC/Core/VideoCommon/SkiesMenu.cpp:1186-1208` (`qol::Write`), `986-1000` (`camera::Write`),
  `RC/Core/Core/SkiesSettings.cpp:119-138`. The launcher offers netplay (`MG/tools/moderngekko_launcher.cpp:1396-1403`),
  but encounters, rewards, battle / field speed, Invincible and Hold B are written into guest RAM from
  each player's own config every poll, while only pad input is shared (`NetPlayClientInput.cpp:239`).
  One player at `Encounters = 50` or `BattleSpeed = 2` → `qol_encounter_rand` / the retrace skipping
  differ → desync within the first walk. Only the language restart checks
  `NetPlay::IsNetPlayRunning()` (`SkiesMenu.cpp:311-314`). The netplay layer itself wasn't traced.
  **Fix:** during netplay write the defaults (100 / 100 / 100 / 100 / 1x / off / off) and grey out the
  Gameplay tab, or send the values with the netplay settings.

### Low

Android:
- [ ] **D11:** picker results use the user folder before the asynchronous directory init.
  `SkiesLauncherActivity.kt:1231` (`startImport`), `485`, `408`, `1053`, `544`;
  `RC/Android/.../utils/DirectoryInitialization.kt:206-213`. `getUserDirectory()` throws until the
  init is done; `onCreate` (226-231) waits with `AfterDirectoryInitializationRunner`, the result
  handlers don't. The OS kills the backgrounded launcher while the system picker is up (low RAM, or
  "Don't keep activities") → recreated activity, the result arrives before the init → crash (the
  save import's worker thread too); texture and music imports run with empty paths. A1's class. **Fix:**
  run each handler through `AfterDirectoryInitializationRunner().runWithLifecycle(this) { ... }`.
- [ ] **D12:** the results of save import, backup and driver install are posted to the Activity that
  started them; a rotation or dark-mode change in between (no orientation lock,
  `AndroidManifest.xml:75`) recreates it and the result panel goes to the old one: the player never
  sees whether it worked. **Fix:** the companion-object result pattern the disc import uses.
- [ ] **D13:** `onCleanExit` in `EmulationActivity.onPause` runs whenever the activity finishes,
  including a frontend's CLEAR_TASK relaunch with the core still running: the driver guard's pending
  marker is cleared and a later crash of that session isn't caught. **Fix:** only when the core is
  stopping; skip `checkDriver()` while `IsRunning()`.
- [ ] **D14:** `checkDriver()` runs from `onResume` during a data-folder move (`SkiesLauncherActivity.kt:291-307`);
  "Keep this GPU driver?" then saves `GFX.ini` into the folder being moved (lost, or an orphan
  `Config/` left behind). **Fix:** return early while `dataMoveRunning`.
- [ ] **D15:** debug builds only: the automation channel (`RC/Android/jni/Automation.cpp:9-10, 293-310`;
  pad input, guest memory read / write, states to any path) trusts `<User>/Automation/`, which after
  a data-folder move is in shared storage that other apps with All files access can write (and D2
  can reach). Release APKs don't have it (`build.gradle.kts:98-103`). **Fix:** anchor it in
  `filesDir`, or require a marker there.

Dreamcast music, Uncensored, European streams:
- [ ] **D16:** the Dreamcast import puts ISO 9660 names in the output path as they are
  (`DCMusicImport.cpp:284-294, 314-319, 442-446`): `Wanted()` only needs `SOUND/M…MLT`, so
  `M\..\..\..\X.MLT` passes, and on Windows `CreateFileW` resolves the `..` (a `*.MLT` written outside
  the music folder; Linux and Android fail on the missing `M`). No cap on the number of wanted
  entries (a 2 MB directory of 4 MB files on the same sectors fills the disk). **Fix:** the
  `NameProblem` rule; a total-size cap (~300 MB).
- [ ] **D17:** `ApplyRequests` reads an MLT (up to 4 MB) with `File::ReadFileToString` inside the
  audio callback under `m_queue_lock` on a song's first play (`DCMusic.cpp:298-359, 374-389`):
  an audio dropout, and the CPU thread's `Send()` waits on the same lock. **Fix:** read it on the CPU
  thread in `SkiesMusic::Play` and pass the bytes with the request.
- [ ] **D18:** `BuildPal60` runs on every disc, USA and Japanese too, and reads all 446 stream files
  in full (>100 MB) at every boot just to compare their headers, before the cache key is computed
  (`SkiesStreams.cpp:648-658, 696-757, 790-810`). **Fix:** only on GEAP8P, and read only the 0x60-byte
  headers.
- [ ] **D19:** the 60 Hz copies served live are in `Cache/SkiesPal60/` (Android: `externalCacheDir`,
  `DirectoryInitialization.kt:125`), which the OS or "Clear cache" can delete mid-session; the next
  read of one ends the session as in D8. **Fix:** D8's fallback, or keep them under the user folder.
- [ ] **D20:** first run only: a pal60 copy published while the game is part-way through the original
  stream can pair the European header with the shortened data (`SkiesStreams.cpp:713-717`); unlike
  `SkiesUncensor::Redirect` (`SkiesUncensor.cpp:402-410`) there is no choice latched at offset 0.
  Cosmetic. **Fix:** latch at offset 0 the same way.
- [ ] **D21:** the detached conversion threads (Uncensored, pal60; up to ~50 s on the first run)
  use `File::GetUserPath` / manifest statics, so quitting during them can crash in static destructors
  (`SkiesUncensor.cpp:382-391`, `SkiesStreams.cpp:805, 840`). After the config is saved, so low
  impact. **Fix:** a shutdown flag checked between files, or join from `Core::Shutdown`.

Texture packs:
- [ ] **D22:** no cap on the bytes a pack writes: LZMA / LZMA2 / stored folders stream straight to
  disk (only the in-memory PPMd / filter path has the 1 GiB cap, `TexturePackImport.cpp:619`); a
  decompression bomb fills the drive (reported as "Is the disk full?", staging removed). And
  `Commit` (335-397) deletes each same-named texture before renaming the new one in, so an I/O error
  part-way leaves a second import half applied (re-importable). **Fix:** a total cap well above real
  packs (~9 GB); delete an old file only after its replacement is in.

Settings menu and input:
- [ ] **D23:** "Reset keyboard keys" with no keyboard found (`FindKeyboard()` null: Wayland, an
  Android device without one) strips every keyboard binding and adds none, then saves, and says
  "Keyboard keys reset" (`RC/Core/Core/SkiesInput.cpp:681-687`, `235`, `241`). **Fix:** "No keyboard
  connected", as `StartCapture` does (612).
- [ ] **D24:** smaller menu items, `SkiesMenu.cpp`: `s_texture_pack` is read in `Draw` outside the
  lock, the one value not in its copy (389, 394-395, 2176); `autoload::Active` saves the config under
  `s_mutex` on the video thread (1397-1398; a few ms once per boot); `CycleSpeed` steps from an
  unclamped `BattleSpeed` (1086-1089, 1284-1287: `BattleSpeed = -5` shows "-4x"); texture filter,
  anisotropy and scaling are read from `GFX.ini` unclamped (405-407; `1 << v.anisotropy` at 2095 with
  `MaxAnisotropy = 40`); the autoload request block is written without a release fence before its
  magic word (1406-1409; the game reads magic, then file; on ARM64 a Continue can rarely fail with
  "save wasn't found"). **Fix:** copy under the lock, defer the save, clamp the three as
  `field_speed` is (415), a release fence before the magic store.

Core, video, platforms:
- [ ] **D25:** `FrameInterpolator::detail::ReadEnabled` (1033-1050) initialises lazily from whichever
  thread calls `IsEnabled()` first, writing plain bools: a formal data race at boot (same values).
  **Fix:** initialise at video-backend init, or `std::call_once`.
- [ ] **D26:** the GPU loop's batching (`Fifo.cpp:339-351`) checks `AtBreakpoint` between batches of
  up to 2 KiB only (upstream: every 32 bytes), so a CP breakpoint inside a batch is stepped over. Skies
  doesn't use FIFO breakpoints; only matters for the generic core. And `WaitForGpuToDrain` gives up
  after 250 ms (`Fifo.cpp:626-657`): a longer GPU-thread stall (a first pipeline compile on a slow
  Android driver) returns a stale token, and the game's "GPU hung" recovery is what the wait exists to
  prevent. **Fix:** stop the batch at `CPBreakpoint` when enabled; no cap while the GPU is making
  progress.
- [ ] **D27:** macOS `applicationShouldTerminate:` returns `NSTerminateCancel` after `Stop()`
  (`RC/Core/DolphinNoGUI/PlatformMacos.mm`, ~100-108): a logout or shutdown with the game running is
  aborted ("the app refused to quit"), although it exits a moment later. **Fix:** `NSTerminateLater`,
  then `replyToApplicationShouldTerminate:YES` when the main loop ends.
- [ ] **D28:** `PlatformWin32.cpp:159, 316, 356` import `GetDpiForWindow` and
  `SetProcessDpiAwarenessContext` statically, so the runner needs Windows 10 1703 or later (the
  README says Windows 10 or 11). **Fix:** `GetProcAddress`, or state the minimum.

Tools and CI:
- [ ] **D29:** `tools/dcmusic/gdrom.py:57, 74-80` writes `os.path.join(out, name)` with the ISO 9660
  name as it is (a record `../../x` writes outside OUTDIR). Developer tool. **Fix:** reject `/`, `\`,
  `..`, or check the real path stays under `out`.
- [ ] **D30:** `tools/sign-apk.sh:11, 25-26` has `set -eu` without `pipefail`, so the testOnly check
  (`aapt2 dump ... | grep -q ...`) passes when `aapt2` fails; `tools/set-release-secrets.sh:67, 84, 86`
  the same (a failed `openssl` / `base64` stores an empty secret; validate.yml's `test -n` then fails
  safe). **Fix:** `set -o pipefail`; check aapt2's exit; `put` refuses an empty value.
- [ ] **D31:** validate.yml's Linux job uses `container: image: ubuntu:22.04` by tag (383-384), and
  the macOS job runs `brew install ninja` (494-495) after the Decrypt step (483-492), with the
  decrypted codegen in the workspace. **Fix:** pin the image by digest; install before decrypting.

## The tabled battle-speed freeze (fixed 2026-10-21)

Changing battle speed while a round plays out can freeze the battle (NOTES *Known issues*, tabled by
the user 10-11). The game-patch reviewer traced `qol.c` for it: the patch holds no derived state
across a speed change (`qol_speed_phase` resets whenever the speed is not fast; the frame-start mark
`lbl_8034768C` is rewritten every frame and, per the asm, read only at `0x801DC49C` and written only
at `0x801DCB88`; the `OSGetTick` lines in `fn_801DC420` feed an FPS counter, not game logic). A
freeze that survives a save-state reload points at game RAM state driven by the host side (the 3x/4x
overclock, or the audio / DSP timing the game waits on), not at the C patch.

**Fixed 2026-10-21 (on `dev`):** it was both of those together: the recomp core's guest timebase ran fast under the 3x / 4x overclock and stalled the KO00NN stream every spell and S-move waits for; `recompcore-overclock-timebase.patch`, `e106420`, development notes *A battle can freeze*. Checked on Windows and the Retroid.

## Checked and OK (so nobody re-reviews it)

- Runner spawned via `SDL_CreateProcess(WithProperties)` with an argv array, no shell; SDL quotes the
  Windows command line. Nickname / address / port / buffer validated in `LoadConfig` / `SaveConfig`,
  fixed arrays + `snprintf`, no `strcpy` / `sprintf`. No command injection.
- CI: no `pull_request` / `pull_request_target`; top-level `permissions: contents: read`, only
  `publish` has `contents: write`; every third-party action pinned to a full SHA;
  `persist-credentials: false` everywhere; no `${{ github.* }}` interpolation into `run:` beyond the
  regex-validated tag; `inputs.vname` via `env:`; codegen artifact AES-256 / PBKDF2 with the key via
  `-pass env:`, 1-day retention, refused without the key, DOL and profiles deleted in `always()`;
  keystore to `$RUNNER_TEMP` with no `set -x`, passwords via `ORG_GRADLE_PROJECT_*` env not argv;
  `make-release-key.sh` / `set-release-secrets.sh` use `-storepass:env`, stdin, 700/600; the publish
  job downloads only the two named artifacts, never codegen; `.bat` files `|| exit /b 1` every step.
- Manifest (brand): `dolphinExported` → false for MainActivity, TvMainActivity, AppLinkActivity;
  EmulationActivity, SettingsActivity, CustomFilePickerActivity non-exported; FileProvider
  non-exported; `SkiesLauncherActivity` exported with MAIN only and reads no intent data;
  `DocumentProvider` behind `MANAGE_DOCUMENTS`; `allowBackup=false`; no cleartext / network config.
  Zip-slip in driver extraction prevented upstream (`ZipUtils.createNewFile` canonicalises +
  prefix-checks); drivers land in app-private `filesDir/GPUDrivers`. JNI: `GetStringChars` released,
  no local-ref leaks in loops, `ToJString` via UTF-16 so hostile file-name bytes are safe. `importSave`
  refuses while the game runs (checked before and after the copy), deletes the cache copy in
  `finally`. Disc identity (GEAE8P rev 0) checked before extraction. Imports / driver installs / save
  imports on worker threads with main-looper `Handler`. `EmulationActivity.launch` has the
  `ignoreLaunchRequests` guard. Release script refuses an APK not signed with the release key;
  `-dontobfuscate` keeps JNI names. Back menu hides settings / screenshot / disc swap / save slots /
  Wii items (except A9).
- Save import: state header offsets match (lzo_size 8, cookie 24, version length 28); a GCS's zeroed
  header can't be mistaken for a state (cookie word decodes to 0x45524542); the size-modulo format
  detection (0x40 / 0x150 / 0xC0) matches Dolphin's `ReadSavefile`; `GCMemcard::Open` enforces the six
  card sizes, `ExportFile` indexes within `DIRLEN`; destination name is
  `Common::EscapeFileName(maker-gamecode-filename) + ".gci"` after the game-ID equality check (no
  traversal); backup before change, failure there changes nothing; staged write + rename; old
  same-identity files removed only after the rename; partial multi-save failures reported honestly;
  imported state names unique; `lastState.sav` excluded from `List`.
- `dol_patch.cpp`: input SHA checked first, output SHA before `ReplaceFile`, already-patched DOL left
  alone; word-patch address mapping in u64 with ambiguity rejection; `ParseHexBytes` / `from_chars`
  reject signs and prefixes; `ReplaceFile` stages then renames (`MoveFileExW` write-through). `mkdolpatch.py`
  output matches the parser. Desktop `ExtractDisc` stages into `<id>.extracting` and never touches
  `StateSaves`, `GC/`, `Config/`.
- Continue: states sorted newest-first; `front()` only after an emptiness check; ties go to the state
  (`>=`); card scan filtered by game code across USA / EUR / JAP, Card A and B; `CardSaveFile` returns 0
  for Card B / odd names so Continue falls back to a plain boot; the runner re-checks `--load-state`
  exists.
- Core: FIFO batching (`RunGpuLoop` up to 64 chunks) correct at the wrap, `CPEnd` is always the last
  chunk, `ReadDataFromFifo` bounds OK, `CPReadWriteDistance` acquire after the CPU's seq_cst
  `fetch_add`; `WaitForGpuToDrain` / `WaitForFifoSpace` bounded (250 ms / 2 s), exit on
  `m_emu_running_state` / `IsInterruptWaiting` / `GPReadEnable == 0`, no-op in single core, no lock
  held while spinning; PE token read consistent. Gather pipe: big-endian `align 1` stores, ≤ 7 bytes
  overshoot into a buffer with room, `UpdateGatherPipe` memmoves spill back (no 128-byte wrap issue),
  pointer passed by reference so `DoState` fix-up is honoured, `cpu_reset` preserves the new fields,
  lockstep keeps journaling, `cpu_state_size` handshake rejects old modules, `test_module_abi.c`
  offsets match, `mtmsr` region-leader encoding correct. Replay: `s_current` / `s_ready` /
  `s_previous` rotation consistent, `FinishJob` before every reuse, `RunJob` restores game state /
  textures / TMEM / dirty flags on every path, `Shutdown` on the GPU thread via the blocking event,
  `HeldTexture` locks released before the cache dies, replay never writes RAM / token / draw-done /
  TLUT / TMEM loads, `RecordArrays` scans bounded and capped at 1 MiB through `GetPointerForRange`,
  resolution / MSAA changes land between slices so no swap is in flight. Vulkan pipeline cache: saved
  on a background thread joined before destroy / reload / shutdown, gated by an atomic, temp + rename,
  validated at load. `SkiesMenu`: all shared state under `s_mutex`, `Draw` copies under the lock,
  config writes after unlock, no indexing overruns, autoload RAM scans masked and compared to
  `ram_size`, state ops via host jobs. `CoreTiming::Idle` bounded, checks `m_emu_running_state`.
  `RestoreBootDolCode` section bounds in u64. Win32 destructor detaches `GWLP_USERDATA` before
  `DestroyWindow`.
- Game patches: `cave.ld` layout asserts hold (`qol_input` 32 bytes before `cam_input` at
  0x80002EC0); asm stubs save LR at 20(1) with 16 / 112-byte aligned frames; `qol_wait_retrace`,
  `dbg_field`, `ws_frame_end` tail-call the original; `FULLWIDTH` touches only r0 / r11 / r12 / cr0;
  the variadic `TextPrintf` wrapper keeps CR6; `fn_8002DC14` ABI matches `qol_damage`; text-node
  offsets match `text.h`; Invincible never writes above max; encounter rate can't divide by zero or go
  negative (≤ 0 → 16383, divide guarded); ship-results scaling happens once (`fn_80144E60` is called
  once from `fn_8015C6A8`). `gamepatch.py`: `subprocess` with argv lists, section overlap / cave bounds
  / `bl` range checked, `redirect_sites` fail unless the expected `bl` is there.

### Checked and OK in the second review (2026-10-22)

- **Withdrawn:** "the 7z LZMA window is smaller than the decoder's minimum dictionary" (a read before
  the window, `TexturePackImport.cpp:665-704`) was reported and then withdrawn by the same reviewer,
  and doesn't hold: the window is `max(min(dictionary, unpack_size), 1)`; when the stream is no
  longer than the dictionary the ring buffer never wraps and every match distance is below the bytes
  decoded so far; when it is longer, the buffer wraps at `dicBufSize` as LzmaDec expects; a 0-byte
  folder never writes. Don't report it again.
- Desktop imports: `disc_extract.cpp`'s `CheckDiscFileNames` / `NameProblem` (empty, `.`, `..`,
  `/ \ :`, control characters, trailing dot or space, Windows device names, case-insensitive
  duplicates) before any write, depth 32 and 2^20 entries, collisions and size mismatches refused,
  `ReplaceDirectory` keeps the old game when the swap fails (A2 / A7 hold). `dol_patch.cpp`: B1 holds;
  output ≤ 64 MiB; data-original and known-change runs bounded by the input; output hash checked
  before temp + rename. `save_import.cpp`: A5 / A6 / B14 hold (block counts, the 3-block identity,
  single-save size cap, state size 1..1 GiB with the LZ4 chain walked at a 255x ratio cap, numbered
  backups, staged copy + rename, region and game ID before writing, name from `GenerateFilename`).
  `user_directory.cpp`: same-folder and nested moves refused (`weakly_canonical`), `<to>.moving`,
  free space, pointer tmp + rename, `RemoveUserData` only ever removes the real current folder, an
  absolute pointer only, a missing moved folder reported rather than created. Runner: argv array, no
  shell, `--load-state` re-checked, port range checked, resolution write-back atomic (B3).
  Continue / state folders: `state-skies-` / `recovery-skies-` filter, romhack states apart,
  `MoveDiscStates` by the header's disc ID.
- Dreamcast music, Uncensored, European streams: GDI / CHD (`FindTrack` bounds, u64 offsets,
  libchdr's hunk range check, `hunkbytes % 2448`), ISO 9660 records (length ≥ 34, name within the
  record, directories capped at 1024 sectors, depth 1), per-file caps 4 / 16 MB, `.part` + rename,
  the NTSC-over-PAL keep rule, Remove limited to its two folders. `DownloadUnits` bounds; Highly
  Theoretical's ARM loads / stores and AICA sample reads are masked; a bad ARM instruction fails the
  engine for good (silence, no spin). Locking: `s_lock` → `m_queue_lock` on both threads, the audio
  thread only try-locks `s_lock`. Uncensored: both inputs SHA-1-checked before parsing, the result's
  SHA-1 and exact size checked, `Current()` needs the size and a `.src` stamp, the choice latched at
  offset 0, a failed conversion deletes its output (same size, so states load either way). Pal60:
  banks by size + SHA-1 prefix, streams by both channels' header counts, the cache key includes
  headers, sizes and version, published only after both channels are written atomically.
- Core and video (re-checked on the current code, since Fifo, the interpolator, CoreTiming and
  StaticRecompCore changed after the first review): the dual-core state hold (`State.cpp:98-110`,
  `Fifo::PauseAndLock` while `PullEvents` keeps the blocking event alive, restored by ScopeGuard);
  no new fields in any `DoState` and `STATE_VERSION` unchanged, so states load both ways; the
  interpolator is shut down on a state load on the GPU thread after video state is restored and
  before the texture cache dies; the gather pipe's ref is rebuilt by `GPFifoManager::DoState`, at most
  39 bytes queued in a 512-byte buffer, the fast path only without REL modules and lockstep; the
  overclock timebase (`last_OC_factor_inverted` changes only in `CoreTiming::Advance`, never in a
  burst; fractions carried in double, no overflow); `WaitForFifoSpace` exits on every condition with a
  2 s cap; PE token read; `RestoreBootDolCode` bounds and the 24 MiB cap; `RecordArrays` (≤ 12
  arrays, 1 MiB per copy, `GetPointerForRange`), 64 MiB recording cap, `Align` capped at 200k cells,
  `s_shown` ≤ 3, replay never touches RAM, cache, token, draw-done, bbox, TLUT or TMEM preload; FIFO
  wrap at `CPEnd`; the Vulkan pipeline-cache thread joined everywhere. DolRecomp: `lhbrx` / `sthbrx`
  (BSWAP32 then LSHR 16) correct for load and store; the `mtmsr` leader test; the Win64 sjlj frame
  with `"frame-pointer"="all"`; `test_module_abi.c` offsets 3536 / 3544 / 3552.
- Settings menu and input: every menu static under `s_mutex` or on the CPU thread (D24 the
  exceptions), CPU-thread config writes after unlock, consistent lock order down to the
  `ControllerInterface` devices mutex, no deadlock found; the language restart only clears
  `m_running` and tears down on the host thread after `MainLoop`, `--load-state` stripped; state ops
  are host jobs that pause the CPU; guest writes clamped (encounters / rewards 0..400, battle speed
  1..4, field speed quarters 4..12, camera offsets, aspects from fixed tables, Match screen 4:3..32:9)
  and gated on each hook word; slots 1..5 / 0..5; `PruneAutomatic` prefix-filtered (player and
  imported states survive); `SplitExpression` / `ParseTerm` safe on any string, out-of-list values
  clamped; `Follow` holds `shared_ptr`s, so an unplug can't dangle; A / B can't be cleared; Win32,
  Wayland, X11 and Quartz input handling bounded. Note: this Dolphin's state loader has no game-ID
  check, so the per-version folders are the only guard against loading another version's state.
- Android: no new exported components; `SkiesLauncherActivity` reads no intent data; the brand's
  `dolphinExported=false`, non-exported picker and FileProvider; `DocumentProvider` behind
  `MANAGE_DOCUMENTS`, now confined to the canonical root with `/`, `.`, `..` and NUL refused in names
  and whole-segment `isChildDocument`; `src/skies/AndroidManifest.xml` only removes; `allowBackup=false`,
  no cleartext, debuggable only in debug. Both `ZipUtils.unzip` overloads canonicalise and
  prefix-check; driver packages capped at 256 MiB, `libraryName` a plain file; backup restore never
  uses entry names as paths (temp copy, content sniffed, deleted in `finally`; 400 entries, 1 GiB
  each). Data-folder move as on desktop; revoked All files access or a missing SD card starts in a
  throwaway cache folder and asks, nothing is recreated in the moved folder. A1 holds; disc / texture /
  music imports and the move keep their results in companion statics; orientation changes ride the
  upstream recreation path. JNI strings, local refs and pending exceptions handled; `ImportDisc`
  checks ID, revision, FST names and free space before writing. API-30/31 calls behind `SDK_INT`.
  (`MANAGE_EXTERNAL_STORAGE` is declared in `src/main`, so the stock-Dolphin flavour asks for it too;
  policy, not security.)
- Game patches: every `fn_` / `lbl_` / `0x80…` token in the `.c`, `.S`, `.json` and `cave.ld` is in
  both region tables (256 addresses; D1's suffixed literal the one escape), the two tables have the
  same keys; the shipped `.dolpatch` files' MetroTRK-area run is 0x2BA0 bytes (< 0x3210) and the
  cutscene stubs sit at +0x26F4 in all three (the ws32 pin); `cs_camera_command` / `UNWIDE` /
  `SQUEEZED` frames and registers; `cs_note` bounds before hashing; `pal60.c`'s window keeps reads in
  the copy and rewrites only `04000000 3F99999A 00000013`; `qol.c` dereferences every battle pointer
  after `qol_ram`, with the phase, slot, unit and target bounds; `dcmusic.c` and `autoload.c` bounds;
  `cave.ld` asserts; `gamepatch.py`, `mkdolpatch.py`, `applydolpatch.py`, `dolknown.py` checks.
- Tools and CI: no `shell=True`, `os.system` or `eval` on untrusted input; `drive.py` is a file-drop
  client, no listener; automation off in release builds; release scripts' ancestry, tag and green-run
  checks; signer digest pinned; `ci-codegen.sh` refuses any DOL in its output. PII sweep outside `lib/`:
  only the `/home/someone` placeholders and the `regen@invalid` identity. CI: no `pull_request` /
  `pull_request_target`; `contents: read` with only `publish` writing; every action pinned to a full
  SHA; `persist-credentials: false`; no event or input interpolated into `run:`; codegen encrypted
  before every upload (cache key includes the key's digest), private inputs deleted in `always()`;
  environments `ci` / `release`; the keystore in `$RUNNER_TEMP`; publish downloads only the named
  artifacts and refuses any original or patched DOL by hash.
- Not assessed: the LZMA SDK's 7z header parser (`7zArcIn.c`) against a crafted `.7z` (memory-unsafe
  C, not fuzzed: an open risk, not a finding); Highly Theoretical's AICA / DSP internals beyond the
  masking checks; anything needing the DOLs or the decomp (the argument conventions of the hooked
  functions, whether a USA / JP script contains pal60's sequence); the netplay layer (D10); whether
  the pinned action SHAs match the tags in their comments; everything on a device.
