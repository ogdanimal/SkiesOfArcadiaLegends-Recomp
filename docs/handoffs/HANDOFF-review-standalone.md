# Handoff: in-depth review of the standalone work (2026-11-18 / 19)

**For:** a Fable session reviewing everything done on branch `aurora-backend` since the v0.83 merge
(2026-11-18) and, as a second tier, the standalone host it builds on. **Asked by the owner (2026-11-19):**
"review for security, bugs, for code that could be improved, or features that could be improved. In-depth."

The deliverable is a findings report, not fixes (see *Output*). Read this whole file before starting.

---

## 1. Ground rules (the owner's standing rules; they override any default)

- **Don't push, tag, publish or post anything.** No `git push`, no tags, no `gh release`, no issue comments, no
  workflow dispatch. Commit only on a local branch of your own (see *Output*). Before any later push the owner
  approves, read the whole output of `~/projects/skies-pii-sweep.sh tree <repo> HEAD`, never just its last
  line, and check that every commit's author and committer are the noreply address
  `306057786+ogdanimal@users.noreply.github.com`.
- **Never put device serials, real names, emails, home paths or hostnames in a commit or a doc.** The history was
  rewritten on 2026-11-18 to remove a device serial; don't reintroduce one.
- **Shared git state:** worktrees share one stash and the same object store. Never use a bare `git stash` or `git stash pop`.
  Commit explicit paths after reading `git diff --cached`.
- **C: drive (Windows):** never delete anything there without explaining it and getting the owner's approval
  **twice**. Creating new folders is fine.
- **Devices:** testing on the Retroid Pocket 5 is allowed without asking, but only through **Windows'
  `adb.exe`** (WinGet's Google.PlatformTools package under the Windows user's `AppData\Local\Microsoft\WinGet\Packages`,
  called from WSL through `/mnt/c/...`), never WSL's adb. Use only the test app `com.skiesofarcadia.recomp.test`. Never touch the release app's data,
  and never revert a setting the owner changed. Restore anything you change: the test app's
  `Config/Dolphin.ini` md5 is `805016d357a3a87c73f1f5bb107920e3`, and `GFX.ini` has no `MSAA` key and an empty
  `DriverLibName`.
- **WSL memory:** never run the game uncapped without a memory cap (`tools/sa-run.sh` caps at 6 GB). Several
  runs at once have taken the WSL VM down. Build with `CMAKE_BUILD_PARALLEL_LEVEL=4` or less.
- **Killing processes:** never `pkill -f <pattern>`, because the pattern also matches your own shell. Use `pgrep -x NAME`.
- **Subagents:** if you fan the review out, start the workers with **model `sonnet`**, never Fable. Give each one a
  narrow area from section 4, and verify every High or Medium yourself in the code before reporting it.
- **Screenshots:** no rolling capture loops. Take single shots only, and delete them after reading.
- **Real fixes, not fallbacks:** when you propose a fix, don't trade the goal away (for example, 60 fps down to
  30 fps). Find the root cause.

## 2. What the project is (enough to review it)

A static recompilation of *Skies of Arcadia Legends* (GameCube) to native code. There are two runtimes:

- **The Dolphin build** (v0.83.0-beta, the current release): the recompiled game hosted inside a trimmed Dolphin
  (`lib/ModernGekko/vendor/dolphin`, "RecompCore").
