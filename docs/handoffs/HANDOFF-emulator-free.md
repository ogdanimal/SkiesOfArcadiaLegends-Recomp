# Handoff: the emulator-free path (decided 2026-10-22)

> **2026-11-19:** the Windows test folders this file names under `C:\dev` (`SkiesAuroraRun`, `SkiesUncensor`, `SkiesPal60`,
> `SkiesLang`, `SkiesGfx`, `SkiesPerf`, `SkiesCI`, `fmabench`) were deleted; mentions of them are history.
> `C:\dev\SkiesRecomp`, `SkiesRomhack`, `SkiesIssues`, `SkiesStandalone` and `SkiesAurora` remain.

The goal: the port runs without Dolphin's core. The recompiled game (DolRecomp's output, the same
modules the port builds today) runs on **GXRuntime**, ModernGekko's standalone runtime
(`lib/ModernGekko/vendor/dolphin/GXRuntime`: CPU state, big-endian guest memory, boot, devices,
high-level emulation of the SDK), with **Aurora** drawing the picture. The Dolphin build keeps shipping
until the standalone build reaches parity; nothing here changes the default builds.

**Performance goal (the user's, 2026-10-25):** the standalone must run *better* than the Dolphin build, and
weaker hardware must run it better too. The baseline is the last section (*Speed against the Dolphin
build*, *GPU time per frame*, *A battle scene and frame pacing*): ahead on CPU (16-25% on a desktop CPU,
30-65% on E-cores, 0.45 vs 0.60 cores at full speed at the save point, 0.35 vs 0.49 in battle) and on GPU
(about 44% less GPU time per frame than Dolphin's Vulkan renderer on an iGPU at 6x after forks 0032-0035:
1.30x vs 0.77x, and at full speed there the standalone holds 33 ms frames while Dolphin is late on every one;
the first baseline said the opposite, from a Dolphin run at the wrong resolution). On the Retroid Pocket 5
(*The Android shell*, *Pacing on the Pocket 5*, at the end): 22-25% faster uncapped than the Dolphin build on
the same DOL and module, and since the pacing fix (scheduled presents on the throttle's clock, fork 0036, and
the emulation thread on the prime core) smoother at full speed at ~0.4-0.5 W less (battle p99 34.2-34.4 vs
37.7-38.3 ms, save point 34.7-35.0 vs 36.2-36.3); at 6x it holds full speed where Dolphin can't (GPU-bound
1.24x vs 0.87x; *The Pocket 5 at higher internal resolutions*). Since the video thread (2026-10-26, *The video
thread*: Aurora off the game's thread) the Pocket 5 runs uncapped 1.77x / 2.15x at 30 fps (save point /
battle) and 1.70x / 2.05x at 60 fps, against the Dolphin build's 1.38x / 1.63x and 1.41x / 1.69x; 60 fps at
full speed p99 ~17.7 ms against 24.5-25.9, at 0.4-0.9 W less; with the PNG HD pack, half its memory.

**Status (2026-11-18): the standalone is the runner of an opt-in test prerelease.** origin/dev merged in,
the Android app plays on it (dev's launcher + `skies/SkiesGameActivity` in a `:game` process), CI packages it
for all four platforms, `v0.90.0-standalone.4` is a verified draft (*HANDOFF-feature-gaps.md* items 9-10,
*HANDOFF-release.md*). **2026-11-19: `v0.90.0-standalone.6` is published** as a prerelease, the first public
test build (the `issues` branch with ws35 on all discs, touch controls, cheats, the parity features, the review
fixes; *HANDOFF-release.md*, first section); `.4` was never published and its draft is deleted, and the
front-page README says where the project stands (v0.83 recommended, the new engine in testing). Next after testers' reports: issue #26 (needs the reporter's state), then a normal release replacing
the Dolphin build. **Review (2026-11-19 afternoon):** `REVIEW-standalone-2026-11.md` (a Fable review; its *Fix
status*): the High (a crafted state's card file names) and the Mediums fixed and pushed (`ceecbed`), the same card
flaw found and fixed in the Dolphin build (`recompcore-gci-state-paths.patch`; the dev fix on a local branch,
*HANDOFF-release.md*); `.4`'s draft is deleted. **Decided 2026-11-19: the Dolphin engine will be dropped**
(the owner: supporting both isn't feasible); the plan and inventory are [HANDOFF-dolphin-removal.md](HANDOFF-dolphin-removal.md).

**Parity (2026-11-19, after the owner's "do all of them except not worth it"):** the new engine now has every
Dolphin-build feature the owner wanted: desktop Renderer choice, touch controls opacity / size, the Menu /
Invincible hotkeys rebindable, custom GPU drivers (Turnip) on Android with the crash safeguard, and MSAA 2x / 8x
from a patched Dawn that CI builds (`standalone-host.yml` per platform since `ceecbed`, `tools/ci-dawn.sh`, cached; the stock package
when it fails). Still only in the Dolphin build: Android 7 / 8 (the new engine needs 9+, older devices fall back
to the Dolphin engine) and the Windows File menu (both left out by the owner), netplay (unused), and cheat codes
beyond plain memory writes (encrypted Action Replay, conditionals, code-patching Gecko types). The gap that
matters now is play time, not features: checked on sweeps (47 starts on WSL, 31 on the Pocket 5), bench scenes and
routes, not yet played through. CI Dawn state: Linux and Android packages built (13-28 min) and checked (Linux
"MSAA modes: 1x, 4x, 8x", the Pocket 5 menu offers 2x); Windows fixed twice (dependencies fetch 61aed3d, Windows'
`tar.exe` 2a50e0c); run 44577309166 built all four, and CI's Windows package gives "MSAA modes: 1x, 2x, 4x, 8x" on
the RTX 5080 (D3D12); macOS built, not yet run on a Mac. The
Performance goal paragraph above holds; its numbers are from 2026-10-25/27.

**Status (2026-10-26):** phases 1-3 done (title screen, audio bit-exact with Dolphin's AX HLE, first
battle at full speed, card saves); phase 4 under way: in-game menu, save states and auto-save, and the
feature-gap plan's items 0-6 done on desktop (European / Japanese discs, Continue, graphics options with
MSAA up to 8x, input with rumble / mapping / controller follow, Language, HD textures, 60 fps), items 5 and 6
checked on the Pocket 5 too (fork 0045), Aurora moved to a video thread (fork 0046), and item 7 (the
Dreamcast features: Uncensored, the Dreamcast music and streams, the European 60 Hz audio) done on desktop
and the Pocket 5; item 8 mostly done (the runner contract, Dolphin `.sav` states converted at load, the
launcher patch, controller Device); graphics APIs on Windows: D3D12, then Vulkan and D3D11 (they never
started before fork 0049), a dialog when none starts; a GUI program on Windows (no console window, the log in
`USER/Logs`); the Language restart on Android; open: the Android shell and packaging (items 9-10,
waiting for dev to be merged in, on hold by the user), and the device checks of items 0-4; phase 5 (Android) started: a test app that plays on the Retroid Pocket 5 (from a
state; pacing fixed; higher resolutions measured; immersive; Home and back work, with the video thread too;
the built-in pad and audio on the device untested by hand; no launcher yet). **macOS (2026-10-27):** builds
unchanged and plays on Metal, ahead of the Dolphin build on the same M4, plus PGO and QoS fixes (*macOS* at
the end).

**Sweep (2026-11-19, ws35, WSL, Vulkan on llvmpipe):** 47 starts, each run twice from the same start
with the same presses (A, D-pad, B every second for 40-50 guest seconds; `SKIES_RTC=847000789`), frames every
300 compared by hash: the Dolphin build's repo states (USA 01-22, European 05 / 07 / 10 / 17), the user's own
release-app states from the Pocket 5 (Oct 11-17, hours in: later towns and the save screen) and its test app's
(USA, European, Japanese), and Continue from each of the release app's 7 card saves (`--continue N`; auto-load
done at 6.21 s guest every time). **No crash or error line, and A = B on every frame of every start.** Contact
sheets by eye: battles (ground, deck, ship, a Japanese special move), cutscenes, the European title, the
Japanese save screens, towns, the world map; nothing wrong. Scripts in the session's scratchpad (`sweep.sh`,
`card-sweep.sh`, run by `ws35-run.sh`, a copy of `build/sa-runs/wsl-conv.sh` with relative press patterns).
Now in the repo as `tools/sa-run.sh` and `tools/sa-sweep.sh` (times relative to the loaded state: `--seconds
+N`, `SKIES_PRESS "+T:A"`, `SKIES_POKE "@+T:..."`). **On the Pocket 5** (`tools/sa-device-sweep.sh`, the test
app's ws35 APK, Turnip not used): 31 USA starts (the repo states, the user's release-app and test-app states)
for 40 guest seconds each with the same presses, **all at 1.00x, no error line**; contact sheets by eye match
the WSL frames' scenes (battles, ship battle, cutscenes, the world map, save screens, Pirate Isle).

**2026-10-27:** a scene not run before found something again: on a fast emulated drive the USA intro
never reached the title screen; disc reads now follow Dolphin's drive model (03d1dc8, feature-gaps item 7).
The Mac session's sweep found a save-state bug at internal resolutions other than 2x (*macOS*, *Sweep*).

**Where it stands (2026-10-26):** as an engine it is ahead of the Dolphin build on every number measured
(speed at 30 and 60 fps, pacing, power, memory, GPU time at high resolutions; pictures matched to Dolphin's
exact frames) and deterministic, which makes regressions easy to catch. It is not yet the better *port*:
items 7-10 are missing (no player could run it without adb pushes), it has been checked on bench scenes,
the opening, the first fight route and save states rather than played through (each new scene or device so
far found something: display lists rebuilt after the tokens, loader threads on the pinned core, the
governor's clock), and it carries 46 fork patches and hardware models of its own on a local branch 40+
commits behind dev. Confidence would come from items 7-10 plus long play, or an automated sweep comparing
both builds' frames over save states spread through the game. The latest sections at the end of this file come first when picking up. **The plan for the
remaining Dolphin-build features (60 fps, Continue, Language, Dreamcast, HD textures, input, graphics options,
the launcher, packaging) is [HANDOFF-feature-gaps.md](HANDOFF-feature-gaps.md).**

Why this and not the Aurora-inside-Dolphin backend alone: inside Dolphin, Aurora only reaches parity with
Dolphin's Vulkan renderer (sessions 5-6, `HANDOFF-aurora.md`); it pays off once the port leaves Dolphin
(a smaller program, no emulator, a renderer and runtime the port owns). A decompilation port in the
Metaforce / Dusklight style is not an option yet: the Skies decomp matches about 9% of the code, mostly
SDK and MusyX library code (`~/projects/SkiesOfArcadiaLegends-Decomp`, `build/GEAE8P/report.json`).

## Where GXRuntime stands (its README, 2026-10-22)

| Area | State |
|---|---|
| CPU semantics | verified against Dolphin's interpreter in lockstep; headless tests |
| Devices (VI, PI, DSP registers, SI, EXI, memory card) | working, headless tests |
| Graphics | gxcore is its default renderer; the vendored Aurora is the platform substrate |
| Audio | audio DMA output only (`src/audio_dma.c`); no DSP microcode emulation (DSP-LLE is on its roadmap) |
| Client | StrikersRecomp (Super Mario Strikers): boots to matches, ~11 fps in a match |
| Hosts | macOS tested; Linux builds in CI; Windows on the roadmap; Android not mentioned |
| HLE layer | `src/hle/` (core, card, DVD, input): about 1,500 lines, written for its client |

## What the Skies port gets from Dolphin today (all must be re-hosted)

The port's Dolphin-side features are about 25,000 lines of patches (`patches/recompcore-*.patch`), each
written against Dolphin's APIs:

| Patch | Lines | Depends on |
|---|---|---|
| uncensor | 3,475 | DiscIO (the Dreamcast disc, model conversion) |
| frame-interpolation (60 fps) | 3,096 | VideoCommon (recording, replay, presenter) |
| skies-dcmusic-eu | 2,840 | AudioCommon, DiscIO |
| skies-android-app | 2,403 | Dolphin's whole Android app (activities, input, surface, settings UI) |
| skies-settings (in-game menu) | 2,367 | VideoCommon OnScreenUI (Dear ImGui), Config |
| input (keyboard, remapping) | 1,822 | InputCommon |
| hd-textures (+ nested) | 1,986 | VideoCommon texture cache (Dolphin's pack format) |
| pal60 | 1,194 | DiscIO streams |
| dcmusic-streams, data-folder, field-speed, language, japan, romhack, match-screen, hud, ... | ~4,000 | various |
| aurora | 5,687 | the Aurora backend itself (most of it carries over) |

The Dolphin build's save states are Dolphin's format and don't load in the standalone build (it has its own,
see *Save states*). Memory card saves are plain GCI files and carry over.

## The three hard gaps

1. **Audio.** Skies mixes its sound with **MusyX 2.0** (Factor 5) on the DSP (`docs/sdk_import.md` in the
   decomp). Without DSP emulation it is silent. Options: a DSP emulator in GXRuntime (Dolphin's DSP LLE
   interpreter is GPL like GXRuntime), or a native MusyX engine on the host (AxioDL's **amuse**, which
   Metaforce uses; the decomp vendors AxioDL's MusyX sources), hooking the game's MusyX calls.
2. **60 fps.** The frame interpolator records and replays the GX stream inside VideoCommon. Outside Dolphin
   it has to be rebuilt on GXRuntime's command path and Aurora (the replay-context work of sessions 5-6 is
   the Aurora half).
3. **Android.** A new Android shell (Surface, input, lifecycle, the disc import, the settings panel) instead
   of Dolphin's app.

## What carries over

- The Aurora fork (`~/projects/skies-aurora/GXRuntime`, 44 commits, `patches/` there): Skies renders exactly
  like Dolphin on the Retroid Pocket 5, the Nova, Windows and WSL; 60 fps through a second GX context;
  the present rectangle (Picture option); the Dear ImGui overlay (the in-game menu). All of it is in the
  vendored GXRuntime on the local `aurora-backend` branch.
- The recompiled modules, the game patch (widescreen, QOL, cutscene framing), the build scripts, CI codegen.
- Card saves; the disc import (DiscIO is used by the launchers, which stay Dolphin-free already).

## Plan, with a gate per phase

1. **Boot spike (about one session).** *Done 2026-10-24.* Build a standalone runner (StrikersRecomp as the template) that loads
   the existing USA module on GXRuntime on Linux or Windows, Aurora drawing. Gate: the title screen. Decide
   gxcore vs the Aurora gx command path for Skies (the latter is the one verified frame-for-frame).
2. **Audio decision.** *Done: Skies' MusyX ucode is Dolphin's AX, ported as HLE; bit-exact.* Prototype amuse against a DSP emulator. Gate: the intro music plays, in time.
3. **Playable on desktop.** *Done 2026-10-25 (WSL and Windows D3D12).* Input, card saves, disc reads and streamed audio, speed. Gate: the first battle at
   full speed, a card save written and read back.
4. **Features re-hosted** (*under way: menu, save states and feature-gap items 0-6 done on desktop*) in priority order: settings menu (the ImGui overlay exists), 60 fps, then the rest
   of the table above.
5. **Android shell** (*started 2026-10-25: test app, last section*), then standalone builds released beside the
   Dolphin builds until parity.

Where the work lives: the local branch `aurora-backend` (worktree `~/projects/skies-aurora-backend`, cut
from origin/dev a7aa0f8). It is deliberately kept local: no upstream, pushes blocked
(`branch.aurora-backend.pushRemote = no-push`), not rebased onto newer dev, not merged. Phase 1 starts
from this branch, whose vendored GXRuntime carries the fork fixes and the overlay.

## Phase 1: boot spike (2026-10-24) — gate reached

The title screen renders with no emulator core: SEGA / Overworks logos, the intro text, the logo, the
opening cutscene, and (Start pressed) the real title screen with PRESS START. WSL software GPU,
unthrottled: 2-3x real time with Aurora, 4-7x headless.

**What it is.** `standalone/` (CMake, C++20): `skies-standalone --game <extracted disc> --module
<gGEAE8P_recomp.so>`. It loads the **existing** module (StaticRecomp ABI v3) with `dlopen`, no regeneration,
and gives it the hooks RecompCore gives it (MMIO, SPRs, cache ops, gather pipe, instruction fallback).
Around it, hardware models written from Dolphin's (GPL-2.0+): PI, VI (half-line beam clock, display
interrupts), CP/PE (FIFO, linked mode, tokens, draw done) with a command splitter that tracks VCD/VAT
so side effects happen in stream order (display lists walked too), DSP (CSR, ARAM DMA, audio DMA), DI
(reads from the extracted folder through the disc's own `fst.bin` offsets), SI (pads), EXI (IPL: SRAM,
RTC, the UART OSReport writes to), AI. The GX stream goes to Aurora with `aurora_backend_write_raw`, as
the Dolphin-side backend does; presentation at the game's display copy.

