# Handoff: the European disc at 60 Hz, and cutscene framing (2026-10-21)

Two branches, stacked, rebased on 2026-10-21 onto the ws28 work (branch `smove` `fea664d`, on
origin/dev `7587e66`). **All three are on `dev` since 2026-10-21 night** (fast-forward `7587e66..2200a73`,
validate run 39701868565 green). The tips before the rebase are kept as `cutscene-pre-ws28` and
`pal60-pre-ws28`.

| Branch | Worktree | Commits on `smove` | What |
|---|---|---|---|
| `cutscene` | `~/projects/skies-cutscene` | `96200de`, `0192d38`, `81efd71`, `d475866`, `d187d2e` | issue #10: listed cutscene shots zoom in widescreen (`cutscene.c/.S`); issue #3: the European intro in sync (`pal60.c`); `d187d2e`: game patch **ws29**, release `.dolpatch` files |
| `pal60` | `~/projects/skies-pal60` | `cutscene` + `d6ca7ca`, `79ebba2`, the docs commits | issue #3: the European disc's 50 Hz audio (`recompcore-pal60.patch`) |

The details are in `docs/development-notes.md`: *Cutscene shots framed for widescreen*, *European
intro in sync with its music*, *European 50 Hz audio*. Backlog: #10, *Dual audio*, *The European
languages for the USA game*.

## What the European release changed for 50 Hz

The European game has no 50 Hz compensation in code (at 50 Hz it runs 5/6 speed); the port runs it
at 60 Hz. Its data was made for 50 Hz in three places, all fixed now, and only for the European disc:

1. **Three cutscene scripts** (`me299a` the intro, `me298a` / `me250a` the `m03` scenes) divide every
   wait and motion time by 1.2. `patches/game/pal60.c` wraps the script loader's `memcpy` and makes
   each 1.2 a 1.0 (game patch, in the DOL).
2. **223 voice and effect streams** are 1.2 times the USA length (13 re-edited longer), and
3. **the sequence tempos of every enemy sound bank** (`e6*.info`, and `m5100000`) are 5/6 of the
   USA ones. `recompcore-pal60.patch` (frontend, `DiscIO/SkiesStreams`) makes 60 Hz copies of both
   in the user's `Cache/SkiesPal60/` on the first run (about 20 s in the background) and serves them;
   same sizes, so save states are unaffected. The table (`SkiesPal60Data.inc`) holds names, sample
   counts, factors, SHA-1 prefixes and field positions only.

## ws29 (the rebase onto ws28)

- Conflicts, all kept both sides: `cave.ld` (`.late_bss_ws28`, then `.late_bss_ws29`),
  `recompcore.series` (`battle-specials`, then `pal60`), `patches/README.md` (both rows and apply lines).
- DOLs (`skies-cutscene/build/game/GEA*-ws29`): USA SHA-1 `2926f2c1`, Europe `a868b179`, Japan
  `397da037`. `qol_normal_speed` stays at `0x80002BDC` on all three; `llvm-nm` against the ws28 ELFs:
  no ws28 variable moved, `cs_shot` (`0x80002BE0`, 0x38 bytes) is the only new one. MetroTRK area:
  0x2810 of 0x3210 bytes used.
- The three release `.dolpatch` files made again; each reproduces its ws29 DOL through
  `tools/applydolpatch.py` from the original disc's DOL.
- `tools/check-patches.sh` passes (the `pal60` worktree's RecompCore copy has `battle-specials` applied).

## Checked

- Audio (frontend): the user by ear on Windows ("that was perfect"; `C:\dev\SkiesLang`
  `test-eu-converted.bat` / `test-eu-original.bat` / `test-us-reference.bat`); Linux, Windows (MSVC,
  `C:\dev\SkiesPal60`), macOS (the Mac session) and the Retroid Pocket 5 (test app) all make the same
  files (Android: 12 streams differ by rounding only). Table in the development notes. Before the rebase.
- Intro (game patch), ws29: WSL, JIT, cold boot, the game frame at each intro camera shot (`cs_shot`),
  European against USA ws29: 0.993-1.005 of the USA frames over the 11 longer stretches (unpatched
  0.84; the first, 48 frames, reads 0.917: the polling step). Windows, the ws29 European static module
  (LLVM, no PGO: no European profile yet; `C:\dev\SkiesRecomp\build\modules-ws29-eu`) with the `pal60`
  MSVC runner, muted, automated: boots, and the same shots within about 1 % of the USA frames
  (the intro ends at 3763 against 3752). Not played: the `m03` scenes; not by ear on Windows; not on
  Android with a rebuilt module.
- #10, ws29: WSL, JIT, a New Game on the European and Japanese discs: the bridge (`0x90763369`) zooms
  1.307 and Fina's ship (`0xE6946063`) 1.288 on both, as on the USA disc (WSL, JIT + static module,
  before the rebase); screenshots: no deck ends at the edges, no waiting ship.