- **The new engine, called "standalone" or "emulator-free"** (`standalone/`): the same recompiled game modules
  on GXRuntime (ModernGekko's runtime: CPU state, guest memory, devices, HLE of the SDK). It draws through
  **Aurora**, a forked GX-to-WebGPU layer on **Dawn**. Audio is Dolphin's AX HLE, ported. It ships only as
  opt-in **test prereleases** (`v0.90.0-standalone.N`, drafts made by `release.yml` on this branch).
  `v0.90.0-standalone.4` is an unpublished draft. `.5` is prepared but not tagged, and that decision waits on
  the owner.

Where to read up first: `docs/handoffs/HANDOFF-emulator-free.md` (its status paragraphs and *Parity
(2026-11-19)*), `docs/handoffs/HANDOFF-feature-gaps.md` (items 0-10), `docs/handoffs/HANDOFF-release.md` (first
section), `docs/backlog.md` (top items), `patches/README.md`.

**Trust model.** Nothing is network-facing at run time. The untrusted inputs are:
- files the player downloads or imports: disc images, card saves, save states, HD texture packs, cheat files
  (`Cheats/<ID>.ini`), GPU driver zips (Turnip), romhack DOLs, the Dreamcast disc;
- for CI, the supply chain: actions, the Dawn sources and DEPS, the stock Dawn tarball, encounter's releases.

A player can also edit their own config files (`Dolphin.ini`, `GFX.ini`, `env.txt` on Android). That can hurt
only themselves, but it must not crash the game or write outside the data folder. The earlier reviews (below)
found no remote-exploitable issue. Hold the new code to the same bar.

## 3. Where the code is and how it's built

- **Worktrees:**
  - `~/projects/skies-standalone-issues`, branch `standalone-issues`: the work happens here;
  - `~/projects/skies-aurora-backend`, branch `aurora-backend`: fast-forwarded from it and pushed.

  They are the same commits, all pushed to `origin/aurora-backend`. Review in a
  **new worktree** of your own: `git worktree add ~/projects/skies-review standalone-issues`, then
  `git checkout -b review-standalone`.
- **Patches, not submodule commits:** our changes to the submodules (`lib/ModernGekko`, its
  `vendor/dolphin`, the nested `DolRecomp`, `lib/HighlyTheoretical`) live as files in `patches/`, applied by
  `tools/apply-patches.sh` (series: `patches/recompcore.series` and the lists inside the script).
  `tools/check-patches.sh` proves that the patches reproduce the working copies. **Review the patch files**:
  they are what ships. In a fresh worktree, run `git submodule update --init --recursive` (or copy the
  submodules from the existing worktree), then `tools/apply-patches.sh`.
- **The Aurora fork:** `lib/ModernGekko/vendor/dolphin/GXRuntime/graphics/aurora` plus the fork patches
  `patches/moderngekko-aurora-*.patch` (up to 0054 and beyond). Tier C only.
- **Builds:**
  - Linux standalone: `CMAKE_BUILD_PARALLEL_LEVEL=4 tools/build-standalone-release.sh build/sa-review`. Add
    `DAWN_PACKAGE=<tar.gz>` for the patched Dawn; CI's packages are in `build/ci-dawn-44567700141/{linux,android}/`
    in the issues worktree.
  - Development build: `build/standalone/skies-standalone` (cmake `-S standalone -B build/standalone`).
  - Windows (MSVC): `rsync -a standalone/ /mnt/c/dev/SkiesStandalone/standalone/`, then
    `cmd.exe /c "C:\dev\SkiesStandalone\build.bat build"`. Before any Windows test launch, call
    `C:\dev\sync-controls.bat <USER_DIR>` (the owner's rule). The module is
    `C:\dev\SkiesStandalone\build\modules-ws35\gGEAE8P_recomp.dll`, the game is `C:\dev\SkiesStandalone\games\GEAE8P-ws35`.
  - Android test APK (US module only):
    `PREGEN=build/ws35/codegen-android/android NAME=ws35t STANDALONE=1 BRAND=1 TEST_ID=1 DOL_EU=/nonexistent DOL_JP=/nonexistent tools/android-ws-build.sh`.
    Add `DAWN_PACKAGE=` to use a patched Dawn. Copy the APK to `C:\Users\Public\` and run `adb.exe install -r -t`.
- **Test tools:**
  - `tools/sa-run.sh OUT START REGION SECS`: one capped, deterministic run with frame dumps and an error count;
  - `tools/sa-sweep.sh`: many starts, each run twice and compared;
  - `tools/sa-device-sweep.sh`: the same on the Pocket 5.
  - Save states for starts: `build/sweep-in/` (pulled from the devices, read-only) and the repo's
    `savestates/`.
  - Hooks: `--seconds +N`, `SKIES_PRESS "+T:A"`, `SKIES_POKE "@+T:ADDR=W"`, `SKIES_RTC`,
    `DOL_AURORA_BACKEND`, `DOL_AURORA_VULKAN_CPU=1`, and on Android `env.txt` / `args.txt` /
    `SKIES_EXTRA_ARGS` in the app's external files folder.
  - Device input: `sendevent` on `/dev/input/event8` (A = key 304, B = 305, D-pad = ABS 16 / 17 ±1, GC R =
    ABS 9 at 32767). `input keyevent` doesn't reach SDL, except `KEYCODE_BACK`, which opens the in-game menu.
- **Earlier reviews, so you don't repeat them:** `docs/handoffs/HANDOFF-review.md` (sections A-D, 2026-10-12
  and 10-22; its *Checked and OK* lists). The emulator-free branch's own review is in
  `HANDOFF-feature-gaps.md`, *Code review of the branch* (2026-10-27, nine findings, all fixed).

## 4. Scope, in priority order

### Tier A: the 2026-11-18 / 19 work (`git log 114fce1..db49447`, 66 commits; diff ~3,400 lines of code)

Review every hunk: `git diff 114fce1..db49447 -- . ':!docs' ':!*.md'`. Grouped by feature:

| Area | Commits | Files |
|---|---|---|
| Issues port + game patch ws35 (#25 ship battle HUD, #22 screen plane, #19 DC gain + F1, #18 60 fps pairing, #24) | `a2ca80c`..`aa064e4`, `76bc51c`..`866d9aa` (cherry-picks dated 10-28) | `patches/game/{widescreen.c,fullwidth.S,shipdebug.*,cave.ld,*.addresses.json}`, `release/*.dolpatch`, `patches/recompcore-{ship-hud,interp-pacing,dcmusic-level,f1-state}.patch`, `patches/moderngekko-ship-hud.patch`, `standalone/src/{interp60.cpp,menu.cpp,dreamcast/AudioCommon/DCMusic.cpp}` |
| Android touch controls + setting + style | `e150580`, `83755ce`, `4ed6b5f`, `cdb6b4e` | `standalone/android/app/java/org/dolphinemu/dolphinemu/skies/SkiesTouchControls.java` (new, 428 lines), `SkiesGameActivity.java`, `video.cpp` (`TouchPadSet`, `TouchModeSet`), `main.cpp` (JNI), `patches/recompcore-touch-{setting,style}.patch` |
| Dead zone / rumble from the profile | `b2bd527` | `menu.cpp` (`ReadDeadZone`, `ReadRumble`), `input.cpp` |
| Local-time game clock (+ the #28 tz fallback in the Dolphin build, `1af2044`) | `24c357d`, `7bd83bd` | `standalone/src/boot.cpp`, `patches/recompcore-tz-fallback.patch` |
| F1 saves a state | `37cbc2e` | `menu.cpp` (`s_f1_request`), `input.cpp` (`InputKeyBound`) |
| Adapter log + dual-GPU exports (#32) | `84519f1` | `main.cpp:57` (`NvOptimusEnablement`, `AmdPowerXpressRequestHighPerformance`), Aurora `info_logging` |
| Test tools | `d18b2ef`, `355a8d2` | `tools/sa-*.sh`, `video.cpp` (pokes `|=`, `@+T`, presses `+T`), `main.cpp` (`SKIES_EXTRA_ARGS`, relative `--seconds`), `state.cpp` (`g_run_start_seconds`) |
| Dreamcast DSP SHIFT wrap (#26 candidate) | `9a160c5`, `a96d0a0` | `patches/highlytheoretical-dsp-shift-wrap.patch`, `tools/apply-patches.sh`, `tools/check-patches.sh` |
| Cheat codes (#5) | `8f3a78e`, `5964079`, `5dd7c22` | `standalone/src/cheats.cpp` (new), `menu.cpp`, `patches/recompcore-cheats-file.patch` (Android picker import), `patches/moderngekko-cheats.patch` |
| Renderer choice (desktop) | `8c1949c` | `main.cpp:955-972`, `menu.cpp` (`RENDERERS`, `VideoRendererName`), `patches/moderngekko-renderer.patch` |
| Rebindable Menu / Invincible hotkeys | `57c3555` | `input.cpp` (`HotkeyBinding`, `LoadHotkeys`, `WriteHotkey`, `CaptureHotkey`, `InputHotkeyHeld`), `menu.cpp` (`MenuKeyWatch`, `ReadHotkeys`) |
| Custom GPU drivers on Android (Turnip) | `0b9ff88` | `main.cpp:431-481` (`LoadCustomGpuDriver`), `standalone/src/android_vkshim.c`, `standalone/CMakeLists.txt` (libadrenotools, `skiesvk`), `SkiesGameActivity.java` (driver markers), `patches/recompcore-{aurora-vulkan-dir,standalone-driver}.patch`, `tools/android-standalone-build.sh` |
| Patched Dawn in CI (MSAA 2x / 8x) | `0872492`, `61aed3d`, `2a50e0c` | `.github/workflows/validate.yml` (`dawn` job + downloads in windows/linux/macos/android), `tools/ci-dawn.sh`, `tools/build-dawn-package.{sh,bat}`, `tools/build-standalone-release.{sh,bat}` |
| Release notes / workflow | `5f23374`, `114fce1`, others | `.github/workflows/release.yml`, `release/test-build-notes.md` |

### Tier B: the 2026-11-18 packaging work (`git log a0211c8..114fce1`)

Items 9-10: the branded Android app on the standalone, release packaging on four platforms, CI fixes:
`patches/recompcore-standalone-app.patch` (359 lines), `SkiesGameActivity.java` (first version),
`standalone/src/glibc_compat.c`, `tools/build-standalone-release.*`, `tools/build-release-*.{sh,bat}`, the
`release.yml` / `validate.yml` changes, `standalone/CMakeLists.txt`.

### Tier C, if time allows: the standalone host as a whole

`standalone/src/*` (host, boot, disc, card, state + converter, DSP HLE, input, menu, video, interp60, HD
textures, dreamcast) and the Aurora fork patches. This is big. Prioritise:
- the parsers of player files: state converter `dolphin_state/`, `sav_unpack.cpp`, HD texture pack loading,
  card / GCI, disc reading;
- thread safety between the game thread, the video thread (fork 0046) and the Android UI thread.

## 5. What to look for, per area (leads to check, not conclusions)

These are places the author would look at first. Each needs confirming or rejecting in the code. Don't
report one as a finding without a concrete scenario.

**Security / robustness of player-supplied files**
- `cheats.cpp`:
  - parsing (`ParseLine` assumes 8+1+8 characters; trailing text after the 17th character is ignored, so is
    that right?);
  - fill counts (an AR 8-bit fill can be `(data >> 8) + 1` = 16 M writes per frame: a slow frame or worse);
  - `InRam`'s arithmetic;
  - Gecko and Action Replay semantics against Dolphin's `ActionReplay.cpp` / `GeckoCode.cpp` (sizes, counts,
    the 0x81 half, and whether fills step correctly);
  - codes that write over the game's code area or the widescreen patch's cave (`patches/game/cave.ld`): what
    happens?
- `LoadCustomGpuDriver` (`main.cpp:436`):
  - `DriverLibName` comes from `GFX.ini`, so check path handling and whether a `../` name can reach outside
    `GPUDrivers/Extracted`;
  - the `symlink` in `GPUDrivers/Shim` (a stale link, or a file already there);
  - `setenv` after threads have started;
  - what happens when `adrenotools_open_libvulkan` succeeds but the driver is for another GPU;
  - the `env` precedence with `DOL_AURORA_BACKEND=gles`.
- `android_vkshim.c`: a global function pointer set once. Check ordering and visibility, and what Dawn does if
  the shim is loaded before `skies_vkshim_set`.
- `SkiesGameActivity.java` driver safeguard markers (`skies-driver-boot-pending` / `-kept` / `-session-done`):
  - can a normal quit, a Home press, a process kill by the OS or a crash *after* boot wrongly drop a working
    driver, or keep a broken one?
  - races between the `:game` process and the launcher.
- The Android cheat import (`recompcore-cheats-file.patch`): content-URI handling, size limit, target path,
  overwriting an existing file, and that it never writes `GameSettings/` (an earlier version overwrote
  Dolphin's per-game settings there).
- Test hooks in release builds: `env.txt`, `args.txt`, `SKIES_EXTRA_ARGS`, `SKIES_POKE`, `SKIES_PRESS`,
  `SKIES_PRESS_FILE`. Are they active in a release APK or desktop build? Who can write the app's external
  files folder on Android 9-15? Do they need gating, or is the risk acceptable (and documented)?
- Logs that players paste into GitHub issues: do new log lines print full paths (home directory, user name)?
  For example `[cheats] %s:` in `cheats.cpp:219`, and the `[card] slot A:` line. Is it worth redacting the
  data folder to `<data>/` in the log?

**Correctness / bugs**
- `input.cpp` hotkeys:
  - capture while the menu is open;
  - a hotkey bound to a key the game also uses;
  - controller vs keyboard;
  - `InputHotkeysReload` against a profile edited by the launcher;
  - default values when `[SkiesHotkeys]` is missing.
- The F1 latch (`s_f1_request`): consumed every frame, so check bound vs unbound F1, a press while the menu is
  open, and a press during a state load.
- `menu.cpp`:
  - the `Row` description pointers (one lifetime bug was already fixed with function-static strings, so look
    for others);
  - new rows' ranges and the values written to `Dolphin.ini` / `GFX.ini` / `GCPadNew.ini`;
  - the Renderer row on each platform (`RENDERERS` {0,1,2,3} on Windows, {0,2,4} elsewhere) against `main.cpp`'s
    mapping;
  - what an out-of-list value already in the ini does.
- `video.cpp` touch pad state (`s_touch_pad` atomic, `TouchModeSet` packing mode | opacity<<8 | size<<16):
  tearing between fields, and the UI thread vs the game thread.
- `SkiesTouchControls.java`:
  - multi-touch (pointer id reuse, ACTION_CANCEL, a finger sliding from the stick onto a button);
  - the layout at sizes 75-110% on 4:3 / 16:9 / 20:9 / tablets / portrait;
  - the opacity on a bright scene;
  - whether a controller's input really hides the controls and a touch brings them back.
- `interp60.cpp`, the #18 repair pass (pairing draws across a texture change): can it pair the wrong draws
  (two models with the same texture change), and what does it cost at 60 fps?
- `boot.cpp` local-time RTC: DST transitions, time zones with no IANA name (the #28 bug class), a clock before
  2000 or after 2038, and the save-state round trip of the clock.
- `highlytheoretical-dsp-shift-wrap.patch`: check it against the AICA DSP's documented behaviour (SHIFT 2 / 3
  saturation vs wrap). Is it right for SHIFT 0 / 1 too?
- `patches/game/widescreen.c` / `fullwidth.S` (ws35): the #22 screen-plane change and the #25 HUD layout.
  EU / JP addresses in `*.addresses.json` (0x80002EEC, `njScale` EU 0x80294BCC, JP 0x80291E90). Check them
  against the disassembly in `~/projects/GameCube-Research` if you can.
- CI (`validate.yml`):
  - the platform jobs now `needs: [codegen, dawn]` with `if: ${{ !cancelled() && needs.codegen.result == 'success' }}`.
    Is that right when the dawn matrix is cancelled (a macOS runner never acquired, twice so far) or skipped?
    Does it keep `release.yml`'s `workflow_call` path working?
  - the cache key (`hashFiles` over the patches, scripts and Aurora's CMakeLists). Does it miss anything that
    changes the output (runner image, compiler version, NDK version)?
  - `continue-on-error` on every step: the job always reports success. The new *Report a missing package*
    step warns. Is that enough, or should a release refuse to publish without the patched Dawn?
  - `tools/ci-dawn.sh` (Windows): it downloads the stock `dawn-windows-amd64.tar.gz` from encounter's release
    **without a checksum**, and its `dxcompiler.dll` / `dxil.dll` ship in our Windows release. Should it pin a
    SHA-256?
  - `fetch_dawn_dependencies.py` pulls DEPS at pinned commits. Is that trustworthy enough?
  - `git am` of `patches/dawn/` onto a clone at `AURORA_DAWN_REF`, a full SHA.
  - the third run, 44577309166, built and uploaded all four packages (Windows checked locally: MSAA 1x-8x on
    D3D12). A job's "success" alone still means nothing: look at the Upload step or the run's artifacts.
- `release.yml`: the test-build notes. Do they match what ships now (MSAA, touch, Turnip, renderer)?

**Code that could be better**
- Duplication between `standalone/src/menu.cpp` and the Dolphin build's `SkiesMenu.cpp`: rows, ranges and
  descriptions written twice. Could a shared table serve both?
- Settings access: `SettingGet` / `SettingGetString` and the ini writers. Check consistency (base-0 parsing,
  quoting), and whether the standalone and the launchers agree on every key's type and range.
- Error handling style: functions that log and continue where they should fail, or the reverse.
- Scripts in `tools/`: `set -euo pipefail` discipline, quoting, and the CRLF `.bat` files.
- Comments that describe history rather than the code (the project style is descriptive comments; flag
  misleading ones).

**Features that could be better** (a separate section of the report, ranked by value to players)
- Cheats: a per-code list in the menu, encrypted Action Replay codes, conditionals (the Dolphin build supports
  all of Dolphin's cheat engine).
- Touch controls: a layout editor, per-button size, haptics, hiding while a cutscene plays.
- Hotkeys: only two are rebindable, so look at what else players ask for (save / load state, speed toggles).
- MSAA fallback: when a release falls back to the stock Dawn, the player sees only Off / 4x with no
  explanation.
- The GPU driver safeguard's message to the player; the Renderer row on Android.
- Anything from the parity list in `HANDOFF-emulator-free.md` *Parity (2026-11-19)* that you'd argue for.

## 6. Verification expectations

- Every High or Medium needs a **concrete failure scenario** (inputs or state leading to a wrong result, crash or
  security effect) and a verdict: **CONFIRMED** (reproduced: a test run, a unit harness, a crafted file) or
  **PLAUSIBLE** (traced in the code, not run).
- Reproduce where it's cheap:
  - a crafted `Cheats/GEAE8P.ini` or `GFX.ini` with `tools/sa-run.sh`;
  - a small C++ harness for a parser;
  - a device run for touch or driver logic.
- Device runs are noisy, so never claim a device result from one run.
- Don't re-report anything in `HANDOFF-review.md` sections A-D or the 2026-10-27 branch review unless it
  regressed.
- Line numbers: from your worktree at `db49447`, as `path:line`.

## 7. Output

1. **The report:** `docs/handoffs/REVIEW-standalone-2026-11.md` in your worktree, with:
   - a summary (overall verdict, counts by severity);
   - **Security**, **Bugs**, **Code quality**, **Feature improvements**: each item with an id (S1, B1, Q1,
     F1…), severity (High / Medium / Low), `path:line`, a one-line claim, the failure scenario, the verdict,
     and a suggested fix (a sketch, not a patch);
   - **Checked and OK**: what you looked at and found sound, so the next review can skip it;
   - **Open questions for the owner**.
2. Commit the report on your local branch `review-standalone` (explicit paths, noreply author). Don't push.
3. **Don't fix anything yet.** The owner picks what gets fixed. If a fix is trivial and you're sure of it, you
   may put it in a separate local commit, one per finding, named in the report. Leave it unpushed and unmerged.
4. Tell the owner in a few lines what you found, the worst first, and where the report is.

## 8. State at handoff (2026-11-19, ~12:40 EST)

**Done (2026-11-19 afternoon):** the review ran; its report is `REVIEW-standalone-2026-11.md` and its *Fix status*
lists what was fixed after it (pushed, `ceecbed`). A next review starts from that report, not from this file.

- Everything is pushed: `origin/aurora-backend` = local `standalone-issues` (docs only after `2a50e0c`, the
  last code change).
- CI run 44577309166 built all four Dawn packages (after the Windows fixes). Runs 44567700141 and 44571866943 built
  the Linux and Android Dawn packages; macOS never got a GitHub arm64 runner in either.
- The Pocket 5's test app holds the ws35t APK built on CI's Android Dawn package. Its settings are restored.
- Known open items that are **not** review findings:
  - #22 not seen in game (forcing the copy bit 0x8030A178 crashes; it needs Drachma's pose by hand);
  - #26 needs the reporter's state;
  - #24 needs the owner's VRR display;
  - two controllers and remapping by hand;
  - EU / JP not run on Android with ws35.
- Draft issue replies (unposted): `build/issue-replies-standalone5.md` in the issues worktree.