- Build: `cmake -S standalone -B build/standalone -G Ninja -DCMAKE_BUILD_TYPE=Release`
  (`-DSKIES_STANDALONE_AURORA=OFF` for headless). Env: `SKIES_DUMP_DIR`/`SKIES_DUMP_EVERY` (PPM frames),
  `SKIES_PRESS="38:START,..."` (scripted pad, guest seconds), `SKIES_MUTE`, `SKIES_MMIO_RANGE` with
  `--mmio-log`. Flags: `--headless --unthrottled --seconds N --idle-pc --dsp-coef --continue N|card`.
- Boot: the apploader is not run (no interpreter for it); `boot.cpp` does its work (DOL sections, bi2 +
  FST at the top of MEM1, low-memory pointers) plus Dolphin's BS2 state. Don't clear the DOL header's BSS
  range: it spans `.sdata`.
- Exception vectors are not in the module: the host emulates the SDK stub (`__OSEVStart`, checked word by
  word on every use) and the 0xC00 system-call handler.

**Audio: the gap is smaller than this handoff said.** Skies' MusyX uploads `dspSlave` (0x19E0 bytes),
Dolphin hash **0x4e8a8b21 = the AX ucode**, which Dolphin runs in HLE (that is how the shipping port
plays sound). So phase 2 is not a DSP emulator or amuse: `standalone/src/dsphle/` is a port of Dolphin's
DSP HLE (mailboxes, ROM/INIT, the whole AX ucode with voices and the accelerator), and the game's MusyX
boots it (log: "CRC 4e8a8b21: AX ucode chosen"). Not yet listened to (runs so far were muted).

**Instruction fallback.** An `rfi` into the middle of a recompiled block (the lazy FP-unavailable
switch) makes the LLVM module call `instruction_fallback` until it reaches an entry point; RecompCore
uses Dolphin's interpreter. `standalone/src/interp/` is a one-instruction interpreter transliterated
from DolRecomp's C emitter on GXRuntime's helpers, checked against the emitter's own output (632k
comparisons, all 236 opcodes, 0 mismatches). About 45k fallback instructions per guest minute.

**Gotchas found:** DI completion must do DIMAR += DILENGTH, DILENGTH = 0 for every command (the SDK
retries Inquiry otherwise); WPAR's BNE bit must read 0 (Dolphin's choice; GXFlush spins otherwise).

**Next (phase 2/3):** listen to the AX output (unmuted run on Windows or with WSLg audio), real-time
speed on a real GPU (Windows build), then gameplay to the first battle.

## Memory card (2026-10-24) — working

`--user USERDIR` inserts a card in slot A over `USERDIR/GC/<USA|EUR|JAP>/Card A/*.gci`, the folder and
file names the Dolphin build uses (no `--user`: no card, as before). `hw_memcard.cpp` is Dolphin's
`EXI_DeviceMemoryCard` (command bytes, status, erase/program timing, delayed DMA completion, no
transfer-complete for immediate transfers) over a whole-card image instead of `GCMemcardDirectory`'s
per-save buffers: a 2043-block card (16 MB), header formatted from the SRAM flash ID at the current time
and the SRAM flash ID set back from it (`SetCardFlashID`), this game's GCI files laid out contiguously
(current game only; other games' files are left alone). A flush, one guest second after a write that
ends a block and at exit, walks the directory and block map with the higher update counters and writes
each save whose bytes changed (atomically, via `.tmp`); a save gone from the directory is renamed
`.deleted`, as Dolphin does. Chip erase is a no-op, as `GCMemcardDirectory::ClearAll`.

Checked on WSL (Aurora build, copies of real saves in `build/sa-runs/user-card*`): Continue lists the
saves and loads one into gameplay (the Pirate Isle underground dock); saving over #01 at the dock's save point
writes only that GCI, and a fresh run lists and loads it; saving into an empty slot creates
`8P-GEAE-SA_LEGENDS.001.gci`. An unchanged card writes nothing. Not yet checked: the Dolphin build
loading a GCI the standalone wrote (same format, written by the game's own CARD library).
**EFB peeks: not needed.** Skies never reads or writes the EFB from the CPU: `GXPeekARGB`/`GXPeekZ`/
`GXPokeARGB`/`GXPokeZ` aren't linked into the DOL (`config/GEAE8P/symbols.txt` in the decomp has only the
`GXPoke*Mode` setters), and a run from boot through Continue to gameplay made no 0xC8000000 access
(`--mmio-log`, `SKIES_MMIO_RANGE=c8000000-c9000000`). Reads return 0, as the Dolphin build's Aurora
backend does (Null EFB interface).

Scripted route to the save point (USA, unpatched DOL, the "P. Isle Underground" save as the newest file): `SKIES_PRESS="38:START,
53:START,56:A,60:A,63:A,66:A,73:RIGHT,73.2:RIGHT,75:A,77:A"` reaches the Save file list.

## The widescreen DOL (2026-10-24) — working

The ws32 game patch DOL runs with its own x86 module (`~/projects/skies-pal60/build/ws32`: plain LLVM,
no PGO, built from `skies-pal60/build/game/GEAE8P-ws32/sys/main.dol`; its `build.sh` shows how). Game
folder: `build/sa-runs/game-ws32` (links to the extracted disc, `sys/main.dol` -> the ws32 DOL); the run
scripts take `GAME=` and `MODULE=`. Checked on WSL: logos, intro, title, Continue, the load and save
screens and the field, all at 16:9 (HUD minimap at the right edge, menus a centred 4:3 panel), the card
saves as with the plain DOL. About 10% slower than the plain DOL's PGO module on llvmpipe.

- **Fix: boot loads `sys/main.dol` itself.** It used to read the DOL through the disc offsets, but the
  disc only maps the original DOL's slot (it ends where `fst.bin` begins), and a patched DOL is longer:
  both patch sections (code at 0x80001900, data with `ws_config` at 0x80002A00) are appended at the
  end, so they were read from the FST. The recompiled code was right but the patch's data and constants
  were not, and every frame came out black (ortho x scale 1.7e-37, empty scissor). The disc's DOL
  extent is now clipped so it can't overlap the FST.
- **Picture shape:** `video.cpp` reads `ws_config.display_aspect` (magic `WSCF` at 0x80002A00) after
  boot: a ws DOL presents at its aspect (16:9 by default; nothing writes it yet), the plain DOL at 4:3.
  As the Dolphin-side backend: the EFB stays 640x480 times `SKIES_EFB_SCALE` (default 0 = follows the
  window) with `aurora_backend_set_frame_buffer_aspect_fit(1)`, and `aurora_backend_set_present_rect`
  letterboxes the picture in the window (default window 960 lines high at the picture's aspect,
  `SKIES_WIDTH`/`SKIES_HEIGHT` override). Frame dumps are the EFB (anamorphic for ws; stretch them to
  16:9 to view). The window itself hasn't been looked at, only the dumps.
- Debug switches added while finding it: `SKIES_GX_TRACE=N` logs frame N's BP/CP/XF loads, draws and
  display-list calls; `SKIES_WATCH=ADDR` logs the block after which a RAM word changed. Later: `SKIES_DI_TRACE=1`
  (each disc read, with its file's name since 2026-10-27), `SKIES_DI_FAST=1` (the disc timing before
  2026-10-27, for routes recorded with it).

## First battle and the graphics check (2026-10-25)

**First battle reached and won** on WSL with the plain USA DOL: the "Valuan Battle Ship" save (copy of
`build/user-ms`'s #01 in `build/sa-runs/user-card3`), a scripted walk until the encounter, A through the
fight, the results screen, back in the field. Repeatable: `SKIES_RTC=847000789` (fixed RTC start, guest
time after; runs with the same seed and input are identical) plus `build/sa-runs/fight.txt` gives the
encounter at 124.5 s and the field again at 173.8 s (`SKIES_WATCH=803475cc` shows the game mode: 6 field,
7 battle).

**Bug found and fixed: stale EFB copies (Aurora).** The results screen drew the battle's ship walls
instead of its parchment panel. The field keeps its UI atlas (1024x512 RGB5A3) at heap address
0x80667540; the battle transition copies the screen into that buffer; after the battle the field reloads
the atlas to the same address, byte for byte. Aurora keeps EFB copies on the GPU, keyed by guest address,
and never let go of that one: nothing ever told it the memory had been reloaded. Fixed in two parts, as
Dolphin's texture cache does it (`TextureCacheBase::UninitializeEFBMemory`):
- the host zeroes the copied tiles in RAM when it processes a copy (`MarkEfbCopy`, `hw_gpu.cpp`), so a
  reload always changes the memory (the Dolphin build's VideoCommon already does this);
- Aurora (`gx.cpp`, `gx.hpp`, `GXFrameBuffer.cpp`): each copy records a fingerprint of its destination
  memory (128 evenly spread words + first + last, XXH3), and `resolve_sampled_textures` drops the copy
  and uses the memory once the fingerprint changes.

**Tools for checking the picture against Dolphin** (debug, all env):
- `SKIES_DFF=FIRST:COUNT:PATH[;...]` writes the frames ending with XFB copies FIRST.. as a Dolphin FIFO
  log (`dff.cpp`: registers shadowed from boot, RAM at each frame's end as memory updates). Stock Dolphin
  renders it (`build/sa-runs/tools/dolphin-dff.sh IN.dff OUTDIR N`, the Windows LaunchBox Dolphin), so any
  difference to the standalone's own frame is Aurora's. Caveat: an EFB copy made before the recorded
  frames is not in the log (Dolphin reads zeros there), so record enough frames to include it.
- `SKIES_DUMP_FRAMES=N,N,...` dumps just those frames; `SKIES_DUMP_FROM`; `SKIES_COPY_LOG=1` logs every
  EFB-to-texture copy (frame, destination, rectangle, format, stride); `SKIES_DRAW_LOG_FRAME=N` Aurora's
  draw log for one frame (with `SKIES_DRAIN=0`); `SKIES_DRAIN=N` Aurora's FIFO drain threshold.
- `build/sa-runs/tools/compare-sweep.sh` puts a sweep's standalone frames beside Dolphin's and scores
  them; `tools/dolphin_battle.py` drives the Dolphin port (`moderngekko-run --automation-dir`) through a
  battle with screenshots.

**Bug found and fixed: copies drawn as bigger textures (Aurora).** The battle transition copies the
640x480 screen into a 1024-wide RGB5A3 buffer and draws it as a 1024x512 texture; Aurora's copy was a
640x480 texture, so the game's UVs stretched its left 62% over the quad (the snapshot zoomed in,
minimap cut off). A copy bound bigger than it was copied now gets a texture of the bound size with the
copy in its top-left corner (fork 0027, `padded_copy_texture`; copy textures gained CopySrc).

**Picture checks against stock Dolphin (2026-10-25), all on the standalone's own command stream** (the
`.dff` of each frame rendered by Dolphin on Windows, `tools/sweep.sh`; share of pixels more than 10%
off, best of the seven aligned frames): the opening dungeon route (title, load screen, red-alarm field,
battle transition, battle, results, field again) 0.0-0.8% where the frames line up, 2-4% in battle
(sub-pixel filtering on the wood grain); the world map in the airship (cloud sea, a floating island)
0.3-0.6%; Pirate Isle's caves 0.1-9% (the high ones mid camera pan); the ws32 DOL (anamorphic EFB)
0.1-5%. Nothing left that differs. Caveats: an EFB copy made before a capture starts isn't in it
(Dolphin then reads zeros), and Dolphin's FIFO player never dumps a capture's last frame (dump k is
frame k-1). The live window was screenshotted too (X11 window via `SDL_VIDEO_DRIVER=x11`): 16:9
window full for the ws DOL, 4:3 window letterboxed.

The Dolphin build's Aurora backend gets the same fork fixes (`recompcore-aurora.patch` regenerated);
`moderngekko-run --graphics Aurora` from a battle savestate shows the results screen right. Its PNG
dumps carry the EFB's alpha: view them with `-alpha off` (alpha-0 areas otherwise look white; that cost
an afternoon of a false "white sky" bug).

**Audio checked: bit-exact.** `SKIES_WAV=FILE` dumps the audio DMA output; the first 45 s (logos,
intro, title) equal the Dolphin build's `dspdump.wav` sample for sample on both channels (Dolphin's
writer skips leading silence: 3.19 s offset). Found on the way: the DMA's samples are right channel
first, and the host had passed them through, so left and right were swapped (fixed). Skies uses no disc
audio streaming (DTK off in the disc header). Not yet heard by a person: run it unmuted on Windows.

