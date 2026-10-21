# Handoff: Uncensored (the Japanese Dreamcast art; 2026-10-20)

User request: "creating a setting that would allow you to uncensor the game" (GameCube and Dreamcast
changes), then "finish the uncensored work before releasing", scope tiers 1+2 (every visual change
that is an asset conversion) on the USA, European and Japanese discs; UI decided by the user: a
**Dreamcast tab** in Settings with the Dreamcast disc import and, below it, the Dreamcast options.
How it works: `docs/development-notes.md` (*Uncensored*) and `tools/uncensor/README.md`; this file is
the state and what's left.

## State

Branch `uncensor`, worktree `~/projects/skies-uncensor` (off `origin/dev` `1e31263`), **not pushed,
not merged**: `a0d1c85` the feature, `31dc26e` docs, then the docs of the victory-pose work.

| Where | What |
|---|---|
| `patches/recompcore-uncensor.patch` | new, last in `recompcore.series`: `DiscIO/SkiesUncensor*`, `DCMusicImport` extra files, `DirectoryBlob` redirect, `[Skies] Uncensored`, in-game menu Dreamcast tab, Android Dreamcast tab + `describeUncensor`, `Data/Sys/Skies/uncensor-manifest.json` |
| `patches/moderngekko-local.patch` | regenerated: launcher Settings, Dreamcast tab; the import copies and converts; Remove takes the uncensor files too; `--import-dc-music` converts |
| `tools/uncensor/` | the manifest generator (Python), `convert_test.cpp`, README |
| docs | README, release/README, patches README, development notes, backlog (*Uncensored, what's left*) |

`tools/check-patches.sh` passes. Research and its scratch: `../GameCube-Research/targets/SkiesOfArcadiaLegends`
(`dcjp/` the extracted Eternal Arcadia discs, `dc/` the US / European Dreamcast discs, `uncensor/` the
working copies of the scripts, `uncensor/manifest/` the reference outputs the C++ test reads,
**`uncensor/victory/` the victory-pose converters**). The Eternal Arcadia TOSEC zips are in the main
checkout's root. Linux release build: `lib/ModernGekko/build-release-linux` (`REGIONS="us eu jp"`);
portable test folder `~/projects/skies-uncensor-e2e` (binaries **copied**: a symlinked launcher
resolves to the build folder and ignores `portable.txt`), Japanese game extracted there, both discs
imported, plus the victory-pose test copies (below).

## Checked

- WSL, Linux release build: both Eternal Arcadia GDIs through `--import-dc-music` (66 + 4 files,
  70 converted for GEAJ8P, about 5 s); the runner from `jp-02-pirate-isle-save-point.sav` warped to
  Pirate Isle `e`: the Japanese Dyne block in RAM with `Uncensored = 1`, the GameCube one with 0
  (edit `Dolphin.ini` only after `skies-run` has exited: it saves its config on exit).
- The C++ converter against the Python reference: 210 of 210 (`tools/uncensor/convert_test.cpp`).
- Windows, before the C++ port (the Python outputs, identical once decompressed), test copy
  `C:\dev\SkiesUncensor` (Japanese game, `run-uncensored-state.bat`): the user saw Dyne smoking in his
  cabin (`dyne-original.png` / `dyne-uncensored.png` there); the village sky (after the texture-ID
  splice fix); eight converted towns warped into without errors.
- **Windows build of the branch (2026-10-20, late):** `C:\dev\SkiesRomhack` (24 engine files and the
  top level synced from the worktree, `build-release` already `REGIONS=us eu jp`,
  `build\uncensor-build.bat`). Portable test app `C:\dev\SkiesUncensor\app`: v0.60.0's USA / European
  modules (their DOL patches are unchanged since), the Japanese module `build\modules-llvm-jp26`, all
  three games extracted with `--extract`, **no Dreamcast import yet** (left for the user's picker
  test); the GDIs in `C:\dev\SkiesUncensor\discs`. A scratch copy `app-cli` got the CLI import: 70 of
  70 for GEAE8P, GEAP8P and GEAJ8P, all 210 byte-identical to `uncensor/manifest/`. The user's picker
  test was interrupted by the victory-pose work and hasn't been reported.
- **Gilder's victory pose (Python only):** the user saw Gilder smoke in game (WSL, Japanese game, test
  build). See *Victory pose* below.

## Victory pose (2026-10-20, night)

How it works and every finding: development notes, *Uncensored* (the bullets *Gilder's victory pose*,
*Decompressed size*, *Testing battles by poking the party*).

- Files: `beff/pcwin.mlk` entries 1800100 and 6700526 converted whole from the Japanese disc's
  `BEFF/PCWIN.MLK` (`uncensor/victory/mld2gc.py`), and `bchara/ma0050.std` with the Japanese records
  from 979 on (`std2gc.py`, `build(..., tail=979)`). Both GameCube files are identical on the three
  discs, so one result serves all.
