# Handoff: the standalone's feature gaps (2026-10-25)

The standalone build (`standalone/`, the recompiled game on GXRuntime + Aurora with no Dolphin core) already
runs better than the Dolphin build (faster, smoother, less CPU and power; *HANDOFF-emulator-free.md*). It is
not a replacement until it has the Dolphin build's features. This file is the plan for re-hosting them: what
each one is, where its code lives, what the standalone has, the port steps, how the Dolphin build verified it
(so the port is checked the same way), and size. Read *HANDOFF-emulator-free.md* first (the standalone's
architecture: *In-game menu*, *Save states*, *The Android shell*, *Pacing on the Pocket 5*).

**Progress (2026-10-26):** items 0-6 done on desktop (2 including MSAA 2x / 8x through a Dawn patch;
3 = input: rumble, release keyboard layout, names, Button mapping, controller follow; 4 = Language with the
restart; 5 = HD textures: import, menu row, files decoded off the render thread; 6 = 60 fps), each with its
checks and leftovers in its section below; the Japanese game's memory card fixed on the way (item 1). The
Pocket 5 checks of items 5 and 6 are done too: HD textures as fast as without the pack after a loader-thread
fix (fork 0045); 60 fps smoother than the Dolphin build's at full speed and at less power, and since the
video thread (Aurora off the game's thread, a4f4014, fork 0046) ahead uncapped too (save point 1.70x
against 1.41x; item 6, *The video thread*). Item 7 (Dreamcast) done too (9862a64): every output
byte-identical to the Dolphin build's, the music level with it at the save point and ahead in battle.
**2026-10-26 later (items 8-10 started):** the desktop runner contract is in (6e06979: `frontend.cpp`,
states under the Dolphin build's names, GCPadNew.ini bindings) and the Dolphin `.sav` converter works
(f0c2131, 0a762c3, c27b402, fork 0047); see *Items 8-10* below, *Done so far*. **2026-10-26 evening:**
converted states checked on WSL (15) and the Pocket 5 (8), a ten-minute play and card saves from one on
Windows, the launcher leftovers as `patches/moderngekko-standalone-launcher.patch` (fb58a2b) and controller 1
following GCPadNew.ini's Device (2d80f3f). **2026-10-26 late:** a window resize loses nothing visible
(`SKIES_RESIZE_AT`, e5e673d); the WSL texture flicker is Mesa's d3d12 GL driver, not Dawn or Aurora (fork 0048
`DOL_AURORA_BACKEND`, 6195e00); on Windows the Vulkan and D3D11 fallbacks start now (fork 0049) and a PC with
no working API gets a "No graphics support" dialog (f3953d2); the manifest says `userLandscape` (039930d).
Windows Sandbox (3319001): Smart App Control blocks the unsigned exe (backlog), D3D11 draws, D3D12 crashes the
sandbox's kernel (Windows' GPU-PV), Vulkan's crash fixed in Dawn (`patches/dawn/0002`). No console window on
Windows, the log to `USER/Logs` (64ea652). The Language restart on Android, checked on the Pocket 5 with the
European disc (fde8bf1). **2026-10-26 night:** a resize during the battle start's screen grab blacked the
grab out for a second; fork 0050 keeps EFB copies across a resize (WSL and Windows checked; item 9,
*Lifecycle*). The desktop Language restart from a loaded state boots clean (item 4).
**Battle speed while choosing commands (2026-10-26 night, the user):** at 2x-4x the command wheel turned too
fast to pick from. `menu.cpp` `qol::ChoosingCommands` now writes normal speed while the game waits for
commands: a ground battle's command input task (`fn_8007CAB0`, EU `8007CE74`, JP `8007CA98`) in the task list
(`lbl_803475A8` -> root task, +4 links; EU `8034ACC0`, JP `80347028`), a ship battle's turn not playing
(`lbl_803472DC` = 0; EU `8034A9F4`, JP `80346D5C`); the turn then plays at the chosen speed. Found by tracing
the task list across a turn (the wheel `fn_80025624`, the party panels `fn_80022F74` and `fn_8007CAB0` come and
go together). Checked on WSL: US battle-start state (normal from the input to the last command, 4x for the
turn, normal at the next input) and the EU / JP deck battles from a cold boot (the same switches every turn).
Ship battles not run. **Not in the Dolphin build yet** (dev's `field-speed` / `battle-specials` speed logic in
SkiesMenu.cpp): the user's call.
Items 9 and 10 need dev's code merged in first; **the user said hold** (no merge or rebase for now; *Items 9 and
10 need dev's current code* below). **Pick up at items 8-10**, now one goal: the standalone **replaces** the Dolphin build as a normal update
(the user, 2026-10-26; *Items 8-10* below has the approach, the runner contract and what doesn't carry over
yet). Standing leftovers:
Continue on the Pocket 5 (item 1); MSAA's
cost on a GPU the game saturates (item 2); shipping the patched Dawn and the macOS build (item 2); rumble by hand,
a capture with a real pad, two pads and the Pocket 5 (item 3); the
cloud-sea edges and the full 4x pack (item 5); 60 fps on the Nova and its scene list (item 6); the
video thread on the Nova, macOS and with a pad by hand; item 7's open list (m03 and the intro in game, the
European intro timing, the save point's music cost).

## Ground rules

- **Where:** worktree `~/projects/skies-aurora-backend`, branch `aurora-backend`. Local only: never push,
  rebase or merge it. Commit explicit paths only.
- **The branch is behind dev.** It was cut from origin/dev `a7aa0f8` (2026-10-22); dev is at `88f81ea`
  (v0.83.0-beta) with 40 more commits, among them `recompcore-review-d.patch` (fixes across 60 fps, Dreamcast
  music, texture packs, the Android app, menu and input), `android-orientation`, `android-pad-follow`,
  `module-fma`, and game patch ws33 / ws34. **Port from dev's versions** without rebasing:
  `git show origin/dev:patches/recompcore-<name>.patch`, `git show origin/dev:patches/game/<file>`,
  `git show origin/dev:docs/handoffs/<file>`. The order the patches apply in is
  `origin/dev:patches/recompcore.series`. Fetch first.
- **The standalone's DOL and modules:** the bench and test runs use the ws26 game patch and its module
  (`build/android/jniLibs-sa26`, `C:\dev\SkiesRomhack\build\game\GEAE8P-ws26`). Features that need newer game
  patch code (Continue needs ws20+, battle specials ws28, Japanese Continue ws34) need a newer DOL and a
  module generated from it (`NAME=... MODULE_ONLY=1 DOL=... tools/android-ws-build.sh`; desktop modules as in
  the Windows build section).
- **Performance goal (the user's):** the standalone must stay better than the Dolphin build, weak hardware
  included. Features that add work (60 fps, Dreamcast music, HD textures) get a head-to-head on the Retroid
  Pocket 5 (`~/projects/skies-aurora/capture/android-ir-bench.sh`) and on desktop before they're called done.
  All Pocket 5 numbers so far are on Retroid's lowest performance setting.
- **Shared settings:** the standalone reads and writes the Dolphin build's own keys in the same files
  (`standalone/src/settings.cpp`: `USER/Config/Dolphin.ini [Skies]`, `GFX.ini`). Keep every new option on the
  Dolphin build's key and default, so both builds share one set of choices.
- **Not shared:** save states (the standalone's `.sst` vs Dolphin's `.sav`, by design); controller
  bindings (see *Input*). Card saves (GCI) are shared.
- Carried-over rules: sub-agents use Sonnet; the Pocket 5 is available but don't unlock it or inject input,
  and ask before replacing an installed app; only Windows' `adb.exe`; delete frame dumps and screenshots after
  viewing, no loop capture; C: deletions need two approvals (so Windows test output goes to WSL: *Testing
  practice* below); Windows Dolphin-build launches call `C:\dev\sync-controls.bat USER_DIR` first; never
  `pkill -f` a pattern from your own command line.

## Testing practice (learned in items 3-6)

- **Profile the Pocket 5 with simpleperf** (the test app is `profileable`): `adb.exe shell simpleperf record
  --app com.skiesofarcadia.standalone --duration 10 -e cpu-clock -f 4000 -o /data/local/tmp/perf.data`
  during a bench run, pull it, report on WSL with the NDK's host
  `simpleperf report --symfs DIR --tids TID --sort dso,symbol --raw-period` (`DIR` + the device's
  `/data/app/.../lib/arm64/` path holding the unstripped `build/standalone-android-dawnsrc/libmain.so`).
  Divide by the game frames in the 10 s for ms per frame. `capture/android-thread-sample.sh` shows the
  threads' cores and the clusters' clocks. Android reads `env.txt` in main: environment switches must be
  read at first use, not in static initialisers.
- **Threads:** Aurora runs on the video thread (*HANDOFF-emulator-free.md*, *The video thread*);
  `SKIES_VIDEO_THREAD=0` keeps one thread (same pictures), `SKIES_VIDEO_STATS=1` logs the game's waits and
  the video thread's work. Windows benches from the bench states without writing on C:: copy
  `user-bench-sa` into WSL and pass `--user`/`RUNLOG` as `\\wsl.localhost` paths (the old `bench-sa.sh`
  copies into `C:\dev\SkiesStandalone\user-bench-run`).

- **Exact comparisons on Windows only.** WSL runs aren't repeatable (two runs with the same settings
  differ in some frames); Windows D3D12 runs with `SKIES_RTC=847000789` and `AURORA_SYNC_PIPELINES=1` are
  (60 fps on vs off: real frames pixel-identical). WSL is fine for "does it work" and the logs.
- **Windows output into WSL:** `SKIES_DUMP_DIR`, `RUNLOG`, `--user` and screenshots take
  `\\wsl.localhost\Ubuntu\...` paths (`wslpath -w`), so nothing new lands on C: and it can be deleted
  without asking. Packs and games can be read from WSL the same way.
- **Frame dumps** don't include the in-game menu (the overlay is drawn after the readback): screenshot the
  window with `C:\dev\SkiesStandalone\shot-dpi.ps1 OUT.png` (DPI-aware; `shot.ps1` cuts the window off
  under display scaling). With 60 fps on, dumps write `frame_N-interp.ppm` too.
- **`--seconds` with `--load-state`** counts the state's guest clock, and `[video] frame` numbers continue
  from the state's.
