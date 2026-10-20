# Handoff: the Dreamcast music option and the European disc (2026-10-17)

Two player requests: the Dreamcast version's music as an option, and the European disc (GEAP8P) for
its languages. The details live in `docs/development-notes.md` (*The European release*, *Dreamcast
music*), `patches/game/README.md` (ws22) and `patches/README.md`; this file is the state and what's
left.

## State

**Shipped in v0.10.0-beta** (2026-10-17, `61ac70e`; release story and the checks on every published
file: HANDOFF-release.md *Released*). Committed as `c131c82` (the work), then `179f817` / `128ac18`
(checks), `affabd9` (player READMEs), `2436886` (Europe in CI, the release session) and `61ac70e`
(the desktop update message, the release session). The files it added or changed:

| Where | What |
|---|---|
| `patches/game/` | ws22: `dcmusic.c` (paused song, channel-volume table), `fullwidth.S` (European widescreen fix: `ws_ortho_m00`), `widescreen.json`, `qol.c` (`dcm_frame`), `GEAP8P.addresses.json`, README |
| `tools/` | `gamepatch.py` (European DOL, refuses 16-bit address halves), `dolmatch.py`, `dcmusic/`, `ci-codegen.sh`, `build-release-linux.sh`, `build-release-win.bat`, `android-ws-build.sh`, `module-win.bat`, `module-llvm-win.sh` (GAME_ID) |
| `release/` | `skies-widescreen.dolpatch` (USA, ws22 at the release, output `e58354e5`; ws23 `018a9ffb` since `b1493b4`), `skies-widescreen-eu.dolpatch` (ws22 `30904d88`; ws23 `8b556a56`), `GEAP8P.ini`, `THIRD-PARTY.md` |
| `patches/` | `recompcore-skies-dcmusic-eu.patch` (new; in `recompcore.series` before `recompcore-input.patch`), `dolrecomp-halfword-byterev.patch` (new; in `tools/apply-patches.sh` / `check-patches.sh`), `moderngekko-local.patch` (regenerated), README |
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

## Open (after v0.10.0-beta)

