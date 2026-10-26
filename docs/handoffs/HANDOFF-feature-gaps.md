# Handoff: the standalone's feature gaps (2026-10-25)

The standalone build (`standalone/`, the recompiled game on GXRuntime + Aurora with no Dolphin core) already
runs better than the Dolphin build (faster, smoother, less CPU and power; *HANDOFF-emulator-free.md*). It is
not a replacement until it has the Dolphin build's features. This file is the plan for re-hosting them: what
each one is, where its code lives, what the standalone has, the port steps, how the Dolphin build verified it
(so the port is checked the same way), and size. Read *HANDOFF-emulator-free.md* first (the standalone's
architecture: *In-game menu*, *Save states*, *The Android shell*, *Pacing on the Pocket 5*).

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
  viewing, no loop capture; C: deletions need two approvals; Windows Dolphin-build launches call
  `C:\dev\sync-controls.bat USER_DIR` first; never `pkill -f` a pattern from your own command line.

## The gaps

The Dolphin build's in-game menu (dev): **Saves** (same in both), **Graphics** (Picture, HUD, Frame rate,
Resolution, Antialiasing, Texture filter, Anisotropy, Scaling, HD textures, V-Sync, Show FPS), **Gameplay**
(same in both), **Language**, **Sound** (Volume), **Dreamcast** (Music, Uncensored; the launchers add Intro
music, `[Skies] DreamcastIntro`), **Controls** (Button mapping, Stick dead zone, Rumble). The standalone's
(`standalone/src/menu.cpp`): Saves, Graphics (Picture, HUD, Resolution, V-Sync, Show FPS), Gameplay, Sound
(Volume), Controls (Stick dead zone, its own key `StandaloneDeadZone`).

| # | Gap | Dolphin source (dev) | Standalone today | Size |
|---|---|---|---|---|
| 0 | European / Japanese discs run right | `release/GEAP8P.ini`, `GEAJ8P.ini`; `ConfigManager` hunk in `skies-dcmusic-eu` | **done** (desktop): idle PC per disc, EU as NTSC, module by disc ID | hours |
| 1 | Continue (card save at boot) | `patches/game/autoload.c`; `autoload` in `SkiesMenu.cpp` | **done** (desktop): `--continue`, `AutoLoadCard`; JP card mount fixed | ~0.5 day |
| 2 | Graphics options (MSAA, texture filter, anisotropy, scaling) | `recompcore-gfx-options.patch`, `skies-settings` | **done** (WSL): four rows, live; fork 0038 | ~1 day |
| 3 | Input: release keyboard layout, rumble, names, Button mapping, menu-close rule | `recompcore-input`, `controller-names`, `menu-close`, `android-pad-follow`, review D23-D26 | Aurora's own layout, dead zone only, rumble ignored | ~2 days |
| 4 | Language (European disc) | `recompcore-language.patch` | SRAM fixed to English | ~1 day |
| 5 | HD textures (+ import) | `recompcore-hd-textures`, `hd-textures-nested`, review D2 / D22 | Aurora has a loader, not wired | ~2-3 days |
| 6 | 60 fps | `recompcore-frame-interpolation`, `battle-specials`, `field-speed`, review D6 / D25 | none (Aurora's replay context exists) | 3-4 sessions |
| 7 | Dreamcast: Uncensored, Dreamcast music, DC streams, EU 60 Hz audio | `uncensor`, `skies-dcmusic-eu`, `dcmusic-streams`, `dcmusic-fx`, `pal60`, review D3 / D4 / D16-D21 | none | 1,200-1,800 lines of glue |
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
- **Not checked yet:** the first field on EU / JP (needs a state or a scripted New Game), the EU intro
  against m01 (item 7), an APK with the EU / JP arm64 modules on the Pocket 5 (`skies-pal60/build/android/
  module-ws34-GEA{P,J}8P` match the ws34 DOLs).

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
- **Not checked yet:** RAM against a load from the game's own menu (needs scripted input), a first battle
  and a card save after Continue, Windows, the Pocket 5.

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
  restored). **Open:** Windows (Dawn with MSVC, D3D12 with its shader compiler; NVIDIA has 2x and 8x),
  macOS (Metal); shipping a patched Dawn (CI builds it, or our own prebuilt packages).
