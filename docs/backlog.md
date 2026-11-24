# Backlog

Requests and ideas not scheduled yet. Move an item to a handoff when work on it starts.

## Drop the Dolphin engine — decided 2026-11-19

The owner: supporting both engines isn't feasible; the port moves to the new engine alone. Plan, gates (a
play-through, testers' reports, re-measured performance, Android 7 / 8) and the inventory of what goes and what
stays: [handoffs/HANDOFF-dolphin-removal.md](handoffs/HANDOFF-dolphin-removal.md). No more Dolphin-engine
releases (the owner, 2026-11-19: not even the card-path security fix, which stays on the local branch
`gci-state-fix`; the new engine is the remedy). Until
the cut-over: no new Dolphin-engine-only features; items below marked "both builds" need only the new engine
once it ships.


## Next build, local branch `next094` — 2026-11-24 (not pushed; push, replies and release on the owner's word)

In `~/projects/skies-interp` (branch `next094`, from v0.93's `8beb8c8`): `i24-loads`, `i26` (`00748e4`), `ws42`
(`f34d82d`) merged, then the 60 fps work up to `deb01e3`. Details: development notes, "60 fps after v0.93",
"Issue #36" (After v0.93) and "Issues #26-#32" (#26).
- **#26 (Dreamcast music pinned):** found from the reporter's log: the AICA channel filter's state truncated each
  sample; `highlytheoretical-lpf-precision.patch`. Checked offline (75 songs) and in game on WSL and the Pocket 5
  (music plays); the failing scenes weren't played in the game.
- **#24 (60 fps):** the reporter's first-person reef fixed (WSL, Windows, Pocket 5); the strict test and its
  fixes (colours, matrices, the camera's motion). Panning warp not reproduced: the reply asks for their 60 fps log.
  Open: the Red Alarm shatter's strict count (pictures look right).
- **#36 (ship HP bar with Invincible):** ws42, checked on WSL, Windows and the Pocket 5.
- **Cost:** the Pocket 5's save point at 60 fps blends in about 6.2 ms an in-between (v0.93 5.2).
- **Replies:** drafted in `build/i37/replies-094.md` (#24 and #26 now, #36 after the release).
- **Test app:** the t94 build (`skies-recomp-t94-2026-11-24-test.apk`) on both Retroids and the Thor; Windows test
  folder `C:\dev\SkiesI37` (`run-fp-*.bat`, `run36-*.bat`, `games\GEAE8P-ws42`, `modules-ws42`).

## After the next release — decided 2026-11-22

The owner: the follow-ups from the 2026-11-22 comments wait until after the release: #25 (the bottom-right HUD
element clips a little on a miss), #22 (a slight stretch on Drachma's first arm slam), #23 (the turret shadows
make two textures in `field/a116c.mld` see-through, hashes `2d1b88e460bcc6bd`, `5c19e6aa4e74d492`), #34 (the black
bars in the second intro). Check what the release session's `notes-091` work covers first. The #36 / #37 replies
go out after the release.

## Issue #36: ship sunk with Invincible on, retry at 0 HP — 2026-11-22 (on dev `3813448`, pushed 2026-11-22 evening; not released)

- **Fixes:** ws40 (the cause: the playback's HP gauge sank the ship) and ws39 (a ship at 0 refilled before a phase);
  development notes, "Issue #36".
- **Merged (2026-11-22 evening, the owner: all of `interp60-warp` into the next build):** `notes-091`'s ws37 / ws38,
  GC depth and #23 committed and merged (`063ad78`); `release/*.dolpatch` are ws36-ws40 (`2b51fa7d`, `5ec79e79`,
  `66521522`). The MetroTRK patch area is full now. Checked: WSL (ws38 vs merged identical with the cheat off, the
  cheat on keeps the ship), Windows, Pocket 5, Thor (#37), Europe / Japan boots. Pushed to dev at the owner's word
  (`090d2b5..3813448`, dev only: `aurora-backend` is no longer pushed).
- **Reply:** drafted (`build/i37/replies-36-37.md`, local), to post after the release on the owner's word.
- (old note) fixed for the next build: with Infinite HP on, the bar no longer counts hits, the ship can't
  sink, and a ship left at 0 by an earlier build is refilled.

## Issue #37: menu text at 60 fps and the menu's blocky clouds — 2026-11-22 (on dev `3813448`, pushed 2026-11-22 evening; not released)

- **Commits:** `6bc2b0f` replays draw with their frame's textures (patch `recompcore-gxruntime-replay-texture-frames`
  + `interp60.cpp`); `f048d44` the padded copy's base stretched bilinearly (patch
  `recompcore-gxruntime-padded-base-bilinear`). Also `6f5bd51`, the release session's fix for `SKIES_INTERP_TASKS`
  being ignored on Android (read before env.txt). Details: development notes, "Issue #37".
- **Checked:** WSL, Windows (RTX, 240 Hz), Pocket 5 and Thor for both; the text by counter
  (`AURORA_REPLAY_TEXHASH_CHECK=1`), not by a garbled picture (none was caught in the old build either). Not:
  Japan/Europe, the reporter's 4-member menu. Test APK on both devices' .test app: `skies-recomp-i40b` (this branch). Windows test folder `C:\dev\SkiesI37` (its own copy of the sources; `shot37.ps1`); owner decides on cleanup.
- **Reply:** drafted with #36's (`build/i37/replies-36-37.md`), to post after the release. (old note) both fixed for the next build; the field-speed trigger for the clouds wasn't reproduced
  separately (the start menu one was).

## 60 fps: object pairing and issue #24 — 2026-11-22 (on dev `3813448`, pushed 2026-11-22 evening; not released)

- **Commits** (worktree `~/projects/skies-interp`, noreply author): `547ada5` the #18 / #24 sprite fix and the FPS
  counter (also applied, uncommitted, to `notes-091` by the release session); `b8772b2` each draw knows its game
  object (the running task, read off the guest stack; no game patch) and two objects never pair; `ab10b24` the
  same-object rule only for small objects (the map is one task); `7c6654f` notes. Details and measurements:
  development notes, "60 fps: draws know their game object" and "60 fps: sprites drawn a frame ahead".
- **Checked:** WSL (seven scenes, in-betweens compared); Windows RTX at 240 Hz and 144 forced (Display refresh at
  full speed, the FPS counter equal to RTSS); Europe (objects found). Not: Android (asked of the release session
  on a Retroid), Japan (addresses only), a real 144 Hz screen.
