# Handoff: Uncensored (the Japanese Dreamcast art; 2026-10-20)

User request: "creating a setting that would allow you to uncensor the game" (GameCube and Dreamcast
changes), then "finish the uncensored work before releasing", scope tiers 1+2 (every visual change
that is an asset conversion) on the USA, European and Japanese discs; UI decided by the user: a
**Dreamcast tab** in Settings with the Dreamcast disc import and, below it, the Dreamcast options.
How it works: `docs/development-notes.md` (*Uncensored*) and `tools/uncensor/README.md`; this file is
the state and what's left.

## State

Branch `uncensor`, worktree `~/projects/skies-uncensor` (off `origin/dev` `1e31263`), **not pushed,
not merged**: `a0d1c85` the feature, then docs.

| Where | What |
|---|---|
| `patches/recompcore-uncensor.patch` | new, last in `recompcore.series`: `DiscIO/SkiesUncensor*`, `DCMusicImport` extra files, `DirectoryBlob` redirect, `[Skies] Uncensored`, in-game menu Dreamcast tab, Android Dreamcast tab + `describeUncensor`, `Data/Sys/Skies/uncensor-manifest.json` |
| `patches/moderngekko-local.patch` | regenerated: launcher Settings, Dreamcast tab; the import copies and converts; Remove takes the uncensor files too; `--import-dc-music` converts |
| `tools/uncensor/` | the manifest generator (Python), `convert_test.cpp`, README |
| docs | README, release/README, patches README, development notes, backlog (*Uncensored, what's left*) |

`tools/check-patches.sh` passes. Research and its scratch: `../GameCube-Research/targets/SkiesOfArcadiaLegends`
(`dcjp/` the extracted Eternal Arcadia discs, `dc/` the US / European Dreamcast discs, `uncensor/` the
working copies of the scripts, `uncensor/manifest/` the reference outputs the C++ test reads). The
Eternal Arcadia TOSEC zips are in the main checkout's root. Linux release build:
`lib/ModernGekko/build-release-linux` (`REGIONS="us eu jp"`); portable test folder
`~/projects/skies-uncensor-e2e` (binaries **copied**: a symlinked launcher resolves to the build folder
and ignores `portable.txt`), Japanese game extracted there, both discs imported.

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

## Next

1. **Windows build** of the branch and the user's look at the Dreamcast tab: import both discs
   through the picker, the "70 of 70 files converted" line, Uncensored on, a cutscene with Dyne
   (Pirate Isle cabin, from `jp-02` or a card save before it) and the tavern sign on Sailors' Island.
   The USA and European games too (same manifest, their own entries).
2. **Android:** APK of the branch on the Retroid (ask the user first): CHD import of both discs
   (convert the GDIs with chdman), conversion time on the device, the Dreamcast tab, the CMPR output
   on ARM (the converter's `dec_sha1` check fails the file if `fp-contract` changed the arithmetic).
3. **macOS** build (the Mac session).
4. Release notes (`build/notes-next.md` in the main checkout has a draft line).
5. The rest (backlog, *Uncensored, what's left*): Gilder's victory pose and Vigoro (motion / effect
   conversion), then the script work.

## Notes for whoever continues

- Rebuild the manifest after changing a rule: `python3 tools/uncensor/make_manifest.py OUT` (about 20
  minutes for the three games), write it compact into `Data/Sys/Skies/`, rerun `convert_test`
  against `OUT` (point `SKIES_RESEARCH`'s `uncensor/manifest` at it), regenerate
  `recompcore-uncensor.patch`.
- Testing in game: warp with `tools/drive.py poke:` of the script name into the map-change buffer
  and 0xF into the field state (USA `0x80305CF0` / `0x80311AEC`, Japanese `0x80305770` /
  `0x8031156C`), as `patches/game/shipdebug.c` describes; area files are re-read from disc on every
  map change, so a converted file can be swapped in under a running game.
- Converted files must keep their size: a different size after loading a state reads the wrong data
  and overwrites the game's code ("unknown guest instruction"), a black screen on Windows.
