# Handoff: the Aurora video backend on Android (2026-10-21, session 4)

> **2026-11-19:** the Windows test folders this file names under `C:\dev` (`SkiesAuroraRun`, `SkiesUncensor`, `SkiesPal60`,
> `SkiesLang`, `SkiesGfx`, `SkiesPerf`, `SkiesCI`, `fmabench`) were deleted; mentions of them are history.
> `C:\dev\SkiesRecomp`, `SkiesRomhack`, `SkiesIssues`, `SkiesStandalone` and `SkiesAurora` remain.

Aurora is GXRuntime's GX renderer (WebGPU through Dawn). Sessions 1-3 proved it draws the game
correctly and, as an in-process Dolphin video backend (`moderngekko-run --graphics Aurora`), at
93% of the Null ceiling on a Windows GPU; the research tree, scripts, scores and the fork live in
`~/projects/skies-aurora` (its `HANDOFF.md`). This file is the Android part: the app builds with
the backend, draws into its own Surface and runs the game on a Retroid. Since session 6 it is
committed as patches on the local branch `aurora-backend`, which is kept local on purpose: it is not
rebased, merged or pushed (see *What is where*).

## How it works on Android

Dolphin's Android app has no SDL and owns the Surface, and Aurora's window code was SDL-only. The
fork gained a build without SDL (`AURORA_ENABLE_SDL=OFF`, fork commit 0016): `window_external.cpp`
stands in for the SDL window (no events, input or audio; the host's native window comes in
`AuroraConfig::nativeWindow` and changes through `aurora_set_native_window()`), the Dawn surface is
built from the raw `ANativeWindow*` (`SurfaceSourceAndroidNativeWindow`), the shipped pipeline
cache's sqlite VFS reads through stdio, and the GXRuntime backend's pad and audio entry points are
stubs (Dolphin's own input and mixer do that). Dawn comes as the provider's prebuilt
`dawn-android-aarch64` package (a 56 MB static library; its link interface's hard-coded `liblog.so`
path is already fixed up by the provider); abseil, libpng, sqlite3 and Tracy are fetched; fmt,
xxhash, zstd and zlib are Dolphin's own.

The Dolphin side (`Source/Core/VideoBackends/Aurora/AuroraBackend.cpp`):

- `Initialize(wsi)` takes `wsi.render_surface` (the `ANativeWindow*` the app hands every backend)
  and its size, and Aurora creates the Vulkan swapchain on it. The cache databases go to
  `Cache/aurora/` in the user folder.
- Surface changes (Home and back, rotation) come through VideoCommon's presenter like for the
  Vulkan backend: `OnFrameEnd` polls `SurfaceChangedTestAndClear()` and swaps the window. When the
  Surface is destroyed, `NativeLibrary.SurfaceDestroyed` (jni/MainAndroid.cpp) pushes a blocking
  event to the GPU thread that releases Aurora's swapchain before the window is released (dual core
  only, where that thread still pulls events while paused; 2 s bound). Checked by code reading
  only so far.
- **The 60 fps Frame Interpolator's replays are skipped** in the command sink
  (`FrameInterpolator::IsReplaying()`): the interpolator restores VideoCommon's registers without
  any command before replaying a frame's stream in slices, so Aurora, which only sees commands,
  sized the replayed draws with the live state and aborted on an "unknown opcode" after 16 s (the
  first device run). Its in-between pictures go to VideoCommon's second EFB, which this backend
  never shows, so with Aurora the game runs at its 30 fps. 60 fps under Aurora is open (below).
- Aurora's log goes to logcat (tags `aurora`, `aurora-backend`); Android drops stdio.

Build: `AURORA=1 tools/android-ws-build.sh` (Gradle gets `-PrecompAurora=1`, which turns
`ENABLE_AURORA_BACKEND` on and asks for CMake 3.25+: the SDK's `cmake;3.31.0` package is installed
on this machine; the plain app keeps 3.22.1). `AURORA_DEPS=~/projects/skies-aurora/android-deps`
reuses the downloaded Dawn package and dependency sources (`-PrecompCMakeArgs`). The in-game
Settings → Graphics → Renderer row offers "Aurora (experimental)" when the build has it
(`SkiesNative.videoBackends()`); `[Core] GFXBackend = Aurora` in `Config/Dolphin.ini` does the
same by hand.

## Results (Retroid Pocket Nova, Adreno 740, Android 13; the Pocket 5 was in use)

Test app `skies-recomp-aurora-2026-10-21-test.apk` (ws patch of this checkout, USA module with the
device profile), the USA game pushed into the test app's `Games/GEAE8P`. Boots to the title at full
speed, Dawn on the stock Adreno Vulkan driver (`Engine Name: Dawn` in logcat), 1280x960 surface.

Uncapped (`[Core] EmulationSpeed = 0`), the game's 30 fps (`[Skies] FrameMode = 0`) for both
backends, fresh boot per state, 16 s settle, 30 s average (`capture/android-bench.sh`, outputs in
`~/projects/skies-aurora/android-bench/nova-{aurora,vulkan}`). Speed 1.0 = full speed.