- **Checked (WSL, Dawn on llvmpipe, field after a Continue, EFB 1x):** each option changes the picture
  (pixels off the baseline: MSAA 4,340, Sharp 35,492, Smooth 16,575, 16x anisotropy 14,793; two baseline
  runs differ by 442); by eye: MSAA smooths geometry edges, Sharp shows texels, Smooth / 16x soften. The
  three resampled outputs viewed through a local readback hack (not kept): area crisp, sharp bilinear =
  bilinear at a 1.5x stretch (Dolphin's formula: prescale floor 1), bicubic softest. No WebGPU errors in
  any mode. Android compiles.
- **Open:** under 4x MSAA a few isolated bright pixels inside the light beam's transparent cone (llvmpipe or
  an EFB-copy path under MSAA): run the frame-dump route (`capture/route-win.sh`) at 1x and 4x MSAA on
  Windows and the Pocket 5 and look. GPU cost at 4x / 6x with `AURORA_GPU_PROF=300`. Menu changes by
  hand on Windows / the Pocket 5 (only the ini path was driven here).
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

## 8. Data folder, romhacks, saves

Most of this is already Dolphin-free in `lib/ModernGekko/tools/` (std-only or `moderngekko/sha256`):
`dol_patch.{hpp,cpp}` (`ApplyDolPatchManifest`, `DolMatchesPin`, `RomhackTag`, `StateDirectory`,
`MoveDiscStates`), `user_directory.{hpp,cpp}`, `release_games.hpp`. Still Dolphin-bound:
`disc_extract.cpp` (DiscIO) and `save_import.cpp` (`GCMemcard`, `SavestateLayout`, `StateFile`).

- **Data folder** (`data-folder.txt`, `portable.txt`; Android `SkiesDataFolder.kt` with All files access and
  the JNI `checkDataMove` / `moveDataFolder` / `pointDataFolder`): desktop calls `FindUserDirectory` at
  startup and passes the result as `--user`; Android reuses the Kotlin and JNI as they are.
- **Romhacks** (data-only `.dolpatch` on the pinned code; code-changing hacks refused; `RomhackTag`
  separates states): call `DolMatchesPin` / `RomhackTag` in the launchers; test `disc.cpp` with a romhack
  whose file sizes differ (untested).
- **States:** the standalone keeps `USER/StateSaves/Standalone/<disc ID>-slotN.sst` / `-auto-*.sst`; Continue
  picks the newest whose header matches the disc ID and DOL hash; Back up saves must include
  `StateSaves/Standalone/*.sst` (today's Kotlin takes only `StateSaves/*.sav` and cards).
- **Import a save:** link `save_import.cpp` with the vendored `GCMemcard` for card saves and card images; add
  a `.sst` header validator; Dolphin `.sav` states can't be imported (say so). Japanese saves are
  `EA_LEGEND.*`, the others `SA_LEGENDS.*`.
- **Check:** `tools/save-import-test.py`'s 16 cases against the standalone's import; a move of the data
  folder on Linux, Windows and the Pocket 5 (as *HANDOFF-datadir.md*).

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

- **Orientation:** `USER_LANDSCAPE` (both landscapes, honouring the rotation lock, #12). The standalone's
  manifest says `sensorLandscape`, which ignores the lock.
- **Lifecycle:** done so far are immersive mode from the start and Home / back at the same surface size (the
  EFB is kept).
  - Open: pause then Home then return (the Dolphin build's black-screen history), process kill and restore,
    "Game in progress" after a task-clearing relaunch, and Exit Game relaunch.
  - Audio must pause with the app.
  - **A size change on return (rotation, split screen) goes through Aurora's resize, which recreates the
    EFB and drops copies held only on the GPU.** Fix that in the fork (keep the EFB at its scale and only
    reconfigure the surface) before the shell ships.
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

Roughly 4-6 weeks of sessions for everything:

- Items 0-4: about a week.
- HD textures: half a week.
- 60 fps: 3-4 sessions.
- Dreamcast: about a week.
- Saves, launcher and packaging: 2-3 weeks.

The order above keeps the build playable and measurable after every step.