- **#24:** the reporter's place is Sailors' Island's lighthouse bridge (area 901) with the rock clouds behind.
  Route: the reporter's state, `SKIES_WARP="scene:me004a"`, Vyse poked to x -140 y 26.3 z 111
  (`build/w24/bridge.sh`, `winbridge.sh`, `winmode2.sh`, `timeline.py` in the worktree). No warp there on WSL or
  Windows in any version (v0.91's pairing included); the Display refresh timeline is even. Open: the reporter's
  orange sky (another story point or time of day), Android, asking the reporter for a save at that spot.
- **Next, if wanted:** blend per object from the object's matrix (decomp `docs/draw.md`, "Hook points") instead of
  vertices, which would need a game patch for the draw queue's object (`DrawObj`).

## Decompilation research and the story warp — 2026-11-22

The decomp's branch `research-1122` (`~/projects/soal-decomp-research`, not pushed; the decomp is public, so a
personal-details sweep first) has `docs/draw.md`, `script.md`, `battle.md`, `save.md`, `cutscene_camera.md` with
headers and 321 names, part of it checked in the game. Uses here: per-object 60 fps (above); achievements (each
doc's "Addresses for achievements"; `~/projects/skies-ra/docs/handoffs/HANDOFF-retroachievements.md`); a
complete cutscene shot list for automatic widescreen framing (`cutscene_camera.md`, option A); battle and save
checks still open (ship battles, spells, Discoveries). The story warp for bug reports, `SKIES_WARP`
(`chapter:B..L`, `scene:meNNNx@KEY`, `stage:`, `flag:`, `byte:`), is on the release session's local branch `warp`
(`5901cd3`, `~/projects/skies-warp`); its notes are in that branch's development notes, "Story warp for bug
reports".

## Simplify CI — noted 2026-11-19 (Dolphin runner build dropped 2026-11-20: Windows job 23 -> 18 min)

The owner asked why CI is so complicated. **Fixed costs** (they stay): no game data may ship or leak (the retail
DOLs in the private secrets repo, codegen in the `ci` environment, encrypted artifacts between jobs, the publish
job's game-data scan); 4 platforms x 3 discs (12 codegen jobs, a module build per platform on four toolchains);
the security hardening (SHA-pinned actions, signing only in `android-sign` behind the owner's approval, builds that
fetch third-party sources kept out of jobs with secrets or decrypted code, release mode failing instead of
shipping degraded). **Transitional or self-made costs**: two engines per release (the Dolphin runner still built
and then overwritten; no longer built from 2026-11-20, `patches/moderngekko-no-dolphin-runner.patch`); ~60 patch files on submodules instead of forks (re-applied and checked in every job,
regenerated by hand); Dawn built from source per platform per tag (tags can't use branch caches); GitHub's limits
(missing macOS runners; environments per branch, hence `standalone-host.yml` runnable without secrets).
Simplifications, biggest first:
- Drop the Dolphin engine (`HANDOFF-dolphin-removal.md`); its phase 4 (launchers without Dolphin) removes most of
  the Dolphin build from CI.
- Real forks instead of patch files for the heavily patched submodules (RecompCore first); the cost is keeping
  the forks merged with upstream.
- Build the patched Dawn once per Dawn version and publish the packages as release assets that CI downloads
  (small and independent: the suggested first step).

## After v0.91 — 2026-11-22 (local, uncommitted in the issues worktree)

- **#34 throne-room flicker:** GameCube-style 22-bit fragment depth (`recompcore-gxruntime-gc-depth.patch`),
  Thor-checked; replaces the draw-order bias plan; **on by default on Android only** (Metal and WSL show moving
  dots on the right-hand wall with it, and keep one copy steadily without it). Watch GPU cost on weak devices (Thor
  EFB pass +35-50 % in heavy scenes; Pocket 5 still 1.00x).
- **Cutscene bars setting** (game patch ws37): hides the attract intro's letterbox bars; WSL, Thor, Pocket 5,
  Windows and macOS checked. Needs a release with the new dolpatches (every install repatches).
- **#25 banners clipped** (game patch ws38): both sides 12 HUD px in from the edges; WSL, Windows, Pocket 5.
- **#18 / #24 60 fps warping** (`interp60.cpp`, other session's `547ada5`, applied here): flipbook and depth-sorted
  sprites paired right; FPS counter counts presented pictures. Not seen on a real 144 Hz VRR screen.
- **#23 turret shadows** (`disc.cpp`, the decal palettes hard-edged in the served `a243ac01.mld`): WSL, Windows, Thor.
- **#22 first arm slam "expanding":** the pose forced and measured (Drachma's motion 27): no width step at 16:9, the
  1-2 % is the game's own shake zoom (same at 4:3). The one black 60 fps frame in the reporter's video is not
  reproduced on WSL or Windows D3D12 (same copy frames 59805-59812; every real and in-between picture 41-43 brightness).
- **#34 "missing pillars":** not reproduced; ask the reporter where.
- **RetroAchievements:** researched by another session (`~/projects/skies-ra`, branch `ra`,
  `docs/handoffs/HANDOFF-retroachievements.md`): a set exists for the European disc only; ask RA first.

## v0.91.0-beta issue round — 2026-11-21 (RELEASED in v0.91.0-beta, 2026-11-22, tag on `21e6c02`)

- **Done, in v0.91:** #35 blocky see-through textures (`823db67`), #25 Ship battle HUD "Bar + grid centred" (`88f931e`),
  game patch ws36 (`d9b5384`): #21 Sailors' Island bridge scenery, #25 attack names at the edges, #22 screen plane at
  the picture's width. Checked on WSL (all discs), Windows, the Pocket 5 and the Mac (development notes, *Issues
  #21-#35 round*). #22 not yet seen in Drachma's pose itself.
- **#23:** the original art (drop-shadow decals); no fix, answered 2026-11-22.
- **#34 throne-room flicker on Adreno (owner: not in v0.91):** the draw-order depth bias
  (`build/fable-34/aurora-order-bias.diff` in the issues worktree, `AURORA_ORDER_BIAS`) fixes it on the Thor at 6e-5.
  Next: a sweep of many states with and without it (every difference looked at), then probably on by default on
  Android only. Reporter answered 2026-11-21.
- **Replies posted** 2026-11-22 after publishing: #35, #25, #21, #22, #23. #16 (damage digits on M4) confirmed
  fixed by the reporter and closed.
- **#29 HD fonts (open):** shoopkid's font textures (78 glyphs, one character per Dolphin `tex1_24x24_` texture,
  8x) work through the HD texture import as they are (checked with the v0.91 Linux build: dialogue and battle
  names go sharp, the number font doesn't change); BC7 DDS won't load on most phones (use the PNG set), and the
  "?" file in both zips carries a stray `(1)` in its name. Never bundle them (game-derived art, no licence).
  Possible next step: a font-only import / separate switch so fonts don't need a full pack. Results posted.

## Uncapped frame rate — BUILT 2026-11-20 (#20), on `origin/aurora-backend` (`bab367e`, Android `ec84654`)

**2026-11-22:** its FPS counter read about 30 too high (a frame plus its in-betweens counted, most of which a
fast display never shows); it now counts presented pictures (`547ada5`). Windows at 240 Hz: 240, and 144 with
`SKIES_DISPLAY_HZ=144`, equal to RTSS.

Frame rate **Display refresh** (`[Skies] FrameMode` 2, `SKIES_INTERP=2`): `video.cpp` *FrameMode 2* (the timeline) and
`interp60.cpp` (`InterpDisplayReplay`, the blend plan cached per pair). Windows RTX 5080 at 239 Hz: field / world map
240 pictures a second (p99 6.2-6.7 ms), the Antonio battle 120 (p99 10.1), full speed, V-Sync on or off. Left: a
VRR screen by eye, the Thor again at 120 after the exact-mode-rate change (the AYN Thor at 120 Hz: ~120 pictures/s
in the field and battle, full speed, `ec84654`), the Pocket 5 (60 Hz: no gain over FrameMode 1, a field later).
The notes below are the plan as it was.


The owner: planned, "like Dusklight" (the #20 reply says so). The game's logic stays at 30 Hz; the picture runs at
the display's rate (120 / 144 Hz, VRR), each presented picture blended between the last two game frames.
- **Dusklight's way** (`TwilitRealm/dusklight`, `src/dusk/game_clock.cpp`, `src/dusk/interp/`): a 30 Hz
  simulation tick (`kSimPeriod`), presentation separate; each presented frame interpolates at step =
  (now - the newest tick's time) / (1/30 s), clamped 0..1, one tick behind. It records matrices per object (it
  has the source); ours blends the recorded GX stream (`interp60.cpp`), which already takes any t
  (`SKIES_INTERP_T`).
- **To build:** in-betweens at any t (today one per frame at 0.5, replayed into a kept slot); presents timed to
  the display's refreshes (a 120 Hz grid on desktop first, then a per-present t from wall time for VRR); the
  adaptive budget from "one in-between fits" to "how many fit"; the setting (60 / 120 / uncapped). The #35
  squash snap and #18 pairing carry over. Cost: one full replay per extra picture (desktop GPUs; phones stay at 60).

## After the third test build (v0.90.0-standalone.8) — 2026-11-20

- **v0.90.0-standalone.9:**
  - Tagged 2026-11-20 on `7ef653b` with the owner's approval (release run 44781370962; `android-sign`
    approved).
  - Contents: the #35 menu speed (retail texture key), the frame-wait skip, background save states, the
    vertex-size cache, small fixes, the CI APK guard.
  - Tested before the tag on WSL, Windows, the Pocket 5 and the Mac mini M4.
  - Every package checked on the draft (the owner's rule), then PUBLISHED 2026-11-20 18:25 UTC; #35 comment
    posted.
- **Next build (v0.90.0-standalone.10, never published: shipped as `.11` with the mouse-camera settle fix; PUSHED to `origin/aurora-backend` 2026-11-20 evening, `bc3ba8c`):**
  - `d1bce57`: the "window" resolution covers widescreen windows (#35's soft textures).
  - `1cfaf23`: the padded-copy base, after Fina's flashback.
  - `60dd22d`: the release-checks docs.
  - `b3f1039`, `6654e9a`, `d4f5a30`: docs.
  - `c85534e`: #35 sky seams, varyings at the centroid with MSAA (the reporter plays at 2x MSAA).
  - `f4be57a`: #35 60 fps "warping", a model turning round in one frame no longer squashed in the in-between.
  - `717a61c`: docs, `tools/interp-check.py`.
  - The secrets' `5e0d68d` (pushed 2026-11-20 evening; `8c0c0cb` before the path entries were dropped): EU and JP
    PGO profiles, -14% to -21% CPU.
  - `f0bc73d`, `821ccab`, `deaff82`, `170aef6`: #34 shader preparation at start (`release/initial_pipeline_cache.db`,
    the bar, the MSAA-count fix); reviewed (Fable) and checked on the AYN Thor.
  - Before tagging: the pipeline list rebuilt from the sweeps' caches (Windows, WSL); a Windows check of the bar
    (keyboard / controller skip).
  - Checked (2026-11-20, development notes, #35): Windows (C:\dev\SkiesStandalone rebuilt; flashback 9.66 ->
    1.02 ms; the cover costs the UHD 770 25-34% uncapped at 1440p, still >= 1.8x); the Pocket 5 test app
    (`ws35n10`: flashback 12.22 -> 2.56 ms, 1.00x); the Mac mini M4 (battle, #16 digits, cover 2560x1920, flashback 3.15 -> 1.01 ms, 1.00x).
  - The Dusklight ports (`522d085`..`f59e370`, `HANDOFF-emulator-free.md` *Dusklight ports*): Display refresh frame
    rate (#20), upstream Aurora GX fixes, crash reports, mouse camera, controller light, launcher drop, the
    Resolution row's size, AppStream metadata.
  - Left before tagging: the owner's word to tag and approve `android-sign` (the push is done). The EU / JP
    profiles first run in CI at the tag: `validate` on `aurora-backend` can't run codegen (`HANDOFF-release.md`).
- **#35:** every reported item has a fix (the seams and the 60 fps squash in the next build); waiting on the
  reporter after it ships.
- **Fast floating point:** an experiment only.
  - Control run: a fresh exact profile gains nothing; fast FP is -14% to -20% on PC.
  - Before shipping (the Fable review): fusion consistent on AArch64, long exact-vs-fast runs, device
    tests, PC first.
- **Waiting on reporters:** #16 (confirm on `.7`+), #26 (the log line), #34 (fixed in `.8`, confirmation
  welcome).
- **Test files on C:** for the owner to decide (ask twice before deleting):
  - `C:\Users\Public\claude\`: `skies.rvz` (1.3 GB), several APKs, and `mac\skies-9.bundle`.
  - `C:\dev\SkiesStandalone\`: `check8\`, `check9\`, `next\`, `perf-wait\`, `perf-d3d12\` (its `after\`,
    `crash\` and `blur\` subfolders), `build\skies-standalone-before-next.exe` (the 11:45 build kept for
    comparison), and `build\` (rebuilt at 13:27 with the next build).
  - `C:\Users\Public\claude\mac\`: also `skies-next10.bundle` and `19-retroid-antonio-start.sav` (for the Mac).
  - PIX was installed by the Windows session with the owner's OK.

## After the second test build (v0.90.0-standalone.7) — 2026-11-20

- **Waiting on testers:** #16 (both reporters asked to confirm the numbers on `.7`), #26 (the new log line when
  the Dreamcast music's output pins: `Logs/skies-standalone.log`), #34 (the Android version, the screen, OpenGL
  ES, `files/log.txt`; `.7` switches the Renderer to OpenGL ES by itself after a crashed Vulkan start). Replies
  posted on all three.
- **#16 damage numbers all 0 (Apple M4, Exynos 1580): fixed in `.7`.** On cores with FEAT_AFP, FPCR.AH (set by
  GXRuntime's non-IEEE mode) makes FNEG keep a NaN's sign; clang builds `atoi`'s 0x80000000/0x7fffffff register
  pair with `movi` + `fneg`, so `__strtoul` got a max width of -1 and returned 0.
  `patches/recompcore-gxruntime-fpcr-no-ah.patch` sets FZ only (the release module's `msr FPCR` path checked).
  Found with debug switches (`SKIES_TRACE_PC`, `SKIES_TRACE_RANGE`, `SKIES_INTERP_RANGE`: native chunks
  interpreted instead), kept out of the tree: `build/i26/debug-tools-all.patch` in the issues worktree.
- **#34 AYN Thor closes at Start Game** (`.6`, fresh install too): not seen on the Pocket 5 / Nova (same chip as
  the Thor); a simulated second, near-square display runs the game fine. Likeliest a native crash in Vulkan's
  start, which `.7`'s safeguard turns into OpenGL ES at the next start. Needs the reporter's log.
- **The 0.83 Dolphin build still has #16** (it sets FZ|AH the same way): no Dolphin-engine release is planned.
- **#35 (2026-11-20, Windows, `.7` against the Dolphin perftest build).** Fixed in `.8` (commits `a0224ea` and
  `0194554`):
  - Blocky textures: Aurora's minification filter codes 2 and 5 were swapped
    (`recompcore-gxruntime-min-filter.patch`).
  - The FPS counter counted only the game's frames, so it showed 30 with 60 fps on.
  - The boss battle entry swirl showed the boss on black: a copy from an EFB without alpha kept the
    renderer's leftover alpha (`recompcore-gxruntime-copy-no-alpha.patch`).

  Open:
  - Direct3D 12 slowness: the reporter sees 2-9 fps in the character menu; here D3D12 runs like D3D11 on the
    RTX 5080 and the UHD 770, so their GPU is needed.
  - Sky seams near an unknown save point.
  - Fina's flashback at 14 fps.

  Replies posted 2026-11-20. Details: development notes, *Issues #26-#32*, the #35 entry.
- **#34, 2026-11-20:** OpenGL ES closes the same way, so it's probably not the renderer. Ask for `.7`'s
  `files/Logs/skies-standalone.log` (`.6` writes none on Android).
- **Next test build:** write `release/test-build-notes.md` fresh (#35's three fixes: credit Cawl87); ask the
  owner to approve `android-sign`.

## After the standalone test prerelease (v0.90.0-standalone.4, never published) — 2026-11-18

- **#26 Dreamcast music stuck at one level** in some songs (Ramirez / Valua scenes, the Dark Rift's end): the
  output pinned at full scale for seconds (half-wave), then a slow drift back. Not reproduced in any setup tried
  (2026-11-19, development notes, *Issues #26-#32*). The next test build has a DC blocker on the game's audio
  and a log line when the Dreamcast music's output pins (song, chip state): ask the reporter for that log.
- **#28 fix to dev**: not happening (no more Dolphin-engine releases, 2026-11-19); the new engine has it.
- **The `issues` branch** (#25 ShipHud, #19, #18, #24, #22): ported 2026-11-18 on the local branch
  `standalone-issues` (ws35 on all three discs, the standalone's menu row, DC gain and #18 pass;
  development notes, *Issue pass #16-#25*). In local aurora-backend; checked on WSL, Windows and the Pocket 5.
  Left: the push and the #24 VRR test on the user's display. #19's F1 is in the standalone too (2026-11-19:
  saves to the menu's slot unless F1 is bound; checked on WSL with a real key event, the bound case not run).
- **Patched Dawn** (MSAA 2x / 8x, the Windows swap chain fix): done 2026-11-19, built in CI (the owner left the
  call to Claude). Since `ceecbed` (review B2-B4) `standalone-host.yml`, called per platform, builds the Dawn package and
  the standalone against it without secrets, and a release fails without the patched Dawn. Before it, `validate.yml`'s `dawn` job (linux / android / darwin / windows) ran `tools/ci-dawn.sh` behind
  `actions/cache` keyed on `patches/dawn/*`, the Dawn scripts and Aurora's CMakeLists.txt (its pinned Dawn) and
  uploads `dawn-<target>`; the platform jobs download it and set `DAWN_PACKAGE` (`build-standalone-release.*`:
  `-DAURORA_DAWN_PACKAGE_URL=file://...`; `android-standalone-build.sh`: the package unpacked). Every step after
  the checkout may fail without failing the job, and the platform jobs run whatever its result: then the stock
  prebuilt, anti-aliasing Off / 4x only, as before, and the job adds a warning to the run's summary. First CI run
  (dispatch 44567700141 on aurora-backend, 2026-11-19): Linux 18 min and Android 13 min, both packages built and
  cached; Windows failed in configure (the .bat turns `DAWN_FETCH_DEPENDENCIES` off, which a fresh clone needs:
  `ci-dawn.sh` now runs `fetch_dawn_dependencies.py` first; the second run 44571866943 built and installed
  it in 25 min, then the .bat's `tar` was Git Bash's GNU tar, which reads `D:\` as a host: now Windows' own
  `tar.exe`); macOS never got a runner (GitHub's arm64 capacity).
  Caches made on a branch don't reach tag runs, so the first tag builds them again.
  CI's own packages checked: the Linux one (X11) under a local release build logs "MSAA modes: 1x, 4x, 8x", the
  Android one in the test APK offers 2x MSAA on the Pocket 5.
  **Third run 44577309166 (2a50e0c): all four packages built and uploaded** (Android 12 min, macOS 16, Linux 22,
  Windows 26). CI's Windows package under an MSVC build of the standalone on the RTX 5080 (D3D12): "MSAA modes:
  1x, 2x, 4x, 8x", 2x applied. The macOS package is built but not yet run on a Mac. Checked locally:
  Pocket 5 APK on the patched Android package (Anti-aliasing Off / 2x MSAA / 4x MSAA in the menu), Linux
  standalone built through the `file://` path (the nox11 package, on Wayland): Aurora logs "MSAA modes: 1x, 4x, 8x"
  on llvmpipe against the stock build's "1x, 4x"; MSVC build of the branch on Windows boots and plays. Not
  proven in CI until the next tag.
- **By hand:** two controllers and remapping with a pad (standalone); the desktop launcher's Continue click.
- **Android:** touch controls done 2026-11-18, opacity and size 2026-11-19 (no layout editor). Custom GPU drivers
  done 2026-11-19: libadrenotools in `libskies.so`, Dawn reaches the driver through `libskiesvk.so`
  (`android_vkshim.c`) and `AURORA_VULKAN_LIBRARY_DIR` (`recompcore-aurora-vulkan-dir.patch`), the launcher's rows
  and crash safeguard (`recompcore-standalone-driver.patch`, SkiesGameActivity). Pocket 5 with Turnip v26.0.0 R8:
  `vulkan.ad07xx.so` mapped, `vulkan.adreno.so` not, the first battle and Baltor's ship battle at 1.00x.
- The standalone's game clock was UTC (`std::time()`); local time since 2026-11-18 (`boot.cpp` `RtcBase`, as the
  Dolphin build's), so card file dates read as the player's clock.

## Dual audio (Japanese voices) — requested 2026-10-17

A player asked for dual audio: a choice of English or Japanese voices with the English text kept.
Skies has no fully voiced scenes; the voices are short clips (battle shouts, Special Move calls,
exclamations under story text boxes), so this swaps that set of clips.

- **Source:** the player's own Japanese disc, imported like the Dreamcast music (no game files in
  the build). Candidates: the Japanese GameCube release (*Eternal Arcadia Legends*) or the
  Dreamcast original (*Eternal Arcadia*).
- **To find out first:**
  1. How the US disc stores the voice clips (format, file names, banks), and whether the Japanese
     disc uses the same layout. Same layout → mostly a file redirect, like the DC music.
  2. Whether the clip lists match one to one (extra, missing or reordered lines in the Japanese
     release).
  3. Whether anything in the US game assumes English clip lengths (text box waits, lip flaps,
     battle timing).
- **Researched 2026-10-21 (not built; answers to the three questions):**
  1. **Layout: the same.** Every voice is in `sound/`, under the same 1799 names on the USA, European
     and Japanese GameCube discs: streams `B702NNNN` (battle lines), `KO00NN` (each spell's / S-move's
     voice and effects pre-mixed), `NNN` (victory lines), `f71000NN` (field voices), and the banks
     `STVnn` / `CRnnn` / `e6xNNN` (`.info` + `.samp`; decomp `docs/sound.md`).
  2. **Clip lists match**: every bank has the same sequence count except two where the Japanese one
     has an extra sound at the end (`e6h011`, `k8900001`), which the USA game never asks for.
  3. **Lengths**: 30 of the 66 `KO` streams differ in length (up to -1.6 / +1.2 s; the action waits
     for its stream, so a spell takes the Japanese length; check no animation is cut). Text boxes
     don't wait on clips.
- **How:** swap the **whole** `sound/` folder, not the voice files alone. The Japanese `.info` files
  of the banks that otherwise match (music, field effects) differ only in MusyX object IDs, numbered
  across the whole project, so a mix of USA and Japanese banks could collide; the music `.samp` data is
  byte-identical, so the whole folder changes nothing but the voices. The Japanese files mostly have
  other sizes (unlike the Dreamcast music's same-size conversion) and a save state holds the disc's
  file table and the banks loaded at boot (`tone`, `s1000000`, `b7000000`), so the setting needs a
  restart (as Language does) and save states kept per voice setting. The reverse (Japanese text,
  English voices) is the same swap; European text with Japanese voices works too, and the Japanese
  streams fit the port's 60 Hz (the European ones were made for 50 Hz, *European 50 Hz audio* in the
  development notes). Dreamcast voices: other formats, and Legends has voiced additions; not worth it.

## The European languages for the USA game — asked 2026-10-21 (issue #3 comment)

A player on the European disc saw the intro out of sync and asked whether the European release could
be imported only for its languages, like the Dreamcast music. The desync is fixed on branch `pal60`
(the European scripts' 50 Hz timing, its streams and enemy sound tempos; development notes *European
intro in sync with its music*, *European 50 Hz audio*), which makes the European disc play like the
USA one. Running the USA game with the European text would be much harder: the USA DOL has no code
for the European `.sot` text files or the language-suffixed graphics, and the European scripts
differ. Answer on #3 once the fix is released.

## Remember controllers by identity, not name — idea 2026-10-17

Controller 1 is saved as Dolphin's `SDL/<n>/<name>`, and a name can change with how the game was
started (Steam's virtual gamepad versus the pad itself; issue #1, HANDOFF-input *Steam Controller*).
Aurora's ports save each port's pad by SDL GUID + serial with a VID/PID fallback and only display the
name. Doing the same would mean our own field beside Dolphin's `Device` (the launcher and
`SkiesInput::Follow` resolve it to the current `SDL/<n>/<name>`). Not needed while the game follows
whichever pad is pressed and the launcher matches by label; worth it if more name reports come in.

## Retry the extracted game's rename on Windows — 2026-10-20

Once in about six Windows extractions (2026-10-20, issue #6 tests) the last step failed with "can't
publish extracted game: can't move the new game folder into place: Access is denied"; the same
extraction worked when run again. Likely a scanner (Defender) still holding the 1.4 GB of new files
when `ReplaceDirectory` renames the staging folder into `games/<id>`. A short retry loop (a few tries
over a few seconds) on that rename would hide it from players. Not reported by players yet.

## World map meter / compass keep the old shape after a Picture change in the world map menu — 2026-10-17

First reported as "widescreen stretches the world map's flight meter and compass"; re-diagnosed the
same night (development-notes *Known issues*): in normal play they match the unpatched game in every
picture. Only after changing Picture while the world map's own menu (Start) is open do they keep the
previous picture's shape, until that menu closes (they're replayed from display lists recorded before
the change). Cosmetic and transient; low priority.

## Romhacks that change the game's code — 2026-10-18

The release takes romhacks that change only data (HANDOFF-romhacks, development notes *Romhacks*);
one that changes code is refused at import, because the module runs the code it was compiled from (a
changed instruction in a chunk reached by a native call silently runs as the old code). Ways in, if
a wanted romhack needs it:

- **Known changes:** add its code change to `patches/game/<disc ID>.known` (`tools/dolknown.py`) and
  give the release an option of its own for what it does, as for hold-B.
- **A module per romhack:** build one from the romhacked DOL (about 8 minutes and 150 MB per platform,
  its own PGO run); modules are found by disc ID only today, so they'd need a key like the code hash.
- **The runtime:** check every chunk against the loaded DOL at boot and send calls into changed ones
  through the dispatcher (needs DolRecomp's direct calls to check a per-chunk flag); unknown cost.

## Windows: Smart App Control blocks the unsigned exes — found 2026-10-26

A fresh Windows 11 with Smart App Control on (state 1 under `HKLM\SYSTEM\CurrentControlSet\Control\CI\Policy`
`VerifiedAndReputablePolicyState`) refuses to start the unsigned game exe: "An Application Control policy has
blocked this file", from a mapped folder or a local copy alike. Seen in Windows Sandbox (build 26100.9711), whose
image now has it on; the 2026-10-13 sandbox test of the release zip didn't hit it. It applies to the released
Dolphin build's exes as much as the standalone's. Smart App Control is on only for some clean installs (it can't
be turned back on once off), so most players won't see it, but those who do get no way past it but turning it off
for good. The fix is code signing (a certificate with reputation, e.g. Azure Trusted Signing); the user's call
(cost). Meanwhile the release README could say what the message means.

Standalone sandbox test (`C:\dev\SkiesStandalone\sandbox`, `skies-sandbox-sa.wsb` + `in\test.ps1`, which turns
Smart App Control off with `CiTool --refresh` first): D3D11 draws the game; D3D12 crashes the sandbox's kernel
while Dawn enumerates adapters (a Windows GPU-PV bug); Vulkan's crash was Dawn calling a missing
`vkCreateSwapchainKHR`, fixed by `patches/dawn/0002` (in the CI-built Dawn since 2026-11-19). Details in
`HANDOFF-feature-gaps.md`, *Items 8-10*.

## macOS release: signing, notarization — 2026-10-18

The macOS build (HANDOFF-macos) is built by CI (validate.yml `macos`, since 2026-10-19) and signed ad
hoc, so Gatekeeper asks once ("Open Anyway"). A smoother release needs an Apple Developer ID (paid
account), `codesign --options runtime` with a timestamp, and `notarytool` submission and stapling in
that job. The hardened runtime would also need checking against Dolphin's needs (no JIT is used by the recomp, but
the fallback interpreter path and MAP_JIT should be confirmed). A macOS PGO profile is a separate,
smaller step.

## Uncensored, what's left — requested 2026-10-20

The Uncensored setting (development notes, *Uncensored*; HANDOFF-uncensor) covers the art that is a
straight asset swap. Still to do, from the same Eternal Arcadia discs:

- **The drunk sailor model** Legends remade (`tc0102` in `A004AB05`).
- **Script work:** the drunk sailor in the Sailors' Island tavern (entry `0xc21` only on the
  Japanese disc; his English lines are still in `field/me004a.sct`), the Hamachou party (disc 2),
  Ramirez's blood (`ME131I.SCT`), and the text ("loqua" for rum in about 104 lines across 21 scripts,
  Salam, the Belleza and Ramirez lines). New English is needed for some; agree the scope first.
- **Android import of GDI zips:** Android takes only CHDs (a GDI's track files can't be opened beside
  a picked document); the TOSEC Eternal Arcadia dumps are zipped GDIs, so reading a GDI inside a zip
  would save players a chdman step.
- **The @barai edition** (v1.003, HDR-0119): not checked; its files fail the SHA-1 check if they
  differ, which leaves those files out.

## Super attacks out of sync with their sound at battle speed — reported 2026-10-21, done (ws27)

**Done 2026-10-21 (game patch ws27, `dev` `c18ddc4`):** spells and special attacks (party and enemies)
play at normal speed, always on (the user's choice); development notes *Spells and special attacks at
normal speed*. Ship battles followed in ws28 (2026-10-21, branch `smove`): their magic, crew specials,
enemy special attacks and the S-Cannon at normal speed too, and frame interpolation stays on for all of
these at speed (they showed at 30 fps before); development notes *Ship battles' spells and special
attacks*. Not seen live yet: the S-Cannon, crew specials and enemy special attacks in a ship battle.

Issue #7 (bobbleyaga) and on Reddit (r/recomps v0.60.0-beta thread, Unleaded_95): every super attack,
the party's and the enemies', plays its sound past the animation, or the game waits for the sound,
"worse when increasing battle speeds", with or without V-Sync. Battle speed skips the game's
end-of-frame waits, so the animation runs 2-4x while the sound hardware keeps real time. #7 asks, in
order: Normal speed during a special attack (a toggle), the sound sped up, or the sound cut when the
animation ends.

**Measured (2026-10-21, Windows test copy, Cutlass Fury, audio dumps + game state):** a spell's or an
S-move's sound is one pre-mixed stereo stream, `SOUND/KO00NN_L/R.dsp` (NN = the skill id; voice and
effects together, no sound effects started during it), and the action's last step waits for the stream
to end. The animation after the stream starts: 10.1 s at 1x, 5.0 s at 2x, 3.3 s at 3x, 2.5 s at 4x; the
stream is 11.6 s, so at 4x the battle sits about 8.6 s with the animation over (the Reddit "I'll still
have to wait until the audio finishes"). No drift at 1x. So "the sound sped up" can only mean the whole
stream faster (higher pitch, or a time-stretch), and "cut when the animation ends" loses most of it at
4x. What runs it: the party's dispatcher `fn_80086C68` puts `fn_80085064` on the unit's task
(`lbl_80309700[unit]`) for command type 1 (magic) and 2 (S-move) in `lbl_80309174[unit]` (0x20 each:
+0 type, +6 skill id; 3 = attack, `fn_80086308`); the enemies' `fn_8008B9E0` puts `fn_80087074` there for
type 1 (their magic, rule codes 500+) and 0xC (their specials, `fn_8008A424`); `lbl_80347334` is the
unit acting. At 3x / 4x the stream used to stall and freeze the battle (the overclock; fixed, development
notes *A battle can freeze*).

## Texture filtering and lower resolutions — requested 2026-10-21

Reddit (r/recomps v0.60.0-beta thread, Unleaded_95): texture filtering options, and resolutions below
the lowest 16:9 one offered (640x528 shown on a 4K display) for an original-resolution look on 16:9:
640x360 or 768x432. **Done on branch `gfx`** (development notes, *Texture filtering, anisotropic
filtering, scaling*): 640x528 is already the original, so Sharp scaling answers the second part.

## AppImage update information — suggested 2026-10-21

The AppImage catalog's test (AppImage/appimage.github.io#10524, since merged) passed on v0.80.0-beta (glibc 2.34),
with two warnings: no update information (embed it with `appimagetool -u "gh-releases-zsync|ogdanimal|SkiesOfArcadiaLegends-Recomp|latest|SkiesOfArcadiaLegends-*-x86_64.AppImage.zsync"`
and publish the `.zsync` beside the AppImage, so AppImageUpdate works), and a "mostly dark" window: its
screenshot shows the launcher without the background art, which loads within 3 s under Xvfb here
(software GL), so likely the bot's sandbox; the launcher's own dark look is by design.

## Drachma's victory pose squeezed into 4:3 — issue #9, fixed on dev (ws31), 2026-10-22

Sandbowl, against v0.80.0-beta, with a video: in 16:9 Drachma's victory pose "reverts back to 4:3". It
copies the screen and draws it back on a plane in front of the camera (a shake), sized to the game's
own field of view, so the whole picture was squeezed into the middle 4:3 with black sides. Game patch
ws31 draws that plane at the game's field of view (development notes *Battle screen plane*). Checked on
WSL, the Retroid Pocket Nova and the Retroid Pocket 5; not released yet.

## Both landscape orientations on Android — issue #12, done on dev, 2026-10-22

MK13X (Pixel): the game is locked to one landscape; wanted the reverse one too. The game screen now
follows the device between both landscapes, unless the system's rotation lock is on
(`recompcore-android-orientation.patch`). On a Retroid the firmware's own landscape overlay
(`com.rp.settings`) decides the orientation, so it can't be seen there; the Activity's request was
checked with `dumpsys` on the Nova. Needs a phone to see it turn.

## ~~Ship battle command grid stretched in the first tutorial~~ — issue #11, FIXED in v0.81.0-beta (ws30)

Sandbowl, against v0.80.0-beta: "The input command screen is still stretched in different aspect
ratios. This only happens during the first tutorial." While Drachma explains the grid ("a 3X3 grid
will appear"), the game shows it as a sprite picture across the whole screen, which the patch drew
full width; the real grid at command input was fine. Game patch ws30 keeps that picture at the HUD's
4:3 shape (development notes *Ship battle tutorial grid*). Released in v0.81.0-beta; #11 closed.

## Cutscene shots that show past their set pieces in widescreen — issue #10, 2026-10-21

Renatodep: "Is there a way to edit models positions for the wide screen cutscenes? I.e. opening with
Fina where you see the valuan ship on the side?" Branch `cutscene` frames listed shots (development
notes *Cutscene shots framed for widescreen*); the opening's two are listed (ws29), and since ws32 two more from GLEclipseGaming's comments: the opening's horizon shot and Aika and Vyse leaving the lookout on the Moon Stone night. The rest of the game's
cutscenes (255 scripts, 5,493 distinct camera commands) aren't surveyed: list shots as players report
them, or survey a scene at a time with the 16:9 / 4:3 shot log. Data-only alternative players can use
today: SPICE (github.com/SoAModTools/SPICE) edits `.sct` camera poses, lens and object placement, and
the port plays such a romhack (it changes the 4:3 picture too).

## The emulator-free path (Aurora + standalone GXRuntime) — decided 2026-10-22

Goal: the port runs without Dolphin's core: the recompiled game on GXRuntime, ModernGekko's standalone
runtime, with Aurora drawing; the Dolphin build ships until the standalone one reaches parity. Plan,
gates and the survey in [handoffs/HANDOFF-emulator-free.md](handoffs/HANDOFF-emulator-free.md). Hard
gaps: audio (Skies mixes with MusyX 2.0 on the DSP and GXRuntime has no DSP emulation: a DSP emulator or
a native MusyX engine such as AxioDL's amuse), the 60 fps interpolator (built into Dolphin's video layer),
a new Android shell, and about 25,000 lines of port features written against Dolphin's APIs. Status
(2026-10-25): the standalone host plays on desktop (WSL, Windows D3D12) with no Dolphin core: audio
bit-exact (Skies' MusyX ucode is Dolphin's AX, ported as HLE), card saves, the in-game menu, save states;
same-machine baseline: faster than the Dolphin build on CPU (battle too: 3.78x vs 3.25x, 1.86x vs 1.27x on
one E-core, 0.35 vs 0.49 cores at full speed), and on GPU about 44% less GPU time per frame (GPU-bound on an
iGPU at 6x: 24.4 against ~43.5 ms, 1.30x against 0.77x, after forks 0032-0035; at full speed there the
standalone holds 33 ms frames, Dolphin is late on every one; the first GPU number, "more GPU time", came
from a Dolphin run at the wrong resolution). Goal set by the user: better than the Dolphin build, weaker
hardware included. Android: a test app (`tools/android-standalone-build.sh`,
com.skiesofarcadia.standalone) plays on the Retroid Pocket 5: 22-25% faster uncapped than the Dolphin build
on the same DOL and module (1.68x vs 1.38x, battle 2.04x vs 1.63x), and since the pacing fix (presents scheduled
on the throttle's clock, fork 0036, with an adaptive delay; the emulation thread on the prime core: schedutil
had clocked the sleeping thread down) smoother at full speed at ~0.4-0.5 W less (p99 34.2-34.4 vs 37.7 ms in
battle, 34.7-35.0 vs 36.2 at the save point; Windows p99 39.8 -> 35.0 too); at 6x it holds full speed where
Dolphin can't (GPU-bound 1.24x vs 0.87x at the save point; fork 0037 dropped the present resample pass). The
shell is immersive and survives background / foreground. Since then (feature-gap items 0-6, 2026-10-25/26): the
European and Japanese discs, Continue, the graphics options with MSAA up to 8x (a Dawn patch; Pocket 5 2x, RTX 5080
8x), input (rumble, keyboard layout, Button mapping, controller follow), Language, HD textures (pack import; on
the Pocket 5 as fast as without the pack, half the Dolphin build's memory, BC7 skipped there; fork 0045 fixed
a loader-thread hitch) and 60 fps (frame interpolation), and the Japanese memory card fixed. Aurora now runs on
a video thread beside the game (fork 0046, `HANDOFF-emulator-free.md`, *The video thread*): Pocket 5 uncapped
1.77x / 1.70x at the save point at 30 / 60 fps (Dolphin 1.38x / 1.41x), 2.15x / 2.05x in battle (1.63x /
1.69x); 60 fps at full speed p99 ~17.7 ms against Dolphin's 24.5-25.9, at less power. Ahead as an engine, not
yet a replacement (`HANDOFF-emulator-free.md`, *Where it stands*). **2026-11-19:** items 7-10 done, the
opt-in test prereleases ship it, and the parity pass brought over the rest the owner wanted (Renderer choice,
touch opacity / size, rebindable hotkeys, Turnip, CI-built Dawn for MSAA 2x / 8x); left in the Dolphin build
only: Android 7 / 8, the Windows File menu, netplay, non-memory-write cheats. What's missing is a play-through
(`HANDOFF-emulator-free.md`, *Parity (2026-11-19)*).
Item 7 since (2026-10-26): the Dreamcast features (Uncensored, the Dreamcast music and streams, the
European 60 Hz audio) from the Dolphin build's modules copied as they are behind shimmed Dolphin headers;
every output byte-identical to the Dolphin build's. Item 8 (2026-10-26): the Dolphin build's save states load
(converted), the launcher patch, controller Device; Windows' Vulkan and D3D11 fallbacks fixed (fork 0049), a
dialog when no graphics API starts; no console window on Windows; the Language restart on Android (Pocket 5,
European disc). Open: the Android launcher and packaging (items 9-10, waiting for dev to be
merged into the branch, on hold by the user); a PC where D3D12 really fails and D3D11's speed on a weak GPU; the EFB
passes on the tiler; the built-in pad and audio on the device (by hand), size changes on return. The plan for
the features: [handoffs/HANDOFF-feature-gaps.md](handoffs/HANDOFF-feature-gaps.md).

The Aurora renderer inside Dolphin (`--graphics Aurora`, `AURORA=1`), the groundwork for it: the picture
matches Dolphin on both Retroids, Windows and WSL (fork fixes: the XF viewport decoded per register;
intermediate EFB passes no longer skipped under the 60 fps replay or before a kept-picture copy); 60 fps
at full speed on both Retroids with Vulkan's on-screen cadence; the Picture option applies; the in-game
menu and OSD draw through a Dear ImGui overlay (fork 0026). Inside Dolphin it is at parity with Vulkan, not
ahead, and Dolphin's HD packs don't apply under it, so it stays opt-in there. Committed as patches on the
branch `aurora-backend` (kept local until 2026-11-18); on dev since the Dolphin removal (2026-11-21).

## Cheat codes from the GameCube databases — issue #5, requested 2026-10-21

**Done 2026-11-19 in the standalone** (`standalone/src/cheats.cpp`): `Cheats/<ID>.ini`, then `GameSettings/<ID>.ini` (Dolphin's format; the Android launcher's Cheat file import writes the first),
decrypted AR type 0 subtype 0 writes / fills and Gecko 00-05, every frame while `[Skies] Cheats` is on (Gameplay
tab row, the file re-read when the menu opens); anything else skipped and logged. Checked on WSL: a 32-bit AR
write to `0x8030BB40` showed 99999999 Gold in the field menu (83 without). Not done: a per-code list in the
menu, encrypted AR codes, conditionals (both launchers have the Cheats switch). The plan as written:

doc-haz: optional cheats from the existing Action Replay / Gecko lists (gamehacking.org game 54824,
USA): infinite gold, Moonberries, max party Spirit, ship HP, items, no random battles, save anywhere,
character and equipment modifiers, toggled in a file or the in-game menu. The recompiled game keeps the
GameCube's RAM layout, so plain RAM-write codes (AR 00/02/04 writes, Gecko 00-05 and their
conditionals) could run as written once per frame from the frontend; codes that patch instructions
(Gecko C2/04 into `.text`) do nothing, since the code is native, and need a game-patch equivalent
(`patches/game/qol.c`, decomp `docs/qol_cheats.md`; Encounters and Invincible are already there).
Steps: a code parser + per-frame applier in the frontend, a Cheats list in the in-game menu, a check of
the listed codes' kinds.

## More save slots — issue #15 and #19, requested 2026-10-22 / 2026-10-27

Renatodep (#15): "increase the number of save slots". Unclear whether the game's memory card files or
the port's save states (slots 1-5, in-game menu *Save state*). Cawl87 (#19) wants the opposite for
states: a cap, keeping just slot 1 (the in-game menu already saves to a chosen slot). Ask #15 which one;
for states, a slot count setting covers both.

## The rest of the reported cutscene shots — issue #10, reported 2026-10-22..26

Not framed yet (ws32 has the four listed above), from the #10 comments:
- Opening, Galcian turns to the camera: the flames' backdrop ends short of the left and right edges.
- Opening: a piece of Alphonso's ship clips through Fina's ship (the original does it too).
- A room shot showing the sky and the outside wall over the top: pan the shot down (three screenshots).
- Sailors' Island, the three shops: move the camera.
- A Nasr merchant who appears from nowhere at the edge.
- Vyse pops up at the far right in a scene with Alfonso.
- Marco running in place at the side of the screen (Cawl87, video).
- Effects still drawn 4:3: the Harpoon Cannon animation, the Red Gigas' beam, some Lynx ship battle
  spells (Cawl87).
Each needs a state or a warp to the scene (`docs/development-notes.md` *Cutscene shots framed for
widescreen*) and a per-shot zoom, or for the effects a screen-plane fix like #9.

## Dreamcast DSF rips as the music source — issue #8, requested 2026-10-21

aro-1790's second point: play the Dreamcast music from the DSF rips (Highly Theoretical plays them
directly), so players don't need the Dreamcast disc. Open: hosting (the rips are game data), and
mapping the rip's track names onto the song numbers the GameCube driver asks for. The reverb half of #8
is done (v0.80.0-beta).
