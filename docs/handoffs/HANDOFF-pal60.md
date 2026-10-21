# Handoff: the European disc at 60 Hz, and cutscene framing (2026-10-21)

Two local branches, stacked, **not pushed or merged**:

| Branch | Worktree | Commits on origin/dev `7587e66` | What |
|---|---|---|---|
| `cutscene` | `~/projects/skies-cutscene` | `7b1b076`, `79f44d7`, `af2083c`, `e34ddf3` | issue #10: listed cutscene shots zoom in widescreen (`cutscene.c/.S`, ws29); issue #3: the European intro in sync (`pal60.c`) |
| `pal60` | `~/projects/skies-pal60` | `cutscene` + `e6dd558`, `f94524a`, the docs commit | issue #3: the European disc's 50 Hz audio (`recompcore-pal60.patch`) |

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

## Checked

- Intro (game patch): WSL, JIT, by frame: unpatched 0.84 of the USA frame, patched the USA frame.
  Not played: the `m03` scenes; not on Windows / Android with a rebuilt module.
- Audio (frontend): the user by ear on Windows ("that was perfect"; `C:\dev\SkiesLang`
  `test-eu-converted.bat` / `test-eu-original.bat` / `test-us-reference.bat`); Linux, Windows (MSVC,
  `C:\dev\SkiesPal60`), macOS (the Mac session) and the Retroid Pocket 5 (test app) all make the same
  files (Android: 12 streams differ by rounding only). Table in the development notes.
- #10: WSL only (JIT + static module), the opening's two shots.

## Left before release

1. **Wait for the ws28 work** (branch `smove`, worktree `~/projects/skies-smove`, another session) to
   land on dev. Then rebase `cutscene` and `pal60` on it: `cave.ld` keeps `.late_bss_ws29` after
   `.late_bss_ws28` (check with `llvm-nm` that `qol_normal_speed` stays at `0x80002BDC`), the game
   patch becomes **ws29** (patch README build table, current-build line), and the release
   `.dolpatch` files are made again with `tools/mkdolpatch.py` (all three regions).
2. After the rebase: `tools/check-patches.sh`, then rebuild and check the modules (desktop PGO
   modules, Android) with the new DOLs: the intro on the European disc on Windows and the Retroid,
   one `m03` scene, the #10 shots on EU / JP.
3. Release notes: the draft is `build/notes-next.md` in the main checkout (entries added
   2026-10-21). Thank the issue #3 reporters there.
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