## Windows build (2026-10-25) — runs at full speed with headroom

`C:\dev\SkiesStandalone`: `standalone\` and the parts of the vendored tree it uses (GXRuntime,
DolRecomp's decoder + `common`, the StaticRecomp ABI header, `dsp_coef.bin`), dependency sources in
`deps\` (copied from WSL's `build/standalone/_deps`; Dawn, libpng and zlib from
`C:\dev\SkiesAurora\build\_deps`). `build.bat configure|build` (MSVC from VS Build Tools 18, Ninja);
Dawn's DLLs and libpng/zlib beside the exe. `run.bat [args]` logs to `%RUNLOG%`. Sync sources from WSL
with `rsync -a standalone/ /mnt/c/dev/SkiesStandalone/standalone/`. Code changes for Windows: `LoadLibrary`,
MSVC byte swaps in the DSP HLE, a static page-aligned RAM. New options that came with it: `--dol FILE`
(a patched DOL without copying the disc), `SKIES_PRESS_FILE` (cmd's `set /p` cuts at 1023 characters),
`SKIES_POKE=ADDR=WORDS` (guest words written every frame, e.g. the QOL patch's `qol_input`).
Since 2026-10-25 (late night) also `build-dawnsrc.bat configure|build`: the same build with Dawn compiled from
the patched tree at `C:\dev\dawn-src\dawn-266c1cf` into `build-dawnsrc\` (MSAA 2x / 8x; details and its two
traps in HANDOFF-feature-gaps.md, item 2). The bat lives only in `C:\dev\SkiesStandalone`.

- Modules: `VerifyModule` hashes every chunk against the DOL in RAM at boot and refuses a module for
  another DOL (it found that `C:\dev\SkiesGfx`'s DLL matches no ws DOL exactly). Matching pairs:
  `C:\dev\SkiesRecomp\build\modules-sa-plain-pgo` (plain USA DOL, LLVM + the WSL PGO profile, built
  with `tools/module-llvm-win.sh sa-plain-pgo --profile-use ...` in the main checkout), and
  `C:\dev\SkiesPerf\build\modules-{perf,layout}` with their `generated\main.dol`.
- Speed (NVIDIA, D3D12, unthrottled, 1280x960 window): 5.5-6x on logos/title, 2-3.5x in the field and
  battle; headless 5.3-5.8x. The old Oct-6 plain module is 2.3x headless (old chunking, many fallbacks).
- Same game as WSL: with the same DOL, seed and input the Windows run hits the encounter at 124.487 s
  and the field again at 173.812 s, exactly as on WSL; still frames are pixel-identical to WSL's
  (llvmpipe Vulkan), moving ones differ only in an effect's phase.

## In-game menu and player options (2026-10-25) — working on WSL and Windows

`standalone/src/menu.cpp` ports the Dolphin build's menu (`VideoCommon/SkiesMenu.cpp`) and settings
(`Core/SkiesSettings.cpp`): Z+Start, Esc or a controller's Back / Select opens it over the game, laid
out and worded as in the Dolphin build, drawn with Dear ImGui (Dolphin's copy in `Externals/imgui`,
compiled into the exe) through Aurora's host overlay (fork 0026; the standalone runs ImGui 1.92's
texture protocol itself and hands the font atlas to `aurora_backend_set_overlay_texture`). Tabs:

- **Graphics**: Picture (16:9 / Match screen / 4:3, through `ws_config`; the window letterboxes to it),
  HUD (`ws_hud_aspect`), Resolution (`VISetFrameBufferScale`, live; a new first value **Match window** =
  Aurora sizes the EFB to the window, the standalone's old default), V-Sync (`aurora_enable_vsync`,
  live), Show FPS (drawn top right).
- **Gameplay**: Camera (hold Z: C-stick tilt, L / R zoom, `cam_input`), Battle speed (a Z tap in battle
  cycles it), Field speed (Z+X), Encounters, Hold B, EXP, Gold, Magic EXP, Invincible (L3 / R3 from
  SDL's gamepads), all through `qol_input` as the Dolphin build writes it every poll, including the
  step-down when the host can't keep up (`EmulationSpeed()`, guest time over wall time for the last
  half second) and the CPU overclock for 3x / 4x (`g_overclock` divides the cycles each block charges).
- **Sound**: Volume (scales what goes to Aurora's audio; the `SKIES_WAV` dump stays the DSP's own).
- **Controls**: Stick dead zone (the standalone's own key `[Skies] StandaloneDeadZone`, applied to
  controller 1's sticks; the Dolphin build keeps its dead zone in its controller profile).

Settings are the Dolphin build's keys in the same files (`settings.cpp`: `USER/Config/Dolphin.ini`
`[Skies]` and `[DSP] Volume`, `USER/Config/GFX.ini`), so both builds share one set of choices; only
changed lines are rewritten (atomically). Without `--user` they last for the run. Messages (speed
changes, Invincible, the speed step-down) are drawn top left for their time.

Not offered yet, as the standalone can't do them (save states came after, see the next section):
60 fps, Language, the Dreamcast tab,
HD textures, Button mapping, Rumble, auto-load of a card save (Continue). (Since 2026-10-25/26 all but the
Dreamcast tab are in: Continue, Rumble, Button mapping and the release keyboard layout, Language, HD
textures, Frame rate; *Input*, *Language*, *HD textures* and *60 fps* at the end, and
HANDOFF-feature-gaps.md items 1 and 3-6.)

Checked (2026-10-25): WSL window (X11 screenshots): the menu over the intro, Picture switched live
to 4:3 and saved; Esc and the arrow keys through real XTEST key events; Resolution to Native live;
Show FPS and a message drawn. Headless with the ws32 DOL (`SKIES_RTC=847000789`, the Valuan ship
route plus a box walk in the corridor's encounter zone, `build/sa-runs/box.txt`): first battle at
119.7 s with Encounters 400%, 137.7 s at 100%, none in 278 s at Off; battle speed by Z taps in that
battle: the battle ends at 171.3 s (1x), 149.7 s (2x from 125 s), 137.9 s (4x, overclock 2); the
camera's words reach `cam_input` (pitch, zoom 1.0 -> 0.5), and the view tilts; Z+X steps the field
speed and saves it. The plain DOL's fight route is unchanged (encounter 124.487 s, field 173.812 s).
Windows (MSVC, D3D12): builds, the menu draws (Segoe UI) and Right changes Picture.
Noted, not chased: on the ws32 DOL, Battle speed 2 set before boot makes the run diverge from Battle
speed 1 during the intro (both runs are repeatable; the speed word differs only on the title, where
the game patch ignores it), so the scripted route then misses the encounter zone.
Press scripts also take CUP / CDOWN / CLEFT / CRIGHT (C-stick) and LT / RT (analog triggers);
`SKIES_WATCH_MAX` raises the watch log's 40-change limit.

**Next:** someone listens to the audio (unmuted Windows run) and tries the menu by hand; Continue
(a card save loaded at boot, `autoload.c`); 60 fps (the replay context); Android shell; the remaining
Dolphin-hosted features (HD textures, Dreamcast music, romhacks, ...).

## Save states (2026-10-25) — working on WSL and Windows

`standalone/src/state.cpp` + `state.h`: the whole machine in one file. Each part has a `DoState`
(`StateIO`, Dolphin's PointerWrap pattern; a named marker before each section): the scheduler (the
clock and each slot's due time; handlers by slot, `SchedRegister`), the CPU (`CPUState` up to its host
hooks, the SPRs, the locked cache, the decrementer / time base leftovers; the module's
`on_state_loaded` after), RAM, PI, VI, the GPU side (gather pipe bytes, CP FIFO, PE, the parser's
vertex formats and pending bytes), the GP registers Aurora has (`dff.cpp`'s shadow: BP, CP, XF, TMEM,
plus the last TLUT load per TMEM offset), DSP (registers, pending interrupts, ARAM, the DSP HLE:
Dolphin's `DSPHLE` / `UCodeInterface` / `AXUCode` / `ROMUCode` / `Accelerator` / `CMailHandler`
DoState ported back), DI, SI (the pads as last polled), EXI + the memory card (the image and the saves
known on disk, by file name; as Dolphin's `GCMemcardDirectory::DoState`, nothing is written back until
the game writes), AI, and video (frame number, Aurora's EFB copies). zstd level 1 (Aurora's copy, or
Dolphin's `Externals/zstd` in the headless build) behind a 56-byte header with the disc ID, a hash of
the DOL booted and `ModuleFingerprint()`; a state of another game, patch or build is refused. 76 MB
raw, 12-15 MB packed, 110-180 ms to save or load.

- **When:** a request waits for the next display copy (`StateFrameEnd`), then runs between two
  dispatches of the run loop (`g_state_request`). After a load: Aurora gets the state's EFB copies back
  (`VideoStateLoaded`), then the GP registers as commands (`DffReloadAurora`, as the Dolphin build's
  `ReloadRegisterState`) ending with the display copy again (its clear: the next frame's EFB),
  and wall-clock pacing restarts (`ThrottleRestart`). A load that fails partway puts back the state from
  just before it (kept in memory without the GPU's copies).
- **EFB copies (fork 0029):** Aurora keeps a copy on the GPU only (the host zeroes the memory under
  it), so a state without them lost the save point's light and the minimap until the game copied again.
  `aurora_backend_copies_snapshot` reads every live copy back after the render worker's frames,
  `aurora_backend_copies_restore` rebuilds them as `copy_tex` does and resets the texture binds.
- **Menu:** a Saves tab first, as the Dolphin build: Save state (slots 1-5), Load state (the newest
  auto-save or a slot, A twice), Auto-save (Off / 5 / 10 / 15 / 30 min, `[Skies] AutoSave`, the same key;
  only on the field or world map outside ship battles with the card left alone 10 s; the newest 3
  kept). Files: `USER/StateSaves/Standalone/<disc ID>-slot<N>.sst`, `<disc ID>-auto-<date>-<time>.sst`
  (not the Dolphin build's: the two builds' states don't load in each other). Without `--user` the rows
  say a user folder is needed.
- **Command line and tests:** `--load-state FILE` (at the first display copy, as a menu load; a state
  that can't be loaded ends the run), `SKIES_SAVE_AT=SECONDS:PATH`, `SKIES_LOAD_AT=SECONDS:PATH` (an
  in-session load), `SKIES_AUTO_SAVE_SECONDS=N`. `--seconds` counts the state's guest clock.
- The memory card is now formatted at the emulated clock's time (`RtcSeconds`, so `SKIES_RTC` runs
  repeat byte for byte, the card included).

Checked (2026-10-25):
- **Headless, plain DOL fight route** (`SKIES_RTC=847000789`, `fight-full.txt`, `user-qa-plain`): a
  state at 100 s, loaded: the battle at 124.487 s and the field at 173.812 s as the straight run, and a
  second state at 185 s byte-identical to the straight run's (CPU, RAM, ARAM, DSP, GPU, card,
  scheduler). Same on the ws32 DOL (`user-card3`, `boxfight.txt`, 110 s -> 159 s). Taking the state
  doesn't change the run.
- **A broken state loaded at 106 s** (a section marker changed): "Couldn't load the state: section exi
  not found", and the run carries on to the battle at 124.487 s as if nothing happened. A truncated file,
  another game patch's module and a card state in a run without `--user` are refused with those reasons.
- **Window (WSL):** frames after a load match the straight run's, the first one included, once
  `AURORA_SYNC_PIPELINES=1` takes out Aurora's skipped draws while pipelines compile (without it a
  freshly started run skips the save point's and minimap's draws for a few frames, as any cold start
  does; frame dumps of two straight runs differ by that too). The Saves tab driven by XTEST keys: save
  to slot 1, "Saved to slot 1", Load state (Right to slot 1, "Press A again to load" in gold, A),
  "Loaded slot 1" with the picture right. Auto-save every 6 s (test override): the newest 3 kept.
- **Windows (MSVC, D3D12):** builds; a state at 100 s with 4 EFB copies read back (112 ms), loaded:
  the battle at 124.487 s. Not tried by hand there.

Found on the way, not chased:
- **Window runs can leave the scripted timeline under heavy load**: two of the WSL window runs with
  blocking pipeline compiles, and one Windows run while another ran beside it, reached frame 3001 later
  than the rest (101.60 s for 101.42 s) and then played differently; headless runs never did, and a
  repeat alone matched. Something wall-clock-dependent reaches the game in the window build (open item 5,
  the ws32 battle-speed divergence, may be the same).
- **Small picture differences between runs** on draws whose vertex data the CPU rewrites every frame (the
  character, the save point's particles): Aurora reads guest memory when it parses, so where its parsing
  falls behind the CPU decides which data a draw sees. Straight runs differ from each other the same way.

## Speed against the Dolphin build (2026-10-25): the baseline

**Goal (the user's, 2026-10-25):** the standalone runs better than the Dolphin build, and weaker hardware
runs it better too. Everything below is one PC (i7-12700K: 8 P-cores + 4 E-cores; RTX 5080 and the CPU's
Intel UHD 770), Windows, the **same DOL and module on both sides** (`C:\dev\SkiesRomhack` ws26 game +
`modules-llvm-pgo-ws26`), the same settings (`C:\dev\SkiesRecomp\build\user-bench`'s Config: internal
resolution 1, 30 fps mode), muted, standing at the opening dungeon's save point (Dolphin:
`savestates/05-save-point.sav`; standalone: `C:\dev\SkiesStandalone\bench-savepoint.sst`, made by the
`fight-full.txt` route at 100 s (2026-10-27 night: the 10-25 `.sst` files are state version 1 and no longer load; the current bench states are `build/sa-runs/bench-states/`, made from the Dolphin bench states, see *HANDOFF-feature-gaps.md* *Testing practice*)). Dolphin: `capture/inproc-win.sh` with `user-bench-dc` (dual core).

| | Standalone | Dolphin Vulkan | Dolphin D3D12 | Aurora in Dolphin | Dolphin Null |
|---|---|---|---|---|---|
| RTX 5080, uncapped | **2.91x** | 2.33x | 2.30x | 2.18x | 2.27x |
| UHD 770 (iGPU), uncapped | **2.82x** | 2.16x | 1.87x | | |
| UHD 770, internal resolution 3 | **2.62x** | 2.05x (first measured at 1x by mistake: 2.16x) | | | |
| UHD 770, internal resolution 6 (GPU-bound) | **1.30x** (after 0032-0034; 1.09x before 0034, 1.05x before 0033) | 0.77x | | | |
| 4 E-cores, uncapped | **1.55x** | 1.06x | | | |
| 2 E-cores | **1.50x** | 1.03x | | | |
| 1 E-core | **1.46x** | 0.88x (below full speed) | | | |
| CPU at full speed (all threads) | **0.45 cores** | 0.60 cores | | | |

- Dolphin is CPU-bound in this scene on every renderer (Null, no drawing at all, is 2.27x); the standalone
  is about 25% faster on the full CPU, 45-65% faster on E-cores, and still above full speed on one E-core.
- ~~Where Aurora loses: GPU time per frame~~ **Wrong, corrected later the same day** (next section): the
  Dolphin runs "at internal resolution 3" were at 1x. ModernGekko's runner sets Dolphin's internal
  resolution from the user folder's `config.ini` (`resolution=640x528`), not from `GFX.ini`. At a real 3x
  Dolphin does 2.05x, and GPU-bound at 6x the standalone is ~35-40% faster.
- Not measured yet: ~~a battle scene~~, ~~frame-time percentiles~~ (both measured later the same day, in
  "A battle scene and frame pacing" below), the Retroids (needs the standalone's Android shell).

Tools (copies in `~/projects/skies-aurora/capture/`, used from `C:\dev\SkiesStandalone`):
`bench-sa.sh LOG SECS` (the standalone from a state: `STATE=`, `SETS='set X=1&& '`, `AFFINITY=HEXMASK`,
`THROTTLE=` empty for real time; averages the `[host] speed` lines after two), `pin.sh EXE MASK` (pins a
running process: `start /affinity` hangs through WSL interop), `cpu-sample.sh EXE WAIT SECS` (cores in use),
`gpu-luid.sh EXE WAIT` (which GPU adapter a process keeps busy, and how busy). `AURORA_LOW_POWER_GPU=1`
(fork 0030) makes Aurora take the iGPU; Dolphin's is `[Hardware] Adapter = 1` (`user-bench-igpu`,
`user-bench-igpu3`; `user-bench-rt` = dual core at full speed). **The Dolphin side's internal resolution
is the user folder's `config.ini` `resolution=` (640x528 per 1x), not `GFX.ini`:** `user-bench-igpu3` was
at 1x for this baseline (fixed since, next section). `AURORA_GPU_PROF=N` (fork 0031, with
`SKIES_AURORA_LOG=1`) gives the standalone's GPU time per frame by pass. An unmuted standalone run
is held to 1x by the audio queue even with `--unthrottled`: benchmark with `SKIES_MUTE=1`.

## GPU time per frame (2026-10-25, later): ahead of Dolphin, and two cuts

**The baseline's GPU conclusion was a measuring mistake.** `user-bench-igpu3` had `GFX.ini`'s
`InternalResolution = 1` and `config.ini`'s `resolution=640x528`; the runner (`tools/moderngekko_run.cpp`,
`frontend_config.cpp`) takes the latter. Now `user-bench-igpu3` is `resolution=1920x1584` (real 3x), and
`user-bench-igpu6` (3840x3168), `user-bench-igpu3-rt` / `user-bench-igpu1-rt` (full speed) are new, all in
`C:\dev\SkiesAuroraRun`. Same scene and DOL as the baseline, UHD 770:

| | Standalone | Dolphin Vulkan |
|---|---|---|
| 3x, uncapped | 2.61x (CPU-limited: the GPU span is 8.7 ms of a 12.8 ms frame) | 2.05x (77% GPU busy) |
| 6x, uncapped (both GPU-bound, 98% busy) | **1.09x** (29.3 ms GPU per game frame) | 0.77x (~43.5 ms) |
| 3x, full speed: GPU busy | 58% | 78% |

GPU busy % at partial load follows the iGPU's clock governor (Dolphin shows 78% at 1x *and* at 2.05x), so
the GPU-bound uncapped speed is the comparison to use; at 3x neither side is GPU-bound.

**`AURORA_GPU_PROF=N` (fork 0031)** prints the GPU time per frame by pass every N frames (needs
`SKIES_AURORA_LOG=1` in the standalone to reach the log; on Android Aurora's info lines go to logcat, tag
`aurora`, not log.txt). At the save point, iGPU 3x, before the cuts:
span 9.86 ms, 11 passes: EFB passes 6.73 ms (4.7), plain EFB copies 1.20 ms (3), EFB copy conversions
0.73 ms (2, the shadow composite's R8 copies), staging 0.28, present resample + blit 0.48.

**The three plain copies were logic-op snapshots.** The shadow composite draws, per frame: a full-screen
quad ORing (1,0,0,1), one ANDing (0,1,1,1), and a textured OR of an R8 EFB copy. The logic-op emulation
(fork 370720f) copies the whole EFB before each and splits the pass there. **Fork 0032:** when the TEV
output is a constant 0x00/0xFF per channel (evaluated on the CPU with the shader's arithmetic; any texture,
vertex colour, lighting, compare op or fog bails), each op reduces per channel to keep / write the source /
other; with only the first two it's a plain draw with a write mask (`PipelineConfig::writeMask`, cache
version 14). The two quads take it (masks R+A and R); the textured OR keeps the snapshot. 3x: 9.86 ->
8.68 ms, 11 -> 9 passes. `AURORA_LOGIC_OP_CONST=0` turns it off. (The old blend approximation,
`AURORA_LOGIC_OP_BLEND=1`, gives 8.50 ms but isn't exact for the textured OR.)

**Fork 0033:** the frame's first EFB pass already load-op clears colour and depth, and the display copy's
clear (a42fe56) then drew a full-screen quad on top every frame. When it comes before any draw of that pass
(colour and alpha together, scissor = whole target, not in the replay context) its values become the load
op's. 6x: EFB passes 22.64 -> 21.46 ms, uncapped 1.05x -> 1.09x. `AURORA_CLEAR_AT_FRAME_START=0` turns it off.

**Checked:** the fight route on Windows D3D12 (`SKIES_RTC=847000789`, `fight-full.txt`, a copy of
`user-qa-plain`, plain DOL + `modules-sa-plain-pgo`, `AURORA_SYNC_PIPELINES=1`, a dump every 60 frames to
185 s: title, intro, field, battle, results, field), both changes off twice and on once: all 91 frames
byte-identical. The save point at 1x (5 frames, three runs) too.

**WSL window dumps are not frame-exact.** The same route on WSL gave different pictures between two runs
of the same build (same dispatches and cycles to the end, so the game is identical): one run's frame 2280
shows the ship's door closed, the other's open. `DumpFrame` requests a readback before present and then
takes whatever readback completes, so a late one can land on the wrong frame number. Compare pictures on
Windows (deterministic there) until that's fixed; likely part of open item 5. Windows isn't immune either: in
five later route runs, one (`bin-check/route-off1`) dumped a different picture for frame 1680 (every pixel,
a fade at another moment) with the same dispatches and cycles, and a rerun of the same build matched again.

At 6x after both cuts (29.3 ms): EFB passes 21.5, R8 copy conversions 3.6 (2), the textured OR's snapshot
2.0, present resample 0.75 + blit 0.25.

**Fork 0034: the textured OR without a snapshot.** For bytes that are 0x00 or 0xFF, `s | d = max(s, d)`
and `s & d = min(s, d)`. The command processor now tracks which EFB channels hold only such bytes: a
channel becomes binary when a draw that provably covers the whole EFB writes a 0x00/0xFF constant into it,
stays binary through draws that write it only with such constants, and stops at anything else. Coverage is
checked on the CPU: one quad (or 4-vertex fan), direct F32 positions put through the position and
projection matrices as the vertex shader does, convex, every pixel centre inside with a 1/16 px margin,
viewport and scissor covering the target, no culling, depth or alpha test, one sample. Skies' composite
quads lie on the far depth plane (clip z = -2e-8 w); that's accepted under an orthographic projection only
(the four depths are equal, so the quad is drawn whole or not at all, and the picture relies on it).
`gfx::efb_epoch()` changes at every non-GX change of the EFB (clears, texture copies, frames, offscreen
passes, context swaps) and the knowledge holds within one. In the composite: the OR quad makes R and A
binary, the shadow volumes add or subtract alpha 1.0 (A stays binary, R doesn't), the R8 copy changes
nothing, the AND quad makes R 0 again, and the textured OR becomes a plain draw with max blending on R and
A and G/B masked (`PipelineConfig::minMax`, cache version 15; the source rounded to bytes as in the
snapshot path, `ShaderConfig::pad2 = LogicOpQuantizeOnly`; nothing merges into it). The constant-source
evaluation of 0032 is now per channel (a texture-dependent channel is unknown; an unknown term drops out
only where both forms of WGSL `mix()` give the same exact value). Draw log: `logic op 7 as min/max blend 5
write mask 9` and `efb binary channels RGBA (covers=N)`. `AURORA_LOGIC_OP_BINARY=0` turns it off.

UHD 770 at 6x, save point: **29.3 -> 24.4 ms** GPU per frame (the 2.0 ms snapshot copy gone, EFB passes
21.5 -> 18.6 ms over 3 passes instead of 4, staging copies 4 -> 3), uncapped **1.09x -> 1.30x**; Dolphin
Vulkan is 0.77x (~43.5 ms), so about 44% less GPU time per frame now. CPU-bound 1x (RTX 5080 machine): 2.92x
on, 2.88-2.91x off (noise). Checked exact: the fight route on Windows D3D12, `capture/route-win.sh` (new:
`NAME [SETS]`, a fresh `user-qa-plain` copy, everything under `C:\dev\SkiesStandalone\bin-check`), runs
off1, off2, on1, on2 against the earlier `dumps-route-c`: on1, on2 and off2 match all 91 frames; off1 is
the frame-1680 dump outlier above.

Now at 6x (24.4 ms): EFB passes 18.6 (3), R8 copy conversions 3.5 (2), present resample 0.75 + blit 0.25,
staging 0.26.

**Fork 0035: one-value copies in R8.** The composite's two copies are I8 (intensity); every conversion whose
output is one value in all four channels (I4, I8, R4, A8, R8, G8, B8, Z4, Z8, Z8M, Z8L) wrote RGBA8 at full
scale. With the device's texture component swizzle (D3D12 and Vulkan have it) they now render into R8Unorm
and are sampled through an R,R,R,R view (`tex_copy_conv::output_format`, `new_conv_texture`);
`AURORA_COPY_R8=0` keeps RGBA8. Save states convert a copy saved in the other format (the old
`bench-savepoint.sst` still restores its 4 copies). 6x: conversions 3.56 -> 3.27 ms, the frame's GPU time
within noise (24.43 -> 24.38 ms; uncapped 1.295x -> 1.32x): the conversions are bound by reading the EFB,
not by writing. Kept for the bandwidth and memory (a quarter of the bytes written and sampled), which counts
more on a tiler. Checked: the fight route's 91 frames byte-identical; a state saved at 104 s with R8 copies
reloads to the straight run's frames (6 of 6).

Next GPU candidates (as of then): present resample + blit as one pass (1.0 ms; done since in fork 0037, see *The
Pocket 5 at higher internal resolutions*); the conversions' EFB read (an R8 copy of a 640x480 rectangle reads the
whole 6x EFB once per copy; the two read it twice). On the Retroid (a tiler), pass splits cost more than on the
iGPU, so 0032-0034 should matter more there (measured since: the save point at 6x is 1.24x against Dolphin's
0.87x there).

## A battle scene and frame pacing (2026-10-25, later)

**The scenes.** Both builds now have a state in the same battle (the Valuan ship's interior arena, Vyse
alone at 327/504 HP, idle at the command menu with the camera drifting; ws26 DOL, `modules-llvm-pgo-ws26`):
- Standalone `C:\dev\SkiesStandalone\bench-battle2.sst` (150 s): from `bench-savepoint.sst` with
  `SKIES_PRESS_FILE=box.txt` (after 100 s that script is only a box walk at the save point, 3 s per
  direction) and `user-bench-sa`'s Encounters 400%; the battle starts around frame 3900 (~131 s).
  (`bench-battle.sst` next to it is a failed first try: `fight-full.txt` on ws26 walks into a wall.)
  Since 2026-10-27 night the standalone's battle bench is this same Dolphin state, converted
  (`build/sa-runs/bench-states/bench-battle2.sst`), so both builds start from one moment.
- Dolphin `C:\dev\SkiesRecomp\build\savestates\20-corridor-battle.sav`: `05-save-point.sav` in a copy of
  `user-bench-dc` (`C:\dev\SkiesAuroraRun\user-mkbattle`), `drive.py config:Dolphin.Skies.Encounters=400`,
  eight rounds of `tap:dpad_right:180 tap:dpad_down:180 tap:dpad_left:180 tap:dpad_up:180`, then `save:`.
  (`02-first-battle.sav` is the opening's deck battle, a different scene.)

| Battle, same PC | Standalone | Dolphin Vulkan |
|---|---|---|
| RTX 5080, uncapped | **3.78x** | 3.25x |
| UHD 770 at 6x, uncapped | **2.48x** (12.2 ms GPU per frame, 4 passes) | 2.07x |
| 4 E-cores, uncapped | **1.93x** | 1.49x |
| 1 E-core, uncapped | **1.86x** | 1.27x |
| Full speed, all threads: CPU | **0.35 cores** | 0.49 cores |

The battle has no shadow composite, so the standalone's GPU lead there is smaller than at the save point
(+20% against +69% at 6x); its CPU lead is about the same (16-46%, 28% less CPU at full speed).

**Frame pacing.** `SKIES_PRESENT_PACING=1` (or the Dolphin build's `RECOMP_PRESENT_PACING=1`) logs the Dolphin
build's `[present-pacing]` line every 600 presents: the interval between presents, p50/p90/p99/max, and the
counts over 40 and 50 ms (Skies presents 30 frames a second in both builds; Dolphin's line counts >20/>25 ms,
which every 30 fps present exceeds). It also logs `[host-busy]`: the emulation thread's wall time per field
outside the throttle's sleep. That includes waits in present and, on a shared core, other threads' time, so
it isn't headroom; cores in use (`cpu-sample.sh`) is. On a phone it also grows when the governor lowers the
clock (*Pacing on the Pocket 5*). `[present-drain]` and `[present-call]` (added 2026-10-25, evening) split out
the emulation thread's time in Aurora's command parsing and in `aurora_backend_present` per frame, and
`[present-late]` how far behind its guest time each copy came; the busy wait (`SKIES_THROTTLE_SPIN_MS`) is
not counted in `[host-busy]`. Since fork 0036 `[present-pacing]` is taken on the render worker after
`Present()`; the tables above it measured the emulation thread's present calls (*Pacing on the Pocket 5*).

| At full speed | Standalone interval ms p50 / p90 / p99 / max | Dolphin Vulkan |
|---|---|---|
| Battle, 1 E-core | 33.35 / 34.4 / 35.3 / 36.2 | 33.0 / 35.3 / 36.5-37.2 / 37-39 |
| Save point, UHD 770 at 6x | **33.4 / 34.5-34.9 / 36.1-36.6 / 37-39 (1.00x)** | 44.1 / 47.8 / 50.8-55.7 / 53-257 (0.77x: every frame late) |

(`C:\dev\SkiesAuroraRun\user-bench-igpu6-rt` is new: `user-bench-igpu6` with `EmulationSpeed = 1.0`.)

## The Android shell (2026-10-25, later): a test app, plays on the Retroid Pocket 5

**What it is.** `com.skiesofarcadia.standalone` ("Skies standalone (test)"), beside the user's apps and
touching none of them. On Android `standalone/CMakeLists.txt` builds the standalone as `libmain.so` with SDL3
linked statically (Aurora takes SDL's `Android_LockActivityMutex`, which a shared `libSDL3.so` doesn't
export) and Dawn's Android prebuilt; SDL's Java glue (`build/standalone/_deps/sdl-src/android-project`) plus
`standalone/android/` (manifest, `SkiesActivity`: loads only `main`) make the APK.
`tools/android-standalone-build.sh` configures `build/standalone-android` offline (FetchContent sources from
the desktop build and `~/projects/skies-aurora/android-deps`), builds, and packs without Gradle (javac,
d8, aapt2, Python's zipfile for the stored dex and libraries, zipalign -P 16, apksigner with the debug key)
into `build/standalone-apk/skies-standalone.apk` (151 MB: libmain.so, libpng16.so, the module).
`MODULE=` picks the arm64 module (default `build/android/jniLibs-sa26`'s, the ws26 DOL's; see *Bring-up*).

**Arguments, environment, log** (standalone/src/main.cpp `AndroidSetup`): SDLActivity calls main without
arguments, so they come from `args.txt` in the app's external files folder
(`/sdcard/Android/data/com.skiesofarcadia.standalone/files`), one per line, `$FILES` standing for that
folder; `env.txt` there holds NAME=VALUE lines for the environment; stderr (the log) goes to `log.txt`
there. The module is the disc's own by default (`libg<disc ID>_recomp.so` in the app's library folder;
`--module` overrides it).

**On the device** (Pocket 5, serial <serial>; always Windows' `adb.exe`, never WSL's adb), pushed into the
files folder (no game files in the APK): `game/sys` from `C:\dev\SkiesRomhack\build\game\GEAE8P-ws26\sys`,
`game/files` from `C:\dev\SkiesRecomp\extracted\SkiesOfArcadiaLegends\files` (1.4 GB, 50 s),
`user-bench-sa` (Encounters 400%, widescreen), `dsp_coef.bin` (Dolphin's `Data/Sys/GC`),
`bench-savepoint.sst`, `bench-battle2.sst` (replaced 2026-10-27 night by the version-2 states made from the
Dolphin bench states), and `args.txt` / `env.txt` (`SKIES_MUTE=1`,
`SKIES_PRESENT_PACING=1`, `SKIES_AURORA_LOG=1`). Started with
`adb.exe -s <serial> shell am start -n com.skiesofarcadia.standalone/.SkiesActivity`.

**Bring-up:** the old `build/android/jniLibs-aurora` module was generated from another DOL than ws26 (5147
of 5461 chunks matched); `NAME=sa26 MODULE_ONLY=1 DOL=/mnt/c/dev/SkiesRomhack/build/game/GEAE8P-ws26/sys/main.dol
tools/android-ws-build.sh` made `build/android/jniLibs-sa26` (5461 of 5461; `MODULE=` it for the APK). Then
Aurora never drew: it draws only while its surface is marked ready through
`Java_org_libsdl_app_SDLSurface_auroraNativeSetSurfaceReady`, which stock SDL never calls; the APK script
patches a copy of `SDLSurface.java` (ready after `surfaceChanged`, not ready before `surfaceDestroyed`). The
device must be unlocked (a locked screen pauses the activity before it has a surface). Since 2026-10-25
(night) `SkiesActivity` hides the status and navigation bars in `onCreate` (WindowInsetsController, the legacy
immersive flags below API 30; a swipe shows them for a while), so the surface is 1920x1080 before Aurora sizes
its swap chain. Not through SDL's fullscreen after start: that resizes the surface mid-game, and Aurora's
resize (window.cpp `resize_swapchain`) recreates the EFB and clears the copy caches, so the copies that live
only on the GPU would be lost (a real problem for rotation or split screen too: lifecycle, open). Pacing
unchanged (p99 34.6 ms in both scenes). **Background and foreground** (`am start` of the home screen, then of
the app again; no input injected): the surface is destroyed and the app paused (the emulation mostly stops,
the present is skipped, "Surface texture is Error, dropping surface"), and on return the surface comes back at
the same size, so Aurora only reconfigures it (the EFB is kept) and the game goes on; one 5.8 s interval in
`[present-pacing]`, then normal. A size change on return (rotation, split screen) would go through the
recreate and is untested.

**The Dolphin build on the same DOL and module** (the user chose a separate app, 2026-10-25):
`org.dolphinemu.dolphinemu.bench`, the plain app under another id (`APP_ID_SUFFIX=.bench`, new in
`tools/android-ws-build.sh`; `-PdolphinAppIdSuffix` in Source/Android/app/build.gradle.kts, in
`recompcore-aurora.patch`): `NAME=sa26 JNILIBS=$PWD/build/android/jniLibs-sa26 APP_ID_SUFFIX=.bench
tools/android-ws-build.sh` -> `build/android/apk/skies-sa26-2026-10-25-bench.apk` (the script no longer
deletes `JNILIBS` when it is its own `jniLibs-NAME`). Its files folder has `Games/SkiesOfArcadiaLegends/{sys,files}`
(ISOPath0 = that sys), `Config/Dolphin.ini` (Vulkan, `EmulationSpeed = 0.0` or 1.0, FrameMode 0, widescreen),
`Config/GFX.ini` (InternalResolution 1), `GameSettings/GEAE8P.ini` = `release/GEAE8P.ini` (**needed**: the idle
PC and dual core the shipping apps have; without it the plain app sat at 1.00x), `Automation/`, and
`PresentPacing.on` for the pacing log (logcat tag `present-pacing`). Boot: open the app once so it scans its
games, then `PKG=org.dolphinemu.dolphinemu.bench SERIAL=<serial> tools/android-boot.sh`; states through
`ADB_AUTO=$B/Automation tools/drive.py load:.../savestates/05-save-point.sav` (and `20-corridor-battle.sav`).

**Pocket 5 (Snapdragon 865 / Adreno 650), internal resolution 1, ws26, 30 fps mode, muted:**

| | Standalone | Dolphin Vulkan (bench app) |
|---|---|---|
| Save point, uncapped (two rounds) | **1.68 / 1.68x** | 1.38 / 1.38x |
| Battle, uncapped | **2.05 / 2.03x** | 1.63 / 1.63x |
| Save point, full speed: interval ms p50 / p90 / p99 / max | 33.6 / 39.1 / 42.4 / 45; 6-7% over 40 ms | **33.3 / 35.0 / 36.7 / 38.6; none over 40** |
| Battle, full speed | 33.5 / 36.9 / 41.5 / 45; ~2% over 40 ms | **33.3 / 36.2 / 37.7 / 38.9; none** |

Faster by 22-25%, but **less smooth at full speed**, the opposite of the desktop (fixed below: *Pacing on
the Pocket 5*).

## Pacing on the Pocket 5 (2026-10-25, evening): clocks, not a wait

**The guess was wrong.** The emulation thread doesn't wait in present: `SKIES_PRESENT_PACING=1` now also logs
`[present-drain]` (time per frame in `aurora_backend_drain_if_over`, Aurora's command parsing on the emulation
thread) and `[present-call]` (time in `aurora_backend_present`): 2.4 ms and 0.4 ms (p99 0.8). The presenting
field's 21 ms is the game's own work on a slow clock. Per-thread CPU and cluster clocks from `/proc/PID/task/*/stat`
and `cpufreq/policy*/scaling_cur_freq` (`~/projects/skies-aurora/capture/android-thread-sample.sh`, run on
the device from /data/local/tmp) during the battle at full speed: the
emulation thread is ~78% busy on a gold core (cpu4-6, top 2.42 GHz, clocked 1.6-2.4 GHz), the prime core
(cpu7, 2.84 GHz) idles at 1.5 GHz. schedutil picks the clock that keeps a thread ~80% busy, so a thread that
sleeps a fifth of every field gets clocked down until the drawing field barely fits, and any spike runs over.
Uncapped, the same thread goes to cpu7 at 2.84 GHz (that's where the 1.8-2.0x comes from). The Dolphin build
doesn't hit this because its CPU thread never sleeps (100% busy on a gold core, clocks pinned high; its Video
thread is another ~60%). The device has no ADPF (`dumpsys performance_hint`: HAL Support false).

**First fix, then replaced (673686e):** the thread pinned to the fastest cores plus a busy wait of the last 6 ms
of each field's throttle wait. Its numbers were taken with a measuring mistake: the first `[present-drain]`
read the clock twice per command chunk, which cost the Pocket 5 15% of its speed whenever
`SKIES_PRESENT_PACING=1` was on (uncapped save point 1.46x with it, 1.70x without; now it times only the
calls that drain). Measured again without it, the 6 ms spin held the battle (p99 35.7 ms) but not the save
point (p99 39.2-39.3): with less work per field the sleep grows, a fixed spin is a smaller share of it, and the
clock drops again. The kernel (4.19 Qualcomm) has no utilization clamp (`cpu.uclamp`) either, so there is no
supported clock floor.

**The fix (scheduled presents, fork 0036):** presents no longer happen when the emulation thread gets to the
copy. `SchedulePresent` (standalone/src/video.cpp) gives the render worker a wall time for each present: the
copy's guest time on the throttle's clock (`WallForGuest`) plus a delay; the worker submits the frame's
passes at once, waits for that time, then records the present blit and presents (Aurora
`aurora_backend_set_present_time`). The game's rhythm is fixed in guest time (the hardware too shows a copied
picture at a retrace), so the intervals come out even however the drawing field's clock moves, as long as
the copy is in before its present time. `[present-late]` logs how late the copy is: on the Pocket 5, unpinned
and unspun, p50 10-11 ms, p99 16-18 (the drawing field runs slower than real time and the light field catches
up); Windows p50 0.7-1.5, p99 6-8. `SKIES_PRESENT_DELAY_MS=auto` (the default, all platforms) keeps the delay at
the last 300 copies' 99th percentile + 1.5 ms (at most 30, starting at 20: a delay that grows makes late
presents, one that shrinks doesn't; log `[video] present delay N ms` when it moves by 1 ms or more with
`SKIES_PRESENT_PACING=1`); `N` fixes it; `off` presents at once. Hitches over 50 ms (state loads) are left
out; unthrottled runs (the routes, benches) present at once. It settles at 14-19 ms on the Pocket 5 and 6-9 ms
on Windows. The cost is latency: the copy is already that late on average, so the added delay is the p99-p50
gap, ~5-8 ms on the Pocket 5. `[present-pacing]` is now measured on the render worker right after
`Present()` (`aurora_backend_set_present_callback`), where Dolphin measures it too; the old numbers were
the emulation thread's calls.

**Defaults now:** Android pins the emulation thread to the cores with the highest `cpuinfo_max_freq` (the
Pocket 5's cpu7; log `[host] emulation thread on cpu 7: ok`), no spin, auto delay. Desktop: no pin, no spin,
auto delay. Knobs: `SKIES_PIN_CPU=fast|none|N`, `SKIES_THROTTLE_SPIN_MS=N` (-1 the whole wait),
`SKIES_PRESENT_DELAY_MS=auto|off|N`.

**Pocket 5, IR 1, ws26, 30 fps, muted, full speed: interval between presents, ms, the second 600-frame window
(the first holds the state load).** Power is rough: the USB input (`power_supply/usb/input_current_now` x
`voltage_now`, capped ~2.4 W by the PC port) plus the battery's draw (`bms/current_now` x 4.36 V), averaged
over 10-30 s (`capture/android-power.sh`; the device shell's arithmetic is 32-bit, so it multiplies mA by
mV); launcher idle ~1.7 W. Runs: `~/projects/skies-aurora/capture/android-ir-bench.sh sa|dol IR
uncapped|full savepoint|battle [ENV;...]` (writes env.txt / args.txt or the bench app's ini files, starts the
app, samples GPU (`android-gpu-sample.sh`: kgsl busy % and clock) and power, reads the pacing or speed
lines). The Pocket 5 was on the charger at 100% for all of them. **All Pocket 5 numbers in this file (both apps, every
session) were taken on Retroid's lowest performance / governor setting** (the user's setting; `settings get
system performance_mode` = 0, `fan_mode` = 1). Its sysfs caps aren't lowered (gold 2.42 GHz, prime 2.84 GHz,
GPU 670 MHz; floors 1.17 / 1.29 / 1.52 GHz), so what the mode changes isn't visible there; the higher modes are
untested.

| Battle | p90 | p99 | max | power |
|---|---|---|---|---|
| Before (no pin, no spin, present at once) | 37.1-37.6 | 41.1-42.5 | 43-45 | ~2.2 W |
| Pin + 6 ms spin (673686e) | 34.6-34.7 | 35.7-35.9 | 37.5 | ~2.3 W |
| Pin + spin the whole wait | 34.1 | 35.3 | 35.5 | ~2.9 W |
| No pin, no spin, 18 ms delay | 34.2 | 35.2 | 35.5 | |
| **Default: pin, no spin, auto delay (14-15 ms)** | 33.6-33.7 | 34.2-34.4 | 34.5-35.1 | ~2.05-2.2 W |
| Dolphin bench app | 35.7-35.8 | 37.7-38.3 | 39.5 | ~2.5 W |

| Save point | p90 | p99 | max | power |
|---|---|---|---|---|
| Before | 38.8-39.3 | 41.9-42.9 | 43-45 | ~2.6 W |
| Pin + 6 ms spin (673686e) | 37.3-37.6 | 39.0-39.3 | 40.0 | ~2.7 W |
| Pin + spin the whole wait | 34.1 | 35.0 | 35.2 | ~3.1 W |
| Pin + 6 ms spin + 12 ms delay | 34.2 | 34.6 | 35.5 | ~2.8 W |
| **Default: pin, no spin, auto delay (16-19 ms)** | 34.3-34.4 | 34.7-35.0 | 35.5-36.6 | ~2.4 W |
| Dolphin bench app | 34.8-34.9 | 36.2-36.3 | 37.4 | ~2.9 W |

The standalone on the Pocket 5 is now smoother than the Dolphin build in both scenes (p99 ~34.5 against
36-38, max ~35.5 against 37-39.5) at ~0.4-0.5 W less, and 22-25% faster uncapped (save point 1.68-1.70x
against 1.38x). Windows (RTX 5080 machine, the user-bench-sa settings, throttled): present at once p99 39.7-39.9,
max 42.5 (4-5 over 40); auto delay p99 34.9-35.1, max 35.5 in both scenes.

## Frames against the display's refreshes (2026-10-26, night)

**What showed:** the user watched the Pocket 5 (Retroid's game assistant overlay) and asked why the frame time
was so high. At 30 fps every frame is 33.3 ms by design (the game's own rate; 60 fps is the Frame rate option,
off in a fresh user folder, as in the shipping Android app). But SurfaceFlinger (`dumpsys SurfaceFlinger
--latency` on the game's `SurfaceView ... (BLAST)` layer, sampled every 2 s and merged: `sfwatch.sh` /
`sfrun.sh` in the session scratch) showed the frames on screen weren't even: at the save point, 30 fps, one
minute, 30 frames stayed up three refreshes (50 ms) and 23 one (17 ms), in bursts every 7-13 s; at 60 fps 18
frames were repeated. Each bad frame was ready 17-22 ms before its refresh instead of 2-6: it finished just
after the compositor's latch and waited a refresh.

**Why:** presents were scheduled on the game's clock (fork 0036, *Pacing on the Pocket 5*), never on the
display's. The game runs at 59.94 Hz, the display at 60.000 (Choreographer measures 16.665 ms), so present
times slide across the refreshes by ~1 ms a second, and a few ms of wobble (a copy's guest time follows the
game's work; the auto delay moves) flips frames between refreshes whenever they pass the latch.

**Fix (Android only; desktop has no refresh times yet):**
- `video.cpp`: a thread with Choreographer frame callbacks measures the refresh grid (`VideoRefreshPeriodNs`).
  `SnapToRefresh` moves each present time to a refresh + `SKIES_VSYNC_OFFSET_MS` (10; offsets 4-12 clean, 0
  and 15 not: the GPU takes 7-15 ms from the present call) and keeps a frame on the last frame's refresh plus
  the refreshes between them unless that slot ends up >5 ms before its time or >1 refresh + 5 ms after
  (without that hysteresis the wobble re-snapped back and forth). `SKIES_VSYNC_SNAP=0` turns it off.
- `main.cpp` `FollowDisplay`: when a game frame is within 0.5% of a whole number of refreshes, guest time runs
  at the display's rate (59.94 -> 60.000 Hz: 1.0011x), so the frames don't drift against the refreshes at all.
  The audio follows through its queue (fork 0051, next section: the playback runs 0.11% fast there). A 50 Hz game on a 60 Hz screen keeps
  its own rate. `SKIES_DISPLAY_RATE=0` turns it off. Log `[host] paced to the display (60.005 Hz): the game at
  1.0011x`.
- Tried and dropped: an auto delay that falls only slowly (to stop it pushing frames across slots): at 60 fps it
  stayed near its early peak and the queued presents stalled the video thread (0.91x).

**Result (Pocket 5, save point, lowest performance mode, 1.00x in all):** frames per minute that weren't the
normal count of refreshes: 30 fps 53 -> 3; 60 fps 18 -> 3. The few left are the slot moving when the delay
jumps. Latency: the present call now waits for its refresh (queue to display ~26 ms median at offset 8,
14-31 before). Not checked: by ear (the 0.1% faster audio and the Dreamcast streams), a battle scene, a
120 Hz display, Windows / macOS (no snap there).

## Sound against the picture (2026-10-26, night; fork 0051)

**What showed:** the user heard S-move sound out of step with the hits in the standalone. Two causes:
- The first Windows window ran the ws26 test game (`C:\dev\SkiesUncensor\app`), older than the issue #7
  fix (ws27: spells and S-moves at normal speed). With the release patch (ws32 / v0.83) the game holds
  normal speed from 0.6 s before Cutlass Fury's stream (`stv66/67.samp`) to 3.4 s after its last read, at 1x
  and at a real 4x on Windows (`SKIES_WATCH=80002BDC`, `SKIES_DI_TRACE=1`). Use the release package
  (`build/sa-runs/pkg-win`) for anything played by hand.
- The audio queue: Aurora's SDL stream held whatever it got, since the game makes sound on its clock and the
  device plays it on its own: 76-102 ms on Windows from the state load's burst on, up to the 250 ms cap after
  hitches, so every sound came that late against a picture presented within ~10 ms.

**Fix (fork 0051, `backends/aurora/aurora_audio.cpp`):** the playback rate (SDL's frequency ratio) follows the
queue's smoothed level toward `DOL_AUDIO_TARGET_MS` (40; Android 60), up to 0.5% fast or slow; 30 ms over, a
pushed block is dropped (a state load's or a stall's catch-up). `DOL_AUDIO_QUEUE_LOG=1` logs level / ratio /
drops every 64 pushes. Windows, the S-move state at 4x: 5 drops after the load, then 63 -> 43 ms in ~20 s and
40-45 ms on (was 76-102); one dip when the S-move's files load (the game thread pauses ~60 ms), no
underruns. Pocket 5 with the display-rate lock: settles at ~72 ms with the playback 0.11% fast (the lock's
surplus; a proportional controller's offset), no drops, frames as before. Not checked by ear.

## The Pocket 5 at higher internal resolutions (2026-10-25, night)

`capture/android-ir-bench.sh` for both apps (same settings as above, lowest performance mode). GPU time per
frame is estimated as busy % x clock / frames per second where the GPU isn't saturated; `AURORA_GPU_PROF=N`
works on the device (Adreno 650 has timestamp queries) but its lines go to **logcat** (tag `aurora`), not
log.txt, which is why it looked silent before.

| Save point | Standalone | Dolphin bench app |
|---|---|---|
| 2x uncapped | 1.66x, GPU 58% @ 305 MHz | 1.37x, 54% @ 305 (standalone ~12% fewer GPU cycles per frame) |
| 3x uncapped | 1.67x, 71% @ 400 | 1.36x, 74% @ 400 (~21% fewer) |
| 4x uncapped | 1.65x, 87% @ 491 | 1.37x, 82% @ 670, the top clock (~36% fewer) |
| 6x uncapped (both GPU-bound, 99% @ 670) | **1.21x, 1.24-1.25x after fork 0037** | 0.87x |
| 4x full speed | p99 34.8 ms, GPU 71% @ 400, ~3.3 W | p99 36.4, 79% @ 490, ~4.4 W |
| 6x full speed | **holds it**: p99 34.4, max 34.8, GPU 81% @ 670, ~5.2 W | can't: p50 39.2, p99 56 ms |

| Battle | Standalone | Dolphin |
|---|---|---|
| 6x uncapped (neither GPU-bound) | 2.02x, GPU 83% @ 587 | 1.61x, 84% @ 441 |
| 4x full speed | p99 34.4, GPU 43% @ 305, ~2.5 W | p99 37.4, 39% @ 305, ~3.0 W |
| 6x full speed | p99 34.2, GPU 77% @ 305, ~2.8 W | p99 37.7, 75% @ 305, ~3.3 W |

So the save point's GPU lead holds on the tiler (more at higher resolutions, where Dolphin's snapshot copies
grow), and the standalone plays 6x at full speed where the Dolphin build can't. In battle (no shadow
composite) the GPU per frame is about even (standalone 3-10% more at 4x-6x), unlike the desktop iGPU (20%
less); smoother and less power overall anyway, from the CPU side.

**Per pass** (`AURORA_GPU_PROF=300`). Save point 6x at 670 MHz: EFB 22.4 ms (3 passes), EFB copy conversions
4.16 (2), present resample 0.69 + blit 0.22, staging 0.15; span 27.7 ms. Battle 4x at 305 MHz: EFB 11.8 (1),
present resample 0.96 + blit 0.51, staging 0.17; span 13.4.

**Fork 0038 (graphics options, item 2 of HANDOFF-feature-gaps.md):** live texture filtering / anisotropy / MSAA
overrides and the sharp bilinear and bicubic output samplers. **Fork 0039:** MSAA counts beyond 4 with the Dawn patch
(`patches/dawn/`, `aurora_get_msaa_modes`).

**Fork 0037: no resample pass with the bilinear output sampler.** The present blit samples the source over the
viewport itself instead of copying a viewport-sized intermediate the resample pass wrote (same bilinear
sample; the old path quantized to 8 bits once more in between). Save point 6x: 27.7 -> 26.8 ms, 1.20-1.21x ->
1.24-1.25x. The area sampler keeps the pass (it works in the target's pixels); `AURORA_PRESENT_RESAMPLE=1`
keeps it always. Frame dumps read the source before either, so they can't see it; one device screenshot at 6x
looked right (placement, letterbox, sharpness), Windows presents normally.

**Next GPU candidates on the tiler:** the EFB passes themselves (22.4 of 26.8 ms at the save point; per-draw
cost needs a vendor profiler); the two copy conversions (4.2 ms; each reads the whole 6x EFB, with passes
between them, so they can't share a read). Then the shell's gaps: immersive mode and background / foreground
are done (*The Android shell*); left are the built-in pad and audio on the device (both need a person: press the
buttons, listen with `SKIES_MUTE` out of env.txt), a size change on return (rotation, split screen: Aurora's
resize recreates the EFB), and a launcher (disc import) instead of adb pushes.

## Feature gaps 0-2 (2026-10-25, late night): other discs, Continue, graphics options

Following [HANDOFF-feature-gaps.md](HANDOFF-feature-gaps.md), which has each item's details, checks and
test inputs. Commits on this branch: 61e39f8 (item 0), 5e171c5 + 364a0d3 (item 1), 15e891f (item 2),
3d96d5f + 1d0a459 + 4d9555a (MSAA 2x / 8x).

- **European and Japanese discs:** idle PC per disc, the European disc runs as NTSC-U (60 Hz, card in
  `GC/USA`, `DiscRunsAsNtsc()`), `--module` optional (`g<disc ID>_recomp` from the disc ID; on Android
  `libg<ID>_recomp.so`, and `tools/android-standalone-build.sh` takes several in `MODULE`).
- **The Japanese game could not mount its memory card at all** before 5e171c5: VI register 0x6E (DTV status)
  read 0, so the SDK's font encoding said ANSI against the card's Shift-JIS header.
- **Continue:** `--continue N|card` or `[Skies] AutoLoadCard` (one shot), as the Dolphin build's autoload;
  `--load-state` wins and clears the key.
- **Graphics options:** Anti-aliasing, Texture filtering, Anisotropic filtering, Scaling rows, live, on the
  Dolphin build's keys (fork 0038; `settings.cpp` parses base 0 since Dolphin writes MSAA in hex). The
  anisotropy default is now Dolphin's (the game's own 2x / 4x; Aurora had made it 16x).
- **MSAA beyond 4x:** WebGPU allows 1 / 4 and Aurora runs Dawn with `skip_validation`; a Dawn patch
  (`patches/dawn/0001-extra-sample-counts.patch`) and fork 0039 list 2x / 8x where the GPU has them.
  Builds with it: WSL `build/standalone-dawnsrc` (`-DSKIES_DAWN_PROVIDER=vendor
  -DFETCHCONTENT_SOURCE_DIR_DAWN=… -DDAWN_USE_X11=OFF`), Android `DAWN_SRC=… tools/android-standalone-build.sh`
  (`build/standalone-apk-dawnsrc`), Windows `build-dawnsrc.bat`. Measured: Pocket 5 Off / 2x / 4x (2x +5% GPU,
  4x +30%); RTX 5080 Off / 2x / 4x / 8x (8x about 2.2 ms GPU per frame at 3x). The default builds still use
  the prebuilt Dawn and offer Off / 4x.
- **Device state:** the Pocket 5's standalone test app is the patched (`dawnsrc`) APK, its files and settings
  as they were (env.txt LF, GFX.ini CRLF; back them up with `adb exec-out cat`, not `adb shell cat`).
- **Next:** item 3 (input: release keyboard layout, rumble, names, Button mapping); before shipping any of
  this, a way to ship the patched Dawn (CI or our own prebuilt packages) and the macOS build.

## Input (feature-gap item 3, 2026-10-25 late night)

Commits 78386f8, 52db873, 86bf2e4; fork 0040 (080ec53) and 0041. Details and checks in
*HANDOFF-feature-gaps.md* item 3.

- **Keyboard:** the release layout (W A S D, I J K L, arrows, Space / Left Shift, F / R, Tab, Q / E, Enter;
  Esc the menu) is Aurora's fallback (fork 0040, `aurora_backend_pad_default_keys`), still only when no
  bindings were saved. Aurora's input files (`keyboard_bindings.dat`, `<pad>_<vid>_<pid>.controller`,
  `controller_ports.dat`) are in `USER/Config/Aurora` with `--user`; its shader caches stay in the app folder.
- **Rumble:** the SI's poll command motor bits (latched by an SISR write with WR) go to
  `PADControlMotor`; stopped while the menu is open and with Controls -> Rumble Off (`[Skies]
  StandaloneRumble`). The game rumbles from the battle encounter on (the fight route: 32 changes).
- **Button mapping page** (`input.cpp` + `menu.cpp`), the Dolphin build's, on Aurora's mapping store.
  Menu and Invincible hotkeys stay fixed (Esc / Back, L3 / R3).
- **Aurora's hidden dead zone** (8000 / 32767 per axis, a hard cut) is off; the Controls row is the only one.
- **Controller 1 follows** the pad last pressed (750 ms idle rule), as the Dolphin build; without it a second
  pad was GameCube port 2.
- **Not checked yet:** rumble felt by hand, capture with a real pad, two pads, the Pocket 5.

## Language (feature-gap item 4, 2026-10-26)

The European disc's language from `[Skies] Language` into the IPL SRAM (`hw_exi.cpp`), a Language tab, and the
Dolphin build's "A twice restarts" on desktop (the standalone execs itself without `--load-state` /
`--continue`; on Android a restarter process ends the game's process and starts it again, 00a8e24). Checked on
WSL, Windows and the Pocket 5 (European disc); details in *HANDOFF-feature-gaps.md* item 4.

## HD textures (feature-gap item 5, 2026-10-26)

Commit 4226b39; fork 0042-0043. Details and checks in *HANDOFF-feature-gaps.md* item 5.

- **Import:** `--user USERDIR --import-texture-pack PACK` (.7z, .zip, folder, archives in a pack) /
  `--remove-texture-packs`, the Dolphin build's `TexturePackImport` word for word (`texture_pack.cpp`, its
  Dolphin helpers in `texture_pack_file.h`; minizip-ng and `lib/lzma-sdk` built in). Into
  `USER/Load/Textures/GEA`, shared with the Dolphin build.
- **Drawing:** `GFX.ini [Settings] HiresTextures` registers the files with Aurora's texture replacement, live
  from Graphics > HD textures (`hd_textures.cpp`). Files decode on one or two loader threads (fork 0043) and
  the game's own texture draws until then, as in Dolphin; `AURORA_SYNC_REPLACEMENTS=1` for frame-exact runs.
  The log line `[textures] N of M loaded, ... decodes ... uploads ...` every 600 frames.
- **Checked:** both v0.06 packs import byte-identical; the fight route on WSL and Windows with 89-91 files in
  use, the moon and skies replaced, off and on live; Windows speed unchanged. **Pocket 5:** the loaders
  inherited the pinned emulation thread's prime core (they start from it) and hitched the battle's first
  seconds; fork 0045 puts them on the other cores. With the PNG pack the standalone is as fast as without it
  (uncapped 1.65x / ~2.0x against the Dolphin build's 1.37x / 1.63x), smoother, at half the memory (PSS
  ~600 MB against ~1.1 GB); BC7 skipped there, no noise. **Open:** the full 4x pack.

## 60 fps (feature-gap item 6, 2026-10-26)

Commit e91e704; fork 0044. Details and checks in *HANDOFF-feature-gaps.md* item 6.

- `interp60.cpp` records each frame's GX stream as `hw_gpu.cpp` applies it, blends it with the previous
  frame's (the Dolphin build's logic) and replays it at the XFB copy into Aurora's replay context; the
  retraces present the in-between picture, then the real one. `[Skies] FrameMode`, Graphics > Frame rate.
- First single-threaded: the replay cost the emulation thread a second Aurora parse of the frame, so
  the Dolphin build's single-core budget decided per scene (Windows uncapped 3.6x with it, 5.0x without).
  **Pocket 5:** full speed at 60 fps in both bench scenes, p99 17.7-18.0 ms and nothing over 20 (the
  Dolphin build's 60 fps: p99 24.5-25.9, 13-42% over 20), 0.45-0.7 W less; but uncapped the save point
  dropped to 1.25x (Dolphin 1.41x, its interpolation on the GPU thread), and the present delay sat at
  27.5-30 ms. Now on the video thread (*The video thread* below): 1.70x at the save point and 2.05x in
  battle at 60 fps, present delay 17.6-22.9 ms; the budget measures the video thread's work.

## The video thread (2026-10-26)

Commit d55317c; fork 0046 (`aurora_backend_set_event_poller`). Why: at 60 fps the Pocket 5 had lost the
uncapped lead (save point 1.25x against the Dolphin build's 1.41x), because the replay ran on the one
emulation thread while the Dolphin build interpolates on its GPU thread. A simpleperf profile of the
emulation thread (the test app is now `profileable`; `simpleperf record --app com.skiesofarcadia.standalone
-e cpu-clock`, report on WSL with the NDK's host `simpleperf` and `--symfs` holding the unstripped
`libmain.so`) gave per game frame at the uncapped save point, 30 -> 60 fps: game module 15.3 -> 15.95 ms,
Aurora's parse 1.8 -> 3.4, interp60 0.1 -> 1.7 (blend 0.96, recording 0.36), libc copies 0.7 -> 1.7, waits
0 -> 1.5.

**The shape** (`standalone/src/video.cpp`, top): the game (CPU, hardware models, throttle) runs on a
thread of its own (`VideoRun`, 16 MB stack: recompiled guest calls nest on the host stack); the main
thread is the video thread and owns the window and every Aurora call. The game's thread stages GX
commands (16 KB chunks) and posts host work between them in order (`VideoPost`: frame ends, retrace
presents, the menu's overlay and settings, HD texture registration). Everything the game's side reads from
Aurora is cached (surface size, quit, MSAA modes) or waited for (`VideoSync`: save states read the EFB
copies back, loads restore them).

**The one real dependency: tokens.** A first version without waits aborted in Aurora
(`draw vertex data overrun`, a 480-byte display list): Skies sets two `GXSetDrawSync` tokens (0x48 then
0x47, twice) right after each frame's copy and retrace, and rebuilds its display lists once they are
passed; Aurora, a frame behind, parsed a list already rebuilt. The game reads the token only in its PE
interrupt handler (no polling loop, `--mmio-log`), and Dolphin's dual core raises it from the GPU thread
when it gets there. Here the interrupt keeps its guest time (runs stay deterministic) and the game's
thread waits at it (`VideoFence`, from `hw_gpu.cpp`'s `Effect::Sync`, display lists included) until the
video thread has parsed every command before it **that reads guest RAM** (`ReadsRam`: draws, display lists,
indexed XF loads, TLUT / TMEM loads); host work queued in between isn't waited for. That distinction is
the whole win at 60 fps: the retrace that presents frame N's in-between comes between its copy and its
tokens, so waiting for everything waited for the replay and the present (4 ms a frame on Windows).

**The 60 fps replay** is split: the frame end keeps the real picture (`InterpFrameEndVideo`); the blend and
replay (`InterpReplayVideo`) are deferred (`VideoPostDeferred`) until the game has staged 16 KB past the
fence (the four tokens come in a row, and a replay posted at the first was waited for at the next three),
or the next retrace / frame end / sync. Recording stays on the game's thread (it reads the CPU-side
shadow registers and copies RAM at command time); recordings move through a pool. The budget measures the
video thread's work (`VideoBusy`, without present waits).

**Phones: the clock.** Pinning wasn't the issue (the video thread was on a gold core already; it is now
pinned to the next fastest tier, `SKIES_PIN_VIDEO`), the governor was: a thread busy a quarter of the time
left the gold cluster at 1.29 of 2.42 GHz, the parse took 5.4 ms a frame instead of ~2.5, and the game's
end-of-frame burst of draws left ~2 ms to wait for at the tokens. With the video thread polling for work
instead of sleeping the cluster runs at 2.42 GHz, the parse takes 2.6 ms and the waits 0.7-0.85 ms. It polls
only while the game has no time to spare (`SKIES_VIDEO_SPIN=auto`, Android's default: uncapped, or the
throttle slept under 15% of the last 30 frames; at most 50 ms with nothing to do); at full speed it sleeps
(gold cluster at 1.29 GHz in the battle). Desktop: off (`SKIES_VIDEO_SPIN=1` forces it).

**Input:** Aurora's controller list changes while events are handled, so the present calls the host's
poller (fork 0046) and the standalone polls under its input lock (`VideoInputLock`), which the game's
side holds for the pad read with the menu's filter (`hw_si.cpp` `Snapshot`), the frame end's menu work
and rumble. On Android, Home pauses the main thread in the event poll holding the lock, and the game stops
at its next pad read; Home and back checked (one 5.8 s interval, then normal, picture right).

**Checks.** Windows D3D12, the fight route with `SKIES_RTC`, `AURORA_SYNC_PIPELINES=1`, 60 fps, 12 dumped
frames: all 24 pictures (real and in-between) byte-identical to e91e704's, with `SKIES_VIDEO_THREAD=0` and
with the video thread; the same with a state saved at 60 s and loaded at 100 s; 30 fps real frames equal
the 60 fps ones. Guest counts (frames, tokens, disc reads) identical in every mode. Menu overlay drawn
(screenshot). WSL: the fight route completes in both modes.

| Uncapped | Before | Video thread | Dolphin build |
|---|---|---|---|
| Pocket 5, save point 30 / 60 fps | 1.65x / 1.25x | **1.77x / 1.70x** | 1.38x / 1.41x |
| Pocket 5, battle 30 / 60 fps | ~2.0x / 1.66x | **2.15x / 2.05x** | 1.63x / 1.69x |
| Windows RTX, save point 30 / 60 fps | 2.89x / 2.07x | 3.15x / 2.95x | |
| Windows RTX, battle 30 / 60 fps | 3.75x / 2.81x | 4.07x / 3.84x | |

Pocket 5 at full speed: 30 fps unchanged (battle p99 34.1, save point 34.1-34.2); 60 fps battle p99
17.6-17.8, save point 17.7-17.9, nothing over 20 ms (the Dolphin build: 24.5 / 25.9, 13-42% over 20);
the 60 fps present delay settles at 17.6-22.9 ms instead of 27.5-30; power ~2.54 W battle, ~2.87 W save
point at 60 fps (Dolphin ~2.9 / 3.8).

**Switches:** `SKIES_VIDEO_THREAD=0` (one thread, the same order and pictures), `SKIES_VIDEO_STATS=1`
(every 600 frames: the game's waits at tokens / frame ends / a full queue, the video thread's work and
present waits; Android reads it lazily, after `env.txt`), `SKIES_VIDEO_SPIN=auto|1|0`,
`SKIES_PIN_VIDEO=fast|none|N`, `SKIES_DRAIN` (the video thread parses each chunk at once by default).
**Open:** the Nova; a pad by hand on Windows and the Pocket 5 (the input lock). macOS holds (2026-10-27,
*macOS* at the end).

## Dreamcast features (feature-gap item 7, 2026-10-26)

Commit d1ebb4b; details and measurements in *HANDOFF-feature-gaps.md* item 7.

- **The Dolphin build's modules as they are** (`standalone/src/dreamcast/`, byte-identical to dev's patched
  tree): Uncensored, the Dreamcast music and its import, the Dreamcast streamed songs, the European disc's
  60 Hz audio. They include Dolphin headers, which `standalone/src/dolphin_shim/` provides at the same paths
  (over `dreamcast_host.h`; only the `skies-dreamcast` library sees them); `dreamcast.cpp` is the glue. This
  is the pattern for the remaining Dolphin-hosted features: copy the files, shim the headers.
- **Where they hook in:** `disc.cpp` (every file registered at open, each read through the redirects),
  `hw_si.cpp` (the music's poll), `state.cpp` (the song after a load), `main.cpp` (`--import-dc`, `--sys`,
  the field heartbeat for "running"), the in-game menu's Dreamcast tab. The music renders in real time in an
  SDL audio stream of its own (`SKIES_DC_MUSIC_SYNC=1`: in the game's audio, in guest time, for dumps).
- **Results:** every output (the import, the converted uncensored files, the converted streams, the 726
  European 60 Hz files) byte-identical to the Dolphin build's; the music plays and restarts after a load.
  Pocket 5 at full speed with the music: ahead of the Dolphin build in battle (p99 34.3 vs 37.7 ms, ~0.5 W
  less), level at the save point (p99 ~36 vs ~36.5 ms), where it costs the standalone ~2 ms of p99.
- **Watch for:** SDL's device thread must keep up, or Aurora's audio push (queue > 250 ms) blocks the game
  thread (seen with the `dummy` driver and with the device thread on the little cores).

## macOS (2026-10-27): builds, plays on Metal, faster than the Dolphin build

The Mac session (M4 Mac mini, 4 performance + 6 efficiency cores, macOS 26.3, Command Line Tools only) built
aurora-backend 452d860 from a bundle in the worktree `~/Projects/skies-aurora-backend`. **No source change was
needed**: `cmake -S standalone -B build/standalone -G Ninja -DCMAKE_BUILD_TYPE=Release` + `ninja`, the prebuilt
Dawn (darwin-arm64) on Metal ("Device: Apple M4"), 12 harmless warnings. The Apple branches (`.dylib`,
`_NSGetExecutablePath`, `Contents/Resources` as the data folder in a bundle) work as written. Then two speed
commits, 8cbdaac (QoS) and 9a77aae (PGO), below.

- **Pairing trap:** a module only fits the DOL it was generated from (`module chunks matching the DOL: 5461 of
  5461`). The player's imported `games/GEAE8P/sys/main.dol` can be an older widescreen DOL than the installed
  app's module (5417 of 5461 there, 23,985 interpreted instructions in 40 s). Make the right one with
  `tools/applydolpatch.py ORIGINAL.dol <app>/Contents/Resources/patches/skies-widescreen.dolpatch OUT` and pass
  `--dol OUT`.
- **Checked (scripted with `SKIES_PRESS`, frame dumps, the log and the ini files; the in-game menu overlay
  wasn't seen, since the terminal has no Screen Recording permission):** logos, intro, title, the Cupil intro;
  `--continue card` into the field (Pirate Isle underground); the first battle from the Dolphin build's
  `02-first-battle.sav` won to the results screen; the menu (Z+Start): graphics rows written to GFX.ini and
  applied live, 60 fps (598 of 600 frames interpolated at 1.00x); a slot save (71 ms) and load (78 ms, the
  second A), auto-save every interval with the newest three kept; Dolphin `.sav` states 01 / 02 (code
  sections restored, the other file table served); the Language restart on the European disc (re-execs itself
  with the same argv); the standalone as `skies-run` in a copy of the release app, as the launcher starts it.
- **MSAA:** the prebuilt Dawn offers 1x / 4x (8x and 16x run as 4x, 2x as off). The M4's Metal
  `supportsTextureSampleCount` gives 1 / 2 / 4, **no 8x**, so a patched Dawn would add 2x only here.
- **Speed, one M4, the same DOL and module on both sides, 1x, muted, uncapped** (the Dolphin build:
  `tools/bench-mac.sh`, the release app dev-87ba4f3, fullscreen):

  | | Dolphin build | standalone | standalone + PGO (9a77aae) |
  |---|---|---|---|
  | 01 opening cutscene | 3.01x | 3.13x | 3.47x |
  | 02 first battle | 4.57x | 5.25x | 5.68x |

  Throttled it holds 1.00x. The PGO column comes from a module generated from this branch's DOL (c615035e):
  3.01x / 5.14x without the profile on that DOL.
- **PGO (9a77aae):** `tools/codegen-targets.txt` named a `pgo/macos.profdata` that was never trained, so every
  Mac module (the Dolphin build's too) was built without PGO. The profiles are keyed by game code, not by the
  host CPU: the desktop profile `pgo/windows.profdata` fits the arm64 build (22 functions discarded), the
  Android profile measures the same, and the pictures are byte-identical with and without it (frames 4200 and
  9700, `AURORA_SYNC_PIPELINES=1`). The macos line now names `windows`, as the linux line does. The Dolphin
  build's Mac module would gain too, but dev was left alone (the user's focus is the standalone).
- **QoS (8cbdaac):** `PinThread` had no macOS branch. The video thread (the main thread) already ran at
  user-interactive QoS; the game's SDL thread ran at the default class (log: `was 0x15`), which macOS moves to
  the efficiency cores when other work runs. macOS now defaults to `fast` = `QOS_CLASS_USER_INTERACTIVE`
  (`SKIES_PIN_CPU` / `SKIES_PIN_VIDEO=none` opt out). State 01 uncapped beside 8 busy default-class processes:
  2.18x before, 3.45x after; no load: 3.48x either way.
- **A covered window doesn't slow it down** (the whole display covered by an opaque window: 3.5x uncapped,
  1.00x throttled), unlike the Dolphin build (~0.67x, `tools/bench-mac.sh`'s note).
- **A minimized window pauses the game**, sound included: Aurora's `window.cpp` `lifecycle_event_watch` marks
  the window backgrounded on `SDL_EVENT_WINDOW_MINIMIZED` under `SDL_PLATFORM_APPLE` as well as Android, and
  `poll_events` waits in `SDL_WaitEvent` until it is restored. On Windows and Linux it keeps running.
  **The user keeps the pause** (2026-10-27), as long as it resumes: minimized at 12 s and restored at 32 s, it
  went on from where it stopped at 1.00x, with no catch-up burst (`Throttle` makes up at most 200 ms) and no
  GPU errors. (The restore came from a temporary test hook, `SDL_RestoreWindow`, as a Dock click does.)
- **Packaging, for item 10:** a plain configure leaves the release table empty, so the standalone never
  patches the game's DOL. Pass `-DSKIES_RELEASE_DISC_IDS="GEAE8P;GEAP8P"` and
  `-DSKIES_RELEASE_DOL_PATCHES=<the patches>`; then a plain DOL gets "Applied native DOL patches", and a DOL
  from an earlier import gets the "The game needs updating" box (modal: a headless test waits on it). libpng was
  linked as `@rpath/libpng16.16.dylib` with an rpath into the build folder; it is static on macOS since 654321b
  (zlib is the system's).
- **Open (the user, at the Mac):** the menu overlay by eye; the DualSense Edge (input, rumble, Button mapping,
  the controller's name) and the keyboard; sound against the picture by ear; Play from the launcher (a scratch
  copy of the app, portable, on a copy of the data); the picture and the sound after a restore from the Dock;
  a profile trained on the Mac (untried).

**Sweep (2026-10-27, later), 18 starts on the Mac:** every save state and card save on it (USA: boot, the
Dolphin build's states 01 / 02 / 03 / 04 / 07 / 10 / smoke, card files 1-3; EU: boot; Japanese: boot, the first
battle, a 45 s state, card files 1-3), each with the matching module (USA: this branch's DOL + the PGO module;
EU / JP: ws28's). Per start, uncapped, `SKIES_RTC=847000789`, `AURORA_SYNC_PIPELINES=1`: run A with scripted
input for 50 s of game time (A and the D-pad every 1.5 s), run B the same again, C1 without input and an
auto-save every 3 s, C2 loading C1's first auto-save in a new process; frames every 150-300 compared by hash.
Scripts and contact sheets in the Mac session's scratchpad (`sweep/run.sh`, `report.py`, `montage.py`).

- **No crash, hang or error line in 72 runs** (plus 24 at 4x + 4x MSAA and 24 at 60 fps on six of the starts:
  field, battle, world map, card, EU, JP). By eye (contact sheets of every start): logos, title, cutscenes with
  text, field, battles to the results screen, the world map, the save screens in English and Japanese, all
  right. State 03's ceiling view is the scene as saved (the Dolphin build shows the same), and `jp-pirate-isle.sav`
  is at the intro, despite its name (45 s after boot).
- **Repeatable:** A = B on 16 of 18 starts. The other differences are 1-2 pixels by 1-10 levels in a frame,
  mostly at the Pirate Isle underground save point's glow (card file 3, either region), once in the first
  battle at 60 fps; reruns are mostly identical (two of three): GPU-level flutter, invisible, but frame
  comparisons on Metal need a few pixels of tolerance.
- **Save-state round trip exact at the default scale:** C2 = C1 for every field start (the same flutter
  aside), and a menu slot save in battle (us-02, jp-battle) loads to frames identical to the run that went on.
  Auto-save never fires in battle, by design (`GAME_MODE` 6 only).
- **Bug found and fixed (fork 0052): a resolution change left the GX state's viewport at the old size.** Seen
  as a save state that changed the red save point's beam at an internal resolution other than 2x (card file 1,
  `GFX.ini` `InternalResolution = 4`: every frame after a load differed in the beam's sparks, 2-2.7% of the
  picture; 1x and 3x too; 2x, the window's scale, and MSAA alone exact). Not the copies: dumped at the frames
  after a load (a temporary `SKIES_COPIES_DUMP` hook), all four of the scene's R8 copies were byte-identical to
  the run that went on, while the picture already differed. Aurora's draw log (`SKIES_DRAW_LOG_FRAME`) showed
  it: in the run that went on, the 640x480 viewport mapped to **1707x960** (the scale before the 4x setting
  applied) while the scissor mapped to 2560x1920. `set_logical_viewport` / `_scissor` map the game's rects
  into `g_gxState.renderViewport` / `renderScissor` only when the game sets them; the frame start refreshed
  gfx's cached viewport from the logical one but not those, and the draws' uniforms (line and point sizes:
  the sparks), the full-screen quad check, the texture LOD bias and the replay context switch read them. A
  state load re-applies the GP registers, which mapped them right, so the loaded picture was the correct one,
  and the bug was in normal play: **after a resolution change (the menu, a config, a window resize while the
  resolution follows the window) some effects drew at the old size until the game set its viewport again.**
  The fix: the frame start keeps the GX state's mapped copies in step, and the replay context switch maps
  them again from the logical rects. Checked on the M4: the round trip exact at 1x, 2x, 3x and 4x, and at 4x
  with 60 fps (interpolated frames too); the 18-start sweep at the default scale gives the same frames as
  before (GPU flutter aside).

## Dusklight ports (2026-11-20)

The owner asked to look at [Dusklight](https://github.com/TwilitRealm/dusklight) (Twilight Princess on Aurora, CC0)
and upstream Aurora for improvements, and to port them all (gyro left out: the owner, "this game doesn't need
it"). Credited in `release/CREDITS.md` and the next build's notes. Local commits on `standalone-issues`:

- `522d085` **`patches/recompcore-aurora-upstream-gx.patch`**: upstream Aurora 4998dcc (no `tev_overflow` on
  inputs already in 0..1), 3094e04 (packed alpha compares on the pre-stage color inputs, as Dolphin), b684c0d (a
  textureless stage reads white), e938637 + 2bf0825 (color texgens read the lit channel) and the raw decoder's
  texgen type 3 = COLOR1. A probe of all 723 packaged pipelines (245 shaders) found Skies uses none of the last
  three: correctness only. Byte-identical frames on 6 sweep starts but one pixel at an alpha-tested edge in two;
  WSL's CPU renderer 5-20% faster. Already ours before: the min-filter swap (#35), Z8 copies, 1024 textures,
  depth correction. Not taken: the partial copy clear (no Skies texture copy clears), the FIFO-thread /
  PresentTarget / texture-cache reworks (ours differ), the Adreno Dawn toggle (not in our Dawn).
- `bab367e` **Frame rate: Display refresh** (`[Skies] FrameMode` 2): see `video.cpp` *FrameMode 2* and
  `docs/backlog.md`. Key points: each frame gets a show time (copy wall + present delay + two fields); a picture
  at T blends the two frames around T at t = (T - A) / (B - A); pictures a refresh apart, each made
  `DisplayLead` before its time, presented at it (Android: snapped to Choreographer's grid); frames join the
  timeline at their frame end (joining at the deferred replay came up to 20 ms late: half the in-betweens
  missing); the blend's pairing is a `BlendPlan` per pair (60 fps in-betweens byte-identical); the picture rate
  steps down by whole refreshes when the measured replay cost doesn't fit (`InterpDisplayAllowed`, up again after
  1 s with 10% room), and `ReplayFits` ignores replays in this mode. `SKIES_DISPLAY_TRACE`, `SKIES_DISPLAY_HZ`,
  `SKIES_MAX_FPS` / `[Skies] MaxFrameRate`. Launchers: `moderngekko-display-rate.patch`,
  `recompcore-android-display-rate.patch`.
- `90fecab` **Crash reports** (`crash.cpp`): signal / exception, native stack as module + offset, the game CPU's
  last registers, into the log; Windows minidump `crash-<time>.dmp` (threads, no heap) and `abort()`; then the
  previous handler (Android's tombstone, the renderer's crash safeguard). Logs `.1`-`.3` kept. Tested WSL +
  Windows with `SKIES_CRASH_TEST=segv|abort`. Not tested: Android (`_Unwind_Backtrace`), macOS (execinfo).
  Dusklight's Sentry upload (with consent) left out: a service and a consent screen are the owner's call.
- `efd581a` **Mouse camera** (`[Skies] MouseCamera`, off) and **controller light** (`[Skies] ControllerLight`,
  on), `SKIES_PEEK`, `SKIES_MOUSE_SCRIPT`. Field yaw found by RE: `fn_80090DBC` turns C-stick X into
  X / -44 * 512 a frame (C-stick right lowers the yaw); the camera object `0x80302F18` keeps its target yaw at
  +0x74 in type-2 rooms (+0x8C is the output, rewritten every frame from it; +0x44 stayed 0 there), so the mouse
  steers C-stick X toward a goal yaw (pokes don't stick). The game caps the turn at ~512 units a frame. WSLg's
  pointer warps feed back motion: test with the script, not xdotool. The light: no controller on the PC then.
- `331b8c5` launcher drag and drop + the new rows (`moderngekko-launcher-drop.patch`; compiled, not run), the
  Resolution row's drawn size, the AppImage's AppStream metainfo (`release/linux/`, appstreamcli passes).
- Left out on purpose: a stub-log split (the log has no stub noise); an update check (the builds don't know their
  version, and it would call GitHub at every start: the owner's call).
- `ec84654` **Android Display refresh**: the app asks for the display's top mode (`AndroidRequestFrameRate`;
  `dlsym(RTLD_DEFAULT)` doesn't find `ANativeWindow_setFrameRate` in the app's namespace, libandroid's handle
  does). AYN Thor (test app `com.skiesofarcadia.recomp.test`, now on `ws35dl1` = the ws35n15 module + this
  host): with the owner's 120 Hz setting on, field 118.8 pictures/s (p99 9-13 ms) and the Antonio battle ~120
  (p99 ~10), full speed; with the system's peak refresh at 60 the app stays at 60 (Android's cap, correct).
  Crash report on Android checked (`SKIES_CRASH_TEST=segv`: stack in log.txt, tombstone still written, the
  renderer safeguard not tripped). Not re-run at 120 after the exact-mode-rate change (the setting was back at
  60). Not run: the Pocket 5 (60 Hz: nothing to gain), a VRR desktop screen by eye.
- Windows test files left (C: rule: not deleted): `C:\dev\SkiesStandalone\dl-run.bat`, `dl*.log`, `crash-*.log`,
  `user-dl1`, `user-crash`, `build\skies-standalone-before-dusklight.exe` (the exe before these changes),
  `C:\Users\Public\skies-dl1.apk`. The `C:\dev\SkiesStandalone` copy now builds this branch's sources.