- **Test files on C: from this plan, for the user to keep or delete** (in `C:\dev\SkiesStandalone`;
  2026-10-26 evening adds `build\user-lp\` (a test card save, from a relative `--user` path), and
  `standalone\src\input.cpp` synced to 2d80f3f with the build rebuilt):
  `user-in1\`, `user lang\`, `user-hd\`, `user-hd-png\` (texture packs imported), `in1.log` `in2.log`
  `in3.log`, `lang.log`, `langtest.bat`, `hd-on.log` `hd-off.log`, `time-hd.log` `time-hd-png.log`,
  `async-hd.log` `async-hd-png.log`, `menu-hd.log`, `menu-hd.png` `menu-hd2.png`, `shot-dpi.ps1` (a tool;
  worth keeping). Earlier: `skies-standalone-msaa.apk`, `user-msaa*`, `dumps-msaa*`, `dumps-cont*`,
  `msaa*.log`, `cont*.log`, `build-dawnsrc\`, `C:\dev\dawn-src`.

## The gaps

The Dolphin build's in-game menu (dev): **Saves** (same in both), **Graphics** (Picture, HUD, Frame rate,
Resolution, Antialiasing, Texture filter, Anisotropy, Scaling, HD textures, V-Sync, Show FPS), **Gameplay**
(same in both), **Language**, **Sound** (Volume), **Dreamcast** (Music, Uncensored; the launchers add Intro
music, `[Skies] DreamcastIntro`), **Controls** (Button mapping, Stick dead zone, Rumble). The standalone's
when this plan began (`standalone/src/menu.cpp`): Saves, Graphics (Picture, HUD, Resolution, V-Sync, Show
FPS), Gameplay, Sound (Volume), Controls (Stick dead zone, its own key `StandaloneDeadZone`). Now (items 2-6):
the Dolphin build's tabs and rows but the Dreamcast tab (Rumble on its own key `StandaloneRumble`).

| # | Gap | Dolphin source (dev) | Standalone today | Size |
|---|---|---|---|---|
| 0 | European / Japanese discs run right | `release/GEAP8P.ini`, `GEAJ8P.ini`; `ConfigManager` hunk in `skies-dcmusic-eu` | **done** (desktop): idle PC per disc, EU as NTSC, module by disc ID | hours |
| 1 | Continue (card save at boot) | `patches/game/autoload.c`; `autoload` in `SkiesMenu.cpp` | **done** (desktop): `--continue`, `AutoLoadCard`; JP card mount fixed | ~0.5 day |
| 2 | Graphics options (MSAA, texture filter, anisotropy, scaling) | `recompcore-gfx-options.patch`, `skies-settings` | **done** (WSL): four rows, live; fork 0038 | ~1 day |
| 3 | Input: release keyboard layout, rumble, names, Button mapping, menu-close rule | `recompcore-input`, `controller-names`, `menu-close`, `android-pad-follow`, review D23-D26 | **done** (desktop): rumble, release layout, names, mapping page, follow; fork 0040-0041 | ~2 days |
| 4 | Language (European disc) | `recompcore-language.patch` | **done** (desktop): SRAM from the key, Language tab, restart | ~1 day |
| 5 | HD textures (+ import) | `recompcore-hd-textures`, `hd-textures-nested`, review D2 / D22 | **done** (desktop + Pocket 5): CLI import, menu row, live; async loads; fork 0042-0043, 0045 | ~2-3 days |
| 6 | 60 fps | `recompcore-frame-interpolation`, `battle-specials`, `field-speed`, review D6 / D25 | **done** (desktop + Pocket 5): record, blend, replay, retrace presents, budget, holds, menu; fork 0044; video thread (fork 0046) | 3-4 sessions |
| 7 | Dreamcast: Uncensored, Dreamcast music, DC streams, EU 60 Hz audio | `uncensor`, `skies-dcmusic-eu`, `dcmusic-streams`, `dcmusic-fx`, `pal60`, review D3 / D4 / D16-D21 | **done** (desktop + Pocket 5): dev's modules as they are, shim, `--import-dc`, menu tab | 1,200-1,800 lines of glue |
| 8 | Data folder, romhacks, save import / backup | `data-folder`, `romhack`, `state-folders`; `lib/ModernGekko/tools/*` | `--user`, `--dol` | 3-5 days |
| 9 | Android launcher (disc import, settings, Continue, saves) | `skies-android-app`, `android*`, review D7 / D8 / D11-D15 | adb pushes, args.txt | 1.5-2 weeks |
| 10 | Packaging and CI | `.github/workflows/release.yml`, `validate.yml` | hand-built test APK and exe | 2-3 days |

**Suggested order.** 0, 1, 2, 3 (small, player-facing, no new architecture), then 4, 5, then 6 (the big
one, with device checks), then 7, then 8-10 together (they're what makes it shippable). Each step ends with
its checks and a commit; device-heavy ones end with a head-to-head.

## 0. European and Japanese discs (done 2026-10-25, desktop)

- **Idle PC per disc** (`main.cpp` `DefaultIdlePc`, the values of `release/<disc ID>.ini`
  `StaticRecompIdlePC`: USA `0x80237BA8`, EU `0x8023B02C`, JP `0x8023772C`; a table, not read from the
  files, since the Android app has no `release/`). All three are the same `lwz r0,RunQueueBits / cmplwi /
  beq` loop in their DOLs.
- **The European disc runs as NTSC-U**, as in the Dolphin build (its `SConfig::SetPathsAndGameMetadata`
  hunk in `recompcore-skies-dcmusic-eu.patch`): `DiscRunsAsNtsc()` (`disc.cpp`) drives the boot TV format
  (`boot.cpp`: 0x800000CC, r1 / r2 / r13), the VI preset (`hw_vi.cpp`) and the card folder (`GC/USA`,
  `hw_memcard.cpp`). Game version differences (addresses in `menu.cpp`) still key on the disc ID's 4th
  letter. The intro script's 60 Hz fix is in the DOL (`pal60.c`, ws29+); the audio data's is item 7.
- **Module from the disc ID:** `--module` is optional; without it `g<disc ID>_recomp` is loaded (Android
  `libg<ID>_recomp.so` from the APK's libraries; desktop the one beside the executable, else the library
  path). `tools/android-standalone-build.sh` takes several modules in `MODULE` (space-separated) and names
  them `libg<ID>_recomp.so`.
- **Checked (WSL, headless and Aurora, 40 guest s from boot):** USA 1,178 frames (unchanged), EU 1,177 (it
  was PAL before: 50 Hz), JP 1,178; cards in `GC/USA`, `GC/USA`, `GC/JAP`; EU and JP pictures at frames 300
  and 1,150 right (Overworks logo, the opening sky). Idle PC worth ~30% of boot time uncapped: EU 11.2 ->
  7.8 s wall, JP 9.7 -> 6.3 s, same frame counts. Test inputs: EU `~/projects/skies-pal60/build/game/
  GEAP8P-ws30` + `build/ws30eu/module`; JP `GEAJ8P-ws34` there + this worktree's `build/ws34jp/module`
  (Linux module made for this check, gen script as `skies-pal60/build/ws30eu/build.sh`).
- **Checked on the Pocket 5 (2026-10-26 night):** a test app with all three modules (US sa26, EU and JP from
  `skies-pal60/build/android/jniLibs-ws34`) and the ws34 games in `files/game-eu` / `files/game-jp`: a cold boot
  with `zt1.txt` (in-app presses) plays the opening, the deck battle and the deck after it on both, 1.00x
  but for two load moments (0.93-0.96x on EU), no errors.
- **Not checked yet:** the EU intro against m01 (item 7).

## 1. Continue (auto-load of a card save) (done 2026-10-25, desktop)

- **What:** the launchers' Continue picks the newer of a save state and a card save; for a card save it
  sets `[Skies] AutoLoadCard = N` (1-99, one shot) and the game boots straight into the field in ~6 s (no
  logos, title or load screen).
- **How:** all the work is in the game patch (`origin/dev:patches/game/autoload.c`, hook at `0x801DC350`
  in `dl_title`, the game's own card functions). The frontend (`namespace autoload` in the Dolphin build's
  `SkiesMenu.cpp`: `Active`, `Drive`) writes the request block at `0x80002EF0`: `{'LOAD' 0x4C4F4144, file,
  status, error}`, **magic last** (a release fence, review D24), only after checking the hook word at
  `0x801DC350` is no longer the original `bl` (`0x4804C6D1`); then polls status (2 done, 3 failed; error 1
  no file, 2 read, 3 bad data), gives up when the title moves on (mode 3 with title state > 1, or mode 6) or
  after 30 s, with the same messages. Japanese: work size 0x274, file `EA_LEGEND.%03d` (ws34 handles it).
- **Done** (`menu.cpp` `namespace autoload`, as the Dolphin build's): `--continue N` (1-99) or
  `--continue card` (the newest save of this disc on card A by write time, `CardNewestSaveFile`, the
  launcher's `CardSaveFile` numbering), else `[Skies] AutoLoadCard` (one shot: set to 0 and saved as soon as
  it's read). The request goes in at the first display copy after the hook word is a `bl`
  (`MenuVideoFrame`, from `VideoFrameEnd`, headless too); same status polling, 30 s wall-clock limit and
  messages as the Dolphin build. While it runs the menu stays shut and the pad passes untouched.
  `--load-state` wins over a Continue and clears the key (as the launchers do). No in-game "Continue" row:
  Continue is a boot choice, the launcher's job (item 9).
- **Found on the way: the Japanese game couldn't mount its card at all.** The standalone's VI DTV status
  (0x6E) was always 0; the SDK's `OSGetFontEncode` reads its NTSC-J bit, so the game expected an ANSI card
  while the card header (`FormatHeader`, Japanese) says Shift-JIS, and every `CARDMount` failed. Bit 1 is
  now set for a 'J' disc, as Dolphin's `VideoInterface::Preset`.
- **Checked (WSL):** USA ws34 (`skies-pal60/build/game/GEAE8P-ws34`, Linux module
  `skies-perf/build/perf/base/module`): load done at 3.55 s guest, the field (the ship's save point) on
  screen at ~9 s; EU ws30 file 3 and JP ws34 file 3 (cards from `SkiesOfArcadiaLegends-Recomp/build/user-eu`,
  `skies-pal60/build/user10`) both at the hideout save point. Failure paths: empty card ("No memory card
  save to continue from"), missing file 5 ("wasn't found on card A"), the unpatched DOL ("can't load a save
  directly"). Key one shot (1 -> 0 after use); a state saved after a Continue loads with the key set and
  no Continue starts, key cleared.
- **Windows (2026-10-26 night):** cold `--continue card` from the ten-minute play's card (`build/sa-runs/user-lp2`,
  ws26 app game and module): auto-load done at 3.55 s guest, the save room, then the save point's menu
  (`lp-play.txt`'s first 18 s) wrote file #01 again; walking on (Encounters 400%) a battle started at ~48 s
  and ran (the walk's direction presses only moved the command cursor, so no round was fought). A longer run
  with the clock pinned took another route and met no encounter: a battle won after Continue is still open.
- **A battle won after Continue (Windows, 2026-10-27).** Cold `--continue card` from a fresh copy of
  `build/sa-runs/user-lp2` (Encounters 400%), ws26 app game and module, `SKIES_RTC=847000789`, `lp-play.txt`
  shifted by 9.5 s, 240 guest seconds, frames every 60: the save room, the save point's menu wrote file #01,
  the walk through the ship's corridors, a battle from ~150 s fought by the script's A presses, the victory
  pose and the results screen (frame ~7020), no errors. The Mac (Metal) did the same from a card save the
  same day (the Pirate Isle save point) and won the first battle from a Dolphin state.
- **Not checked yet:** RAM against a load from the game's own menu (needs scripted input), Continue on the
  Pocket 5.

## 2. Graphics options (done 2026-10-25, WSL; Windows / Pocket 5 checks open)

- **Done:** Graphics tab rows Anti-aliasing, Texture filtering, Anisotropic filtering, Scaling (between
  Resolution and V-Sync, as the Dolphin build), on the Dolphin build's keys and values: `GFX.ini [Settings]
  MSAA` (Dolphin writes it as `0x00000004`; `settings.cpp` now parses base 0 like Dolphin's `TryParse`),
  `[Enhancements] ForceTextureFiltering` (0 / 2 Smooth / 1 Sharp), `MaxAnisotropy` (-1 Off = the game's
  own, 1-4 = 2x-16x), `OutputResampling` (0 Default, 5 Sharp, 6 Smooth, 4 Bicubic). All four apply live
  from `MenuApplyGame` when the setting changes.
- **Fork 0038** (`~/projects/skies-aurora/GXRuntime` a3a6939, `patches/0038-*`; in `recompcore-aurora.patch`):
  `aurora_set_texture_filtering` / `aurora_set_max_anisotropy` apply Dolphin's
  `TextureCacheBase::GetSamplerState` rules in `TextureBind::get_descriptor` (samplers are built from it
  every draw, so live): forced filter on min / mag / mip; a forced anisotropy only where the game doesn't
  filter nearest both ways; "Off" = the game's GX 2x / 4x (before, Aurora's default `maxTextureAnisotropy`
  16 turned the game's 4x into 16x). `aurora_set_msaa(1|4)` is taken by the next frame buffer resize
  (both window builds let a pending change past their unchanged-size check). The present resampler gained
  `SAMPLER_SHARP_BILINEAR` and `SAMPLER_BICUBIC` (Catmull-Rom), Dolphin's maths in gamma space like the
  area sampler; the cubic family (2, 3, 4, 8) maps to Bicubic, nearest (7) to bilinear.
- **MSAA counts (2x / 8x, the Dolphin build's Off / 2x / 4x / 8x):** WebGPU allows 1 and 4 only and the
  prebuilt Dawn enforces it (its Vulkan backend maps only 1 and 4; Aurora runs Dawn with
  `skip_validation`, so nothing would stop an unsupported count before the driver). **Dawn patch**
  `patches/dawn/0001-extra-sample-counts.patch` (on `encounter/dawn` 266c1cf, Aurora's pinned ref; tree with
  local git at `~/projects/skies-aurora/dawn-src/dawn-266c1cf…`): 2 and 8 are valid where the adapter
  renders and resolves them on its colour and depth formats (Vulkan: framebuffer colour & depth sample
  counts, as Dolphin's `VulkanContext`; D3D12: multisample quality levels of RGBA8 / BGRA8 / D32 / D24S8;
  Metal: `supportsTextureSampleCount`; GL: colour / depth texture and `GL_MAX_SAMPLES` limits), plus
  `VulkanSampleCount` 2 / 8, and a native query `dawn::native::SupportsSampleCount(device, n)` with the
  macro `DAWN_NATIVE_EXTRA_SAMPLE_COUNTS`. **Fork 0039:** Aurora lists the device's counts at init
  (`aurora_get_msaa_modes`; 1 and 4 only without the macro, so the prebuilt is unchanged) and
  `aurora_set_msaa` rounds down to one; the menu row steps through that list, and a count from the Dolphin
  build the device lacks shows and runs as the next lower one (the key keeps its value until changed here).
  **Build:** `-DSKIES_DAWN_PROVIDER=vendor -DFETCHCONTENT_SOURCE_DIR_DAWN=<patched tree>` (default
  `package`, the prebuilt); on WSL also `-DDAWN_USE_X11=OFF` (no `libx11-xcb-dev`), which leaves Dawn's GL
  backend without a surface under Wayland, so test there with `DOL_AURORA_VULKAN_CPU=1` (lavapipe).
  Linux build dir `build/standalone-dawnsrc`. **Checked (WSL):** GL ES (Mesa) reports 1x / 2x / 4x / 8x,
  lavapipe Vulkan 1x / 4x / 8x; on lavapipe 8x switches live and renders (field, beam edge smoother than
  4x, 1,176 pixels differ, all on edges), 2x falls back to Off there. **Pocket 5 (2026-10-25):**
  `DAWN_SRC=<patched tree> tools/android-standalone-build.sh` (Dawn from source, `-DDAWN_BUILD_PROTOBUF=OFF`,
  into `build/standalone-android-dawnsrc` / `build/standalone-apk-dawnsrc`); the Adreno 650 reports **1x,
  2x, 4x** (no 8x in its hardware). Battle state (`bench-battle2.sst`, Native resolution, throttled), GPU
  per frame from `AURORA_GPU_PROF`: Off 4.31-4.33 ms, **2x 4.54-4.62 ms (+5%)**, 4x 5.60-5.68 ms (+30%);
  pictures differ from Off only on geometry edges (2,665 pixels at 2x, 3,635 at 4x), edges visibly
  smoother, no artefacts. The patched APK is left installed as the standalone test app (device config
  restored). **Windows (2026-10-25):** `C:\dev\SkiesStandalone\build-dawnsrc.bat configure|build` (MSVC,
  into `build-dawnsrc\`, the prebuilt `build\` untouched; Dawn tree copied to `C:\dev\dawn-src\dawn-266c1cf`
  with the dependencies' shallow `.git` folders so Dawn's fetch script skips them). Two traps: (1) without
  `DAWN_USE_BUILT_DXC` Dawn forces FXC and fails to open a bare `d3dcompiler_47.dll` (its
  `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` needs an absolute path, Windows error 87) and every backend falls to
  Null; the prebuilt builds DXC. Fix without building DXC: define `DAWN_USE_BUILT_DXC=1` in `CMAKE_CXX_FLAGS`
  (Dawn only loads `dxil.dll` / `dxcompiler.dll` at run time; `dxcapi.h` comes from the Windows SDK) and copy
  the prebuilt's DXC DLLs (same Dawn ref) beside the exe; (2) `webgpu_dawn.dll` lands in
  `_deps\dawn-build\` (the bat copies it). RTX 5080 D3D12 reports **1x / 2x / 4x / 8x**; battle state at 3x
  (1920x1440): each applies (`MSAA 1x -> Nx`), pixels changed from Off 8,062 / 11,284 / 11,839, edges
  visibly smoother at each step; GPU work in passes (span minus not-in-a-zone, one 300-frame window,
  throttled, rough) Off 0.25 ms, 2x 0.34, 4x 0.96, 8x 2.24. **Open:** macOS (Metal); shipping a patched Dawn
  (CI builds it, or our own prebuilt packages); D3D11 has no `SupportsExtraSampleCount` (stays 1 / 4).
- **Checked (WSL, Dawn on llvmpipe, field after a Continue, EFB 1x):** each option changes the picture
  (pixels off the baseline: MSAA 4,340, Sharp 35,492, Smooth 16,575, 16x anisotropy 14,793; two baseline
  runs differ by 442); by eye: MSAA smooths geometry edges, Sharp shows texels, Smooth / 16x soften. The
  three resampled outputs viewed through a local readback hack (not kept): area crisp, sharp bilinear =
  bilinear at a 1.5x stretch (Dolphin's formula: prescale floor 1), bicubic softest. No WebGPU errors in
  any mode. Android compiles.
- **The light-beam specks under 4x MSAA were llvmpipe's (closed 2026-10-27).** The fight route on Windows
  D3D12 (`SKIES_RTC=847000789`, `fight-full.txt`, `AURORA_SYNC_PIPELINES=1`, frames every 60, native
  resolution) with MSAA Off and 4x: both runs end on the same dispatch count, and the alarm beam's cone
  (frames 2160-2280) is the same inside; only its edge is smoother (1,130 pixels in the beam's area of
  frame 2220). A scan of all 91 frames for isolated pixels much brighter at 4x than Off and than their
  neighbours finds only geometry edges (door frames, armour, floor seams). The Pocket 5 showed none either
  (above).
- **MSAA at 4x / 6x on the RTX 5080 costs no speed (2026-10-27).** The fight route (as above, 185 s guest,
  `--unthrottled`, `SKIES_VIDEO_THREAD=0` so Aurora's log lines don't interleave with the host's, stderr
  apart from stdout, `AURORA_GPU_PROF=300`, `build-dawnsrc`) at EFB 4x and 6x with MSAA Off / 2x / 4x / 8x:
  all eight end on the same dispatch count, and the host's speed samples match within ±0.05x (1.0-2.4x
  along the route), so the CPU side sets the pace in every case. GPU work per frame (span minus not in a
  zone, 18 windows of 300 frames) stays under 5.1 ms in every window of every run; the means don't rise
  with the sample count (4x: Off 0.73, 2x 1.78, 4x 3.22, 8x 2.18 ms; 6x: Off 2.33, 2x 2.73, 4x 1.85,
  8x 1.88 ms) because the card idles between frames and its clock follows the load, so these times measure
  the clock more than the work. A cost figure would need a GPU the game saturates (the Intel UHD 770 via
  the low-power switch, fork 0030, or the Pocket 5, whose 2x / 4x figures are above).
- **Open:** menu changes by hand on Windows / the Pocket 5 (only the ini path was driven here).
- **Frame rate** belongs to item 6, **HD textures** to item 5.

## 3. Input: keyboard layout, rumble, names, Button mapping

Bindings in the Dolphin build live in `Config/GCPadNew.ini [GCPad1]` (`<controller terms> | <keyboard
terms>`), hotkeys in `[SkiesHotkeys]`, the dead zone and rumble in the pad profile (`Main Stick/Dead Zone`,
`Rumble/Motor`). Logic: `Core/SkiesBindings.{h,cpp}` (defaults, expression parsing, key names,
`DeviceLabel()`); glue: `Core/SkiesInput.cpp` (follow, setup, keyboard), the Controls tab in `SkiesMenu.cpp`.
The standalone reads pads through Aurora (`dol_platform_pad_read`, SDL; `aurora_input.cpp`), which keeps
its own mapping store (`PADSet*Mapping`, `PADSerializeMappings`), so **profiles are not shared** with the
Dolphin build; decide whether to translate `GCPadNew.ini` once on first run.

1. **Release keyboard layout** (WASD stick, Space / LShift = A / B, F / R = X / Y, Tab = Z, Q / E = L / R,
   Enter = Start, IJKL = C-stick, arrows = D-pad, Esc = menu) in place of Aurora's fallback in
   `aurora_backend_pad_init` (`PADSetKeyButtonBindings` / `PADSetKeyAxisBindings`), keeping "only if none
   persisted". Note Aurora binds E / R twice (button and trigger axis).
2. **Rumble:** the standalone's SI ignores the motor (`hw_si.cpp`: the 0x38 write latches only the poll
   mode; the 0x40 command's low bits are the motor, 0 stop / 1 rumble / 2 hard stop). Call
   `PADControlMotor(ch, M)` (Aurora: `aurora_backend_pad_control_motor`, `aurora_input.cpp:99`); the Rumble
   row = `PADSetRumbleIntensity` (0 = off); stop the motor when the menu opens and at exit. ~40 lines.
3. **Names:** port `DeviceLabel` (Steam's `#SettingsController_SteamController` -> "Steam Controller");
   "Controller: <name>" message from `PADGetName(0)`. Controller 1 following the last pad pressed (dev's
   `android-pad-follow`): check what SDL / Aurora do with two pads before porting anything.
4. **Button mapping tab** on Aurora's `PADGetNativeButtonPressed`, `PADGetNativeAxisPulled`,
   `PADBlockInput`, `PADSet*Mapping`, `PADRestoreDefaultMapping`. Rules to keep: a controller's A and B can't
   be cleared; separate resets for controller and keyboard; "No keyboard connected" without one (D23).
5. **Menu-close rule** (`recompcore-menu-close.patch`): after the menu closes only the buttons that acted in
   it wait for release; sticks and D-pad pass at once. Compare with the standalone's `MenuFilterPad`.
6. **Dead zone:** move onto `PADGetDeadZones(0)`; read the old `StandaloneDeadZone` once.
- **Check (as the Dolphin build):** WSL real XTEST key presses (D walks Vyse, Esc opens the menu, rebinding
  writes the store); Windows `SendInput` scan codes; the Pocket 5's built-in pad (needs the user: press the
  buttons), a Bluetooth pad, rumble felt by hand.

**Done (2026-10-25, late night; d950049, 1caa857, 6b9abe1; fork 0040 080ec53, 0041):**

- **Keyboard (step 1):** the release layout is Aurora's fallback (fork 0040; since 0041 the call
  `aurora_backend_pad_default_keys(port)`, which the keyboard reset uses too), still only when nothing was
  saved. Q / E are bound twice in Aurora's store on purpose: the trigger button and the full analog pull.
  Aurora's `user_path` is now `USER/Config/Aurora` (with `--user`): `keyboard_bindings.dat`,
  `<pad name>_<vid>_<pid>.controller`, `controller_ports.dat`. Its shader caches (`cache_path`) stay in SDL's
  per-user app folder. Profiles are **not** translated from `GCPadNew.ini` (decided against: two
  expression languages, and the Dolphin build's keyboard terms name devices the standalone doesn't have).
- **Rumble (step 2):** `hw_si.cpp` latches the motor bits (`out & 3`) with the mode at an SISR write with WR
  (as `SI_DeviceGCController::SendCommand`), and `SiPollDevices` sends a change to `VideoPadMotor` ->
  `dol_platform_pad_control_motor` -> `PADControlMotor` (Aurora: SDL rumble, or the phone's vibrator on port
  1 when the pad has none). Stop while the menu is open and with Controls -> Rumble Off (`[Skies]
  StandaloneRumble`, default 1; the Dolphin build's is the profile's `Rumble/Motor`, not shared). Not in save
  states: a loaded state starts still. The first rumble of a run is logged (`[si] controller 1: the game's
  first rumble`).
- **Names (step 3):** `input.cpp` `InputDeviceLabel` (= `DeviceLabel`); "Controller: <name>" when a pad takes
  port 1, at start too. **Follow:** Aurora puts pads on ports in connection order, so a second pad was
  GameCube port 2 (ignored by the game). Ported `Follow`: no pad on port 1 -> the first (or the pressed one);
  a button on another pad with controller 1 idle 750 ms -> `PADSetPortForIndex(i, 0)` (kept in
  `controller_ports.dat`); buttons only for the others, not during a capture. The pad list is logged when it
  changes.
- **Button mapping (step 4):** Controls -> Button mapping (first row), the Dolphin build's page and hints
  (`menu.cpp` `FilterMapping`, drawn in `Draw`), backed by `input.cpp` on Aurora's store: rows A, B, X, Y, Z,
  Start, L, R, Stick / C-stick up-down-left-right, D-pad; controller and keyboard columns; "Reset
  controller buttons", "Reset keyboard keys". A capture takes the next press of an input that was let go
  since it started (6 s timeout); Esc / Back / Guide cancel instead of binding (they're the menu keys), and the
  menu hotkey doesn't close the menu during a capture. Button rows take pad buttons only (Aurora's button
  mapping is button-to-button); stick rows an axis direction or a button; L / R an axis (the click comes from
  Aurora's emulated trigger) or a button (click and full pull). X clears (not the controller's A / B); no
  device -> "No controller connected" / "No keyboard connected" and nothing changes (D23). Saved at once
  (`PADSerializeMappings`). Labels: the face buttons as printed (SDL's button labels: Cross / Circle /
  Square / Triangle, or A / B / X / Y), PlayStation pads L1 / R1 / L2 / R2 / L3 / Options / Create, others LB /
  RB / LT / RT / LS / Start / Back; keys by SDL's names (Return shown as Enter). The hints name the keyboard's
  keys while the keyboard was used last. **Not done:** the Menu and Invincible hotkeys stay fixed (Esc / Back,
  L3 / R3); the Dolphin build lets them be rebound (`[SkiesHotkeys]`), Aurora's store has no slot for them.
  **Fork 0041:** Aurora averaged a stick's two directions (`(pos - neg) / 2`), so a direction bound to a button
  gave half deflection; now `max(0, pos) - max(0, neg)` (the default mapping reads the same); a cleared axis
  reads 0 instead of asserting.
- **Menu-close (step 5):** the rule was already the Dolphin build's (only the menu's own buttons and the
  triggers past 0xA0 wait for release); added its "closed 2 s ago, still waiting for release" log.
- **Dead zone (step 6), changed from the plan:** Aurora applies its own stick dead zone by default (8000 of
  32767, about 24%, a hard cut on each axis without rescaling), so "Stick dead zone: Off" wasn't off.
  `InputPoll` turns `useDeadzones` off on every pad (each read: a saved mapping loads it back), and the
  Controls row (radial and rescaled, as Dolphin's) stays the only dead zone, on its own key
  `StandaloneDeadZone` (the Dolphin build's is in its profile). Aurora's trigger activation zone (emulated
  L / R clicks) is unchanged.
- **Checked:** WSL real XTEST keys: D walks Vyse; Esc / arrows / Space work the menu; Rumble Off saved; the page
  draws with the keyboard column chosen and keyboard hints; A rebound to G (saved to `keyboard_bindings.dat`),
  B cleared, Esc cancels a capture without closing the menu, "Reset controller buttons" says no controller,
  "Reset keyboard keys" restores the layout, Stick right on G walks Vyse right, the binding survives a
  restart. Headless fight route: 32 motor changes from the encounter (~121 s) on, none with Rumble Off; the
  run is otherwise the same (frame 3601 at 121.45 s). Windows (DualSense Edge over USB): named on port 1, the
  only pad listed (the "XBOX Synthetic Gamepad Bus" device adds none); the page (opened by `SKIES_PRESS`)
  shows Cross / Circle / Square / Triangle / R1 / Options / L2 / R2 / Left stick up... Android arm64 builds
  (`build/standalone-apk-dawnsrc`, not installed).
- **Open (needs hands):** rumble felt on the DualSense and the Pocket 5 (the vibrator path), a capture with the
  pad (a button, a trigger, a stick direction), two pads switching, the Pocket 5's built-in pad and a
  Bluetooth pad with the page, the dead-zone change felt (no drift at rest with Off?).

## 4. Language (European disc)

- **What:** `[Skies] Language` 0-3 (English, Deutsch, Français, Español), EU only; a Language tab; a
  different value asks "Press A again to restart", the second A saves and restarts on the title screen
  (unsaved play lost); closing instead applies it next start. USA / JP show a one-line note. A save state
  keeps its language (the text pack is in guest RAM).
- **How (Dolphin):** `SConfig::GetLanguageAdjustedForRegion` feeds the game's boot-time `OSGetLanguage`
  (EU `0x801DE38C`); the restart is a host message plus the frontend re-exec (desktop runner without
  `--load-state`; Android relaunches the activity through `SkiesNative.takeRestartRequest`).
- **Port:** the standalone's IPL SRAM sets the language byte `s_sram[0x16] = 0` (`hw_exi.cpp`
  `InitSram`); set it from the key for GEAP8P and `FixSramChecksums()`. Confirm the recompiled OS reads SRAM
  (if `OSGetLanguage` is HLE'd, hook that). Restart: re-exec without `--load-state` on desktop; on Android
  finish and relaunch the activity (mind the lifecycle: the Dolphin build's black-screen history).
- **Check:** each language to the opening text, frames compared; one A then B saves without restarting.

**Done (2026-10-26, early):**

- `hw_exi.cpp` `InitSram`: the language byte (`s_sram[0x16]`, Dolphin's SRAM with the RTC word first) is
  `[Skies] Language` (0-3, clamped) for a 'P' disc, 0 otherwise; the checksums are fixed after it as before.
  The recompiled OS reads it through the EXI SRAM read at boot: no HLE.
- `menu.cpp`: a Language tab between Gameplay and Sound (the Dolphin build's order) with the Dolphin build's
  row: four languages on the European disc; "English" / "Other languages need the European disc." on the
  USA one, "Japanese" / "The Japanese disc is in Japanese only." on the Japanese one; a changed value: A
  asks "Press <A> again to restart" (gold), the second A closes the menu (saving the key) and asks for a
  restart; moving, a tab change or Left / Right drop the question; B instead keeps it for the next start.
- Restart (`main.cpp` `Restart`): `MenuTakeRestartRequest` ends the run as a quit does; after the shutdown the
  standalone execs itself with the same arguments less `--load-state` / `--continue` (Linux: the real path
  of `/proc/self/exe`, since exec'ing the link names the process "exe"; Windows `_execv` with each argument
  quoted for the C runtime, since it joins them unquoted; macOS `argv[0]`). `SKIES_PRESS(_FILE)` are cleared
  for the new run (a test script would otherwise change the language again). **Android (2026-10-26):**
  `Restart` writes `restart.flag` in the files folder and calls `SkiesActivity.restartGame` (JNI);
  `SkiesRestartActivity`, in its own process (`:restart`), ends the game's process and starts `SkiesActivity`
  in a new one (SDL runs the native main once per process); `AndroidSetup` takes the flag away and leaves out
  `--load-state` / `--continue` and `SKIES_PRESS(_FILE)` for that start. Checked on the Pocket 5 with the
  European disc (ws34 game in `files/game-eu`, its module bundled beside the US one): Language -> Deutsch, A, A
  by `SKIES_PRESS`; the system log shows the game's process ending at 12.7 s, `:restart` starting, a new game
  process 0.1 s later and no second restart; frame 750 of the new run is the German opening text. A restart
  that drops a `--load-state`: checked on WSL 2026-10-26 (`linux-eu-1017.sav`, Language -> Deutsch, A, A
  by `SKIES_PRESS` 3 s after the load; the new run boots at 0 s with no state load and shows German at frame
  750). Android, Pocket 5 2026-10-26 night: the same state and presses in the test app, one restart (the game's
  process ends ~10 s in, `:restart`, a new process 0.1 s later), the new run boots at 0 s with no state load,
  `restart.flag` consumed, frame 750 German.
- **Checked:** WSL, European disc ws30 (`skies-pal60/build/game/GEAP8P-ws30`, `build/ws30eu/module`), frame 750
  (the opening text) with Language 0 / 1 / 2 / 3: English, German, French, Spanish; in the window: Language ->
  Deutsch, A, A: "restart for the language", the new process (named skies-standalone) shows German; one A then
  B: Français saved, no restart. Windows (`C:\dev\SkiesLang` game + module, user folder `user lang` with a
  space, the menu driven by `SKIES_PRESS`): restart, German at frame 750, the presses not replayed. Save
  states keep their language (SRAM is in the EXI section and the text pack in RAM).

## 5. HD textures

- **What:** an imported pack (.7z, .zip, folder, nested archives; never bundled) in
  `USER/Load/Textures/GEA/**/tex1_*.{png,dds}` with the marker `skies-texture-pack.txt`; an import adds
  (same name replaces); Remove deletes all; `GFX.ini [Settings] HiresTextures`, Graphics > "HD textures"
  (Off with a hint when nothing is imported).
- **Aurora already has Dolphin's loader:** `graphics/aurora/lib/gfx/texture_replacement.cpp`,
  `include/aurora/texture.hpp` (`load_replacement_directory(root, options)`, `reload_replacement_directory`);
  it parses `tex1_WxH[_m]_<hash>[_<tlut>]_<fmt>.dds|png` with `_mipN` sidecars, DDS BC1 / BC3 / BC7 and PNG.
  Its key (`build_source_key`) follows Dolphin's `TextureInfo::CalculateTextureName`
  (`vendor/dolphin/Source/Core/VideoCommon/TextureInfo.cpp`): XXH64 of the base level, the TLUT hash over the
  used palette range, the numeric GX format. Compare hit counts against the Dolphin build on the same state
  (its reference: 149 of the 745-texture pack in use at the first battle) in case old packs hash differently.
  EFB-copy textures never match in either build.
- **Port:** fork: export `aurora_backend_load_texture_dir(path)` (and an unload); call it at video init when
  `HiresTextures` is on, toggle live from the menu. Import: lift `VideoCommon/TexturePackImport.{h,cpp}`
  (Dolphin-free apart from `File::`; LZMA SDK 7z with `Skies7zPrefix.h`'s renamed symbols, minizip-ng, the
  24 GB cap, zip-slip rejection, rename-in-before-delete: review D2, D22) as a CLI `--import-texture-pack`
  and later the Android picker. BC7: Adreno 650's stock driver drew noise (the Dolphin build disables BPTC
  there via `DriverDetails`); check Dawn's `TextureCompressionBC` on the Pocket 5 and skip BC7 there.
- **Check (as the Dolphin build):** CLI import `diff -r` against the pack (745 textures; the 4x pack
  15,497); first-battle faces change and toggle live; the Pocket 5 at full speed with the PNG pack (the
  Dolphin build: 1.003x); GPU and memory cost head-to-head.

**Done (2026-10-26, desktop):**

- **Import** (`standalone/src/texture_pack.{h,cpp}`): the Dolphin build's `TexturePackImport.{h,cpp}` from dev
  (with review D2 / D22), word for word below its includes, so fixes there copy over; `texture_pack_file.h`
  stands in for the Dolphin `Common` helpers it calls (`File::` on `std::filesystem` with UTF-8 paths,
  `IOFile` with 64-bit seeks, `ScopeGuard`, `UTF16ToUTF8`, `DIR_SEP`). `.zip` through Dolphin's minizip-ng
  (`Externals/minizip-ng`, on Aurora's zlib), `.7z` through `lib/lzma-sdk` (as `skies7z`; no renamed symbols
  needed, no mGBA here). CLI: `skies-standalone --user USERDIR --import-texture-pack PACK` /
  `--remove-texture-packs` (imports or removes, prints the summary and the BC7 count, exits). The Android
  picker belongs to item 9.
- **Loading** (`standalone/src/hd_textures.cpp`): `GFX.ini [Settings] HiresTextures` with something imported
  registers `USER/Load/Textures/GEA` (and the disc ID's folder, as Dolphin) with Aurora's texture replacement
  (`aurora::texture::load_replacement_directory`); off unregisters them. `MenuApplyGame` applies it every
  frame (live), the pack's presence read at the first frame and at each menu open. Every 600 frames the log
  says how many files have loaded, failed, GPU MB, and the decode / upload times.
- **Menu:** Graphics > HD textures after Scaling (the Dolphin build's row: Off with "Needs a texture pack you
  downloaded: import it in the launcher's Settings, Graphics tab." when nothing is imported; the user folder
  is shared, so the Dolphin build's launcher import works for the standalone too), plus a note naming the
  pack's BC7 count when the GPU can't draw BC7.
- **Fork 0042:** Aurora's name parser took at most six `_` parts, so Dolphin's longest names
  (`tex1_WxH_m_<hash>_<tlut>_<fmt>_arb`) were rejected: 5 of the v0.06 pack's palette textures never loaded
  (now 744 of 745 register; the 745th is the pack's stray duplicate `..._0(1).png`, which Dolphin skips too).
  BC7 off on Qualcomm's own Android driver (vendor 0x5143, driver string without Mesa / turnip), as the
  Dolphin build's `BUG_BROKEN_BPTC`. `replacement_stats()`, `bc7_supported()`.
- **Fork 0043, files decoded on loader threads** (as Dolphin's `CustomAssetLoader`; one or two threads): the
  first lookup queues the file and the game's own texture draws meanwhile, tagged in the static texture
  caches with the count of finished decodes; a cache hit looks again once a decode has finished, keeping the
  same game texture while it waits (no second conversion). Windows fight route (RTX): the render thread spent
  478 ms in all on the PNG pack (longest 89.5 ms, a visible hitch), 167 ms (58.7) on the DDS pack; now 43 ms
  (11.6) and 19 ms (6.7) of uploads, the decodes (349 / 111 ms) off it. `AURORA_SYNC_REPLACEMENTS=1` loads on
  the render thread at first use (frame-exact runs: with async loads a replacement shows a frame or a few
  later, like Dolphin's).
- **Checked:** PNG `.7z` on WSL: 745 textures in 3.5 s, `diff -r` against the unpacked `GEA` identical (only
  six empty placeholder folders aren't made); DDS `.7z` on Windows read from `\\wsl.localhost`: identical, 535
  BC7. The 4x pack's 6.5 GB `06_SoAL_HD_Unsorted_Assets.7z` opens (its header lies past 4 GB) and unpacks
  (stopped after 980 of 9,829 files, 15 s; not imported in full, it would add 8.7 GB). Fight route
  (`fight-full.txt`, plain USA DOL): WSL and Windows D3D12, 89-91 of 744 files loaded by the first battle,
  none failed; the battle intro's moon and the skies replaced (frames compared on and off); the menu turned
  it off and on mid-run (the game's moon back, then the pack's). Windows speed unchanged: 5.28x on, 5.24x off
  (uncapped means of 36 samples). GPU memory at the first battle: PNG 256 MB, DDS 107 MB. The Android APK
  builds with it (`DAWN_SRC=... tools/android-standalone-build.sh`, not installed).
- **Not compared:** the Dolphin build's "149 of 745 across 9 states" came from Dolphin states the standalone
  can't load; the fight route reaches 89-91. Both hash the same way (no misses among the files the route
  uses, and the replaced textures are the ones expected), so the counts differ by coverage.
- **Open:** a side-by-side with the Dolphin build of the cloud sea in the first battle (a few straight
  cloud-quad edges show with the pack: possibly the pack's own); the full 4x pack (15,497 textures, about 4 GB
  of GPU memory if all were used; Aurora's replacement cache evicts past 4 GB).

**Pocket 5 (2026-10-26):** the PNG pack imported on WSL and pushed into both test apps
(`user-bench-sa/Load/Textures/GEA`, the bench app's `Load/Textures/GEA`), `HiresTextures = True`, IR 1,
30 fps, muted, lowest performance mode, `capture/android-ir-bench.sh`; memory from `dumpsys meminfo` and
kgsl's `gpumem_mapped` 40 s in.

- **A hitch found and fixed (fork 0045):** the battle state's load window had 6-8 intervals over 40 ms with
  the pack (one without it: the load itself); the emulation thread took up to 58-67 ms a field, copies came
  111 ms late at p99. The loader threads start at the first lookup, which comes from the emulation thread
  (Aurora parses there), and threads inherit their creator's affinity, so both loaders ran on cpu7 with the
  pinned game (a thread sample showed them there). Now a loader started by a thread kept on some cores runs
  on the other cores (Linux / Android; desktop unpinned, unchanged). After it, two runs: one interval over
  40 ms (the load), the game thread at most 32 ms a field, copies 12.4 ms late at p99; the decodes took
  286 ms in all instead of 657 (longest 30 instead of 69). logcat (tag `aurora`) says `loader threads off
  the 1 core(s) of the thread that started them`.
- **Head-to-head with the pack:**

  | | Standalone | Dolphin bench app |
  |---|---|---|
  | Battle, full speed: p99 / max ms | 34.4 / 36.3 (2.2 W) | 38.0 / 39.1 (2.4 W) |
  | Save point, full speed | 34.8 / 36.4 (2.5 W) | 36.3 / 37.4 (2.7 W) |
  | Uncapped, save point / battle | 1.65x / 1.97-2.03x | 1.37x / 1.63x |
  | PSS, battle (pack off) | 604 MB (601) | 1,118 MB (1,038) |
  | PSS / kgsl mapped, save point | 596 / 534 MB | 1,162 / 2,091 MB |

  Speed is the same as without the pack in both apps. In use: 45 of 744 files at the battle (64 MB), 20 at
  the save point (25 MB), none failed.
- **BC7 (the DDS pack):** `HD textures on: 744 files registered (...), BC7 skipped on this GPU`; at the
  battle 25 files loaded and 20 BC7 files refused (`unsupported format: 62` in logcat), the game's own
  textures drawn for those; one device screenshot, no noise; pacing as with the PNG pack.
- The packs were removed from both test apps afterwards and `HiresTextures` set back to False.

## 6. 60 fps (frame interpolation)

**The Dolphin build** (`VideoCommon/FrameInterpolator.{h,cpp}`, 2,028 lines; this branch's vendored copy is
current: `lib/ModernGekko/vendor/dolphin/Source/Core/VideoCommon/FrameInterpolator.cpp`; Aurora glue in
`VideoBackends/Aurora/AuroraBackend.cpp`): records each game frame's GX stream (display lists expanded,
indexed XF loads turned immediate, the vertex-array bytes each indexed draw reads), ends it at the XFB copy
(BP 0x52), and builds an in-between by averaging XF matrices / projections (paired by address and order, keyed
by vat, count, size, texture 0) and **float vertex positions** (Ninja transforms on the CPU, so positions
matter most). `Align` (a banded DP) pairs alike draws; `Blendable` / `VertexJumps` snap values that jump
(camera cuts, the menu's fade quad: more than half the value or 50 units); a frame with too few paired loads
isn't interpolated. The blend is replayed on a second EFB with its own state; while replaying, draw-done,
token, clear-bbox, `LOADTLUT1` and `PRELOAD_MODE` are skipped (the cloud-sea garbage fix). Key `[Skies]
FrameMode` (-1 default, 0 = 30, 1 = 60), live at an XFB copy (`RequestEnabled` / `ApplyPendingEnabled`).
Held off while the game runs fast (field speed > 1x; battle speed except during spells / specials, read from
the game patch's `qol_normal_speed` at `0x80002BDC`, ws28; resumes only if it was on when the hold began).

**Already there for the standalone:** the Aurora fork's replay context (patches 0019, 0022, 0024, 0025):
`aurora_backend_replay_context(enter)` (drains the FIFO, swaps the GX state and the spare framebuffer),
`aurora_backend_keep_picture(slot)` / `aurora_backend_present_kept(slot)` (`aurora_graphics.cpp` ~895-926),
the host-pointer array command (opcode 0x50, sub `0x0010+attr`), and 0036's scheduled presents.
`standalone/src/dff.cpp` shadows BP / CP / XF / TMEM per frame and `DffReloadAurora()` re-feeds them.

**The standalone is simpler:** no GPU thread, one FIFO in order. After the frame's XFB copy: keep the real
picture, build the blend, `replay_context(1)`, feed the start-state registers (a `DffReloadAurora` variant from
the frame's start snapshot), feed the blended stream with host-pointer arrays from an arena that lives until
Aurora has parsed it, keep the in-between picture, `replay_context(0)`. No slicing, no state swap.

1. `standalone/src/interp60.{h,cpp}`: lift the ~900 lines of pure logic (`Frame` / `Load` / `Prim`, `Sort`,
   `Blendable`, `VertexJumps`, `Align`, `BuildBlendedStream`, the `Shown` ring, `Budget` / `ReplayFits`).
   Feed it from `hw_gpu.cpp` (`Apply`, `ProcessCommands`; expand 0x40 display lists inline, since Aurora's
   guest-address call would see changed RAM), with vat / vcd layout from the standalone's own tracking. Drop
   Dolphin's texture-cache machinery (`HeldTexture`, `AddReplayCopy`, ...): the replay context has its own.
2. Presents: two per game frame. With 60 fps on, the in-between at the first field after the copy and the
   real picture one field later (the Dolphin dual-core order and latency), each through
   `aurora_backend_set_present_time(wall(guest field time) + delay)` + `aurora_backend_present_kept(slot)`
   from `VideoRetrace` (or a small queue of guest times). Keep `SchedulePresent`'s adaptive delay;
   `[present-pacing]`'s thresholds become 20 / 25 ms when 60 fps is on.
3. Menu: Frame rate (Graphics), the field-speed and battle-specials hold (`MenuApplyGame`), live switch at a
   frame end.
4. States: on a load, drop the interpolator's frames and kept pictures; the first replay after a load
   re-feeds the replay context's registers (fork 0024's fix); nothing interpolator-side goes in a state.
5. Budget: the replay feed costs the emulation thread (Windows ~3.5 ms per game frame with dereference);
   keep the adaptive budget on the throttle's clock, maybe only replay when `EmulationSpeed()` allows.

**Risks:** replayed TLUT / TMEM-preload / texture loads re-reading RAM that changed since the game's load
(Aurora parses on the render worker; check the replay context skips or protects them as fork 0019's own TLUT
slots suggest); tokens and draw-done in the replay stream (they must not reach the guest); cost on the
single emulation thread on weak CPUs.

**Checks (as the Dolphin build):** frame dumps (`SKIES_DUMP_DIR`, view with `-alpha off`, Windows for exact
frames): the opening cutscene ~298 of 300 interpolated; the in-between differs only on moving things; the
replay issues the same draw count as the live frame; `[present-pacing]` p50 16.67 ms, none over 20 on the
Pocket 5; full speed with FrameMode=1 on Windows, the Nova and the Pocket 5 (ship deck state); scenes: title
clouds, menu close (fade quad), the Valuan ladder (green bands), cloud sea (TLUT / preload), world-map
flight, battle at speed 2-4 with spells (hold and resume), a scene cut, a state load while replaying, Home and
back on Android. Then the head-to-head against the Dolphin build's 60 fps on the Pocket 5.

Size: ~1,800-2,200 lines (1,400 in `interp60.cpp`); 3-4 sessions (record + blend with dumps; replay +
presents; menu + holds + states; devices).

**Done on desktop (2026-10-26):** commit e88be2c, fork 0044.

- **Correction to the plan:** dev's `review-d` patch changes `FrameInterpolator.cpp` (D6 / D25: the test
  switch for a failed second EFB, `ReadEnabled` once, Shutdown deferred), so the vendored copy is not
  current; none of it touches the blend, and the standalone has no second EFB to fail or GPU thread.
- **Recording** (`standalone/src/interp60.cpp`, `InterpCommand` from `hw_gpu.cpp` `Apply`, every command,
  display lists walked): the stream without the `0x40` calls; indexed XF loads (0x20-0x38) become
  immediate loads of the RAM read then; each draw's key (vat, count, size, texture unit 0's image from
  `DffBp()[0x94]`), its direct positions, and copies of the array ranges it indexes (the layout decoded
  from the CP's VCD / VAT as `hw_gpu.cpp` `VertexSize` does; the array base kept to put back).
- **Blend:** the Dolphin build's `BuildBlendedStream`, `Align`, `Blendable`, `VertexJumps` (trace lines
  dropped). `SKIES_INTERP_T` moves the blend (0 gives the previous frame: used to prove the replay draws
  the blend).
- **Replay** at the frame's XFB copy, before the copy goes to Aurora: `aurora_backend_keep_picture(real)`,
  then the replay context, the registers the frame started with (`DffSnapshot` at each XFB copy,
  `DffRegisterCommands` = `DffReloadAurora` without the display copy), the blended stream with each
  indexed draw's arrays as host pointers (an arena, reset per replay; leaving the context drains), the
  in-between kept just before the stream's own XFB copy, whose clear then readies the spare EFB.
- **Presents** (`video.cpp`): with 60 fps on, `VideoRetrace` presents: the first retrace after the copy the
  in-between, the next the real one. The Dolphin build waits for the VI to show the frame's XFB; here
  that left about 15 fields in 20 s without a picture (the game flips a field late now and then), so it
  counts from the copy, as the 30 fps presents do. Slots: real `frame % 4 * 2`, in-between + 1.
- **Budget** (the Dolphin build's single-core `ReplayFits`): work = wall time less the throttle's sleep
  (`ThrottleIdle`) and the time in Aurora's present calls (`VideoPresentTime`: they wait for the render
  worker's scheduled presents; counting them as work turned replays off on a fast PC); off past 95% of
  the frame, back on after 30 frames under 88%; meanwhile frames present as at 30 fps.
  `SKIES_INTERP_ADAPTIVE=0` replays always.
- **Holds** (`menu.cpp` `qol::Write`): off while the game runs fast (field speed, battle speed but for
  spells / specials: `qol_normal_speed` 0x80002BDC, ws28). **Menu:** Graphics > Frame rate after HUD, the
  Dolphin build's row and texts, `[Skies] FrameMode`. Not ported: the Dolphin build's "slow for 20 s"
  note that opens the menu on this row (the budget keeps full speed instead).
- **Fork 0044:** the replay context's TLUT slots get ids of their own (`0xC0000000 | offset`): the TLUT
  texture caches are shared and each context counts its own versions, so one context could get the
  other's palette; `aurora_backend_in_replay_context`; `aurora_backend_synchronize` (frame dumps:
  without it a readback came from a present still queued, and dumps didn't match their frames).
- **Test switches:** `SKIES_INTERP=0/1` (overrides the key), `SKIES_INTERP_TRACE=1` (frame ends and
  retrace presents), `SKIES_INTERP_STATS=1` (counts and ms per frame), dumps write `frame_N-interp.ppm`
  beside `frame_N.ppm`.
- **Checked (Windows D3D12, RTX):** six real frames of the opening pixel-identical with 60 fps on and off
  (`SKIES_RTC`, `AURORA_SYNC_PIPELINES=1`); in-betweens halfway (ship and gull, airship, Aika's map hand,
  Aika, Fina, the searchlights, the Valuan ship after a state load); 595 of the opening's first 600
  frames interpolated; the fight route in real time: p50 16.7 ms, 0-2 intervals over 25 ms in most
  10 s windows (5-12 around loads and the battle transition, where the game itself takes a third
  field), nothing skipped after the boot logos; uncapped 3.6x with it, 5.0x without. Per frame: keep
  0.3-1.0 ms, blend 0.1-0.8, feed 0.2-1.7 (Aurora parses the replay on the emulation thread). WSL
  runs aren't deterministic (off vs off differs), so exact comparisons are Windows'.
- **Open:** the Nova; the scene list above (title clouds, the game's menu fade, the Valuan ladder,
  cloud sea, world-map flight, battle at speed with spells for the hold).

**Pocket 5 (2026-10-26):** the APK of e88be2c (+ fork 0045) installed over the test app; IR 1, muted,
lowest performance mode, `[Skies] FrameMode = 1` in both apps (`capture/android-ir-bench.sh`). The 30 fps
baseline on this build is unchanged (battle p99 34.4, max 35.0).

| 60 fps, full speed | Standalone | Dolphin bench app (60 fps) |
|---|---|---|
| Battle: p50 / p99 / max ms; over 20 ms | 16.6 / 17.7 / 18.5; none | 17.3 / 24.5 / 25.5; 80 |
| Save point | 16.7 / 17.7-18.0 / 19.4-20.0; none | 18.5 / 25.9 / 26.7; 251 (18 over 25) |
| Power, battle / save point | ~2.45 / ~3.1 W | ~2.9 / ~3.8 W |
| Uncapped, save point (30 fps) | **1.25x** (1.65x) | **1.41x** (1.38x) |
| Uncapped, battle (30 fps) | 1.66x (~2.0x) | 1.69x (1.63x) |

- **At full speed it is better in every number:** even 60 fps where the Dolphin build has 13-42% of its
  intervals over 20 ms, at 0.45-0.7 W less. 1197 of 1200 battle frames interpolated, none skipped. The
  budget switched replays off once, right after the save point's state load (31.2 ms of work in a frame,
  shader compiles), and on again after 30 frames (32 frames at 30 fps).
- **Uncapped, the lead is gone:** the Dolphin build interpolates on its GPU thread, so its speed doesn't
  move; the standalone does it on the emulation thread. Save point uncapped: a game frame takes 26.7 ms
  instead of 20.2; the stats (`SKIES_INTERP_STATS`) account for 3.9 ms of the 6.5 (keep 0.5, blend 1.1,
  feed 2.1, keep in-between 0.2), the rest is outside them (the recording in `InterpCommand`, the second
  present, Aurora's parse of the replay where it isn't in "feed"). The save point at 1.25x still has 25%
  headroom at IR 1, but less than the Dolphin build's 41%: against the performance goal, the next step is
  the one planned: Aurora's FIFO parse (and the replay feed) on a thread of their own, then measure the
  recording.
- **Present delay:** with 60 fps the auto delay settles at 27.5-30 ms (the 30 ms cap) instead of 14-19: it
  is now set from each retrace's lateness, and the retrace inside the heavy drawing field is late. Pacing
  holds anyway (nothing over 20 ms), so the cost is latency; worth a look with the parse thread.

**The video thread (2026-10-26, a4f4014, fork 0046):** Aurora (the parse, the replay, the presents) on the
main thread, the game on its own; details in *HANDOFF-emulator-free.md*, *The video thread*. The game waits
at the frame's tokens only for commands that read guest RAM; the replay goes after the tokens; on Android
the video thread polls while the game has no time to spare (its cluster's clock). Same Pocket 5 runs:

| | Before | Video thread | Dolphin bench app (60 fps) |
|---|---|---|---|
| Uncapped save point, 30 / 60 fps | 1.65x / 1.25x | **1.77x / 1.70x** | 1.38x / 1.41x |
| Uncapped battle, 30 / 60 fps | ~2.0x / 1.66x | **2.15x / 2.05x** | 1.63x / 1.69x |
| 60 fps full speed, battle p99 / max | 17.7 / 18.5 | 17.6-17.8 / 18.0-18.7 | 24.5 / 25.5 |
| 60 fps full speed, save point p99 / max | 17.7-18.0 / 19.4-20.0 | 17.7-17.9 / 18.3-18.7 | 25.9 / 26.7 |
| 60 fps present delay | 27.5-30 ms | 17.6-22.9 ms | |
| Power at 60 fps, battle / save point | ~2.45 / ~3.1 W | ~2.54 / ~2.87 W | ~2.9 / ~3.8 W |

Ahead of the Dolphin build again in every column, 60 fps costing 4-5% of the uncapped speed (25% before).
Home and back on Android checked with it (one 5.8 s interval, then normal). Windows uncapped: save point
60 fps 2.07x -> 2.95x, battle 2.81x -> 3.84x; the fight route's 24 dumped pictures byte-identical to
e88be2c's in both modes and after a state save + load.

## 7. Dreamcast features and disc audio

Two standalone hook points carry all of it:

- **Disc reads:** `standalone/src/disc.cpp` serves `DiscRead` from per-file extents built from `sys/fst.bin`
  (via `hw_di.cpp`). This is where Dolphin's `DirectoryBlob` hooks sit, so one shared
  `Redirect(path, offset_in_file)` chain goes in at the file open / read, **latched per file at offset 0**
  (review D20), for Uncensored, the DC streams and pal60.
- **Audio out:** the DSP HLE mix goes through `hw_dsp.cpp AudioDmaTick` -> `AudioPushSamples` (`video.cpp`,
  32 kHz stereo, the menu volume applied, then `dol_platform_audio_push`). The Dreamcast music mixes in here.

**First, a Dolphin-free library** of the logic (most of it is std-only already, apart from `File::`,
`Config` and `OSD` shims): `DiscIO/SkiesUncensorConvert.{h,cpp}` and `SkiesUncensor.cpp`
(Prepare / Describe / Remove / Redirect), `AudioCommon/DCMusicImport.cpp` (GDI / CHD readers with libchdr),
`AudioCommon/DCMusic.{h,cpp}` (+ `dcsound` from lib/HighlyTheoretical), `Core/SkiesMusic.cpp`,
`SkiesStreams.cpp` with `SkiesPal60Data.inc`. Settings through `settings.cpp`.

- **Uncensored** (`[Skies] Uncensored`, Dreamcast tab; ~70 model / texture files per game swapped, e.g.
  Dyne's cigar, Vigoro's armour, Gilder's smoke pose): the user's Eternal Arcadia (JP Dreamcast) discs 1 and 2
  (GDI or CHD) imported to `<user>/DreamcastUncensor/dc/disc1|disc2/`, converted files in
  `<user>/DreamcastUncensor/<game ID>/`, manifest `Data/Sys/Skies/uncensor-manifest.json`. Port: the library,
  a CLI `--import-dc DISC [--user DIR]` (import + Prepare), `Register` per FST entry and `Redirect` in
  `disc.cpp` reading the setting live, a Dreamcast tab with `Describe`'s status. **Same-size rule:** a
  served file must keep its size (and not decompress smaller), or state loads corrupt code. Checks:
  `tools/uncensor/convert_test.cpp` on the lifted library (216 / 216 byte-exact); served bytes against
  `uncensor/manifest/` (dump `DiscRead` over an FST file); Vigoro's scene (`vigoro.sh`, state
  `jp-14-vigoro-scene.sav` is a Dolphin state, so recreate it with a poke or a standalone state).
- **Dreamcast music** (`[Skies] DreamcastMusic`, `DreamcastIntro` default 1): the DC disc's
  `BONITO.DRV`, `ARESNEW.MLT`, `SOUND/M*.MLT` (+ `M01S` / `M03S` streams) in `<user>/DreamcastMusic/`.
  `DCMusic` runs two AICA `Engine`s (Manatee commands, fx 0x84 / 0x82, gain 358, 44.1 kHz, resampled in
  `Mix`); `SkiesMusic` reads the game patch's event ring in guest RAM (`patches/game/dcmusic.c`; box
  addresses per region in `patches/game/*.addresses.json`; USA / EU tables `0x8031A890` / `0x8031DE50`)
  and drives it. Port: build `dcsound` + `DCMusic` + `SkiesMusic` with a `host::Ram` shim; `SkiesMusic::Poll`
  once per frame; mix the AICA output at `AudioPushSamples` before the volume, rendered on the audio side
  (Dolphin mixes in its audio thread, independent of emulation speed); `OnStateLoaded` after a standalone
  state load; menu rows. Checks: a `SKIES_WAV` dump (deterministic runs make a scripted battle repeatable),
  then listening on Windows; CPU on the Pocket 5 (two Engines; the Dolphin build held 0.994-0.999x with fx).
- **DC streamed songs** (m01 intro, m03): `SkiesStreams` converts the DC recordings once into GameCube
  DSP-ADPCM files of the same header and size, cached in `DreamcastMusic/STREAMS/`, served by the redirect;
  a background worker started at boot and joined at exit (D21). Check: the cache files' MD5 against the
  Dolphin build's (identical on Linux, Windows, macOS).
- **European disc at 60 Hz** (no setting): item 0's NTSC override, the DOL's `pal60.c` (ws29+), and
  `BuildPal60` (WSOLA-compressed `.dsp` streams and tempo-patched `.info` banks, same sizes, cached in
  `Cache/SkiesPal60/`, ~20 s on first run) served by the redirect. Checks: offline envelope fit (scale
  0.99-1.0), the intro frame-by-frame with `cs_shot` against the USA disc (ratio 0.993-1.005), by ear.

Size: ~2,500 lines moved, 1,200-1,800 lines of glue. Risks: audio-thread timing under the standalone's
scheduler; the right song after a state load; first-run conversion time on weak Android hardware; the AICA
engines' CPU cost against the performance goal (measure on the Pocket 5).

**Done (2026-10-26, commit 9862a64; desktop + Pocket 5):**

- **The modules as they are on dev** (`standalone/src/dreamcast/`: `DiscIO/SkiesUncensor*`,
  `DiscIO/SkiesStreams.{h,cpp}` + `SkiesPal60Data.inc`, `AudioCommon/DCMusic*`, `AudioCommon/DCMusicImport*`,
  `Core/SkiesMusic*`; byte-identical to the patched tree, so dev's fixes copy over with `cp`). They include
  Dolphin's headers, which `standalone/src/dolphin_shim/` provides at the same paths with only what they
  use (`File::`, logs, `Common::ToLower`, SHA-1, `Config::SKIES_*` on the Dolphin build's keys,
  `MemoryManager` on guest RAM, `OSD::AddMessage` to the menu's messages, `SConfig::GetGameID`,
  `Core::GetState`), over `dreamcast_host.h`; only the `skies-dreamcast` library sees that folder. The
  glue is `dreamcast.cpp`. To refresh from dev: apply dev's series to a scratch worktree of the vendored
  tree (`build/devtree`, this session's recipe) and copy the files over.
- **Disc** (`disc.cpp`): every file registered at open (`SkiesStreams::Register`, `SkiesUncensor::Register`,
  DirectoryBlob's order), each read through both redirects (latched at offset 0 in the modules), the
  disc's own file when a served copy can't be read. Open streams are now cached by path.
- **Import:** `skies-standalone --user U --import-dc IMAGE [--game DIR] [--sys DIR]` (GDI or CHD via libchdr;
  the extra uncensor files from the manifest; then `Prepare` for `--game`, Android's `SkiesImport.cpp`
  order). `--sys` holds `Skies/uncensor-manifest.json` (default: the source tree's `standalone/data/Sys`, a
  copy of dev's; the Android test app gets `$FILES/sys` pushed and `--sys $FILES/sys` in args.txt).
- **Music:** `SkiesMusic::Poll` at each pad poll (`hw_si.cpp`, the game thread, as Dolphin's CPU thread),
  `OnStateLoaded` after a load (`state.cpp`). The AICA renders in real time in an SDL audio stream of its
  own (`SDL_OpenAudioDeviceStream` with a callback, 48 kHz, the Volume setting applied; SDL mixes it with
  Aurora's stream), silent while the game's thread doesn't advance (`dchost::GameRunning`: a field
  heartbeat from `Throttle`, 250 ms). `SKIES_DC_MUSIC_SYNC=1` mixes it into the game's audio in guest
  time instead (deterministic; in `SKIES_WAV` dumps), `=2` renders it nowhere (the game's audio with its
  music muted). Menu: the Dreamcast tab (Music, Uncensored; the Dolphin build's keys and texts).
- **Build:** Highly Theoretical's `dcsound` and libchdr as dev builds them; libchdr on the standalone's zstd
  (`cmake/Findzstd.cmake`: Aurora's zstd also builds the `zstd` program, which took the target name); the
  7z decoder gets dev's `Skies7zPrefix.h` names (libchdr's LZMA decoder clashed); `IOFile` gains `Open`
  and `operator bool`.
- **Checked:** both Eternal Arcadia GDIs imported in ~5 s: the 123 music files byte-identical to the Dolphin
  build's import (`C:\dev\SkiesUncensor\app-cli\User`), its 70 uncensored files identical and 3 more from
  the newer manifest (73 of 73); `SKIES_DISC_DUMP=DIR:REL,...` (a test switch) shows the converted bytes
  served with Uncensored on and the disc's own with it off (four files). The converted m01 / m03 streams
  identical to the Dolphin build's (`C:\dev\SkiesUncensor\app\User\DreamcastMusic\STREAMS`). The
  European ws29 game (`C:\dev\SkiesPal60\GEAP8P-ws29`): "280 sound banks and 223 streams ready" ~20 s in,
  all 726 files identical to the Dolphin build's cache. Music (Windows, ws26 battle state): the battle
  theme (M0458) starts, the GameCube music fades out over ~1.5 s after the state load (SYNC=2), levels as
  the Dolphin build's choice; the real-time stream renders the same profile; after a mid-run state load
  the song starts again. Pocket 5: Uncensored converted on the device in the background and served
  (identical to WSL's); the music at full speed:

  | Full speed, music on (Volume 0, real audio driver) | Standalone | Dolphin bench app |
  |---|---|---|
  | Battle: p99 / max | 34.3 / 37.3 ms (off: 33.9 / 34.6) | 37.7 / 38.0 |
  | Battle power | ~2.2 W | ~2.7 W |
  | Save point: p99 / max (3 runs) | 35.9-36.6 / 37.5-39.0 (off: 34.3 / 35.1) | 36.2-36.7 / 37.2-37.6 |
  | Save point power | 2.9-3.0 W | 3.0-3.3 W |

  The AICA thread (SDL's device thread) takes ~10-20% of a gold core. Ahead in battle; level with the
  Dolphin build at the save point, where the music costs the standalone ~2 ms of p99 (the game thread's
  `host-busy` p99 30 -> 32 ms: contention from the chips beside it) in the lowest performance mode.
- **Found on the way:** with SDL's `dummy` audio driver and two streams the device drained the game's
  stream slowly and Aurora's push wait (queue > 250 ms) stalled the game (40% late frames); the real
  driver doesn't. Moving SDL's device thread to the little cores (a tried `SKIES_PIN_AUDIO`) did the same:
  the device thread must keep up, or Aurora's push blocks the game. With the game's audio on, uncapped runs
  are held at 1.00x by that same queue (benches stay muted). At the save point in the lowest mode the
  video thread's polling flips on and off (the game sleeps 12-18%, the threshold is 15%).
- **Home and back with the music on** (Pocket 5): in the background neither the game's thread nor the
  audio thread takes measurable CPU (the music stops with the game), both resume on return.
- **Open:** the m03 scene and the intro with the Dreamcast recordings in game; the European intro timing
  (`cs_shot`, ratio 0.993-1.005) in the standalone; the Android picker import (item 9); the save point's
  music cost. (A romhack whose file sizes differ: done 2026-10-26, item 8.)

## Items 8-10: the standalone replaces the Dolphin build (decided 2026-10-26)

**The user's decisions:** "The goal is for standalone to replace dolphin", and "ideally users would download
this updated version and be able to continue on like any other update." So the standalone is not a runtime
option beside Dolphin: the existing launchers start it as the game's runtime, and an update over an
existing install must carry on with the same data folder, imported game, settings, saves and imports.

**Desktop approach (started, nothing committed yet):** the standalone takes the runner's place
(`skies-run.exe` in the release package) and keeps its command-line contract, so dev's launcher starts it
unchanged. Dev's launcher (`tools/moderngekko_launcher.cpp` ~3700) runs `SiblingRunner(argv[0])` (the
runner beside the launcher) with `--game GAME --user-dir USER [--load-state FILE] [--wayland | -X11]`
(netplay flags are dropped: no netplay). The runner (`tools/moderngekko_run.cpp` `RunMain`) does, before
the runtime, what the standalone must do too:

1. `FindUserDirectory(executable_directory)` when no `--user-dir` (portable.txt, data-folder.txt, the
   default `Skies of Arcadia Legends` folder); a box when a moved data folder is missing.
2. `default-game.txt` when no `--game`; `config.ini` (`frontend_config` `LoadConfig`: resolution, which it
   sets every boot and writes back when the menu changes it; fullscreen; FPS in the title).
3. The release table (`release_games.inc` from CMake lists `MODERNGEKKO_REQUIRED_DISC_ID`,
   `..._DOL_SHA256`, `..._DOL_PATCH_MANIFEST`; dev's `tools/build-release-win.bat` fills them from
   `release/skies-widescreen*.dolpatch`): `ApplyDolPatchManifest` patches the game's `sys/main.dol` in place
   ("The game needs updating" box on failure, exit `RUNNER_EXIT_EXPLAINED`), romhack notes,
   `RomhackTag` / `StateFolderFor` / `MoveDiscStates` for the state folder, `DolMatchesPin`.
4. The module beside the program or in `USER/StaticRecompModules` ("Game files missing" box).
5. Restart for a language change (the standalone has its own).

`lib/ModernGekko/tools/{user_directory,dol_patch,frontend_config}.cpp`, `release_games.hpp` and
`include/moderngekko/sha256.hpp` are identical to dev's on this branch and std-only: the standalone can
compile them directly (only `moderngekko_launcher.cpp` is newer on dev). Dev's tools as patched:
`build/devmg` (a worktree of the ModernGekko submodule with dev's three `moderngekko-*.patch`); dev's
patched vendored tree: `build/devtree` (item 7's).

**Test plan:** a copy of a release package (e.g. `C:\dev\SkiesPerf\build\SkiesOfArcadiaLegends-v0.83.0-perftest1-windows`:
the launcher `Skies of Arcadia Legends.exe`, the runner `skies-run.exe`, `gGEAE8P_recomp.dll`, `patches/`,
`Sys/`) with `skies-run.exe` replaced by the standalone (plus Dawn's DLLs, libpng / zlib); the launcher
started as players do, on an existing data folder.

**What doesn't carry over yet (the seamless update):**
- **Dolphin save states** (`state-skies-*.sav`, `recovery-skies-*.sav`): the standalone can't load them.
  The launcher's Continue, state list, Import a save and Back up saves know only those; they must learn the
  standalone's `.sst` (and Continue fall back to the newest card save when the newest state is Dolphin's),
  or the old states be converted. **Decided (the user, 2026-10-26): "Converter before release".** The
  release that swaps the runtime waits for a `.sav` -> `.sst` converter, checked against the Dolphin
  build's own load of the same state. Dolphin's state has RAM, ARAM, the hardware registers and the EFB
  copies too (`TextureCacheBase::DoState` stores them as textures), so nothing is missing in principle.
- **Controller bindings:** Dolphin's `Config/GCPadNew.ini` (the launcher's Controls page writes it); the
  standalone uses Aurora's bindings in `Config/Aurora`. Convert on first start, or have both read the same.
- Card saves, `Dolphin.ini` / `GFX.ini` settings, the imported game and module, HD packs and the Dreamcast
  imports carry over as they are (shared formats and folders).

**Done so far (2026-10-26, all local commits):**
- **Runner contract (6e06979).** `standalone/src/frontend.cpp` does skies-run's pre-launch part with the
  runner's own std-only code (`lib/ModernGekko/tools/{user_directory,dol_patch,frontend_config}.cpp`,
  `release_games.hpp`): the launchers' command line (`--game G --user-dir U [--load-state F] [-X11|--wayland]
  [--title T]`, or no arguments), data folder, `default-game.txt`, config.ini (resolution every start and
  written back by the menu, full screen, FPS in the title "T | 29.9 FPS", first-start 60 fps), DOL patch +
  pin (CMake `SKIES_RELEASE_DISC_IDS` + `SKIES_RELEASE_DOL_PATCHES`; the pin is each patch's output-sha256),
  romhack / other-version state folders, module beside the program or in StaticRecompModules, `Sys/`
  (dsp_coef, Dreamcast manifests), the runner's boxes and exit codes. The test form (`--game G [--user U]`)
  skips it. Checked on WSL and on Windows in a copy of the v0.83.0-perftest1 package with skies-run.exe
  swapped (`build/sa-runs/pkg-win`, portable, `skies-run-dolphin.exe` kept for A/B).
- **States under the Dolphin build's names and folders:** `StateSaves[/GEAP8P|/GEAJ8P|/romhack-*]/
  state-skies-slotN.sav`, `recovery-skies-<date>-<time>.sav`, header v2 starting with the game ID (the
  launcher's MoveDiscStates reads it; an old Dolphin build refuses the file on its cookie). So the
  launcher's list, Continue and Android's Back up saves need no change. A state of another DOL / module
  build now loads (as the Dolphin build).
- **Controller bindings:** GCPadNew.ini is the source of truth (dev's `Core/SkiesBindings.cpp` verbatim in
  `standalone/src/skies_bindings/`): keyboard applied at the first poll, a controller's when it takes port 1,
  the mapping page writes back. Keyboard checked on WSL. 2d80f3f: controller 1 starts on the profile's
  `Device` (`SDL/<n>/<name>`, what the launcher's Controls page writes) when it's connected, and port 1's
  device goes back into the file when it changes (the Dolphin build's SwitchTo); checked on Windows with a
  DualSense Edge (bindings applied, `Device` written, nothing else in the file changed). **Still to do by
  hand: remapping with a pad, and two pads (the profile's one must win at start).**
- **Converter (f0c2131, fork 0047).** `dolphin_state/dolphin_state_reader.cpp` walks the Dolphin stream
  (state 191; every marker checked, must end on the last byte; all 10 test states pass), `dolphin_state.cpp`
  applies it, each part has a `*FromDolphin`. Results: Windows room state matches the Dolphin build's frames
  at ~45 dB; audio bit-exact for 11.7 s after loading a battle state; battle, ship deck, Antonio intro/fight,
  JP cabin, EU and US intro states all play. Fork 0047 `aurora_backend_copy_restore_rgba` rebuilds the
  Dolphin build's EFB copies.
- **Two fixes found by it:** (1) 0a762c3: after every load the DOL's code goes back over a state's older code
  (the Dolphin build's restore-boot-dol): older-patch Retroid states were black. (2) c27b402: the Dolphin
  build lays extracted games out with DirectoryBlob (other file offsets than sys/fst.bin); after every load
  files are served at the offsets of the game's FST in RAM. Without it any disc read after a converted load
  read the wrong file (intro states wrote through garbage pointers ~10 s in).
- **Converted states on WSL and the Pocket 5 (2026-10-26 evening).** WSL (Aurora on Mesa's GL ES, the
  US ws32 / EU ws30 / JP ws34 DOLs and Linux modules): all 10 test states plus both intro states and four
  of the Dolphin build's bench states (`05-save-point`, `10-worldmap-android`, `17-ship-battle-baltor`,
  `20-corridor-battle`), 30 guest seconds each: every load restored the DOL's code and remapped the files
  (5551 US / 5608 EU / 5547 JP), Dolphin EFB copies rebuilt where a state had them (1-2), no errors,
  pictures right. Pocket 5 (test app rebuilt with the converter and installed over the old one, data
  kept; ws26 DOL): the two Retroid states, both Retroid bench states (18, 19), a Windows, a Linux, the
  save-point and the US intro state, 40 s each: all 1.00x, pictures right. WSL runs 0.45x in some scenes
  (Alfonso's throne room): GL ES on llvmpipe, the same after loading the standalone's own re-save, so
  not the converter (headless 2.56x there).
- **Ten-minute play (Windows, 2026-10-26 evening).** From the Dolphin build's `05-save-point.sav`: the save
  point's menu, a card save to file #01, then 600 guest seconds of walking and pressing A (encounters
  normal by mistake: the user folder went in as a relative path): out of the save room through the
  battleship's corridors, battles with Vyse and Aika from about six minutes in (attacks, a counter, a
  spell), a battle won after it with the results screen. 1.00x throughout, no errors. A second run kept in
  the save room overwrote file #01 113 times over ten minutes (every one "Saving completed", game time
  0:13 to 0:22), and a cold boot's `--continue card` came up in the save room from that card.
- **Launcher (fb58a2b, `patches/moderngekko-standalone-launcher.patch`, after dev's three moderngekko
  patches).** CMake `MODERNGEKKO_STANDALONE_RUNNER`: no Graphics backend row (Aurora picks D3D12 / Vulkan /
  Metal itself, GL ES as the fallback; the standalone ignores config.ini's `backend=`); Import a save also
  takes the standalone's own states ("SKST" at 24, version 2, unpacked size, one zstd frame to the end
  of the file); without the option they're refused as another version's. `tools/save-import-test.py`
  takes a standalone state as a fifth argument (8 more cases); a Linux build of dev's launcher with the
  option (`build/devmg-build`: `build/devmg` with `vendor/dolphin` linked to `build/devtree/rc`, whose
  empty third-party folders and DolRecomp are linked to this branch's copies) passes 30 of 30. The hidden
  row is compile-time only, not looked at in the window. Android's import (item 9) needs the same define.
- **Graphics APIs (2026-10-26).** Aurora tries D3D12, Metal, Vulkan, D3D11, OpenGL ES, then Dawn's Null (draws
  nothing); a build has what its Dawn was built with: Windows D3D12 / Vulkan / D3D11 (no GL ES), Linux and
  Android Vulkan / GL ES, macOS Metal. On Windows Vulkan and D3D11 never started before fork 0049: Dawn loads
  `vulkan-1.dll` and `d3dcompiler_47.dll` by full path only (its bare-name try fails with error 87 under
  `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR`); Aurora now gives it System32. Checked on the RTX 5080 with
  `DOL_AURORA_BACKEND=vulkan|d3d11`: the same picture as D3D12 (over 100 dB). If only Null starts, the standalone
  says so in a dialog ("No graphics support") and exits instead of running behind a black window
  (`DOL_AURORA_BACKEND=null` shows it). Not checked: a PC where D3D12 really fails, D3D11's speed on a weak GPU.
- **Windows Sandbox (2026-10-26; `C:\dev\SkiesStandalone\sandbox`, `skies-sandbox-sa.wsb` + `in\test.ps1`; GPU-PV
  shares the RTX 5080 and the UHD 770).** Smart App Control is on there and blocks the unsigned exe (the script
  turns it off; backlog). D3D11 forced: Pirate Isle drawn right, 0.84x. D3D12: the sandbox's kernel crashes
  (bug check 0x3B) while Dawn enumerates D3D12 adapters, before any device: a Windows GPU-PV bug, nothing to
  change here. Vulkan: Dawn took the Intel adapter, whose driver there lacks `VK_KHR_swapchain`, and its swap chain
  called the null `vkCreateSwapchainKHR` (minidump: execute at 0 from `webgpu_dawn.dll`, the call named by
  Dawn's "CreateSwapChain" string); `patches/dawn/0002` makes the surface query fail instead, checked in the
  sandbox (Aurora logs "Failed to get surface capabilities" and moves on). Only in a source-built Dawn: the
  Windows build still takes the prebuilt package (shipping the patched Dawn is item 2's leftover). Logs: stdout
  is unbuffered now (main.cpp), so a crash keeps Aurora's last lines. No debugger is installed on Windows:
  `build/sa-runs/tools/mdump.py DUMP` reads a minidump's exception (module + offset) and scans the stack.
- **No console window on Windows (2026-10-26).** The sandbox showed one beside the game (the release runner was
  made a GUI program on 2026-10-13 for the same reason). The standalone is a GUI program now (`WIN32_EXECUTABLE`,
  entered at `main` through `/ENTRY:mainCRTStartup`; `SKIES_STANDALONE_CONSOLE=ON` keeps the console). Output a
  parent redirects still arrives (the test scripts); started without it (the launcher, a double click) the log
  goes to `USER/Logs/skies-standalone.log`, both streams unbuffered. Checked on Windows both ways; the
  swapped package (`build/sa-runs/pkg-win`) has this build as `skies-run.exe`.
- **macOS (2026-10-27, the Mac session, Apple M4, macOS 26.3, Command Line Tools only).** 723ec15 builds unchanged
  (cmake + Ninja, Dawn's darwin-arm64 prebuilt; 12 harmless warnings) and runs on Metal ("Backend: Metal,
  Device: Apple M4"). libpng links dynamically (`@rpath/libpng16.16.dylib` in the build's `_deps`): the
  package must bundle it or link it static; the rest is system frameworks. Boot to the title and the
  Cupil intro right (EFB dumps), 1.00x throttled. Unthrottled, 1x, muted, same DOL / module / Dolphin
  `.sav` states on both sides: opening cutscene 3.13x vs the Dolphin build's 3.01x, first battle 5.25x vs
  4.57x. `--load-state` of a Dolphin `.sav` converts and plays (code sections restored, 5551 files
  remapped, 75-85 ms). Pairing trap there: an older ws DOL in the imported game against a newer app's
  module ran 5417 of 5461 chunks native (the rest interpreted) until the DOL matched.
  Later the same day (scripted input, log and ini checks, no source changes): the menu (Z+Start, Graphics
  rows written to GFX.ini and applied live; 60 fps 598 of 600 frames interpolated at 1.00x); MSAA on the
  prebuilt Dawn is 1x / 4x only, and the M4's Metal itself offers 1 / 2 / 4 (no 8x), so a patched Dawn
  adds only 2x there; save states (slot save 71 ms, load 78 ms, auto-saves every 8 s with 3 kept); the
  Language restart on the EU disc (re-exec with the same argv); the standalone as `Contents/MacOS/skies-run`
  in a copy of the installed .app, started as the launcher does (data from `Contents/Resources`, config.ini
  and GCPadNew.ini picked up). Packaging: a build without `SKIES_RELEASE_*` never patches the DOL; with the
  app's patches it did (31e439…). An older imported DOL got "The game needs updating" (correct; a headless
  test waits on the box). No overlay screenshot (no Screen Recording permission there). Left for the user:
  the menu by eye, the DualSense (play, rumble, mapping, name) and keyboard, sound against the picture by
  ear, the launcher's Play in the scratch app copy.
- **WSL only: textures flicker in some runs under GL ES (Mesa's d3d12 driver), not a player issue.** On WSL
  Dawn skips the llvmpipe Vulkan adapter (a CPU one) and runs OpenGL ES on Mesa's d3d12 gallium driver
  (GL translated to D3D12 on the RTX 5080; llvmpipe GL itself crashes compiling Aurora's first pipeline).
  There Pirate Isle (`retroid-us-1012-slot1.sav`, or the standalone's own state from it) draws a wrong
  texture on a few frames (a house, a wall, the garden, a rug, gold), different frames each run, in most
  runs. RAM and ARAM come out byte-identical in good and bad runs. Not the video thread, the parse lag, the
  per-frame hash memo or llvmpipe's threads. Identical, clean frames every run on: Windows D3D12 (6 runs),
  the Pocket 5's Vulkan (3), the Pocket 5's **OpenGL ES** on Adreno (3, `DOL_AURORA_BACKEND=gles`, fork
  0048, 1.00x) and WSL's Vulkan on llvmpipe (4, `DOL_AURORA_VULKAN_CPU=1`, about as fast as the d3d12 GL
  path here). So Dawn's GL ES backend is fine; the d3d12 GL driver is the cause. Use
  `DOL_AURORA_VULKAN_CPU=1` for WSL runs that are compared frame by frame (`build/sa-runs/glflick.sh TAG
  RUNS [ENV=VAL]` counts the distinct versions of 19 frames across runs).
- **Pitfall:** a cold first run skips draws while pipelines compile (the shadow pass: picture 3x too bright
  for a second): compare with `AURORA_SYNC_PIPELINES=1`.
- Tools: `build/sa-runs/sa-win.sh` (Windows run from a state, guest time read from the state);
  `win-play.sh` (the same with a press script in times after the state, `SAVEAT=`, `AUTOSAVE=`, unthrottled),
  `wsl-conv.sh` / `wsl-sweep.sh` (WSL runs per region), `dev-conv.sh` (a Pocket 5 run from a state pushed to
  `files/conv/`, log and frames pulled); `lp-play.txt` / `lp-play2.txt` (the long plays' scripts: the
  save-point menu is RIGHT x4, DOWN x3, LEFT, then A; A on Slot A, A on the file, A on Yes);
  `SKIES_GP_DUMP=FRAME:PATH`, `SKIES_DI_TRACE=1`; the Dolphin build saves states on request through
  `--automation-dir D` (`D/commands/001.txt`: `command=save_state` / `path=x.sav`; `D/status.txt` has
  frame_count) and dumps frames / audio with `[Movie] DumpFrames`, `[Settings] DumpFramesAsImages`,
  `[DSP] DumpAudio` in the user dir's ini files.

**Next for items 8-10:** by hand (the user): remapping with a pad and two pads; Continue in the launcher
with the swapped package (`build/sa-runs/pkg-win`, `skies-run.exe` = the standalone as of 2d80f3f,
`skies-run-dolphin.exe` kept). macOS frontend check. Then item 9 (Android shell) and 10 (packaging: the
standalone in build-release-win.bat etc. with `SKIES_RELEASE_*`, Dawn DLLs, Sys, and the launcher built
with `moderngekko-standalone-launcher.patch` and `MODERNGEKKO_STANDALONE_RUNNER=ON`). Both wait for the
merge the user put on hold (next paragraph).

**Items 9 and 10 need dev's current code on this branch** (2026-10-26): `aurora-backend` is 79 commits ahead
of origin/dev and 40 behind (merge base a7aa0f8, before ws31 / v0.82). The launcher patch is made against dev's
launcher and doesn't apply to this branch's; dev's Android launcher and release scripts have moved on too. A
trial merge of origin/dev (in a throwaway worktree, deleted after) conflicts only in the patch lists
(`patches/README.md`, `recompcore.series`, `tools/apply-patches.sh`, `tools/check-patches.sh`: keep both,
dev's patches first and `aurora` last), and on dev's stack `recompcore-aurora.patch` misses one hunk (the
one line `s_placeholder.reset();` in `FrameInterpolator.cpp`'s `Shutdown`, beside review D6's change);
`moderngekko-standalone-launcher.patch` applies on top of dev's three moderngekko patches plus
`moderngekko-aurora.patch`. Merging or rebasing is the user's call: asked 2026-10-26, the answer was
**hold** (no merge, no rebase); don't ask again unless something changes.

**Android (item 9):** the same goal: dev's app (Kotlin launcher screens + JNI in Dolphin's native library)
must start the standalone's game activity (`libmain.so`, SDL) instead of Dolphin's emulation, with its
imports and settings JNI built against the standalone's code (item 7's modules show the pattern).

## 8. Data folder, romhacks, saves

Most of this is already Dolphin-free in `lib/ModernGekko/tools/` (std-only or `moderngekko/sha256`):
`dol_patch.{hpp,cpp}` (`ApplyDolPatchManifest`, `DolMatchesPin`, `RomhackTag`, `StateDirectory`,
`MoveDiscStates`), `user_directory.{hpp,cpp}`, `release_games.hpp`. Still Dolphin-bound:
`disc_extract.cpp` (DiscIO) and `save_import.cpp` (`GCMemcard`, `SavestateLayout`, `StateFile`).

- **Data folder** (`data-folder.txt`, `portable.txt`; Android `SkiesDataFolder.kt` with All files access and
  the JNI `checkDataMove` / `moveDataFolder` / `pointDataFolder`): desktop calls `FindUserDirectory` at
  startup and passes the result as `--user`; Android reuses the Kotlin and JNI as they are.
- **Romhacks** (data-only `.dolpatch` on the pinned code; code-changing hacks refused; `RomhackTag`
  separates states): call `DolMatchesPin` / `RomhackTag` in the launchers. `disc.cpp` with a romhack whose
  file sizes differ: checked 2026-10-26 on WSL with Maeson 1.51 (`C:\dev\SkiesRomhack\discs\maeson.iso`
  extracted to `build/game-maeson151` by a small GCM extractor; 292 of its 5552 files differ in size from
  the US disc's, the battle data among them): a cold boot with the ws32 DOL (the module's code; the hack's
  own DOL kept in `sys/orig`) and `zt1.txt` plays the opening into the first deck battle like the US game,
  331 disc reads, no errors. **The release path (2026-10-27, WSL):** a headless build pinned like a release
  (`build/standalone-rel`: `-DSKIES_RELEASE_DISC_IDS=GEAE8P -DSKIES_RELEASE_DOL_PATCHES=` skies-pal60's ws32
  `release/skies-widescreen.dolpatch`), packaged as `build/sa-runs/rel-test/pkg` (program, ws32 module,
  `patches/`), started as the launcher does (`--game G --user-dir U`) on the hack's own DOL: "Applied native
  DOL patches", "modified game data (a romhack)", 5461 of 5461 chunks, states in
  `StateSaves/romhack-4d5febec` (the frontend now logs the state folder), and with `zt1.txt` the same run as
  the hand-patched one (frames and disc reads equal at 150 s, the first deck battle at 380 s). A second
  start leaves the patched DOL alone. `codehack.iso`'s DOL is refused (code changed) with "The game needs
  updating" and exit 20, as the Dolphin build's runner; the launcher refuses such hacks at import.
- **States:** the standalone keeps `USER/StateSaves/Standalone/<disc ID>-slotN.sst` / `-auto-*.sst`; Continue
  picks the newest whose header matches the disc ID and DOL hash; Back up saves must include
  `StateSaves/Standalone/*.sst` (today's Kotlin takes only `StateSaves/*.sav` and cards).
- **Import a save:** link `save_import.cpp` with the vendored `GCMemcard` for card saves and card images; add
  a `.sst` header validator; Dolphin `.sav` states can't be imported (say so). Japanese saves are
  `EA_LEGEND.*`, the others `SA_LEGENDS.*`.
- **Check:** `tools/save-import-test.py`'s 16 cases against the standalone's import; a move of the data
  folder on Linux, Windows and the Pocket 5 (as *HANDOFF-datadir.md*).
- **Finding the data folder, checked (2026-10-27, WSL, `build/sa-runs/rel-test`, no arguments, a scratch
  `XDG_DATA_HOME`):** the default folder; `data-folder.txt` there pointing at a moved one (card, states and
  config.ini all in the moved folder); the moved folder gone ("Data folder not found", exit 1, no empty
  folder made); `portable.txt` beside the program wins (`<program>/User`). Each start read its
  `default-game.txt`. The move itself is the launchers' (item 9 / dev's launcher), not run here.

## 9. Android launcher

Today's shipping app is Dolphin's APK with `-PskiesBrand`: `SkiesLauncherActivity` (~720 lines: home menu,
disc import, Continue, game in progress, picker, driver guard), `SkiesSettingsPanel` (~520), `SkiesDataFolder`,
`SkiesDriverGuard`, `SkiesNative.kt`, JNI `SkiesImport.cpp`, and Dolphin's `EmulationActivity` for the game.
Reusable as is: the launcher layout and pad-friendly dialogs, `SkiesDataFolder`, Back up saves (zip), the
picker flow (incl. the process-death case, D11). Dolphin-bound: config through `NativeConfig` /
`Settings.FILE_DOLPHIN`, `SkiesDriverGuard` (`GpuDriverHelper`; Aurora uses Dawn, so it goes), the
`EmulationActivity` running checks, the pad-profile rows (`GCPadNew.ini`).

**Recommendation: a minimal new shell forked from the launcher**, not libmain.so inside Dolphin's app:

1. Disc import: `DiscIO::CreateVolume` (ISO, GCM, RVZ, WIA, GCZ, CISO) compiled from `vendor/dolphin` into a
   small `libskiesimport.so` with `disc_extract.cpp`, then `PatchDol` for the region. The heavy part (DiscIO's
   dependencies: mbedtls, lzma, bz2, zstd, fmt).
2. Settings: a small ini helper instead of `NativeConfig` (same files and keys as `settings.cpp`); only the
   rows the standalone has; no backend / driver / pad-profile rows.
3. The game: `SkiesActivity` in `android:process=":game"`; the launcher writes `args.txt` (`--game`,
   `--module`, `--user`, `--load-state` / `--continue`) before `startActivity`; `AndroidSetup` already reads
   it. A separate process makes Exit Game clean and "game in progress" a process check.
4. Modules: bundle all three (`libgGEAE8P_recomp.so`, `libgGEAP8P_recomp.so`, `libgGEAJ8P_recomp.so`), pick
   by the imported disc.

**Behaviours to match** (dev's `recompcore-android*.patch`, `HANDOFF-android.md`):

- **Orientation:** `USER_LANDSCAPE` (both landscapes, honouring the rotation lock, #12). Already so once the
  window is up: Aurora's SDL hint is `LandscapeLeft LandscapeRight`, and SDLActivity's `setOrientationBis`
  then requests `USER_LANDSCAPE` (the Pocket 5's log: `requestedOrientation=11`). The manifest said
  `sensorLandscape` for the moment before the window; it says `userLandscape` now (not yet installed).
- **Lifecycle:** done so far are immersive mode from the start and Home / back at the same surface size (the
  EFB is kept).
  - Open: pause then Home then return (the Dolphin build's black-screen history), process kill and restore,
    "Game in progress" after a task-clearing relaunch, and Exit Game relaunch.
  - Audio must pause with the app.
  - **A size change on return (rotation, split screen) goes through Aurora's resize**: with Resolution on
    Auto (the default, the EFB follows the window) a new size rebuilds the EFB and clears the EFB copy
    cache (`resize_swapchain_internal`); a fixed scale keeps the EFB's size (the aspect fit undoes the
    window's shape). Checked 2026-10-26 with `SKIES_RESIZE_AT=SECONDS:WxH[,...]` (the window resized at
    those guest times): the world map, Pirate Isle (5 live copies), the romhack and the 1022 state show
    nothing lost, frame by frame on Windows (the only difference is the scaling, 2.6% RMSE, no spike at
    the resize): the game remakes those copies every frame. **A copy kept across frames** (the battle
    start's screen grab, spun and broken up for about a second) **went black when a resize caught it**:
    Aurora cleared every EFB copy on a new size. Fork 0050 keeps the copies (sampled by normalised
    coordinates; the padded copies scale by their own size) and drops only the reuse cache keyed by the
    old size, so the next copy to each address replaces its texture. Checked 2026-10-26 from
    `05-save-point.sav` with Encounters 400%, a box walk and `SKIES_RTC=846000000` (the encounter roll
    follows the clock; pinned, two runs match frame for frame), resized to 1920x1080 mid-spin: before,
    the spin frames were black (5-23% RMSE against the run without a resize); after, 0.2-1.2% on WSL
    (Vulkan on llvmpipe) and 0.2-0.9% on Windows D3D12, the scaling's level (the battle frames after it
    1-1.6%); the run without a resize byte-identical to the old build. 40 resizes through the field
    and the transition: every frame right, peak memory 1.30 GB against 1.24 GB without (bigger final
    windows), so a dragged window edge doesn't pile up copies. Recheck on the Pocket 5 with split
    screen once the shell exists.
- **Pads:** controller 1 follows the last pad pressed, Bluetooth over the built-in one, resting Android axes
  ignored. Check what SDL does with two pads. The built-in pad is untested by hand.

Size: ~1.5-2 weeks (launcher and settings mostly reuse; DiscIO for Android 2-4 days; lifecycle and device
checks 3-5 days).

## 10. Packaging and CI

- `release.yml` already checks one module per region in every package and refuses anything game-shaped;
  `validate.yml` generates the modules in a codegen matrix (`tools/codegen-targets.txt`, PGO profiles
  `PROFILE.profdata`, `-eu`, `-jp`). The standalone needs the same modules (generated from the same patched
  DOL the import produces).
- Desktop: add the standalone binary to the release jobs (Windows MSVC as `C:\dev\SkiesStandalone`'s
  `build.bat`, Linux / AppImage, macOS).
- Android: `tools/android-standalone-build.sh` packs one module without Gradle. Extend it to three modules,
  the launcher classes and `libskiesimport.so`, then call it from CI (it needs the Android build tools).
  Sign with `tools/sign-apk.sh` and the release key. The final app ID replaces `com.skiesofarcadia.standalone`.
- Ships: `libmain.so`, libpng, the modules, `dsp_coef.bin` (Dolphin's DSP coefficient table, not game data).
  No game files. Keep the release workflow's game-file refusal.
- Release beside the Dolphin builds until parity, as the emulator-free plan says (phase 5).

## Totals

Roughly 4-6 weeks of sessions for everything (as estimated; by 2026-10-26 items 0-6 took about two days
on desktop, the device checks still to come):

- Items 0-4: about a week.
- HD textures: half a week.
- 60 fps: 3-4 sessions.
- Dreamcast: about a week.
- Saves, launcher and packaging: 2-3 weeks.

The order above keeps the build playable and measurable after every step.
