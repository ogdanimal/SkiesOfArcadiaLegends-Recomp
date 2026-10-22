# Handoff: the Japanese disc (GEAJ8P, Eternal Arcadia Legends; 2026-10-20)

User request: "implement the japanese version of Skies of Arcadia Legends" (their ISO, now at
`../GameCube-Research/targets/SkiesOfArcadiaLegends/jp/Eternal Arcadia Legends.iso`, extracted beside
it in `jp/disc`). How it works: `docs/development-notes.md` (*The Japanese release*) and
`patches/game/README.md` (*The Japanese release*); this file is the state and what's left.

## State

Branch `japan`, worktree `~/projects/skies-romhack` (its submodules are worktrees of the main
checkout's with the patches applied), rebased on `origin/dev` `4bd2c05` (the credits, pushed
2026-10-20): `f55f594` the Japanese disc, `3f0bc95` the European Import a save fix, `9c00386` and
later docs, then the device checks' fixes: `8d3c50a` save states per version (all platforms: the
European and Japanese games keep `StateSaves/<disc ID>`), `adaced9` the Android first-run text, and
their docs. **Not pushed, not merged.** The ISO matches Redump (disc SHA-1 `bb06f5ba...`; the
research notes' regions table); research notes: `../GameCube-Research/targets/SkiesOfArcadiaLegends`
(*Japanese release*, `jp-address-map.md`).

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

1. **Secrets repo: done** (2026-10-20, the user's go-ahead): `main-jp.dol` (SHA-256 `4aa0f3dc...`)
   is in RecompSecrets (`83fe499`), so validate builds the `jp` region.
2. **Windows: done** (development notes: import, Continue, 1.000x on the world map, battle at 2x at
   full speed; `C:\dev\SkiesRomhack\build\rt-jp-win`, module `build\modules-llvm-jp26`, no PGO).
   **macOS:** the Mac session has the disc, the original DOL and the branch (sent privately as the
   user chose, deleted after download; `japan` was not pushed for it), made the Japanese arm64 module
   (2.5 min, no PGO, `d4867b46` / `27a4261c`) and passed everything it could test on `8d3c50a`
   (development notes), card-save Import and Continue and world-map speed included (with the
   converted Japanese card files; it had none). One unreproduced green window seen by the user on the
   Mac (Metal), noted there.
3. **Android: done** (development notes): `build/android/apk/skies-recomp-jp26b-2026-10-20-test.apk`
   (USA ws26 + Japanese modules, no Japanese PGO, the state-folder and text fixes) on the Retroid
   Pocket 5's and the Retroid Nova's test app (`com.skiesofarcadia.recomp.test`; the release apps not
   touched). Picker import, Start Game, card Continue, and full speed at 60 fps without a Japanese
   profile (two rounds on each device). Both test apps are left with the Japanese game imported and
   `Eternal Arcadia Legends.rvz` in Downloads; the Pocket 5's settings restored (30 fps, field speed
   2x as they were); the Nova's test app is a new install (60 fps set for the bench).
4. **In game, not yet seen:** a save written by the Japanese game itself (early in the story only at
   save points; the scripted walk didn't open the menu), the in-game menu's Language row on desktop
   and Android (automation input doesn't open the menu there; seen on macOS), a ship battle. Battle speed: done (WSL and Windows).
5. **Known difference:** Continue (the patch's direct card load) skips the Japanese load screen's
   bonus-file check (`EA_LEGEND_EXTRA #00`-`#02` on the card set or clear event flags 2852, 1096,
   2850), so the flags stay as the save has them. Loading through the title's own Continue menu
   (not the launcher's) does the check. Up to ws33 that direct load also gave the card code 0x270
   bytes of work where the Japanese load screen gives 0x274 (`fn_801A4070` stores the probe's mask at
   +0x270): one word written past the block on every Japanese Continue. Fixed in ws34 (review D9,
   `autoload.c` under `GAME_GEAJ8P`), checked on WSL only.
6. **Fixed on the way (European, every release since 0.10.0-beta):** Import a save put European card
   saves in `GC/EUR` (`save_import.cpp`'s `RegionFolder`, by the disc ID's letter), but the European
   game runs as NTSC-U, so Dolphin reads its card from `GC/USA` (`CEXIMemoryCard::GetGCIFolderPath`
   takes `SConfig::m_region`). Reproduced in the Linux release copy `build/rt-eu`: file 3 imported to
   `GC/EUR`, Continue's auto-load failed (`load_request` status 3); with `GEAP8P` -> `USA` the import
   lands in `GC/USA` and the same Continue loads Pirate Isle (status 2). Separate commit. Saves a
   European player imported before stay in `GC/EUR` unused: importing them again puts them in place
   (worth a line in the release notes). The Japanese disc is NTSC-J: `GC/JAP` on both sides.
7. **Fixed on the way (European since 0.10.0-beta, and the Japanese game): save states per version**
   (`8d3c50a`, `recompcore-state-folders.patch` + `moderngekko-local.patch`): Continue offered another
   version's newest state from the shared `StateSaves` and crashed on it (seen on the Retroid). Now
   `StateSaves/GEAP8P` and `StateSaves/GEAJ8P`; an earlier release's states move there by their header's
   game ID. Release notes draft (main checkout `build/notes-next.md`) has the line.
