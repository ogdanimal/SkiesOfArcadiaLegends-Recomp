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
  vendored GXRuntime once the `aurora-backend` branch is merged.
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

Before phase 1: merge the `aurora-backend` branch (behind its flag, off by default) after a rebase onto
origin/dev; its fork fixes and overlay are what phase 1 starts from.
