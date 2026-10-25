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

- The Aurora fork (`~/projects/skies-aurora/GXRuntime`, 30 commits, `patches/` there): Skies renders exactly
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
HD textures, Button mapping, Rumble, auto-load of a card save (Continue). Aurora's keyboard layout is
its own default (arrows = D-pad, WASD = stick, J / K = A / B, U / I = X / Y, Q = Z, E / R = L / R,
Enter = Start), not the release's.

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
`fight-full.txt` route at 100 s). Dolphin: `capture/inproc-win.sh` with `user-bench-dc` (dual core).

| | Standalone | Dolphin Vulkan | Dolphin D3D12 | Aurora in Dolphin | Dolphin Null |
|---|---|---|---|---|---|
| RTX 5080, uncapped | **2.91x** | 2.33x | 2.30x | 2.18x | 2.27x |
| UHD 770 (iGPU), uncapped | **2.82x** | 2.16x | 1.87x | | |
| UHD 770, internal resolution 3 | **2.62x** | 2.16x | 1.90x | | |
| 4 E-cores, uncapped | **1.55x** | 1.06x | | | |
| 2 E-cores | **1.50x** | 1.03x | | | |
| 1 E-core | **1.46x** | 0.88x (below full speed) | | | |
| CPU at full speed (all threads) | **0.45 cores** | 0.60 cores | | | |

- Dolphin is CPU-bound in this scene on every renderer (Null, no drawing at all, is 2.27x); the standalone
  is about 25% faster on the full CPU, 45-65% faster on E-cores, and still above full speed on one E-core.
- **Where Aurora loses: GPU time per frame.** On the iGPU at internal resolution 3, Aurora kept the GPU 86%
  busy at 2.62x; Dolphin's Vulkan renderer 54% at 2.14x: per unit of game speed Aurora spends about 30%
  more GPU time. On a weak GPU (the Retroids, an iGPU at higher resolutions) that is the limit to work on.
- Not measured yet: a battle scene (the ws26 DOL drifts off the scripted route's encounter; the ship
  route (`boxfight.txt`) didn't reach one either), frame-time percentiles, the Retroids (needs the
  standalone's Android shell).

Tools (copies in `~/projects/skies-aurora/capture/`, used from `C:\dev\SkiesStandalone`):
`bench-sa.sh LOG SECS` (the standalone from a state: `STATE=`, `SETS='set X=1&& '`, `AFFINITY=HEXMASK`,
`THROTTLE=` empty for real time; averages the `[host] speed` lines after two), `pin.sh EXE MASK` (pins a
running process: `start /affinity` hangs through WSL interop), `cpu-sample.sh EXE WAIT SECS` (cores in use),
`gpu-luid.sh EXE WAIT` (which GPU adapter a process keeps busy, and how busy). `AURORA_LOW_POWER_GPU=1`
(fork 0030) makes Aurora take the iGPU; Dolphin's is `[Hardware] Adapter = 1` (`user-bench-igpu`,
`user-bench-igpu3` at resolution 3; `user-bench-rt` = dual core at full speed). An unmuted standalone run
is held to 1x by the audio queue even with `--unthrottled`: benchmark with `SKIES_MUTE=1`.
