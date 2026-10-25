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

- The Aurora fork (`~/projects/skies-aurora/GXRuntime`, 26 commits, `patches/` there): Skies renders exactly
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

**Next (phase 2/3):** listen to the AX output (unmuted run on Windows or with WSLg audio), EFB peeks
(read 0 now), the ws game patch DOL + its module, real-time speed on a real GPU (Windows build), then
gameplay to the first battle.

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
Scripted route to the save point (USA, unpatched DOL, the "P. Isle Underground" save as the newest file): `SKIES_PRESS="38:START,
53:START,56:A,60:A,63:A,66:A,73:RIGHT,73.2:RIGHT,75:A,77:A"` reaches the Save file list.
