# Handoff: the Japanese disc (GEAJ8P, Eternal Arcadia Legends; 2026-10-20)

User request: "implement the japanese version of Skies of Arcadia Legends" (their ISO, now at
`../GameCube-Research/targets/SkiesOfArcadiaLegends/jp/Eternal Arcadia Legends.iso`, extracted beside
it in `jp/disc`). How it works: `docs/development-notes.md` (*The Japanese release*) and
`patches/game/README.md` (*The Japanese release*); this file is the state and what's left.

## State

Branch `japan`, worktree `~/projects/skies-romhack` (off `dev` a087b2e; its submodules are worktrees
of the main checkout's with the patches applied). Committed locally, **not pushed, not merged**.

| Where | What |
|---|---|
| `patches/game/GEAJ8P.addresses.json` | the address table (209), from `tools/dolmatch.py --target GEAJ8P` |
| `tools/dolmatch.py` | `--target GEAJ8P`, writes `<ID>.addresses.json` (verified: reproduces the European table) |
| `tools/gamepatch.py` | knows the Japanese DOL's SHA-1 |
| `release/` | `skies-widescreen-jp.dolpatch` (ws26, output `d4867b46`), `GEAJ8P.ini` |
| `patches/recompcore-japan.patch` | new, last in `recompcore.series`: in-game menu, DC music, input, Android |
| `patches/moderngekko-local.patch` | regenerated: launcher `IsSkies`, Language tab, `EA_LEGEND.` saves |
| `tools/` | `build-release-{linux,macos}.sh`, `build-release-win.bat`, `ci-codegen.sh`, `codegen-targets.txt`, `android-ws-build.sh` |
| `.github/workflows/` | validate.yml (region `jp` in the codegen matrix, modules in every package), release.yml (publish check) |
| docs | README, release/README, BUILDING, patches READMEs, development notes, `.recomp.json`, bug template |

`tools/check-patches.sh` passes. Builds on this machine: `build/game/GEAJ8P-ws26` (SHA-1 `27a4261c`),
Linux module `build/jp-gen26/module-linux/gGEAJ8P_recomp.so` (no PGO), release build
`lib/ModernGekko/build-release-linux` configured `REGIONS="us eu jp"`, portable test copy
`build/rt-jp` (USA / European modules from `build/rt`, older), Android test APK (see below).
States: `build/savestates/jp-01-first-battle-results.sav`, `jp-02-pirate-isle-save-point.sav`.

## Checked (WSL)

Boot, opening, new game, first battle and results at 1.00x, widescreen; the launcher's import of the
ISO (pinned DOL); QOL and camera hooks live; Continue from a converted USA card save; save states;
the Dreamcast music. Details: development notes.

## Open

1. **Secrets repo:** add the original Japanese `main.dol` as `main-jp.dol` (SHA-256 `4aa0f3dc...`)
   before CI runs this branch; validate.yml now builds the `jp` region and fails without it. (The
   user's step; it's their private repo.)
2. **Windows: done** (development notes: import, Continue, 1.000x on the world map, battle at 2x at
   full speed; `C:\dev\SkiesRomhack\build\rt-jp-win`, module `build\modules-llvm-jp26`, no PGO).
   **macOS:** not built (the Mac session; `tools/build-release-macos.sh` takes the third module).
3. **Android:** test APK `build/android/apk/skies-recomp-jp26-*-test.apk` (USA ws26 + Japanese
   modules, no Japanese PGO). Not installed: ask before using the Retroid. Likely needs a Japanese
   Android PGO profile for 60 fps, as Europe did (0.93 without one).
4. **In game, not yet seen:** a save written by the Japanese game itself (early in the story only at
   save points; the scripted walk didn't open the menu), the in-game menu's Language row (ImGui, not
   in screenshots), a ship battle. Battle speed: done (WSL and Windows).
5. **Known difference:** Continue (the patch's direct card load) skips the Japanese load screen's
   bonus-file check (`EA_LEGEND_EXTRA #00`-`#02` on the card set or clear event flags 2852, 1096,
   2850), so the flags stay as the save has them. Loading through the title's own Continue menu
   (not the launcher's) does the check.
6. **Fixed on the way (European, every release since 0.10.0-beta):** Import a save put European card
   saves in `GC/EUR` (`save_import.cpp`'s `RegionFolder`, by the disc ID's letter), but the European
   game runs as NTSC-U, so Dolphin reads its card from `GC/USA` (`CEXIMemoryCard::GetGCIFolderPath`
   takes `SConfig::m_region`). Reproduced in the Linux release copy `build/rt-eu`: file 3 imported to
   `GC/EUR`, Continue's auto-load failed (`load_request` status 3); with `GEAP8P` -> `USA` the import
   lands in `GC/USA` and the same Continue loads Pirate Isle (status 2). Separate commit. Saves a
   European player imported before stay in `GC/EUR` unused: importing them again puts them in place
   (worth a line in the release notes). The Japanese disc is NTSC-J: `GC/JAP` on both sides.
