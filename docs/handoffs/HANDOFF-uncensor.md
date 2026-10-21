# Handoff: Uncensored (the Japanese Dreamcast art; 2026-10-20)

User request: "creating a setting that would allow you to uncensor the game" (GameCube and Dreamcast
changes), then "finish the uncensored work before releasing", scope tiers 1+2 (every visual change
that is an asset conversion) on the USA, European and Japanese discs; UI decided by the user: a
**Dreamcast tab** in Settings with the Dreamcast disc import and, below it, the Dreamcast options.
How it works: `docs/development-notes.md` (*Uncensored*) and `tools/uncensor/README.md`; this file is
the state and what's left.

## State

Branch `uncensor`, worktree `~/projects/skies-uncensor` (off `origin/dev` `1e31263`), **not pushed,
not merged**: `a0d1c85` the feature, `31dc26e` docs, `e7100f0` victory-pose docs, `379cda7` the victory pose
in the converter, then its docs.

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

- In the app since `379cda7`: manifest ops `mlk_entry` (entries 1800100 and 6700526 of
  `beff/pcwin.mlk`, converted whole from the Japanese `BEFF/PCWIN.MLK`), `std_records`
  (`bchara/ma0050.std`: GameCube records, the Japanese ones from 979 on) and `pad` (zero-pad to the
  original decompressed size: smaller `pcwin.mlk` files crashed). Reference `tools/uncensor/victory.py`
  (`make_manifest.py OUT --victory-only` merges it in about a minute); C++ `MldToGc`, `MlkReplace`,
  `StdRebuild`; `convert_test` 216 of 216. Both GameCube files are identical on the three discs.
- **Checked in a normal build (WSL, Japanese game, `Uncensored = 1`):** the disc 1 import picked up
  `BEFF/` and converted both files (identical to the reference); the user saw Gilder smoke.
- **Seeing it every time:** `tools/victory12.sh GAME` in the test folder (from `jp-12`, Invincible
  off live, Vyse and Aika at 0 HP Focus, Gilder attacks and wins in round one): Gilder's pose,
  variant 26, three runs of three. With **Invincible** on (the test folder's `Dolphin.ini` has it on), the
  0-HP members stay eligible and the pose usually goes to them.
- Not checked: the USA or European disc in game (no state with Gilder in a battle; the files are the
  same as on the Japanese disc and convert byte-identically).
- WSL test copies in `~/projects/skies-uncensor-e2e/User/games`: `GEAJ8P` original (the one to use
  now), `GEAJ8P-unc`, `-unc2`…`-unc7`, `-swap`, `-swap-padded`: experiments from before the port,
  hard-link copies; ask the user before deleting them.
- Test tools (`~/projects/skies-uncensor-e2e/tools`): `jprun.sh GAME [STATE]`, `victory12.sh GAME`,
  `autofight12.sh GAME` (the `jp-12` battle, Attack until it ends, half speed), `ramdump.sh OUT`
  (24 MiB of RAM through `peek`), `snaptrace.py`. **No rolling captures** (`ringcap.sh` is not to
  be used; the user objected): one screenshot at a known moment, deleted after reading. The
  automation config key for speed is `config:Dolphin.Core.EmulationSpeed=0.5`.

## Next

1. **The user's Windows check** of the Dreamcast tab (still open): picker import of both discs in
   `C:\dev\SkiesUncensor\app`, Uncensored on, Dyne's cabin, the Sailors' Island tavern sign; USA and
   European games too.
2. **Android:** APK of the branch on the Retroid (ask the user first): CHD import of both discs
   (convert the GDIs with chdman), conversion time on the device, the Dreamcast tab, the CMPR output
   on ARM (the converter's `dec_sha1` check fails the file if `fp-contract` changed the arithmetic).
3. **macOS** build (the Mac session).
4. **Vigoro** (79 bones against 50: his scene motions from the Japanese file too) — ask the user
   whether it blocks the release. Then the script work (backlog).
5. Push, merge, release (HANDOFF-release says to hold the tag until this is merged); draft notes in
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
