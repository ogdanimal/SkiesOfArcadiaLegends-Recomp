# Handoff: the Dreamcast music option and the European disc (2026-10-17)

Two player requests: the Dreamcast version's music as an option, and the European disc (GEAP8P) for
its languages. The details live in `docs/development-notes.md` (*The European release*, *Dreamcast
music*), `patches/game/README.md` (ws22) and `patches/README.md`; this file is the state and what's
left.

## State

**Uncommitted** in the main checkout (shared with other sessions: commit with explicit paths, check
`git diff --cached` first). `tools/check-patches.sh` passes. The user (relayed by the release session)
wants it committed locally, not pushed, and held for a bigger release (DC music, EU disc,
keyboard / remap, Back up saves); the input session (projects-65) rebases on that commit.

| Where | What |
|---|---|
| `patches/game/` | ws22: `dcmusic.c` (paused song, channel-volume table), `fullwidth.S` (European widescreen fix: `ws_ortho_m00`), `widescreen.json`, `qol.c` (`dcm_frame`), `GEAP8P.addresses.json`, README |
| `tools/` | `gamepatch.py` (European DOL, refuses 16-bit address halves), `dolmatch.py`, `dcmusic/`, `ci-codegen.sh`, `build-release-linux.sh`, `build-release-win.bat`, `android-ws-build.sh`, `module-win.bat`, `module-llvm-win.sh` (GAME_ID) |
| `release/` | `skies-widescreen.dolpatch` (USA, now ws22, output `e58354e5`), `skies-widescreen-eu.dolpatch` (ws22, `30904d88`), `GEAP8P.ini`, `THIRD-PARTY.md` |
| `patches/` | `recompcore-skies-dcmusic-eu.patch` (new, last in `recompcore.series`), `dolrecomp-halfword-byterev.patch` (new; in `tools/apply-patches.sh` / `check-patches.sh`), `moderngekko-local.patch` (regenerated), README |
| `lib/HighlyTheoretical`, `lib/libchdr` | submodules (already on dev since `177be84`) |
| docs | `development-notes.md`, the decomp's `docs/sound.md` (separate repo, untracked) |

Builds on this machine (all with the DolRecomp fix): `build/game/GEAE8P-ws22`, `GEAP8P-ws22`; Linux
modules `build/us-gen22/module-linux/gGEAE8P_recomp.so`, `build/eu-gen22/module-linux/gGEAP8P_recomp.so`
(no PGO); WSL runner `lib/ModernGekko/build-egl/moderngekko-run`; `lib/ModernGekko/build/dolrecomp`
rebuilt with the fix. Windows tree `C:\dev\SkiesRecomp`
synced 2026-10-17: the branded release (`lib\ModernGekko\build-release`, `REGIONS=us eu`) builds.

## Done and checked

- **Dreamcast music in game (WSL, USA ws22):** see *Checked in game* in the development notes:
  battle / victory / game over / camp menu / pause and continue on two players / switching mid-song /
  old and new savestates / a 5-pair list with matching segment lengths / the world map's channel
  volumes / loudness matched to the GameCube music (1.4x). Measured with debug dumps on game time,
  **not listened to** (WSL has no audio here).