- Verified: `test_mlk.py` (all 37 unchanged entries match up to texture pixels), `std2gc.py` (968 US
  records reproduce, the rebuilt file is identical to the one tested), both recompress to the
  original's exact size and decode to their stated size. In game, the smoke only shows in victory
  variant 26, which the game rarely picks: confirmed with `ma0050_swap2627.std` (actions 26 and 27
  swapped, a **test build only**) on the three-member battle state.
- Not done: pad to the original decompressed size (the `pcwin.mlk` that worked is 715,992 against
  761,016; smaller ones crashed), the C++ port, the manifest entries, a check in a normal build.
- WSL test copies in `~/projects/skies-uncensor-e2e/User/games`: `GEAJ8P` original, `GEAJ8P-unc`
  (both converted files, my 64x64 smoke textures: the version that worked), `-unc2` (Sega's 8x8
  textures from `pcp09.mlk`, unpadded: crashes at battle load), `-unc3`/`-unc4` (padded variants,
  load fine), `-unc5` (Sega's native `pcp09` entry 200501 in the slot), `-unc6` (spawn mode 5),
  `-unc7` (original `pcwin.mlk` + the Japanese script), `-swap` (the 26/27 swap). All but `GEAJ8P`
  and `-unc` can be deleted (they're hard-link copies; ask the user first).
- Test tools (`~/projects/skies-uncensor-e2e/tools`): `jprun.sh GAME [STATE]`, `ringcap.sh` (rolling
  200-frame capture until `cap/stop`), `autofight12.sh GAME` (the `jp-12` battle, Attack until it
  ends, half speed), `snaptrace.py` (RAM snapshot tracer; too noisy, not useful so far). The
  automation config key for speed is `config:Dolphin.Core.EmulationSpeed=0.5`.

## Next

1. **Victory pose into the app:**
   - Converter (C++, `SkiesUncensorConvert`): an `mlk_entry` op (convert one Japanese MLK entry with
     the `mld2gc` rules; NJCM through the existing block conversion, NMDM, texlists, PVR to GVR with
     the existing CMPR / RGB5A3 encoders plus RGB565; rebuild the archive, pad to 32 / 8) and an
     `std_records` op (the output record list: GameCube record *i* or Japanese record *k* converted
     with a layout string of `H` / `W` / `B` per word, shipped in the manifest: it's structure, not
     game data); then zero-pad to the original decompressed size and recompress to the exact size.
   - `make_manifest.py`: emit those ops for `beff/pcwin.mlk` and `bchara/ma0050.std` (Dreamcast
     sources `BEFF/PCWIN.MLK`, `BCHARA/MA0050.STD` from disc 1; add them to the import's file list),
     regenerate the reference outputs, extend `convert_test.cpp`, regenerate the patch.
   - Check in a normal build: battles with Gilder until variant 26 plays (or the swap build), on the
     USA disc too.
2. **The user's Windows check** of the Dreamcast tab (still open): picker import of both discs in
   `C:\dev\SkiesUncensor\app`, Uncensored on, Dyne's cabin, the Sailors' Island tavern sign; USA and
   European games too.
3. **Android:** APK of the branch on the Retroid (ask the user first): CHD import of both discs
   (convert the GDIs with chdman), conversion time on the device, the Dreamcast tab, the CMPR output
   on ARM (the converter's `dec_sha1` check fails the file if `fp-contract` changed the arithmetic).
4. **macOS** build (the Mac session).
5. **Vigoro** (79 bones against 50: his scene motions from the Japanese file too) — ask the user
   whether it blocks the release. Then the script work (backlog).
6. Push, merge, release (HANDOFF-release says to hold the tag until this is merged); draft notes in
   the main checkout's `build/notes-next.md`.

## Notes for whoever continues

- Rebuild the manifest after changing a rule: `python3 tools/uncensor/make_manifest.py OUT` (about 20
  minutes for the three games), write it compact into `Data/Sys/Skies/`, rerun `convert_test`
  against `OUT` (point `SKIES_RESEARCH`'s `uncensor/manifest` at it), regenerate
  `recompcore-uncensor.patch`.
- Testing in game: warp with `tools/drive.py poke:` of the script name into the map-change buffer
  and 0xF into the field state (USA `0x80305CF0` / `0x80311AEC`, Japanese `0x80305770` /
  `0x8031156C`), as `patches/game/shipdebug.c` describes; area files are re-read from disc on every
  map change, so a converted file can be swapped in under a running game.
- Converted files must keep their compressed size (a different size after loading a state reads the
  wrong data and overwrites the game's code, "unknown guest instruction", a black screen on Windows)
  **and must not decompress smaller than the original** (battle load ran out of memory).
- The session scratchpad is wiped when a session restarts: keep scripts in the research folder or
  the test folder, not in `/tmp`.
- The game window: the user allowed the WSLg display for these tests; WSL can't see their
  controller, so their keyboard layout (`~/.local/share/Skies of Arcadia Legends/Config/GCPadNew.ini`)
  is copied into the test folder (the stock one is `GCPadNew.ini.stock`). Delete captures right after
  reading them: the user asked several times.
