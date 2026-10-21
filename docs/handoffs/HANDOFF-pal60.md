# Handoff: the European disc at 60 Hz, and cutscene framing (2026-10-21)

Two local branches, stacked, **not pushed or merged**, rebased on 2026-10-21 onto the ws28 work
(local branch `smove` `fea664d`, itself on origin/dev `7587e66`; another session's, **not on dev yet**).
The tips before that rebase are kept as `cutscene-pre-ws28` and `pal60-pre-ws28`.

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
  0.84; the first, 48 frames, reads 0.917: the polling step). Not played: the `m03` scenes; not on
  Windows / Android with a rebuilt module.
- #10, ws29: WSL, JIT, a New Game on the European and Japanese discs: the bridge (`0x90763369`) zooms
  1.307 and Fina's ship (`0xE6946063`) 1.288 on both, as on the USA disc (WSL, JIT + static module,
  before the rebase); screenshots: no deck ends at the edges, no waiting ship.
- One European JIT run ended by itself after about 10 minutes (no signal, no error, no OOM kill);
  the rerun didn't. Possibly another session stopping `moderngekko-run`; not confirmed.

## Left before release

1. When `smove` reaches dev: check `cutscene` still fast-forwards from it (if `smove` changed, rebase
   again and redo the ws29 checks above), then merge.
2. Modules with the ws29 DOLs: Windows LLVM + PGO (`tools/module-llvm-win.sh`, patch README), Android
   (`NAME=ws29 tools/android-ws-build.sh`). Then: the European intro on Windows (launch through
   `C:\dev\sync-controls.bat`) and the Retroid (ask first), one `m03` scene (`me298a` / `me250a`) on
   the European disc, a ws28 state loading on ws29.
3. Release notes: the draft is `build/notes-next.md` in the main checkout (entries added
   2026-10-21). Thank the issue #3 reporters (Renatodep, GLEclipseGaming) and #10's (Renatodep).
   The release re-patches the discs (ws29).
4. **Replies to #3 and #10 only after the release** (the user's call, 2026-10-21). #3: the
   European intro and battle sound are fixed; the language-import idea is in the backlog. #10: the
   opening's shots; more shots can be listed.

## Test setups left in place

- `C:\dev\SkiesLang`: the European game and a copy with the converted sound (`games\GEAP8P-60`),
  the three `test-*.bat` launchers, the user's controls (`C:\dev\sync-controls.bat`).
- `C:\dev\SkiesPal60`: the branch's sources and an MSVC dev build (`run-eu.bat`).
- Retroid Pocket 5, test app `com.skiesofarcadia.recomp.test`: the `pal60` APK
  (`build/android/apk/skies-recomp-pal60-2026-10-21-test.apk`, the ws28 modules), with
  `Games/GEAP8P` beside `Games/GEAE8P` (the app boots the USA one first; move it aside for the
  European one).
