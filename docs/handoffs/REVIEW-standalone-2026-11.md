# Review of the standalone work (2026-11-19)

Asked for in `HANDOFF-review-standalone.md`: security, bugs, code quality and features, for the 2026-11-18 / 19
work on `aurora-backend` (Tier A, `114fce1..db49447`), the packaging work (Tier B) and, as far as time went, the
standalone host (Tier C). Line numbers are from `db49447`. Branch `review-standalone`, local, not pushed.

## Summary

**Verdict: one thing must be fixed before the next test build (S1); the rest is sound enough for a test
prerelease.** Nothing found is reachable over a network. Everything needs a file the player loads, or another
app on the same Android device.

| Severity | Count | Items |
|---|---|---|
| High | 1 | S1 |
| Medium | 7 | S2, S3, B1, B2, B3, B4, B5 |
| Low | 21 | S4-S10, B6-B14, Q1-Q5 |

How it was done: I read every hunk of Tier A myself. Three Sonnet helpers covered the file parsers, thread
safety and packaging / CI; I checked each High and Medium of theirs in the code before listing it. Verdicts:
**CONFIRMED** = reproduced (by whom is stated), **PLAUSIBLE** = traced in the code, not run.

Two fixes are committed here as separate local commits (the handoff allows trivial ones): `f51b34a` (S1) and
`efa2fdf` (B1). Nothing else was changed.

CI run 44577309166 finished its `dawn` job: all four packages were built and uploaded (Windows 14.0 MB,
Android 11.6 MB, Linux 9.7 MB, macOS 5.1 MB), so the Windows `tar.exe` fix works. Its platform jobs didn't run
(see B4).

## Fix status (2026-11-19, after the review)

Done on `standalone-issues` (Linux build, a 5-start A/B sweep US / EU / JP with no error and every frame repeated;
the crafted inputs below re-run):
- **S1** `f51b34a` (the reviewer's), and the **same flaw in the Dolphin build**, found while checking question 1
  (`recompcore-gci-state-paths.patch`, see S1). Both checked with crafted states.
- **S2** the Android test hooks (`env.txt`, `args.txt`, `SKIES_EXTRA_ARGS`) are read only by the test app
  (package ending `.test`) and the bare shell; the release app logs that it ignored them.
- **S3** each count needs the bytes its elements take (`count(name, min_item)`), plus a 2,000,000-field cap; the
  crafted `event_count` / `entry_count` states are refused at once (0.7 s, was 5 GB); real states still convert.
- **S4** the list check divides (no wrap) and an own-format load catches exceptions and restores the state before.
- **S5** `recompcore-png-limits.patch`: libpng's limits 8192 x 8192, the size and the allocation checked.
- **S6** the disc's file table: 64-bit count, names inside the table, plain names only.
- **S7** the close broadcast needs the app's signature permission (`${applicationId}.permission.CLOSE_GAME`).
- **S8** `Log()` writes the home folder as `~`; Aurora's lines (a `log_sink` in GXRuntime's backend config) and
  the DSP's go through it too: no `/home/...` left in a run's log.
- **S10** the `.sav` size bounded by LZ4's 255:1 before zero-filling; saved copies' sizes in 64 bits and capped.
  The Dreamcast importer's re-reads (a stall) are left.
- **B1** `efa2fdf` (the reviewer's). **B2 / B3 / B4**: `standalone-host.yml`, called once per platform: Dawn and
  the standalone built without secrets (so a dispatch run on any branch builds and links them), each platform job
  waiting for its own; a release fails without the patched Dawn. The platform jobs unpack the host before they
  decrypt the game's code (also S9's install order for Linux).
- **B5** the Invincible key latched like the menu key (xdotool: P on, P off); **B6** the key watch reads atomic
  copies, the game thread loads; **B7** a state load waits for the video thread first; **B8** the video thread's
  event poll doesn't wait for the input lock; **B9** no usable button falls back to the defaults; **B10** AR size 3,
  code writes reported as skipped (by address), AR / Gecko names kept apart, byte fills as one `memset`;
  **B11** only devices with a stick count as controllers, touches released when the window loses focus;
  **B12** local time by `timegm(localtime())`, a clock before 2000 starts at 2000; **B14** the unused Dawn flag,
  `pkill -f` replaced by `pgrep -x java`, Dawn artifacts kept 7 days, the cache key has the workflow and runner OS.
