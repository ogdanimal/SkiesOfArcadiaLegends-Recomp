# Romhack support (2026-10-18)

## Status: options 1 + 2 implemented on branch `romhack`

Worktree `~/projects/skies-romhack` (submodules are worktrees of the main checkout's, patches applied),
branch `romhack` off `dev` (8e34d40), **uncommitted**. The user picked options 1 + 2 below
("this all sounds good... continue"), and said every GameCube version will be supported, so nothing
is USA-only (the known-change table is per region; Europe has none yet). How it works: development
notes, *Romhacks*.

- `tools/mkdolpatch.py` (code hashes, `data-original`, known changes), `tools/dolknown.py` (new),
  `tools/applydolpatch.py` (new keys); `patches/game/GEAE8P.known` (Maeson's hold-B code);
  `release/skies-widescreen.dolpatch` / `-eu.dolpatch` regenerated as **ws25**.
- `patches/game/qol.c` Hold B (ws25), `GEAP8P.addresses.json` +`0x8034763C`.
- `patches/moderngekko-local.patch`: `dol_patch.{hpp,cpp}`, runner, launcher (always extracts the
  disc picked, "[GEAE8P, modified]", Hold B in Gameplay).
- `patches/recompcore-romhack.patch` (new, last in `recompcore.series`): `[Skies] HoldB`, the in-game
  menu row, Android (Hold B row, `DolMatchesPin` in `PatchDol` and `SkiesNative.dolMatches`).
- Docs: README, release README (*Romhacks*), development notes, `patches/README.md`,
  `patches/game/README.md` (ws25 row; the SMC note corrected).
- `tools/check-patches.sh` passes.

**Tested (WSL, Linux release build `REGIONS="us eu"`, portable copy in `build/rt`, Linux ws25 USA
module without PGO in `build/gen-ws25`):** `dol_patch.cpp` against 7 DOLs (original, Maeson, Maeson
without its code change, an unknown code change, a change to the patched data pointer, ws24, ws22:
patched / patched with hold-B undone / patched / refused / refused / left alone / refused; a second
run changes nothing; `DolMatchesPin` true for exactly the accepted ones). `--extract`: a Maeson ISO
with one more code byte flipped is refused ("main.dol's game code is modified..."); Maeson imports
("a modified disc (romhack); its hold B ... is Settings, Gameplay, Hold B here"), its DOL has ws25's
code hash, every Maeson data byte and the patch's 3-byte pointer, its files are Maeson's; the
release runner accepts it (log "main.dol: modified game data (a romhack)"), 1.00x; world-map
encounters (`10-worldmap-android.sav`): HoldB=1 + B held no battle in 60 s, HoldB=1 without B a
battle after 20 s, HoldB=0 + B held a battle after 21 s, `smc_failed=0`. The vanilla RVZ imported over
it replaces it (DOL `31e43962`, vanilla files) and boots.

**Windows (2026-10-18):** `C:\dev\SkiesRomhack` (a copy of `C:\dev\SkiesDataDir` with this branch synced over,
`build-release` configured fresh, `REGIONS=us eu`), portable test app `C:\dev\SkiesRomhack\app`, ws25
module `build\modules-llvm-pgo-ws25` (LLVM + the desktop PGO profile, `tools/module-llvm-win.sh`).
`import.ps1`: the code-modified ISO refused with the message, Maeson imported (DOL `82a9fca1`); the
launcher's home screen reads "Ready: Skies of Arcadia Legends [GEAE8P, modified]" (`shots\launcher.png`);
encounters: HoldB=1 + B no battle in 60 s, HoldB=1 a battle after 17 s, HoldB=0 + B a battle after 16 s,
1.00x. (Start the runner with `Start-Process` from a script that returns at once: a PowerShell call
whose child keeps its redirected output blocks the WSL shell.)

**Retroid (2026-10-18; the device was idle on its home screen, the user's "do what you need to do"):**
the debug test app only (`com.skiesofarcadia.recomp.test`, `BRAND=1 TEST_ID=1 NAME=ws25`
`tools/android-ws-build.sh`, both modules with the Android profile). Its European import was renamed
aside, the Maeson game pushed as an earlier import (`sys/main.dol` unpatched + `main.dol.orig`): the
launcher repatched it on resume through `PatchDol` (DOL `82a9fca1`, the same as Windows) and offered
Start Game (`SkiesNative.dolMatches`). With `[Skies] HoldB = 1` (Android reads `Dolphin.ini` at app
start, and its automation has no `config:`): B held no battle in 60 s, without B a battle after 16 s;
with HoldB 0 and B held a battle after 28 s; 1.00x. Then put back: Maeson removed, the European import
and `Dolphin.ini` restored, test app stopped, home screen. The test app keeps the ws25 APK (it
repatches its European import to ws25 at its next start). The user's release app wasn't touched.

**European game (WSL, Linux ws25 EU module `build/gen-ws25-eu`):** the import gives the ws25 European
DOL (`54adb1a4`); `eu-10-worldmap-ws23.sav`: HoldB=1 + B no battle in 60 s, HoldB=1 a battle after
12 s, HoldB=0 + B a battle after 22 s (the derived pad pointer `0x8034AD5C` is right).

**Android picker import (2026-10-18, the test app, device idle):** the two ISOs pushed to
`Download/romhack-test` (the picker's Downloads view goes by the media index, which doesn't list
files pushed by adb: browse *Retroid Pocket 5 → Download* instead); the test app's European import
renamed aside first (an import removes the other region's folder). Select Disc → `codehack.iso`:
"This disc's game code is modified (a romhack that changes code). This port takes romhacks that
change only the game's data.", nothing left in `Games`; → `maeson.iso`: imported (DOL `82a9fca1`,
`main.dol.orig` the romhack's own), Start Game, `StateSaves/romhack-4d5febec` made, boots at 1.00x.
Put back after: the ISOs, the import and the empty folder removed, the European import restored.
Since then Android's Start Game carries "Ready: Skies of Arcadia Legends [GEAE8P, modified]" for a
romhack (its state folder is a `romhack-` one), and the no-disc hint names the USA and European discs;
both seen on the Retroid. The test app was put back on the language session's APK afterwards: its
European game was pushed by hand there (a ws24 `main.dol`, no `main.dol.orig`), which a ws25 build
can't repatch ("imported by an older version").

**Not done yet:** ws25 European modules for Windows and Android with PGO (the Android EU module is built without
a profile, as before); release modules through CI (`tools/ci-codegen.sh` needs nothing new: the
patches carry everything); a release. Merged into `dev` 2026-10-18 (`0e16132`, fast-forward); then **save states per disc**
(development notes, *Romhacks*): checked on WSL (a Maeson auto-save and an imported state in
`StateSaves/romhack-4d5febec`, the USA and European originals in `StateSaves`), Windows (Import a save
into the same folder) and the Retroid test app (auto-save there, Continue offers it; device put back
as before).



First case: **Skies of Arcadia: Legends "Maeson" 1.51** by Maeson (romhacking.net hack 2859, 17 Apr 2023), a
full rebalance: enemies, equipment, magic, ship parts and spells, EXP / magic EXP curves, fish, drops. It
ships as an xdelta against the USA ISO (MD5 `3e7fa5033c4a2704434fb6ba98195ecd`, which the user's
`skies.rvz` converts to) plus an optional 98-byte IPS "hold B for no random encounters" (cleartonic's
hack 5314, adapted). Same disc ID GEAE8P (the author says the game breaks card saves with another ID).
Archive: `SoALM 1.51.7z` in the repo root (ignored). Work files: `build/romhack-maeson/` (ignored).

## What the hack changes

Applied with xdelta3 (built from source, `build/romhack-maeson/xdelta3`) and diffed after
`dolphin-tool extract`: 310 files, same file list, sizes change (FST rewritten).

| Where | Files | What |
|---|---|---|
| `files/battle/` | 247 | `ebinit*.dat` x245 (enemy stats), `.tec`, `.lmt`, `.bin` |
| `files/field/` | 59 | `*.enp` x51 (encounter tables), `.evp`, `.bin` |
| `files/opening.bnr`, `sys/boot.bin`, `sys/fst.bin` | 3 | banner/title, FST |
| `sys/main.dol` | 1 | **data:** 8,955 bytes in D2 (`0x802ad000-0x802d9fff`: item, magic, character tables) + 2 in D3 (`0x802ec398`); **code:** 58 bytes in T1 |

The code change is the hold-B patch, already in the 1.51 xdelta (its bytes equal the optional IPS,
which writes ISO offsets `0xddfc8` / `0xe48f0` = DOL `0x800c1fc8` / `0x800c88f0`; main.dol sits at disc
`0x1f000`). The unrolled zero-fill at `0x800c88e4` (clears `0x80305a5c`, 0xac bytes) is rewritten as a loop
(it uses r16 as the index and leaves it 0), freeing a cave at `0x800c8920`. The encounter step's
`lwz r3,-0x79f8(r13)` at `0x800c1fc8` becomes `b 0x800c8920`; the cave reads `lwz r3,0x4b77(r8)` and,
when that word is 2 (B held), uses 0 instead of the counter. No section or size changes; header equal.

**Overlap with our game patch: none.** ws24 (`release/skies-widescreen.dolpatch`, 15,176 bytes) and ws22
(12,771 bytes) share no byte with the hack, so both stack on one DOL (`build/romhack-maeson/overlap.py`).

## What the runtime does with it (tested on WSL, ws22)

Game root `build/romhack-maeson/game-ws22` = the hacked disc's files + (hacked DOL with the ws22 delta).
Encounter test as in `tools/qol-test-win.sh encounters` (`10-worldmap-android.sav`, fly forward 60 s,
game mode 7 = battle), with and without `b=1` held.

| Module | Boot | Speed | Hold B | No B | Shutdown |
|---|---|---|---|---|---|
| shipped USA ws22 (`build/us-gen22/module-linux`) | OK, intro | 1.00x | battle after 14 s (**hack inert**) | battle after 20 s | `smc_failed=1 fallback=0` |
| built from the hacked DOL (`build/romhack-maeson/gen/module-linux`, no PGO, about 8 min) | OK | 1.00x | no battle in 60 s (twice) | battle after 15 s | `smc_failed=0` |

- **Data in `files/` just works.** Nothing pins the assets (`MODERNGEKKO_REQUIRED_ASSETS_SHA256` empty).
- **DOL data changes just work**, because the module reads data from RAM and only code chunks are hashed.
- **DOL code changes are silently ignored by the shipped module.** The runtime caught only the cave's
  chunk `[0x800C8800,0x800C8A00)`. The chunk holding the changed branch is reached by native-to-native
  calls, which never re-verify, so the old compiled code ran and nothing reported it. The same applies
  to any pre-boot or AR/Gecko-style code patch made without an `icbi`. Only a module compiled from the
  hacked DOL runs such changes.
- The release launcher/runner would refuse the hacked DOL today: `ApplyFilePatch`
  (`lib/ModernGekko/tools/dol_patch.cpp` ~255) requires the input SHA-256 to be the original's, and the
  release pins the patched DOL's SHA-256 (`MODERNGEKKO_REQUIRED_DOL_SHA256`).

## Options

1. **Data-only hacks (cheap, all platforms, shipped module).** Make import accept a DOL whose **text
   sections** equal the original's. Apply the dolpatch as a 3-way merge: each run's target bytes must
   still be original, and the hack's changed bytes must not overlap the patch. Pin the patched **text**
   hash instead of the whole-file hash (in the launcher, the runner and Android's repatch). Anything that
   changes code is refused with a clear message.
2. **Known code hacks reimplemented as our options (recommended for Maeson).** At import, put the
   hack's two text runs back to the original bytes (keep its data), and offer "Hold B to avoid
   encounters" in our game patch (`patches/game/qol.c` already has an Encounters option). Maeson then
   runs on the shipped module everywhere, PGO included, and the B feature works on every disc, EU too.
   Cost: one small qol.c feature plus a per-hack "known text runs" table.
3. **Per-hack module variants.** Build a module from each hacked DOL, keyed by a text hash (modules
   are found by disc ID only today, so the hack's module would collide with the USA one). This is
   generic, but each module is about 150 MB per platform, needs about 8 min codegen + build per
   platform, and needs its own PGO run.
4. **Runtime fix (generic, research).** At boot, check every chunk against the loaded DOL and send
   calls into mismatched chunks through the dispatcher/JIT. This needs DolRecomp's direct calls to
   check a per-chunk flag or go through a table. Cost and speed impact are unknown. It would also make
   cheats safe.

## Other points

- **Patching in-app:** players can patch the ISO with DeltaPatcher and import it. An in-app "apply
  .xdelta/.ips" (needed on Android) would have to patch the **image**, not the extracted files, because
  the IPS uses ISO offsets. xdelta3 is GPL-2, which fits the Dolphin-based GPL code.
- **Saves:** the hack keeps GEAE8P, so card saves and states would share folders with the vanilla game.
  Savestates carry the DOL's data sections, and `RestoreBootDolCode` restores only text. A vanilla state
  loaded into the hack therefore brings back vanilla item/magic tables, and the reverse also happens. Keep
  states per variant (or tag them); card saves are the player's choice, as the author intended.
- **Our QOL multipliers** (EXP, Gold, Magic EXP, Encounters) stack on top of the hack's own rebalance.
  HD textures and Dreamcast music are unaffected: no texture or sound files change, only the banner.
- **Region:** the hack is for the USA disc only.
- **Not checked:** a battle's enemy stats in-game (the files are read from disk, so this should work),
  Windows, Android, a full playthrough.