- **Import:** GDI and CHD (both of the user's discs), disc numbers, NTSC files kept over PAL;
  launcher Settings → Sound and `--import-dc-music`; Android Settings → Sound (CHD) compiles, not run on
  a device.
- **European disc:** boots, auto-load (Continue) of a card save, a battle, widescreen (the double image
  of the screen-copy effects fixed), the Dreamcast music on it. Static check of every function the
  patches use that differs (autoload, saves, ship battle rewards: all right as they are).
- **Release plumbing:** per-region pins (`release_games.hpp`), launcher / runner / Android import by
  disc ID, CI codegen per region (tested the European path locally), the Windows release builds with
  both regions (MSVC needed `/W0 /WX-` on Highly Theoretical and libchdr, and libchdr's LZMA without
  its MASM file).

- **Windows (2026-10-17):** a test release, `C:\dev\SkiesRecomp\build\SkiesOfArcadiaLegends-v0.9.2-dcmusic-test-windows.zip`
  (sha256 `7dd3fa8d...`; both regions, ws22 modules with the DolRecomp fix: USA LLVM + desktop PGO
  `build\modules-llvm-pgo-ws22b`, European LLVM without PGO `build\modules-llvm-eu-ws22b`; local
  only, not published). The checks below were made with the first package (before the DolRecomp fix);
  the USA module with the fix was checked on WSL (cold boot, a battle state). Checked in
  `C:\dev\SkiesRecomp\build\dctest` with a scratch `LOCALAPPDATA`: the launcher's
  `--import-dc-music` from the disc 1 CHD (116 banks), the runner patching a fresh USA `main.dol` to
  ws22 and booting at full speed, the Dreamcast engine started (block enabled) and the opening's
  songs started muted for it; volume was 0, so not heard.

## Open

1. **Listen** on Windows (the user): Settings → Sound → Import a Dreamcast disc, Music: Dreamcast;
   a town (channel volumes), a battle and back (pause / continue), the camp menu.
2. **European save screens: fixed** (a DolRecomp bug, see below). With the fixed module
   (`build/eu-gen22/module-linux`, DolRecomp with `dolrecomp-halfword-byterev.patch`): the text-pack
   count reads `0x68`, the title's Continue shows Load / Slot A / the file list with the European place
   names ("Pirate Isle Underground"), loading file 3 lands in Pirate Isle, the camp menu shows its text;
   no invalid reads. Saving from a save point not tried (A didn't open it where I stood); it uses the
   same screens. Even the converted USA saves load (the save format is the same).
3. **Release checklist** (the release session's, for the next release with ws22): (a) the Retroid at
   full speed with the existing `android.profdata` on ws22 (ask the user before using the device);
   (b) a USA regression pass: Continue / card auto-load (done on WSL ws22: save 1 auto-loaded), a
   battle at QOL speed (done on WSL ws22: battle speed 2x, the boss battle from `13-pre-battle` won
   and back to the world map), a 0.9.1 (ws20) save state loading on ws22 (states from older DOLs load: the
   state layout is the runtime's, which this work doesn't change; `05-save-point` etc. were made
   before ws21); (c) listening (1).
4. **European CI:** the secrets repo needs `main-eu.dol` (the user), workflows to call the two-region
   forms (the release session), European PGO profiles (desktop: `tools/pgo-wsl.sh` on the European
   DOL; Android: on the Retroid, the user's call).
5. **Streamed music** (`m01`-`m04`) stays GameCube: different recordings of different lengths, and
   the staff roll and ending are timed to them.
6. Android: build an APK with both modules (`android-ws-build.sh` with the European DOL), import a CHD
   for the music, the Settings → Sound rows, a European disc import (ask before using the Retroid).

## The DolRecomp halfword byte-reverse bug (release note)

`patches/dolrecomp-halfword-byterev.patch`: `lhbrx` / `sthbrx` in DolRecomp's IR / LLVM path stored
or loaded the wrong halfword (see the development notes, *The text packs and a DolRecomp bug*). It
broke the European game's text packs (save / load screens). The USA game's only uses are two
`sthbrx` in `fn_80298798`: Ninja's PVR texture loader (it checks the chunk tags `GBIX` / `PVRT` and
byte-swaps the little-endian Dreamcast-format header: a word with `stwbrx`, then the two halfwords
after it with `sthbrx`, which the bug zeroed), called three times from early code around
`0x800078AC`. No symptom of it has been seen in USA play (0.9.1's LLVM modules have it; the C-backend
builds never did), so 0.9.1 players seem unaffected, but the generated code of every LLVM module
changes with the fix.

## Gotchas

- The in-game menu (Z+Start) is an ImGui overlay that the automation screenshots don't show: test it by
  its effects (`tools/states-test-win.sh` does), not by screenshots. Open it with
  `press:z=1 wait:0.5 press:z=1,start=1 wait:0.3 release` (a `tap:start` replaces the held Z). The
  European Language row was therefore not seen; its effect (the boot language) was, in French.

- `tools/drive.py config:` writes the setting into `Dolphin.ini` for good (the dev config got
  `Invincible = 1` that way); `[Skies] AutoLoadCard` is one-shot (the game resets it).
- An invalid-read storm writes gigabytes of log in seconds: watch the log size when testing the
  European save screens.
- Debug dumps of the music (removed before the patch was made): `DCMusic` rendered on game time from
  `SkiesMusic::Poll` into a raw file, compared with Dolphin's `DumpAudio` DSP dump. Worth bringing back
  if the music needs measuring again (`git log -p` won't have it: it was never committed; the
  development notes describe it).
- From WSL, run Windows scripts as `cmd.exe /c 'call C:\...\x.bat'` (without `call` the exit code is
  lost) and set variables unquoted (`set REGIONS=us eu&& ...`).