| state | Dolphin Vulkan | Aurora | Aurora / Vulkan |
|---|---|---|---|
| 01 opening cutscene | 1.68 | 1.20 | 0.72 |
| 02 first battle | 2.72 | 2.15 | 0.79 |
| 03 Valuan ship field | 1.97 | 1.44 | 0.73 |
| 04 Valuan ship later | 1.81 | 1.27 | 0.70 |

That was the first build. A simpleperf profile of the GPU thread (`PERF=1`, report with the
NDK's simpleperf and the unstripped `libmain.so`) showed where the gap was: 36% of the thread in
`clock_gettime` (the backend's per-command timing, cheap on Windows, not on ARM), 10% in
VideoCommon decoding the XFB from RAM for a picture nobody showed, 13% in the full XXH3 texture
hash per bind; Aurora's own parse and Dawn were the minority. Four cuts followed: the clock reads
only with `MODERNGEKKO_AURORA_LOG=1`; `OpcodeDecoder::g_skip_vertex_loading` (the decoder skips
VideoCommon's vertex loaders under Aurora: they only fed the Null vertex manager); the presenter's
`FetchXFB` skips the XFB texture under Aurora; the fork's retail texture hash samples 128 x 32
bytes past 4 KB (`AURORA_RETAIL_TEXHASH_FULL=1` for the old full hash). Same batch, after the cuts:

| state | Dolphin Vulkan | Aurora | Aurora / Vulkan |
|---|---|---|---|
| 01 opening cutscene | 1.68 | 1.59 | 0.95 |
| 02 first battle | 2.71 | 2.62 | 0.97 |
| 03 Valuan ship field | 1.96 | 1.99 | 1.01 |
| 04 Valuan ship later | 1.80 | 1.54 | 0.85 |

Parity with the Vulkan backend in three states; the fourth is the scene with the fringe (below),
so the same effect probably costs the time. The desktop in-process run still matches Dolphin
after the cuts (ship interior 3373 px, as before). One run each: device numbers vary a few
percent between runs. Savestates load under Aurora (the
register re-feed works: "register state reloaded" in logcat). Home and back: the Surface
destroyed/changed path released and re-created the swapchain, the game went on (logcat
`native window 0x0` then the new handle). The cache databases open after the path fix
(`Cache/aurora/`).

Picture (screencaps in `android-bench/nova-aurora/*.png`, `*.compare.png` beside Dolphin's
reference frame): the first battle and the ship field match. **Two scenes draw with a red/cyan
fringe** on every edge of the 3D picture (not the text), the opening's Alfonso shot (01) and the
ship's lower deck (04), and both come out darker than Dolphin's; the Vulkan backend on the same
device draws them clean, and the offline Aurora replays of these states on the desktop scored
4777 and 3180 px, so this is Aurora on Adreno (Dawn/Vulkan) specifically. Looks like a
full-screen EFB-copy effect of the game (both scenes use one; the battle and the ship field do
not) drawn back with its colour channels a couple of pixels apart. The env-driven desktop tools
(AURORA_DRAW_LOG, frame dumps) work on the device through `aurora.env` in the user folder
(KEY=VALUE lines the backend puts in the environment before Aurora starts;
`capture/android-bench.sh` writes it from `AURORA_ENV="K=V;K=V"` and pulls frame dumps).
Bisect status: the effect is the game's **shadow-volume
composite**, not a blur: per frame (device and desktop logs agree) a red-only EFB copy
(`GX_CTF_R8`), a logic-op OR fill, the shadow volumes drawn with add/subtract blending into red,
a second red copy, a logic-op AND, a full-screen OR that draws copy 1 back (point sampled), and a
blend that draws copy 2 back. The final red channel comes from the copy, green and blue from the
live EFB, which is why the fringe is red against cyan. Ruled out on the device: the logic-op
shader emulation (`AURORA_LOGIC_OP_BLEND=1` fringes too), the retail texture cache, resolution
(the desktop is clean at 1280x960 and 640x480, through Dawn's Vulkan backend on llvmpipe as well),
a texel-edge rounding of the copy's point sampling (snapping to texel centres in the shader
changed nothing; the copy pass itself downsamples with a linear box filter). Dawn reports
`CoreFeaturesAndLimits` on the Adreno, so depth copies are not skipped; it reports no
`TextureComponentSwizzle`, `BC` or `ASTC`, which Aurora handles with CPU fallbacks. The draw-range
bisect (`AURORA_DRAW_RANGE=0-N` with per-frame dumps) does not work for this on the device: the
frame containing the cut always came out fully blank, never partial, so no cut inside the shadow
sequence could be looked at. Next: find out why the cut frame is blank (the present source after a
dropped draw?), or dump the two R8 copy textures themselves, or test the add/subtract blend of the
volumes in isolation. Suspects, in order: the subtract blend (`GX_BM_SUBTRACT`) or the logic-op
EFB snapshot on a tiled GPU, and the R8 copy conversion. Also: Aurora presents its picture stretched to the Surface
(no letterbox, Dolphin's aspect setting is not applied; the Vulkan frames show 16:9 bars), and
the fps/vps counters the automation reports are 0 under Aurora (VideoCommon's Null presenter).

## Session 5 (2026-10-21 night): the fringe, found and fixed

The red/cyan fringe was not Adreno-specific after all: it was a **2-pixel viewport offset in the
fork, doubled by every EFB copy drawn back**. Measured, not guessed: in the Nova's own EFB readback
of state 04 the red channel sat exactly (+4, +4) render pixels (2 game pixels) from green and blue,
everywhere in the frame, with either logic-op path and with the texture cache off; a copy-texture
dump (new `AURORA_COPY_DUMP_DIR`) showed the R8 copy itself aligned with the frame, so the shift came
from the full-screen quad that draws the copy back; and the frame had a 4-pixel black border at the
top and left in every channel, in every scene, which the desktop never had. The draw log, extended
with the raw XF viewport and BP scissor registers, gave the cause: the widescreen-patched game sets
the XF viewport origin to 662 (the SDK's 342 + x + w/2; the unpatched game uses 660), the scissor
box offset register (BP 0x59) holds 342 on both, and Aurora subtracted a fixed 340 instead of the
scissor box offset, so every draw landed at (+2, +2) game pixels: the scene once, the copy drawn
back twice. The desktop looked right only because its in-process runs used the *unpatched* game
(origin 660, which the 340 happened to cancel) while the device ran the patched one; Dolphin
derives the origin from the scissor box offset (`BPFunctions::SetScissorAndViewport`, 342 + twice
the offset) and draws the patched game at 0, the unpatched one at -2.

Fix (fork commit 0018 in `~/projects/skies-aurora/GXRuntime`, `patches/0018-*`, vendored copy
synced): the viewport and the scissor rectangle are positioned relative to the scissor box offset
register, like Dolphin; the XF viewport is re-applied when 0x59 changes. Checked on the Nova (fixed
test APK): states 01 and 04 draw clean, red and green align at (0, 0), the black border is gone,
and the battle and ship-field scenes moved by exactly (-4, -4) with identical content. Desktop:
the WSL in-process run uses the unpatched game and now draws it at -2 like Dolphin (which shows the
fringe there too: the unpatched game really does double its own offset; the widescreen patch's +2
is what makes the shipped game clean). Windows (ws26 game) check: see the research tree's HANDOFF.

Debug toggles that stay in the fork (all env/`aurora.env`): `AURORA_COPY_DUMP_DIR` (+`_EVERY`,
default 150: raw RGBA8 rows of every EFB-copy texture as `copy_<frame>_<n>_fmt<fmt>_<WxH>_<rgba|bgra>.rgba`;
host: `magick -size WxH -depth 8 rgba:in out.png`), `AURORA_COPY_TEXLOAD=1` (conversion shader
through textureLoad), `AURORA_COPY_NUDGE=x,y` (copy source window shift in render pixels),
`AURORA_VIEWPORT_ORIGIN=n` (fixed origin for both axes, the old behaviour at 340), and the draw log
now prints `raw xf viewport ...` and `viewport logical=... render=... scissor ...` per draw.

## Session 5, part 2: 60 fps under Aurora (built; WSL-verified; device numbers below)

The Frame Interpolator's replay now runs through Aurora instead of being skipped. Design (all on the
GPU thread, nothing new in the interpolator's blending):

- **Replay context.** The fork keeps a second, complete GX state (`gx::swap_replay_context`: registers,
  matrices, texture binds, TLUT slots, its own copy-texture cache) drawing on a spare EFB
  (`webgpu::swap_spare_framebuffer`, same size, created on first use). `FrameInterpolator::BackendHooks`
  (`g_backend_hooks` in FrameInterpolator.h) tell the backend when a slice starts and ends: the backend
  drains Aurora's FIFO, swaps EFB and GX state (`aurora_backend_replay_context`), starts a new EFB pass
  on the swapped targets (`gfx::restart_efb_pass`) and, on the job's first slice, re-feeds VideoCommon's
  registers (which then hold the frame's start state) with `ReloadRegisterState`. Both contexts keep
  their state across switches, so a slice can fall anywhere between the game's commands, as in
  VideoCommon's own sliced replay.
- **Vertex arrays.** The recorded stream's CP array bases point at RAM the game may have changed. For
  each replayed draw the sink asks `FrameInterpolator::ReplayArrays` for the interpolator's copies
  (per draw, `lo` bytes past the base), lays each out in a bump arena so the base-relative reads land
  on the copy, hands it to Aurora with its host-pointer command (`GX_AURORA_LOAD_ARRAYBASE`, opcode
  0x50) before the draw, and writes the guest bases back after it. The arena is reset per slice
  (Aurora parses the FIFO later, so the bytes must outlive the slice's drain).
- **Pictures and presents.** The game's XFB copy no longer presents: the EFB is copied into a kept slot
  (`aurora_backend_keep_picture`, 4 frames x real/in-between); the replay's XFB copy keeps the
  in-between from the spare EFB; and the interpolator's retrace decision (`PictureForRetrace`, now
  reporting `kind` through the hook) presents a kept slot (`aurora_backend_present_kept`, the fork's
  present override). So presents follow the VI retrace, like Dolphin's backends, which is also the
  frame-pacing model for 30 fps once the hook has fired once. `EndJob` marks an in-between as present
  when a backend hook exists (the Null texture cache under Aurora yields no XFB entry, which is why
  nothing was ever shown at first). The frame readback (`MODERNGEKKO_AURORA_PNG_DIR`) follows the
  presented picture.
- **Display copy clear.** Aurora stubbed the display copy entirely; it now applies the copy's clear
  (colour/alpha/depth per the update flags), as Dolphin's `ClearScreen` does for every copy with the
  clear bit.
- `MODERNGEKKO_AURORA_INTERP=0` restores the old behaviour (replays skipped, present at the XFB copy);
  `MODERNGEKKO_AURORA_INTERP_REFEED=1` re-feeds the registers on every slice (a test: no effect).
  `MODERNGEKKO_AURORA_LOG=1`'s 600-frame line now counts kept/shown real and in-between pictures.

Verification on WSL (opening cutscene, llvmpipe): 298 of 300 frames interpolated, two presents per
game frame, no Aurora errors; the in-between picture differs from the real one only on the animated
characters and geometry edges (static walls pixel-identical), i.e. a genuine halfway picture (checked
with a difference mask; `capture/inproc.sh` dumps every present with `PNG_EVERY=1`, `WIDTH=640
HEIGHT=480` keeps that affordable). A replayed frame issues exactly the live frame's draws (4024 =
4024, draw-by-draw signature/texture/viewport identical; draw log `ctx=L/R`).

Retroid Nova, 60 fps mode (`FRAMEMODE=1`), uncapped, one run each: Aurora 1.20 (opening) / 1.02 (ship
deck, min 0.79) vs Vulkan 1.69 / 1.89. Vulkan loses nothing to the replay (its GPU thread had idle
time; the sliced replay fills it), Aurora's GPU thread was already near saturation at 30 fps parity,
so the second parse+draw per frame shows. Next lever: the GPU-thread cost of Aurora's parse and draw
(profile `android-bench/s5-interp-perf`, `capture/perf-report.sh FILE.perf.data [COMM]`).

### Session 5, part 3: costs, pacing, and two bugs on the way

- **Capped pacing on the Nova** (`capture/android-pacing.sh NAME BACKEND STATE`: full speed, 60 fps mode,
  with the interpolator's statistics file and the presenter's `PresentPacing.on` in the app folder;
  `[present-pacing]` lines in logcat for any backend, `RECOMP_PRESENT_PACING=1` on the desktop): Vulkan
  holds 60 fps on the ship deck (speed 1.00, present interval p50 17 ms, GPU thread 7 ms FIFO + 3-5 ms
  replay per game frame); Aurora ran at 0.74 (p50 32 ms; 30 ms FIFO + 15 ms replay). Per present the
  backend's own split was feed+drains 9.5 ms, present 5.9 ms, VideoCommon+waits 7.7 ms.
- **Resolution mismatch.** The device profile has no `InternalResolution`, so Dolphin's backends draw at
  native 640x480 while Aurora filled the 1280x960 surface: four times the pixels, and the shadow
  composite's full-screen snapshots and copies scale with it. The backend now applies Dolphin's
  internal resolution (`iEFBScale`) through `VISetFrameBufferScale` (`MODERNGEKKO_AURORA_EFB_SCALE=N`
  overrides; re-checked at every present so the in-game setting applies). Numbers at 1x are the next
  thing to measure.
- **Hash memo.** Retail textures are hashed once per frame (`texture_frame_tick` at the game's display
  copy; fork 0020): XXH3 went from 14% to 3% of the GPU thread; with the placeholder real entry (no XFB
  decode) uncapped speed rose from 1.20/1.02 to 1.47/1.26 (opening/ship deck).
- **Clear-draw bug (fork 0021).** The display copy's clear, added in part 2, drew a full-screen clear
  whose viewport (full target, the clear depth) stayed on the encoder while Aurora's cached viewport
  still said the game's: every draw of the next frame rendered with a collapsed depth range and the
  frame came out as the clear colour. Only the interpolation path survived (its picture copy restarts
  the pass). Fixed by re-pushing the cached viewport and scissor after the clear
  (`gfx::reapply_viewport_scissor`); `AURORA_DISPLAY_COPY_CLEAR=0` disables the clear for A/B. The sink
  also keeps or presents the picture *before* feeding the XFB copy command, so the clear lands at the
  start of the next frame packet. WSL scores after the fix (unpatched game with
  `AURORA_VIEWPORT_ORIGIN=340` to match the widescreen references): ship interior 5335, ship deck 38966.
- **Profiles.** simpleperf's "Video thread" comm covers Aurora's render worker too (it inherits the
  name): split with `--sort tid,comm`. In capped runs a third of the GPU thread's samples are
  `__kernel_clock_gettime` from `Fifo::RunGpuLoop`'s idle loop (the vdso falls back to the syscall on
  this kernel), not work. `capture/perf-report.sh FILE.perf.data ["Video thread"]` resolves symbols with
  the unstripped `libmain.so` (`--symdir` on the Gradle `cxx/RelWithDebInfo/*/obj/arm64-v8a`).
- Windows: `C:\dev\SkiesAuroraRun` rebuilt from the synced tree (a tree-wide rsync forces a full MSVC
  rebuild, about an hour; later ones are incremental). The ship-deck frame has no fringe there either.
  `user-bench-60` is `user-bench-dc` with `FrameMode = 1` for 60 fps runs (`USER_DIR=` in
  `capture/inproc-win.sh`, `LOG=1` for the backend's pacing line).

### Session 5, part 4: full speed at 60 fps on the Nova

The cost was draw count, not pixels. In a captured ship-interior frame, 1423 of the 1960 draws are
separated only by a texcoord array-base write (each strip points into the next slice of one big
array); Dolphin's vertex loaders read the arrays and batch across that, Aurora treated every base
write as new state, uploaded a fresh storage range per draw and merged nothing (merged = 1 per
frame). Fork commit 0022: indexed vertex attributes are dereferenced on the CPU into direct vertex
data (what Dolphin's loaders do; `vtxDesc` shows them as `GX_DIRECT`, `vtxDescStream` keeps the
game's descriptor; `AURORA_NO_DEREF=1` restores shader-side indexing), base and stride writes no
longer dirty the pipeline state, the merge checks the vertex format (it did not), and ShaderInfo is
cached per ShaderConfig. Draws per frame: ship interior 1960 -> 537, ship deck 3622 -> 1145, first
battle 1042 -> 393; the Dolphin difference scores improved with it (5335 -> 4920, 38966 -> 21264,
32106 -> 25209; WSL, unpatched game with `AURORA_VIEWPORT_ORIGIN=340`).

Retroid Nova, capped, 60 fps mode (`capture/android-pacing.sh`): ship deck at full speed (wall
33.37 ms per game frame), GPU thread 11.8 ms FIFO + 10.6 ms replay (was 30 + 15), present interval
p50 17.6 / p90 21.7 / p99 23.8 / max 24.3 ms, none over 25; the Vulkan backend in the same run:
p50 17.3 / p90 23.2 / p99 24.5. Opening cutscene: full speed, p50 20.7, p99 27-30. Mind the APK
name: `tools/android-ws-build.sh` dates it, so a build after midnight is a new file (one run here
measured the old one).

Pacing notes: the present intervals above are enqueue times on the GPU thread; the two retraces
of a game frame are not evenly spaced in real time (the throttle lets emulation run ahead, in
single core the frame's GPU burst follows), so for the on-screen cadence the presentation itself
has to be queued against vsync. `MODERNGEKKO_AURORA_VSYNC=1` (Aurora's FIFO present mode) is the
first thing to measure, with `capture/android-sf-latency.sh` (SurfaceFlinger's presented-frame
timestamps of the (BLAST) SurfaceView layer; `capture/android-pace-onscreen.sh` runs a capped
bench and samples it twice).

Windows (NVIDIA, D3D12, `C:\dev\SkiesAuroraRun`, ws26 game, `user-bench-60c` = `user-bench-dc` with
`FrameMode = 1`, `EmulationSpeed = 1.0`, the interpolator's statistics and `PresentPacing.on` files; note
its interpolator reports "FIFO work 0.00": that runner comes up single-core, so `Deferred()` is false and
the replay runs synchronously at the frame end, and its adaptive budget then never replays; the runs use
`EXTRA_SETS='set RECOMP_FRAME_INTERPOLATION_ADAPTIVE=0&& '` in `capture/inproc-win.sh`): full speed at
60 fps mode with the dereference (feed 3.5 ms per present, 2213 draws for both contexts per game frame,
the ship-deck frame correct), presents bunched in pairs about 10 and 23 ms apart because single core
processes the two retraces of a frame back to back while emulation runs and the frame's GPU burst
follows; smaller Aurora drains (`DRAIN_KB=8`, `2`) and `TimingVariance` 4/12 do not change it, so the
on-screen cadence has to come from the swapchain: `VSYNC=1` (`MODERNGEKKO_AURORA_VSYNC=1`, Aurora's
Fifo present mode) queues them against vsync (`Using surface format BGRA8Unorm, present mode Fifo`);
the on-screen timing itself was not measured on Windows (no PresentMon here).

**On-screen pacing, Retroid Nova** (`capture/android-pace-onscreen.sh`, SurfaceFlinger's presented-frame
timestamps, capped, 60 fps mode, ship deck, two samples of 125 intervals each): Aurora (Mailbox,
default) p10-p99 16.66-16.67 ms with one 33 ms hiccup in 250; Aurora with `MODERNGEKKO_AURORA_VSYNC=1`
(Fifo) every interval 16.66-16.67 ms; the Vulkan backend p10-p99 16.66-16.67 ms with one 33 ms hiccup
in 250. Speed 1.00 in all three. So the device shows a flat 60 Hz with Aurora, the same as with Dolphin's
Vulkan backend. The merge-heavy build draws the ship field pixel-identically to the previous device
capture (35 pixels of 307200 differ); the first battle differs only by animation timing (by eye).

## Session 6 (2026-10-22 morning): the Pocket 5, the commit, presentation, and the fringe again

**Pocket 5 (Adreno 650, stock driver), 04-valuan-ship-later, capped, 60 fps mode (FrameMode 1):**

| | Aurora | Vulkan |
|---|---|---|
| speed | 1.009 (0.986-1.012) | 1.008 |
| on-screen interval (SurfaceFlinger, two samples) | p50 16.66-16.67, p99 16.78-16.92, max 16.93 ms, none over 20 | p50 16.66, p99 16.78-16.82, max 16.85 ms |
| uncapped, FrameMode 1 (the replay skipped by the adaptive budget) | 1.341 | 1.334 |

Full speed and the same cadence as Vulkan, like the Nova. The first runs said 0.43-0.55x with
`MODERNGEKKO_AURORA_LOG=1`: the timing log read `steady_clock::now()` twice per GP command, and on
the Pocket 5's kernel that is a `clock_gettime` syscall (58% of the GPU thread in simpleperf, the
CPU thread then waiting 34 ms per frame at draw-sync reads); the counter register `cntvct_el0` is
trapped there too (worse). The log now times only the drains (`aurora_backend_fifo_over` tells
whether the next `drain_if_over` would drain), and the frame line reads `drains ... feed+dolphin+cpu-wait`.
With the log on the Pocket 5 runs at 0.99x now. Never measure that device with a per-command clock.

**Presentation.** `ApplyPresentRectangle` (backend): Aurora alone knows the surface size (its own
window on the desktop, the app's Surface on Android), so the backend gives it to VideoCommon's presenter
(`SetBackbuffer`), which computes the target rectangle from the aspect settings (the port's Picture
option, Match screen included, through `SkiesSettings::OnScreenSize`), and passes that rectangle to
the fork (`aurora_backend_set_present_rect`; `gfx::present_rect()` overrides `calculate_present_viewport`
in `end_frame`). Fork: `aurora_backend_surface_size`. The frame buffer keeps the GX base aspect
(`aurora_backend_set_frame_buffer_aspect_fit(1)` in `ApplyEfbScale`: 640x480 times the scale on a
16:9 screen too; it was 853x480 on the Pocket 5). On the desktop the deferred frame-buffer resize is an
SDL user event, and the port's shared-queue input thread (`SDL.cpp`) swallowed the whole user range,
so the resize never happened there; it now takes only its own two event types. Checked: the Pocket 5
draws full screen at 16:9 (`surface 1920x1080`, EFB 640x480), the Nova 1280x960, WSL/Windows at 1920x1080
windows. Picture = 4:3 / Match screen under Aurora not yet looked at by eye.

**Lifecycle (Pocket 5, 60 fps on, `capture/android-lifecycle.sh`):** loaded 1.007, Home and back 1.005
(`am start` on the launcher activity brings the task back; the surface is recreated, `state=running`,
picture drawn), after the menu sequence 1.004, three loads in a row 1.006, a second state (first battle)
1.001; no crash. Two things to look at: the Z+Start sequence left no menu on the screencap (the press
timing, or the menu closed again; inconclusive), and after the three loads in a row the picture had no
red channel at all (mean colour 0,28,37: the shadow composite's red restore lost), the same look as the
Nova showed on a cutscene state and WSL shows always; a single load is fine.

**The fringe, found for real.** Session 5's fix (the viewport origin from the scissor box offset)
was verified on the Nova through Start Game and a state load; the Pocket 5's launcher (1920x1080)
put the old tap position on **Continue** (the device's newest state first), and that sequence, like a
direct `--load-state` on Windows, showed the red/cyan fringe. The draw log (Windows) said the fork
drew with a viewport origin of `660,580` while VideoCommon's `xfmem` had `662,582`: the fork took
the XF viewport only from a load of all six registers starting at 0x101A, so the game's later
origin-only write at 0x101D (and a savestate re-feed, one register at a time) were ignored, two
pixels off from Dolphin (a frame-matched comparison with Dolphin's Vulkan picture put the scene at
the 340-origin position: dx=0 dy=0 against Aurora with `AURORA_VIEWPORT_ORIGIN=340`). Fork 0024
decodes the viewport and the projection per register. Pocket 5 Continue path, Nova, Windows at
1280x960 and 1920x1080: clean, R/G/B aligned.

**The black / red-less picture after a savestate reload, found too.** With the 60 fps replay active
(the Pocket 5 only starts replaying some seconds in, which is why a first load looked fine), a reload
of the same scene gave a black picture, and on Windows every picture lacked red from the start.
`render_pass` skips an intermediate EFB pass without a resolve target, and the replay's context
switches (`restart_efb_pass`) cut the live frame into such passes: the draws after the last EFB copy
and before the XFB copy (the shadow composite that restores the scene's red channel) were dropped.
`restart_efb_pass` now marks the pass it ends observable (fork 0024, with: a replay context switch
asked for while no frame is open is deferred to the next begin_frame instead of dropped, and the
logic-op EFB snapshot is per context). Pocket 5 with the replay active (582 real + 577 in-between
pictures kept per 600 frames): single load, same-state reload and two rapid reloads all at full speed
with the red channel present. Two earlier attempts at the copy-texture cache were wrong and are gone.

**Windows with the replay, found too (fork 0025).** Every picture lacked red whenever the replay ran
(mean colour 0,33,42 against 36,33,42 without it), with the session-5 sources as well. Same family:
`queue_texture_copy` (the kept-picture copy) ended the live frame's last pass without marking it
observable, so `render_pass` skipped it and the kept picture missed the shadow composite; on Android
that last pass usually ended at a context switch instead. Windows with the replay forced on
(`RECOMP_FRAME_INTERPOLATION_ADAPTIVE=0`, `user-bench-60c`): red 36 on six consecutive pictures, 903
real + 895 in-between kept per 600 frames, full speed; the Pocket 5 reload sequence re-checked.

WSL (llvmpipe) draws this scene with a blue tint and a white shadow blob with the session-5 runner as
well (Dolphin's Vulkan backend on llvmpipe is right): a WSL-only rendering problem, not looked into.
Gotcha that cost an hour: Aurora's frame dumps carry the EFB alpha (near 0 over the scene), and an
image viewer that composites alpha shows them white with only the HUD and the shadow blob; strip it
(`convert f.png -alpha off`) or measure (`convert f.png -resize 1x1 -format '%[fx:int(255*r)]...' info:`)
before concluding anything. Windows (NVIDIA) renders the scene at 1280x960 and 1920x1080 windows, EFB
640x480 with the fit (the fringe as above).

**Commit.** The whole backend is now in this repo as patches: `patches/moderngekko-aurora.patch`
(ModernGekko `CMakeLists.txt`, `dolphin_runtime.cpp`) and `patches/recompcore-aurora.patch` (the fork's
`GXRuntime/`, `VideoBackends/Aurora/`, the VideoCommon / InputCommon / Android hooks), last in
`recompcore.series`; `tools/apply-patches.sh`, `tools/check-patches.sh` and `patches/README.md` know
them; `tools/check-patches.sh` passes. The work was ported from the stale main checkout onto origin/dev
(a7aa0f8) in the worktree `~/projects/skies-aurora-backend` (branch `aurora-backend`; the USA module
there is built from the ws30 DOL in `~/projects/skies-pal60/build/game/GEAE8P-ws30`). Nothing is on by
default: `-DMODERNGEKKO_ENABLE_AURORA=ON`, `AURORA=1 tools/android-ws-build.sh`.

**Picture modes and the Nova (after 0025).** Pocket 5 screencaps with the Picture option: 4:3 is
pillarboxed with 4:3 geometry, Match screen on the 16:9 screen and 16:9 fill it; the presenter's
rectangle drives Aurora as intended. Nova lifecycle script with the final APK: loaded 1.004, Home and back
1.001, menu sequence 1.002, three loads 1.002, second state 0.999, picture present throughout.

**The in-game menu and the OSD under Aurora (fork 0026).** `SkiesMenu::Draw` is Dear ImGui through
Dolphin's `OnScreenUI`, which the presenter only creates for a non-headless gfx; the backend runs on the
Null gfx classes, so there was no OSD at all (the lifecycle script's "menu open" screencap showed the
plain scene). Now `Null::g_presentable` makes the Null gfx report itself presentable (its draws stay
no-ops; `Null::g_surface_info` is the surface the presenter starts from, the real size follows through
`ApplyPresentRectangle`), `OnScreenUI::Finalize` hands each frame's finished draw data to
`VideoCommon::g_imgui_draw_sink`, and the backend's `ImGuiDrawSink` flattens it (vertices, 16-bit
indices, per-command clip rect and texture, the font atlas uploaded when ImGui creates or updates it)
into `aurora_backend_set_overlay` / `aurora_backend_set_overlay_texture`; the fork's `gfx/overlay.cpp`
draws it in the present pass after the EFB blit (alpha blend, scissor per command, the surface as the
display). `MODERNGEKKO_AURORA_OSD=0` turns it off. Checked: the Settings menu (Z+Start) over the game on
Windows and on the Pocket 5, speed 1.008 with it open; the presenter's FPS counter reads 59.7-59.9 now
(it counts presents only on a presentable gfx). The backend links `imgui` for the draw-data types.

**Direction (2026-10-22, end of session 6).** Asked how promising Aurora really is: inside Dolphin it is at
parity with the Vulkan renderer (speed and picture), not ahead, and it loses Dolphin's HD packs; better
lighting is the one strong reason to own the renderer there. The user chose the **emulator-free path**
(the recompiled game on standalone GXRuntime with Aurora): [HANDOFF-emulator-free.md](HANDOFF-emulator-free.md).
This backend's fork fixes and overlay are its starting point; the branch stays local (not rebased, not merged, not pushed). A Pocket 5 coverage survey (eight states,
Aurora vs Vulkan, `android-bench/p5-cov-*`) was started and then cut off (the restart's `pkill -f`
matched its own shell): its partial output is not a result; rerun it with the state names
`02-first-battle 06-save-menu 07-valuan-red-alarm 10-worldmap-android 12-streaks 16-deck-battle-ws10
17-ship-battle-baltor 19-retroid-antonio-start`.

## Open

1. The emulator-free path (the branch stays local, see *What is where*): phases 1-3 done, phase 4 under
   way (feature-gap items 0-7 done, item 8 mostly, Aurora on a video thread since 2026-10-26, fork 0046), ahead of the
   Dolphin build on the Pocket 5 at 30 and 60 fps; its performance goal, numbers and *Where it stands* in
   [HANDOFF-emulator-free.md](HANDOFF-emulator-free.md).
2. The coverage survey above (battles, menus, world map, Baltor, Antonio) on the Pocket 5.
3. OSD details: the OSD message font scale on a 1920x1080 surface (ImGui scale 1 everywhere),
   the on-screen keyboard / mouse paths of `OnScreenUI` are not wired (menu by pad only).
4. Coverage: battles, menus, world map night, EU/JP, HD textures (Aurora's own replacement; Dolphin's packs
   don't apply); macOS; a Linux GPU (WSL's llvmpipe tint).
5. Cheaper still: the interpolator's own bookkeeping (~10% of the GPU thread), VideoCommon's decode of
   a stream Aurora parses again, Dawn's per-draw cost; `Fifo::RunGpuLoop`'s idle spin reads the clock
   through a syscall on this kernel (a third of the thread's samples when capped: not work, but power).
6. Upstream `patches/` (the fork's commits) to ModernGekko.

## What is where

- Branch: `aurora-backend` in the worktree `~/projects/skies-aurora-backend`, cut from origin/dev
  a7aa0f8. Kept local by the user's choice: no upstream (`git branch --unset-upstream`), pushes blocked
  (`branch.aurora-backend.pushRemote = no-push`), never rebased onto newer dev, never merged. dev and its
  releases carry none of this work. The main checkout's stale session-5 copies of these docs were removed
  from its working copy on 2026-10-22 (backup: `~/projects/skies-aurora/main-checkout-stale-2026-10-22/`).
- Fork: `~/projects/skies-aurora/GXRuntime` (git, 54 commits over the vendored copy, last 0054 (0054: a kept picture presented at a retrace mid-frame (60 fps) no longer lets the next frame's first pass clear the half-drawn EFB; 0053: an EFB copy sampled as a bigger texture gets the texture's memory decoded around it, not zero (the world map menu's panels share the screen grab's 1024x512 buffer); 0052: the GX state's mapped render viewport / scissor kept in step with the render size at the frame start and the replay context switch, so a resolution change or window resize no longer leaves line and point sizes, the full-screen quad check and the texture LOD bias at the old size until the game sets its viewport again (seen as a save state changing the red save point's beam at 4x); 0051: the audio queue held near a target latency; 0050: EFB copies kept across a resize, only the reuse cache cleared; 0049: System32 as Dawn's runtime search path, so Vulkan and D3D11 start on Windows; 0048: `DOL_AURORA_BACKEND` picks the API tried first; 0047: an EFB copy rebuilt from another build's RGBA8 texels; 0046: a host's own event poller; 0045: loader threads off a pinned thread's cores; 0044: the replay context's own TLUT slot ids, `aurora_backend_in_replay_context`, `aurora_backend_synchronize`; 0042: texture replacement names of seven parts, BC7 off on Qualcomm's Android driver, `replacement_stats`; 0043: replacement files decoded on loader threads, the game's texture drawn until ready; 0040: the Skies release keyboard layout as the fallback, 0041: full stick deflection for a direction bound apart, cleared axes at rest, `aurora_backend_pad_default_keys`, 0027: stale and padded EFB copies, 0028: draw-log clear colour, 0029: EFB copy snapshot / restore for the standalone's save states, 0030: the low-power adapter switch, 0031: `AURORA_GPU_PROF=N` per-pass GPU times, 0032: constant-source logic ops without a snapshot, 0034: OR/AND logic ops onto 0x00/0xFF EFB channels as max/min blends, 0035: one-value EFB copies in R8, 0036: scheduled presents at a host-given time + a present callback, 0037: no present resample pass with the bilinear sampler, 0038: live texture filtering / anisotropy / MSAA and the sharp bilinear and bicubic output samplers, 0039: MSAA 2x / 8x listed from the Dawn patch's query (`aurora_get_msaa_modes`), 0033: the display copy clear as a load-op clear, 2026-10-25); `patches/` = format-patch of its commits), copied
  verbatim into `lib/ModernGekko/vendor/dolphin/GXRuntime` (and `C:\dev\SkiesAuroraRun`'s copy).
- On that branch: `patches/moderngekko-aurora.patch`, `patches/recompcore-aurora.patch` (regenerate with
  `tools/regen-recompcore-patch.sh aurora` after editing the working copy), `tools/android-ws-build.sh`
  (AURORA, AURORA_DEPS), this file.
- Research tree: `~/projects/skies-aurora/{android-deps, android-build, android-bench, capture/, inproc/,
  inproc-win/, states/}`; `HANDOFF.md` there has the desktop tools.
- Devices (2026-10-22, 14:00): the Pocket 5's test app (`com.skiesofarcadia.recomp.test`) runs the issue
  #9/#12 session's ws32 build (`skies-recomp-ws32-2026-10-22-test.apk`, disc re-patched to ws32); the Nova's
  runs the 12:17 Aurora APK (before the overlay). For an Aurora run, reinstall
  `build/android/apk/skies-recomp-aurora-2026-10-22-test.apk` from this worktree (12:34, with the overlay;
  built from the ws30 DOL, so the app re-patches the disc back to ws30). The other session borrows the
  devices after asking.