0. **Import a save for the European disc (fixed 2026-10-20 on branch `japan`, `3f0bc95`, not on
   `dev` yet):** card saves went to `GC/EUR`, but this game runs as NTSC-U, so Dolphin reads
   `GC/USA`: an imported European save never loaded (Continue's auto-load failed). Now `GC/USA`.
   Saves imported with 0.10.0-0.60.0 stay in `GC/EUR` unused; importing them again fixes it.

1. **European speed on the Retroid:** measured 2026-10-17 evening, **0.93 (55 fps)** in the
   European ship battle against 1.003 for the USA one on the same APK (HANDOFF-android.md, top), so a
   European Android profile is needed: train it on ws23 (the game patch changed after the
   measurement; the plan and progress: HANDOFF-android.md *European PGO profile*). Started
   2026-10-17 night and paused by the user: the three European field states are resaved on ws23
   (`eu-05-pirate-base-ws23`, `eu-07-red-alarm-ws23`, `eu-10-worldmap-ws23`) and the instrumented
   APK is built (not installed); next a European battle state, then baseline, training and the
   profiled build. A desktop European profile
   (`tools/pgo-wsl.sh` on the European DOL) is open too. Ask before using the device.
2. **Android Dreamcast import** (Settings → Sound, CHD) and an Android European disc import: in the
   release APK, not tried on a device by this session.
3. **Streamed music** (`m01`-`m04`): done 2026-10-18 (branch `dcstreams`, on `dev` since `49526ab`,
   pushed, validate green; not released yet): `m02` / `m04` are the
   same recordings on both; `m03` (another mix) and the intro `m01` (another cut, behind **Intro
   music**) are converted into the GameCube streams' format and served in their place; the old reason
   here (different lengths) was wrong. Development notes, *Dreamcast music*.
4. **Continue crash (found while making those states, fixed in ws23 `b1493b4`):** the first battle
   after a Continue from a card save read `0x00000004`, both games (development-notes *Memory card
   auto-load*). Release patches are ws23: USA output `018a9ffb`, European `8b556a56`.

## Done (history of the open list before the release)

1. **Listen** on Windows (the user): Settings → Sound → Import a Dreamcast disc, Music: Dreamcast;
   a town (channel volumes), a battle and back (pause / continue), the camp menu. Before the release
   the user said "We've verified everything pretty much works right" (2026-10-17).
2. **European save screens: fixed** (a DolRecomp bug, see below). With the fixed module
   (`build/eu-gen22/module-linux`, DolRecomp with `dolrecomp-halfword-byterev.patch`): the text-pack
   count reads `0x68`, the title's Continue shows Load / Slot A / the file list with the European place
   names ("Pirate Isle Underground"), loading file 3 lands in Pirate Isle, the camp menu shows its text;
   no invalid reads. Saving from a save point **works** (2026-10-17, `build/eu-gen22b` module,
   ws22 DOL `30904d88`): file 3 auto-loaded, Vyse walked onto the save point, A opened Save / Slot A /
   the file list, "Create New File" wrote file 4 ("Pirate Isle Underground 2:08"), and a reboot with
   `AutoLoadCard = 4` loaded it (camp menu time 2:09). A savestate saved, walked away from and loaded
   back also works on the European game. No invalid reads; the test file was removed from
   `build/user-eu` afterwards. Even the converted USA saves load (the save format is the same).
   **Ship battle: works** (2026-10-17): `tools/shipbattle-state-win.sh`'s method with European
   addresses (debug DOL `build/game/GEAP8P-ws22-shipdbg` = widescreen + shipdebug.json, `30306223`;
   module `build/eu-shipdbg/module-linux`): card file 3 auto-loaded, `poke:0x8031DFE4:53485000` gave the
   Little Jack, weapons at `0x803103F4` (= `lbl_8030EE74 + 0x1580`, same layout as USA), `me500a.sct` to
   `0x80309370` + 0xF to `0x8031517C`. Baltor's battle at 1.00x, widescreen HUD, `qol-test-win.sh`'s
   ship input pattern: Focus phase, won at cycle 36, back to map 205, results (Vyse Lv 7, Aika Lv 6,
   300 gold), `qol_ship_live` 1 in battle and 0 after, ship HP 10000 throughout (Invincible), no
   invalid reads. Its opening state: `build/savestates/eu-17-ship-battle-baltor.sav` (European game only).
3. **Release checklist** (the release session's, for the next release with ws22): (a) the Retroid at
   full speed with the existing `android.profdata` on ws22: **passed 2026-10-17** (debug test app,
   `skies-ws22-2026-10-17.apk`, two rounds: red alert 1.004 / 1.004, world map 1.007 / 1.006, first battle
   1.003 / 1.003, ship field 1.004 / 1.004, 59.8-59.9 fps; `build/android/bench/ws22-60fps-r{1,2}`);
   (b) a USA regression pass: Continue / card auto-load (done on WSL ws22: save 1 auto-loaded), a
   battle at QOL speed (done on WSL ws22: battle speed 2x, the boss battle from `13-pre-battle` won
   and back to the world map), a 0.9.1 (ws20) save state loading on ws22 (states from older DOLs load: the
   state layout is the runtime's, which this work doesn't change; `05-save-point` etc. were made
   before ws21); (c) listening (1). All passed before the tag.
4. **European CI:** the secrets repo needs `main-eu.dol` (the user), workflows to call the two-region
   forms (the release session), European PGO profiles (desktop: `tools/pgo-wsl.sh` on the European
   DOL; Android: on the Retroid, the user's call). Done 2026-10-17 except the profiles: `main-eu.dol`
   added, `validate.yml` `REGIONS: us eu` (`2436886`).
5. **Streamed music** (`m01`-`m04`): done 2026-10-18 (branch `dcstreams`), see 3.
6. Android: build an APK with both modules (`android-ws-build.sh` with the European DOL), import a CHD
   for the music, the Settings → Sound rows, a European disc import (ask before using the Retroid).
   The APK is built (`skies-ws22eu-2026-10-17.apk`) and CI's release APK has both modules; the device
   checks are open item 1-2 above.

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