- **Q5** the stale comments.
- Not done: S9's checksums (stock Dawn tarball, Aurora's own downloads), B13 (settings read once), Q1-Q4, the
  feature ideas.

---

## Security

### S1. High: a crafted save state writes a file anywhere the player can write
- **Where:** `standalone/src/hw_memcard.cpp:515` (`file.path = s_folder / name;`), used by `Flush` at `:352-359`
  and `:375-377`.
- **Claim:** `CardDoState` takes each card file's name from the state and joins it to the card folder unchecked.
  `../` or an absolute path leaves the folder. `Flush` later writes the save to that path, or renames the file
  there to `.deleted`.
- **Scenario:** a shared save state (the standalone's own format) holds a card whose file entry is named
  `../../../../x`. The player loads it (in the menu, Import a save, or Continue). When the game next flushes the
  card, at the latest on exit, it writes the save's bytes to that path. The 64-byte header and the blocks all
  come from the state, so the content is the attacker's: a `.bat` in the Windows Startup folder, a `.desktop`
  autostart file, or on Android a library inside the app's own `GPUDrivers/Extracted`, which the next start
  loads.
- **Verdict: CONFIRMED.** I saved a state from a card Continue, renamed one card file in it to
  `../../../../PWNED-BY-STATE` (same length) and changed one byte of that file. Loading it in a fresh user
  folder wrote `PWNED-BY-STATE` (24,640 bytes) outside the user folder at exit (`[card] wrote PWNED-BY-STATE`).
- **Fix: done in `f51b34a`.** A name that isn't a plain file name fails the load. Checked with a rebuilt binary:
  the crafted state is refused ("the state's memory card names a file outside the card's folder") and the
  unmodified state loads as before. Not yet built on Windows, Android or macOS.

- **The same flaw in the Dolphin build (v0.83 and earlier; found while answering question 1):**
  `GCMemcardDirectory::DoState` stores the card folder and each save's full path, and a load takes both.
  `CEXIMemoryCard::DoState` reads the card contents whenever the *state* says so (the flag is the state's, not
  "a movie is running"), so any loaded or imported Dolphin state can carry them. **CONFIRMED** on the Dolphin
  runner: a state saved with card contents, one save's path changed to a file outside the user folder and
  marked dirty, wrote that file at exit. **Fixed in `recompcore-gci-state-paths.patch`:** a load keeps the open
  card folder and only plain `.gci` names; anything else fails the load and restores the card (Dolphin's undo
  after a failed load doesn't, since its own states carry no card contents). Checked: the outside `.gci` name
  lands in the card folder, a `.bat` name is refused with every card file unchanged, a normal state loads. The
  released Dolphin build needs the patch on `dev` and a release to reach players.

### S2. Medium: the Android test hooks are active in the release APK
- **Where:** `standalone/src/main.cpp:530-604` (`AndroidSetup`): `env.txt` (`:551`), `SKIES_EXTRA_ARGS` (`:562`),
  `args.txt` (`:586`).
- **Claim:** every Android build reads these from `Android/data/<package>/files`. `env.txt` sets any variable,
  over the ones `SkiesGameActivity` set (`SKIES_NATIVE_LIB_DIR`, `SKIES_GPU_DRIVER_DIR`, which
  `LoadCustomGpuDriver` passes to `dlopen`). `SKIES_EXTRA_ARGS` adds arguments after the launcher's, including
  `--module`, `--user`, `--load-state`.
- **Scenario:** on Android 9-10 any app with the storage permission can write that folder. It drops an `env.txt`
  that loads a crafted state at every start; with S1 that becomes a file written inside the app's private
  storage. Loading a library straight from that folder should fail, because external storage is mounted
  no-exec; I did not test that on a device.
- **Verdict: PLAUSIBLE.** Earlier review C compiled Dolphin's automation folder out of shipped APKs for the
  same reason.
- **Fix sketch:** read the three only in a test build (a define set by `TEST_ID=1`, so the device sweeps keep
  working). If release builds must keep some, allow-list the names (`SKIES_PRESS*`, `SKIES_POKE`,
  `DOL_AURORA_*`) and never take paths or `--module`.

### S3. Medium: a crafted Dolphin-format state makes the converter allocate gigabytes
- **Where:** `standalone/src/dolphin_state/dolphin_state_reader.cpp:535-540`; the same shape at `:499`, `:641`,
  `:865`, `:960` and in `vec()` / `count()` (`:242-248`).
- **Claim:** a 32-bit count from the file bounds a loop with no check against the bytes left. Each item costs
  about 300 bytes of heap for 8 bytes of file.
- **Scenario:** a real `.sav` with one count set to 0xFFFFFFFF; a 2 MB file can unpack to the 512 MB limit and
  then needs many GB. On a phone the system kills the game; on a PC it swaps.
- **Verdict: CONFIRMED by the parser helper** (a 92 MB state with one count patched: 5.0 GB in 18.7 s, then
  `bad_alloc` under a 6 GB cap). I read the loop; I did not rerun it.
- **Fix sketch:** require `count * smallest item size <= bytes left` before each loop, and cap the total number
  of fields.

### S4-S10. Low
- **S4. A crafted own-format state ends the game with an uncaught exception.** `standalone/src/state.h:68`:
  `count * sizeof(T)` wraps for a huge count, then `resize` throws, and nothing in `state.cpp` catches it.
  CONFIRMED by the helper's harness. Fix: compare `count > bytes left / sizeof(T)`, and catch in `Load` so the
  backup is restored.
- **S5. A PNG in a texture pack whose header claims a huge picture crashes the game.**
  `.../aurora/lib/gfx/png_io.cpp:85-89` (Aurora fork): the buffer's failed allocation isn't checked, then libpng
  writes through a null pointer. CONFIRMED by the helper's harness. Fix: cap width, height and total bytes
  before allocating.
- **S6. The disc's file table is trusted.** `standalone/src/disc.cpp:151-166`: `count * 12` wraps in 32 bits
  (out-of-bounds read, CONFIRMED by harness), and names aren't checked for `../` (PLAUSIBLE; it reads another
  file into the game, it doesn't write). The file is the player's own extracted disc or a romhack.
- **S7. On Android 12 and earlier any app can close the game.** `SkiesGameActivity.java:136` registers the
  `CLOSE_GAME` receiver without `RECEIVER_NOT_EXPORTED`. The card is flushed, so it only interrupts. PLAUSIBLE.
- **S8. Logs carry the player's folders, so their user name.** About 29 log lines print full paths
  (`hw_memcard.cpp:461`, `state.cpp:222, 247`, `cheats.cpp:219`). The notes now ask players to attach the log.
  Fix sketch: one helper that prints the data folder as `<data>` and the home folder as `~`.
- **S9. CI downloads that aren't pinned by hash.** `tools/ci-dawn.sh:42` takes the stock Dawn tarball by tag,
  and its `dxcompiler.dll` / `dxil.dll` ship in the Windows package. Aurora's own stock-Dawn download
  (`AuroraDawnProvider.cmake:185-192`), SDL, nod and abseil are the same. Also the Linux job installs a PPA
  compiler and pip's cmake after the generated code is decrypted (`validate.yml:572-584`; macOS avoids this
  since review D31). Fix sketch: SHA-256 for each, and move the installs before the decrypt step.
- **S10. Smaller parser limits.** `sav_unpack.cpp:73` zero-fills up to 512 MB before checking the first chunk;
  `aurora_graphics.cpp:1200` multiplies a copy's size in 32 bits; `DCMusicImport.cpp:261-293` re-reads the
  `SOUND` folder once per duplicate record. All crash or stall only.

---

## Bugs

### B1. Medium: the test build's notes never reach the draft release
- **Where:** `.github/workflows/release.yml:275-277`.
- **Claim:** the publish job checks out only three `.dolpatch` files into `src/` and deletes `src/` at `:206`.
  The notes step tests `release/test-build-notes.md` at the workspace root, which never exists, and skips
  silently.
- **Scenario:** the `.5` draft gets the generic text and none of "New in this test build".
- **Verdict: PLAUSIBLE** (the file was added after `.4`, so no run has shown it).
- **Fix: done in `efa2fdf`** (the file joins the sparse checkout and is kept past the removal; a test build
  without it stops). The YAML parses; it has not run in CI.

### B2. Medium: a release can ship without the patched Dawn while its notes promise 2x / 8x
- **Where:** `validate.yml:472, 578-582, 678-680, 824-826` (the fallbacks); `release/test-build-notes.md`.
- **Claim:** every step of the `dawn` job is `continue-on-error`, so a failed build shows green. The platform
  jobs then use the stock Dawn (Off / 4x only). The only sign is a warning annotation.
- **Scenario:** in run 44571866943 the Windows leg's Build step reads "success" although it produced no package.
  At a tag the same thing gives a Windows build without 2x / 8x under notes that list them.
- **Verdict: CONFIRMED** (run log and code).
- **Fix sketch:** each platform job records "Dawn: patched / stock"; at a tag the publish job stops if any is
  stock, or writes the missing platform into the notes.

### B3. Medium: every platform waits for all four Dawn builds
- **Where:** `validate.yml:395, 503, 615, 765` (`needs: [codegen, dawn]`).
- **Claim:** `needs` waits for the whole matrix. A macOS leg queued without a runner holds the Windows, Linux
  and Android packages too, and cancelling the run skips them all.
- **Scenario:** runs 44567700141 and 44571866943: the macOS leg waited 15 minutes, the run was cancelled, and
  every platform job was skipped. Tag runs also can't use the branch's caches, so each release builds all four
  (Linux 18-28 minutes, Windows about 25).
- **Verdict: CONFIRMED** for the dependency, **PLAUSIBLE** for a stalled release.
- **Fix sketch:** each platform job needs only its own Dawn leg (four jobs instead of a matrix, or the Dawn
  build as a cached step inside the platform job).

### B4. Medium: the patched Dawn has never been linked into a CI build
- **Where:** environment `ci` (allows `dev`, `main` and `v*` tags only).
- **Claim:** all three dispatch runs on `aurora-backend` failed every codegen job in 3 seconds on that rule, so
  no platform job has downloaded a Dawn package and built against it. The last full run (`.4`) is from before
  the `dawn` job.
- **Scenario:** the first real test is the `.5` tag. The Linux package is built with clang on Ubuntu 24.04 and
  linked in the 22.04 container with GCC 13; the helper found only the four `__isoc23_strto*` symbols
  `glibc_compat.c` covers, but the link itself is untested.
- **Verdict: CONFIRMED** (run results).
- **Fix sketch:** allow `aurora-backend` in `ci`, or add a job without secrets that links the standalone
  against each package, and run it before tagging.

### B5. Medium: a keyboard key bound to the Invincible hotkey does nothing
- **Where:** `standalone/src/menu.cpp:1860` and `:1888-1889`; `standalone/src/input.cpp:678, 1003-1006`.
- **Claim:** the mapping page captures, saves and shows a keyboard key for both hotkeys. Only the Menu hotkey's
  key is ever read (`InputHotkeyKey(0)` in `MenuKeyWatch`). Invincible is read from gamepads only.
- **Scenario:** a keyboard player binds Invincible to `I`; the row shows `I`; pressing it never toggles. The
  notes say both hotkeys can be rebound.
- **Verdict: PLAUSIBLE** (every reader of the binding traced).
- **Fix sketch:** in `MenuKeyWatch`, latch a press of hotkey 1's key and toggle on it in `MenuFilterPad`.

### B6-B14. Low
- **B6. Hotkeys are reloaded from two threads on Android.** `input.cpp:644-705`, `menu.cpp:1860`: a keyboard
  key-down runs `MenuKeyWatch` on the UI thread, which can run `LoadHotkeys` while the game thread does (after
  each menu open, `:1010`). Rare, but a double free. Desktop is serialised by the input lock. Fix: the watch
  reads an atomic copy of the Menu key and never loads.
- **B7. A state load replaces RAM before the video thread has finished the old frame.** `state.cpp:334`: the
  only sync is in `VideoStateLoaded`, after `DoAll`. At 60 fps the old frame's pending replay then runs against
  the new RAM. No symptom seen; reads only. Fix: `VideoSync([]{})` before the backup in `Load` and
  `LoadDolphin`.
- **B8. A possible deadlock, far from normal play.** `video.cpp:1148-1150` holds the input lock while
  `VideoPost` can wait for the queue (`:224`), and the video thread's present takes the same lock (`:242`). It
  needs over 8 MiB of commands queued behind a present. Fix: post after releasing the lock.
- **B9. A Menu hotkey bound to something that isn't a button leaves no controller button for the menu.**
  `input.cpp:668-672`: a trigger or an axis-named D-pad counts as "bound" but adds no button, so the defaults
  don't apply either. Fix: fall back to the defaults when nothing mapped.
- **B10. Cheats, four small things** (`cheats.cpp`):
  - a code that patches game code with a plain write (very common: `04xxxxxx 60000000`) counts as enabled and
    does nothing, since the code is native (`:107-125`); report it as skipped by its address;
  - Action Replay size 3 is a 32-bit write in Dolphin and is skipped here (`:90-103`);
  - an Action Replay and a Gecko code with the same name: the second is dropped silently (`:195-199`);
  - an 8-bit fill may be 16 million writes a frame (`:93`); cap it or use a block fill.
- **B11. Touch controls** (`SkiesTouchControls.java`): a phone that reports a phantom gamepad device starts with
  them hidden, and Off then hides them for good (`:142-148`, `:165`; the launcher's setting is the way back).
  Presses aren't released when the window loses focus without a cancel event. Neither was run on a device.
- **B12. The clock:** a system clock before 2000 underflows the game clock (`boot.cpp:66`, as before this
  work); around a daylight-saving change the offset can be an hour off for the first hours.
- **B13. Settings are read once.** `settings.cpp` loads the three ini files at start and writes whole files
  back. The comment at `menu.cpp:964` says a menu open picks up hotkeys the launcher rebound; it re-reads the
  copy in memory, and the next save from the game overwrites what the launcher wrote meanwhile.
- **B14. CI odds and ends:** Dawn artifacts are kept one day, so re-running failed jobs the next day builds on
  the stock Dawn silently; the cache key leaves out the runner image and the workflow's own steps;
  `-DSKIES_STOCK_DAWN=1` (`build-standalone-release.bat:241`) is read nowhere; the macOS package's
  `skies-standalone` isn't stripped; `tools/android-ws-build.sh` ends with `pkill -f`, which your own rule
  forbids.

---

## Code quality

- **Q1. Every setting is written four times:** `standalone/src/menu.cpp`, the Dolphin build's `SkiesMenu.cpp`,
  the desktop launcher and the Android panel each carry the label, range and description (this work added
  seven rows to each). One shared table would end the drift; the two C++ menus could share it first.
- **Q2. Settings are searched line by line on every read,** and `MenuApplyGame` and `CheatsApply` read several
  each frame (`settings.cpp:181-193`). Cheap today; a parsed map would also give one place for a lock (B6).
- **Q3. Failures that look like success:** `continue-on-error` on every `dawn` step (B2), and `if [ -f ... ]`
  around the notes (B1). Prefer one explicit "optional" switch and a hard failure at a tag.
- **Q4. The 60 fps draw key mixes fields that overlap** (`interp60.cpp:743`: the texture shifted by 20 bits
  covers the vertex count's bits). Harmless now, because pairs are re-checked by `same_layout` (`:612`), but
  the new pass's `shape` depends on undoing that XOR exactly. A struct key would be clearer.
- **Q5. Comments that no longer match:** `host.h:298` and `menu.cpp:200` name `GameSettings/<ID>.ini` (it is
  `Cheats/<ID>.ini` first); the trailing comment on `InputHotkeysReload` in `host.h:445` belongs to
  `InputKeyBound`; `menu.cpp:964` (B13). Java methods named like classes (`FromController`, `IsController`).

---

## Feature improvements (by value to players)

1. **Cheats:** a list of the file's codes in the menu with a switch each, and the reason when one is skipped.
   Action Replay conditionals and pointer writes are cheap to add (they're evaluated per frame anyway) and
   cover most published codes.
2. **Anti-aliasing on a stock-Dawn build:** say in the row why only Off / 4x are offered.
3. **Touch controls:** keep clear of camera cut-outs and rounded corners (the layout ignores the safe area); a
   dark outline so they show on bright scenes; then per-button placement.
4. **Hotkeys:** save state, load state and the speed toggle rebindable too; warn when a key is already bound.
5. **Logs:** a "copy log for a report" action that redacts paths (S8).
6. **State loads:** check the whole state before touching the running game, so a bad one gives a message
   instead of a restore (S3, S4).
7. **GPU driver safeguard:** for a driver not yet kept, a session that drew frames for 30 seconds and was then
   swiped away reads as a crash; it could ask "keep it?" instead.

---

## Checked and OK

- **Game patch ws35:** `WIDE_PLANE` saves and restores the argument registers and the link register correctly;
  `ws_ship_hud` sits in free low memory and the linker script guards overlaps; `WS_VERSION` was bumped. The
  EU / JP addresses for `njScale` and the hook sites match the three DOLs byte for byte (compared from
  `build/secrets`). The debug hooks (`shipdebug.*`) are in test builds only.
- **Cheat parser:** `InRam` can't overflow for any accepted line; Gecko 00 / 02 / 04 and Action Replay fills
  match Dolphin's counts and steps; pointer-based Gecko types are refused. The Android import caps the file at
  1 MB, keeps a `.bak` and never writes `GameSettings/`.
- **GPU driver loader:** the shim's pointer is set before Dawn can load it; a stale link is replaced each start;
  OpenGL ES skips the driver; `DriverLibName` can only name files in the app's private folder; the APK extracts
  its libraries, which the hooks need. The safeguard matches the Dolphin build's, and the launcher waits for
  the game process to end before judging a marker.
- **Touch state:** one atomic word for the pad and one for the mode, so no torn reads; multi-touch, cancel and
  hide-while-pressed release correctly; the layout doesn't overlap at 4:3, 16:9 or portrait at 75-110%.
- **Menu:** the two rows with built text use static strings; out-of-list values step to the nearest entry; a
  Renderer value from another system falls back to the usual order. F1 can't fire in the menu, during a
  Continue, or when it is the Menu key.
- **60 fps repair pass:** paired draws are re-checked for layout before blending; cost is bounded (64
  candidates per draw).
- **Dreamcast DSP shift:** matches MAME's AICA DSP (SHIFT 2 / 3 wrap to 24 bits; 0 / 1 saturate).
- **Local-time clock:** the offset arithmetic is right, and the `gmtime` result is copied before `localtime`.
- **Threads:** the video queue, state saves, interp60's hand-over, quit and the Dreamcast audio callback are
  correctly locked (the helper's map is in its report; I spot-checked the queue and the fences).
- **Parsers:** `sav_unpack`, the state header, texture-pack archive names (`StaysInside`), DDS, the card folder
  loader and the Dreamcast importer's output names hold up.
- **CI hygiene:** all actions pinned by SHA; least-privilege permissions; no pull-request triggers; the signing
  key only in `android-sign`; the publish job's game-data check; `SkiesGameActivity` isn't exported.

**Not reviewed, or only skimmed:** `recompcore-interp-pacing.patch`'s present pacing (skimmed, no finding), the
Dreamcast uncensor converters, libchdr / LZMA internals, Aurora's own threads, and the Aurora fork patches
beyond the files named above. Nothing was run on the Pocket 5 or on Windows.

## Open questions for the owner

1. **S1's fix:** merge `f51b34a` before `.5`? Is the `.4` draft safe to leave unpublished rather than publish
   as it is? (v0.83 and earlier use Dolphin's card code, which I did not check for the same flaw.)
2. **S2:** do release APKs need any of `env.txt` / `args.txt`, or can all three be test-build only?
3. **B2:** at a tag, should a missing patched Dawn stop the release, or only be written into the notes?
4. **B4:** may `aurora-backend` be added to the `ci` environment, so the full build can run before a tag?
5. **States and the card:** a state loaded in a folder whose card lacks the state's saves shows them in the
   game, but they aren't written to disk unless the game changes them (seen in the S1 run: 7 saves in the
   game, 0 files afterwards). Intended?
6. **The game clock after a state load** stays the state's, as in Dolphin. Card saves made later then carry old
   times, which Continue's "newest save" compares. Worth resyncing?
