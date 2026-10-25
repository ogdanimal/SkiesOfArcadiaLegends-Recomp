# Handoff: the emulator-free path (decided 2026-10-22)

The goal: the port runs without Dolphin's core. The recompiled game (DolRecomp's output, the same
modules the port builds today) runs on **GXRuntime**, ModernGekko's standalone runtime
(`lib/ModernGekko/vendor/dolphin/GXRuntime`: CPU state, big-endian guest memory, boot, devices,
high-level emulation of the SDK), with **Aurora** drawing the picture. The Dolphin build keeps shipping
until the standalone build reaches parity; nothing here changes the default builds.

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

Save states are Dolphin's format and will not load in the standalone build. Memory card saves are plain
GCI files and carry over.

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

- The Aurora fork (`~/projects/skies-aurora/GXRuntime`, 27 commits, `patches/` there): Skies renders exactly
  like Dolphin on the Retroid Pocket 5, the Nova, Windows and WSL; 60 fps through a second GX context;
  the present rectangle (Picture option); the Dear ImGui overlay (the in-game menu). All of it is in the
  vendored GXRuntime on the local `aurora-backend` branch.
- The recompiled modules, the game patch (widescreen, QOL, cutscene framing), the build scripts, CI codegen.
- Card saves; the disc import (DiscIO is used by the launchers, which stay Dolphin-free already).

## Plan, with a gate per phase

1. **Boot spike (about one session).** Build a standalone runner (StrikersRecomp as the template) that loads
   the existing USA module on GXRuntime on Linux or Windows, Aurora drawing. Gate: the title screen. Decide
   gxcore vs the Aurora gx command path for Skies (the latter is the one verified frame-for-frame).
2. **Audio decision.** Prototype amuse against a DSP emulator. Gate: the intro music plays, in time.
3. **Playable on desktop.** Input, card saves, disc reads and streamed audio, speed. Gate: the first battle at
   full speed, a card save written and read back.
4. **Features re-hosted** in priority order: settings menu (the ImGui overlay exists), 60 fps, then the rest
   of the table above.
5. **Android shell**, then standalone builds released beside the Dolphin builds until parity.

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
  `--mmio-log`. Flags: `--headless --unthrottled --seconds N --idle-pc --dsp-coef`.
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
  display-list calls; `SKIES_WATCH=ADDR` logs the block after which a RAM word changed.

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