- The moon shots in the movie that plays when Start isn't pressed (Fina's ship crossing the full moon over
  the cloud sea, then the purple moon over the snow) show only the ship's sparkle trail, not the ship:
  the same in the original (the user checked, 2026-10-21). Not a bug. That stretch has no script
  camera commands (`cs_shot` logs nothing between the intro, map 299, and the map 297 demo).
- Android, ws29, after the push (2026-10-21 night): CI's module (`android-module` artifact of run
  39701868565) in a test APK (`BRAND=1 TEST_ID=1 NAME=ws29 JNILIBS=...`,
  `build/android/apk/skies-recomp-ws29-2026-10-21-test.apk`) installed over the Retroid Pocket 5's
  test app with its data. Both games had ws28 DOLs; the launcher re-patched each to ws29 on its own
  from `main.dol.orig` (USA `ac356927`, Europe `8e4cf16c`, the dolpatches' outputs). The European
  folder had been copied in by hand without `main.dol.orig` and first asked for the disc again ("imported
  by an older version"); a real import keeps the original, so it was given one. European intro from a
  cold boot against the USA one on the same device, `cs_shot` polled about 6 times a second with the
  game's frame count: the 10 shots both scripts share (the European scripts' timing words are part of
  the hash, so the rest differ) within -15..+18 frames of the USA's over 3,600 frames, ratio 1.005.
  Continue from the ws28 auto-save (European, mid-intro) on ws29: loads and plays at speed 1.00.
- The `m03` scenes, offline instead of in play (no state reaches disc 2's story there): the three
  European scripts decompressed (`tools/uncensor/aklz.py`) and `pal_script_copy`'s rewrite applied:
  `me299a` 320, `me298a` 348, `me250a` 348 `x / 1.2` timings, all rewritten; the `1.2` constants
  left (3, 5, 36) are in the USA scripts too, the same number, so they're not 50 Hz retiming. The
  intro shows the rewrite works in play; the `m03` scenes themselves still unplayed.
- One European JIT run ended by itself after about 10 minutes (no signal, no error, no OOM kill);
  the rerun didn't. Possibly another session stopping `moderngekko-run`; not confirmed.

## Left before release

1. When `smove` reaches dev: check `cutscene` still fast-forwards from it (if `smove` changed, rebase
   again and redo the ws29 checks above), then merge.
2. ~~Android ws29 and the European intro on the Retroid; a ws28 state on ws29~~ (done 2026-10-21
   night, *Checked*); the `m03` scripts checked offline (*Checked*). Left: Windows, the European
   intro by ear (`C:\dev\SkiesPal60\run-eu-ws29.bat`), optional. The release modules come from CI;
   local ones are for testing only.
3. Release notes: the draft is `build/notes-next.md` in the main checkout (entries added
   2026-10-21). Thank the issue #3 reporters (Renatodep, GLEclipseGaming) and #10's (Renatodep).
   The release re-patches the discs (ws30 since #11's fix; ws30 = ws29 + one wrapper, no data moved).
4. **Replies to #3 and #10 only after the release** (the user's call, 2026-10-21). #3: the
   European intro and battle sound are fixed; the language-import idea is in the backlog. #10: the
   opening's shots; more shots can be listed.

## Test setups left in place

- `C:\dev\SkiesLang`: the European game and a copy with the converted sound (`games\GEAP8P-60`),
  the three `test-*.bat` launchers, the user's controls (`C:\dev\sync-controls.bat`).
- `C:\dev\SkiesPal60`: the branch's sources (from before the ws28 rebase) and an MSVC dev build
  (`run-eu.bat`); ws29: `run-eu-ws29.bat` (syncs the controls, then the European game in `GEAP8P-ws29`:
  `sys` with the ws29 DOL, `files` a junction to `C:\dev\SkiesLang\games\GEAP8P\files`, the module from
  `C:\dev\SkiesRecomp\build\modules-ws29-eu`, user folder `ud`). `ud-ws29` / `auto-ws29`: the automated run's.
- Retroid Pocket 5, test app `com.skiesofarcadia.recomp.test`: the ws29 test APK (CI's modules,
  2026-10-21 night), with `Games/GEAP8P` (now with `main.dol.orig`) beside `Games/GEAE8P` (the app
  boots the USA one first; move it aside for the European one).
