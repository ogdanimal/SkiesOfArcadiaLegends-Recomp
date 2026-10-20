# Handoff: releases (Windows, Linux, macOS, Android)

## After v0.60.0-beta (2026-10-20)

- **On `origin/dev`:** `4bd2c05`, credits for cleartonic (the Hold B idea), Maeson (the rebalance
  romhack support was made for) and Shoopkid (the HD texture pack the import was tested with) in
  README, release/README and release/CREDITS.md (docs only: no CI run).
- **Branch `japan`, not pushed** (worktree `~/projects/skies-romhack`, on `4bd2c05`): the Japanese
  disc (GEAJ8P, `f55f594`, docs `9c00386`) and a fix for every release since 0.10.0-beta: Import a
  save put European card saves in `GC/EUR`, which the European game (run as NTSC-U) never reads
  (`3f0bc95`). HANDOFF-japan. `main-jp.dol` is in the secrets repo (`83fe499`, 2026-10-20); every package
  carries three modules. Checked on WSL, Windows, macOS and both Retroids (no Japanese Android
  profile needed); then save states per version (`dd9c0f7` after the rebase onto `3f4c4ec`).
- Draft notes for the next release: `~/projects/SkiesOfArcadiaLegends-Recomp/build/notes-next.md`
  (rewritten 2026-10-20 for the release after 0.60).

## Released (2026-10-20 UTC): v0.60.0-beta

**v0.60.0-beta is the current release** (prerelease, the user's version number; tag on `0eb4012`,
`main` fast-forwarded to it, release run 39382371726, published 07:10 UTC):
https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.60.0-beta. Notes:
`build/notes-0.60.0-beta.md` (local; same layout as 0.51), put on the release by `tools/release.sh`
(then edited once: the macOS file name needs the `v`). What it carries over 0.51.0: field speed
(below; game patch ws26), the macOS download (next sections), HD texture archives inside a pack and
Android's multi-select, and the in-game menu's dead-controls fix (`recompcore-menu-close`).

- The user gave full approval for the whole way through release (2026-10-20 ~00:40 local), version
  0.60.0-beta, README and notes in the earlier layout. `android-sign` approved by Claude on that
  approval after checking the run (release.yml on the tag at `0eb4012`, every other job green, only
  `android-sign` waiting) and the workflow diff since 0.51 (macOS packaging, codegen matrix).
- Before the tag: validate 39380369446 green on `0eb4012`; PII sweep of the tree clean (only the
  existing `/home/someone` test placeholders); the macOS zip of `1e7e586` checked on the Mac by the
  Mac session (below), the Windows and Linux CI packages here.
- Verified after publishing: SHA256SUMS OK; APK signer `e2c00b0b...` (the pinned one), versionCode
  420892, versionName 0.60.0-beta, no testOnly / debuggable; both games' modules and both
  `.dolpatch` files (outputs `c4152e68` / `c3bb09fe`, ws26) in the zip, the tar.gz, the AppImage,
  the macOS app and the APK; the packages' README says v0.60; 666 64-byte samples of the USA and
  European original DOLs: 0 hits in 14069 unpacked files.
- Not checked: the Retroid's own release app (the user's; only the debug test app was used), the
  release APK on a device, the AppImage on Bazzite / Fedora.

## On dev after v0.60.0-beta (2026-10-20): portable mode fixes (issue #6), unreleased

[Issue #6](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/6) (Windows): "with
portable.txt, installing the game still puts ~1.5 GB in AppData". Not reproduced on v0.60.0; the
launcher changes that came of it are on `dev` as `3f4c4ec` (pushed after a PII sweep of the tree and
history: no new hits; validate run 39474645790 green: codegen for all 8 targets, Windows, Linux, macOS, Android). Details and tests: development-notes *Data folder*
(the *Portable mode, issue #6* item); draft release-notes entry in `build/notes-next.md` (local).

- The reply on the issue is **not posted** (draft: check Settings → Disc → Data folder for
  *Portable*, file name extensions, the file beside `Skies of Arcadia Legends.exe` not in the zip's
  top folder, data left from before, which version; and that the next release handles the first
  three by itself).
- Branch `japan` (`~/projects/skies-romhack`) also rewrites `patches/moderngekko-local.patch`:
  regenerate it when it rebases onto `dev` (fresh worktree, local patch only; the macOS patch applies
  after it, and its context includes `user_directory.hpp`'s header comment).
- The main checkout's `lib/ModernGekko` working copy is **behind the committed patches** (found
  2026-10-20 with `tools/check-patches.sh`: the macOS hunks, field speed in the launcher and
  `recompcore-field-speed`, DolRecomp's `fp_fixups.cpp`, ...). CI and releases build from `patches/`,
  so only local builds from the main checkout are affected; re-apply before building there.

## Field speed, game patch ws26 (2026-10-20): shipped in v0.60.0-beta

User request: speed up cutscenes and traversal like the battle speed; then (same night) full approval
to release it as **v0.60.0-beta** with the README and release notes in the earlier releases' layout.
Built in the worktree `~/projects/skies-romhack` (branch `speed`, off `origin/dev` 87ba4f3), pushed to
`dev`. **Field speed** (Normal / 1.5x / 2x / 3x) in the in-game menu, both launchers and `[Skies]
FieldSpeed` (percent); hold Z and press X outside battle cycles it (a Z tap there is the game's). Game
patch **ws26** (`qol.c`: `QOL_SPEED_FIELD`, speeds in quarters, per-frame retrace budget), so CI
regenerates every module; both release `.dolpatch` files regenerated. Notes: development-notes *QOL*
(Field speed), `patches/game/README.md` (ws26), `patches/README.md` (`recompcore-field-speed.patch`),
decomp `docs/qol_cheats.md` *Field speed*. Release notes: `~/projects/skies-romhack/build/notes-0.60.0-beta.md`.

- Windows (`C:\dev\SkiesRomhack`, synced from the worktree; release build, ws26 module
  `build\modules-llvm-pgo-ws26`; `RUNNER=lib\ModernGekko\build-release\skies-run.exe
  tools/qol-test-win.sh ... field|speed`): 1.5x / 2x exact at 1.00x (world map, ship), 3x ~2.8x;
  battle 2x / 3x / 4x unchanged; Z tap unchanged, Z+X cycles; the menu row by eye.
- Retroid Pocket 5, the debug test app only (`BRAND=1 TEST_ID=1 NAME=ws26 DOL_EU=/nonexistent
  tools/android-ws-build.sh`, USA module only; the ws26 DOL pushed over its ws25 import; started by
  tapping Start Game, the branded app doesn't take the dev deep link; frontend-only rebuilds with
  `JNILIBS=` around the module): 2x settles at 1.5x (ship) / 1.25x (world map, cutscene) at 1.00x,
  retries back off; the launcher's Gameplay row by eye. The test app was left on the last of those
  (`skies-recomp-ws26g-2026-10-20-test.apk`, USA module only), with `[Skies] Encounters = 0` and
  `FieldSpeed` set (`Config/Dolphin.ini.before-fieldspeed` is its earlier file).
- The CI package of `1e7e586` checked: Windows (USA and European, `C:\dev\Skies060`), macOS on the
  Mac by the Mac session (zip, ad hoc signature, ws25 import asks for the disc and re-imports to
  `c4152e68`, Field speed 2x at 1.00x, Z+X to 3x, save state round trip), Linux on WSL (European:
  WSL can't hold even 1x on the world map, so it stepped to normal, as it should).
- Linux release tree compiles (WSL). macOS / Android release builds: CI.

## Shipped in v0.60.0-beta (on dev after v0.51.0-beta, 2026-10-19): macOS, HD texture archives, codegen in parallel

For the next release's notes: macOS (below), HD texture packs whose archives were put into one
folder / `.zip` / `.7z` now import, and Android picks several archives at once (`127b9a1`,
HANDOFF-hdtextures *2026-10-19*), and CI's codegen runs in parallel (next section; nothing for
players).

- **macOS joins the packages** (HANDOFF-macos): `6d54f6f` (the port, its patches and the Sys /
  Bluetooth pad / keyboard fixes; rebased onto dev, `check-patches` exact) and `04bc86d` (CI). The
  codegen job also generates `codegen/macos{,-eu}` (in the encrypted artifact; the cache key changed,
  so the first run regenerates everything), validate.yml's `macos` job builds
  `SkiesOfArcadiaLegends-<version>-macos.zip` on `macos-15`, and release.yml downloads it, unpacks it
  in *Refuse anything game-shaped* (both `.dylib` modules required by name) and names macOS in the
  notes. The zip is packed without extended attributes (`ditto --norsrc --noextattr --noqtn`). First
  run: 39341292040.
- Done for v0.60.0-beta: the macOS job green, the zip checked on the Mac (import, start, a state;
  by the Mac session on the `1e7e586` package), and the README's download table no longer says
  "from the release after v0.51.0-beta".
- The app is signed ad hoc, not notarized: players allow it once (README). Developer ID signing is in
  the backlog (*macOS release*).

## CI since v0.51.0-beta: codegen per target and region (2026-10-19)

- With macOS (above), codegen was 8 DolRecomp runs in one job, one after another; each takes ~12-15 min on the 4-core runner (4 min
  locally), so a cache miss held every build up for 1.5-2 hours (earlier misses: 52, 73, 90, 90 min).
  `04bc86d`'s run 39341292040 was such a miss (codegen 105 min): it changed `tools/ci-codegen.sh`, a
  cache input. Its `macos` job failed (macos-15's Xcode 16.4 libc++ has no `std::jthread`); `a8d0007`
  selects Xcode 26, and run 39354677233 was all green (codegen cached, 1 min). The zips of `127b9a1`
  and `b62186d` didn't start on the Mac (Homebrew's zstd etc. from the runner, linked by path);
  `87ba4f3` builds the bundled ones and fails the package on such a link (HANDOFF-macos).
- Now: `codegen` is a matrix, one job per target and region
  (8 at once, `fail-fast: false`). The targets' settings moved from the script to
  `tools/codegen-targets.txt` (NAME PROFILE CPU TRIPLE ARGS); each job's cache key has its target's
  line (spacing evened out) and its region's DOL patch only, so a new or changed target, or one
  region's patch, regenerates only those. Each job clones the secrets but keeps only its DOL and
  profile. Artifacts: `run-codegen-NAME` per run (the build jobs take `run-codegen-TARGET*`), and
  `codegen-NAME-KEY` kept 30 days. The build jobs still wait for all 8.
- On dev as `b62186d`. Its run 39357356589 regenerated everything (new keys) and was all green:
  codegen 02:09-02:28 UTC, 18.5 min for all 8 (Generate code 7 min windows-eu to 17 min android-us;
  the USA ones are slower, with PGO), the whole run 41 min (the run before: 105 min of codegen, 2 h
  14 min in all). 8 caches of 250-265 MB on `dev` and the 8 keyed artifacts (30 days).
- The `ci` environment takes only `dev` and tags, so a workflow change can't be tried on a side
  branch (a dispatch on branch `ci-codegen` was refused).
- Considered and not done (the user chose to wait): codegen on a self-hosted runner on the dev PC,
  or uploading locally made codegen for CI to fetch. The repo is public, so GitHub-hosted minutes
  (macOS too) cost nothing.

## Earlier release (2026-10-19 UTC): v0.51.0-beta

**v0.51.0-beta was the current release** (prerelease, the user's version number; tag on `992ae4f`,
`main` fast-forwarded to it, run 39193654173, published 03:39 UTC):
https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.51.0-beta. Notes:
`build/notes-0.51.0-beta.md` (local; same layout as 0.10 / 0.50), put on the release by
`tools/release.sh`. What it carries over 0.50.0: *Shipped in v0.51.0-beta* below (Linux libraries +
AppImage, Language tab, Romhacks + Hold B with game patch ws25, Dreamcast streamed songs, the HD
textures fix). The version strings were bumped before the tag this time (`992ae4f`).

- Before the tag, the PII sweep found `7f0e385` (the AppImage commit, made in the Mac clone) authored
  under a real name and a `.local` host, already on public `origin/dev`. With the user's OK, `dev`
  was rebased with only that author changed (trees identical) and force-pushed (2026-10-18); the
  commits from it up got new hashes (it is now `4c9dcdb`). The Mac clone must commit as the noreply
  identity. Check every new commit's identity before a push.
- `android-sign` approved by Claude on the user's delegation for this release only, after checking
  the run (tag, `992ae4f`, every other job green, only `android-sign` waiting) and the workflow diff
  since 0.50 (AppImage only).
- Verified after publishing: SHA256SUMS OK; APK signer `e2c00b0b...` (the pinned one), versionCode
  419244, versionName 0.51.0-beta, no testOnly / debuggable; both games' modules and both
  `.dolpatch` files in the zip, the tar.gz, the AppImage and the APK; the packages' README says v0.51;
  666 64-byte samples of the USA and European original DOLs: 0 hits in 11416 unpacked files.
- Not checked before release (the user chose to release): the HD textures fix on Windows, Android and
  in game; the AppImage / tar.gz on Bazzite or Fedora.

## Earlier release (2026-10-18): v0.50.0-beta

**v0.50.0-beta** (prerelease, the user's version number; tag on `db0f23a`, run
39104413365, published 10:49 UTC):
https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.50.0-beta. Notes are in
`build/notes-0.50.0-beta.md` (local). The run published the generic fallback body; it was replaced
with the notes file afterwards, so pass the notes file to `tools/release.sh` next time.

What it carries over 0.10.0: everything listed under *Shipped in v0.50.0-beta* below (Match screen,
HUD placement, HD textures, data folder, game patch ws23 + ws24, Windows fullscreen / DPI,
controllers, the missing-module message). Builds took ~26 min; `android-sign` waited for the
`release` approval until 10:48.

**Version strings lag the tag:** `README.md`, `release/README.md` (shipped in every package),
`BUILDING.md`'s examples and the bug template still said v0.10 at the tag. They were fixed after the
release (`dc5641e`, `dev` and `main`), so the v0.50 packages' own README says "v0.10". Before the next
tag, bump those four files first.

**macOS (2026-10-18, not in this repo yet):** a separate session on the user's Apple Silicon Mac
(Remote Control) builds the port from its own clone at `dc5641e`: Metal, Cubeb, DolRecomp LLVM to
`arm64-apple-macosx`, `gGEAE8P_recomp.dylib`. The USA disc boots there with `fallback=0` at about full
speed without PGO. Its work and notes live in that clone until it's merged; known gaps from this side:
`recompcore-win32-exit` / `-win32-fullscreen` have no macOS counterpart, `user_directory.cpp` falls back
to the XDG folder on macOS, and the default keyboard layout has no macOS key names.

## Earlier release (2026-10-17): v0.10.0-beta

**v0.10.0-beta** (prerelease, `61ac70e`, run 39070491053, published 22:29 UTC):
https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.10.0-beta. Notes are in
`build/notes-0.10.0-beta.md` (local; `release.sh` puts them on the release).

What it carries over 0.9.1:
- **European disc** (`GEAP8P`; both games' modules in every package, one disc installed at a time) and
  **Dreamcast music** (optional, from the player's Dreamcast disc, GDI / CHD): the DC-music / EU
  session, `docs/handoffs/HANDOFF-dcmusic-eu.md`. The USA game patch moved from ws20 to **ws22**.
- **Keyboard + remapping** (launcher Settings → Controls, in-game Button mapping; controller 1 follows
  the pad in use; the launcher no longer needs a controller): `docs/handoffs/HANDOFF-input.md`.
- **Android Back up saves** (`b527364`) and **codegen reuse in CI** (`177be84`, below).
- **A DolRecomp fix** (`patches/dolrecomp-halfword-byterev.patch`): the LLVM path byte-swapped `lhbrx` /
  `sthbrx` as 32-bit, so halfword stores wrote 0 (broke every European text lookup; in 0.9.1's USA
  modules it zeroed two header halfwords in Ninja's PVR texture loader, no symptom seen).
- **Desktop update message** (`61ac70e`): Windows / Linux patch the imported DOL in place, so a 0.9
  import can't be repatched to ws22 by itself. The launcher now says the update needs the disc once
  more, and the runner shows a message box instead of a silent failure. Select Disc with the same
  image refreshes the import; saves, states and settings are kept. Android keeps `main.dol.orig` and
  repatches by itself.

Checks before the tag (2026-10-17): the Retroid at 60 fps on ws22; a USA regression pass from 0.9.1
(the release app's auto-save loaded through Continue after the update repatched ws20 to ws22, battle
at 59.9, Z battle speed); European checks on WSL (save point + autoload, state round trip, a ship
battle at 1.00x); Windows keyboard re-run on the release code; the desktop upgrade path (launcher text
on Linux, runner box on Windows); Back up saves round trip on the Retroid (11/11 files identical).
Not measured: European speed on the Retroid (its module has no PGO; the device dropped off adb).

The user approved the `release` environment themselves (from their phone). Every published file was
checked: checksums; the APK is signed with the same key as 0.9.1 (`e2c00b0b...`), versionCode 417449
(> 0.9.1's 417012, so it installs over it), no `testOnly`; `gGEAE8P` + `gGEAP8P` modules and both
`.dolpatch` files in the APK, the Windows zip and the Linux tar; no disc image / DOL / GCI / save; 289
sampled 64-byte runs of the USA and European DOLs matched nothing in the 8701 unpacked files.

## Earlier releases (2026-10-17): v0.9.0-beta, then v0.9.1-beta

v0.9.1-beta: `4eebf7f`, run 39035626182, notes `build/notes-0.9.1-beta.md`.

How it went:
- **v0.9.0-beta, first try** (run 39004174568, tag on `3bd58db`): every build passed, then `android-sign`
  failed: the runner's newest build-tools (37.0.0) print `V3.0 Signer: certificate DN:` where older ones
  printed `Signer #1 certificate DN:`, so `tools/sign-apk.sh` counted no signer. Fixed in `a311b1a`
  (matches the text after the label; tested with 36.0.0 and 37.0.0). Nothing was published; the tag was
  deleted and made again on `a311b1a` (run 39027600679, published 11:36 UTC).
- **A player couldn't install the 0.9.0 APK** ("App not installed" after Play Protect passed, Snapdragon
  8 Elite Gen 5): it was `android:testOnly`, because `-Pandroid.injected.build.abi` (how the build stays
  arm64-only) is an Android Studio property and AGP marks such builds test-only; only `adb install -t`
  takes them, which is why the Retroid never showed it. `838f4c0`: `-Pandroid.injected.testOnly=false`,
  and `sign-apk.sh` refuses a test-only APK.
- **No Z on Android controllers** (found writing the controls table): the default profile's Z is SDL's
  `Shoulder R`, not an Android name; the Retroid only worked because its Z was mapped to L1 by hand. A
  fresh install also lost the face buttons. `4eebf7f` (HANDOFF-android *2026-10-17*).
- **v0.9.1-beta** from `4eebf7f` (published 14:32 UTC) carries both Android fixes; Windows and Linux are
  unchanged. The 0.9.0 APK was deleted from that release, whose notes now point Android players to 0.9.1.
- Signing: the user approves the `release` environment, or delegates it for one release; before
  approving check that the run is `release.yml` on the tag at the expected SHA, that only
  `android-sign` waits and that every other job passed.
- Every published file was checked: checksums, the APK's signer is the pinned key, no `testOnly`, no
  disc image / DOL / REL / GCI, and 3000 sampled 32-byte runs of the DOL only matched alphabet and
  counting tables that the MS runtime DLLs and the fonts hold too.
- The Retroid runs the release 0.9.1 APK with the user's data (backups in
  `C:\Users\Public\claude\retroid-backup-20261017`). Restoring with plain `adb pull` / `adb push`
  resets file times and Continue then picks a stale save: use `adb pull -a` / `adb push -a`.
- Players found the `dev` builds in the Actions artifacts (the repo is public); the Android one is
  debug-signed, so it can't update to or from a release install without an uninstall.

Since 0.9.1, before 0.10 (all in 0.10.0-beta):
- `177be84`: a release reuses its commit's codegen. Each cache key's encrypted codegen is also kept as an
  artifact named by the key for 30 days (once per key), and a cache miss downloads it (it must decrypt
  with `SOAL_ARTIFACT_KEY`) instead of generating; tags no longer pay ~55 min of codegen. Verified by
  run 39043183100 (dev cache deleted: codegen job 1.5 min, the reuse step 25 s). A tag must be on a
  commit validated within 30 days, or it generates again. `177be84` also carries the DC-music
  session's `lib/HighlyTheoretical` + `lib/libchdr` submodules (staged in the shared checkout and swept
  into that commit; the user approved keeping them).
- `b527364`: Android **Back up saves** (Settings → Saves, one `.zip` through the system picker) and
  Import a save takes that `.zip`; README and release README got the default controller table.
- `2436886`: the European disc in CI (`main-eu.dol`, SHA-256 `d0e9fc91...`, in the secrets repo):
  `REGIONS: us eu`, both modules in every package; the publish check knows both games' DOL hashes and
  requires both modules. No European PGO profiles yet.

## Shipped in v0.51.0-beta

Released 2026-10-19 UTC as v0.51.0-beta (tag `992ae4f`, validate 39191738208, release 39193654173).
Hashes here are the ones after the author rewrite (*Released* at the top); the earlier validate runs
(39175810228, 39178782588, 39187359736, 39189129132) ran on the same trees under the old hashes.
- **Linux on other distributions** (`4c9dcdb`, docs `ed60d09`; *Linux on other distributions* below):
  the launcher didn't start on Bazzite (the build machine's `libbz2.so.1.0`); Dolphin's own bzip2,
  zstd, lzma, zlib and lz4 are linked in now, and an AppImage ships beside the tar.gz. glibc 2.38.
- **Language** (`cd294e4`..`5e36c00`): a Language tab in the in-game menu, the launcher and Android
  Settings; A twice in the menu restarts the game in the new language (European disc).
- **Romhacks** (`5fa0082`..`0b01896`, HANDOFF-romhacks): a romhack that changes only the game's data
  plays on the release's module (Maeson's rebalance, romhacking.net 2859); one that changes code is
  refused at import with a message; **Hold B** (Settings → Gameplay) for no random battles while B is
  held; a romhack's save states in a folder of their own; "[GEAE8P, modified]" under Start Game.
  **Game patch ws25** (USA output `31e43962`, European `54adb1a4`): the release patches changed, so
  CI must regenerate every module (the patches carry the new keys; `tools/ci-codegen.sh` needs no
  change) and players' imports get repatched on update as for 0.10.0. The European Android module is
  still without a profile.
- Release-note wording: "Romhacks: play a romhack that changes the game's data, such as Maeson's
  rebalance: patch your disc image with it and pick that image. Hold B (Settings → Gameplay) stops
  random battles while you hold B. A romhack's save states are kept apart from the original game's."
- **Dreamcast streamed songs** (`d2e8430`..`49526ab`, development notes *Dreamcast music*,
  HANDOFF-dcmusic-eu): with the Dreamcast music on, the intro's song (`m01`, a different cut, disc 1)
  and a story scene's (`m03`, another mix, disc 2) play the Dreamcast disc's recordings, converted
  once into the GameCube streams' format (`recompcore-dcmusic-streams.patch`); **Intro music**
  (Dreamcast / GameCube) under the import in the launcher and on Android. The title / staff roll and
  ending songs are the same recordings on both, so they stay. No game patch or codegen change.
  Checked on WSL, Windows and the Retroid (test app). Music imported with 0.10 / 0.50 has no streamed
  songs: the player imports the disc again (the Sound tab says so). Release-note wording: "Dreamcast
  music: the intro and one story scene now play the Dreamcast recordings too (import your Dreamcast
  disc again to get them). Intro music (Settings → Sound) picks the intro's song: the Dreamcast cut
  doesn't line up with this intro's scenes."
- **HD textures fix** (`6ddc770`, HANDOFF-hdtextures 2026-10-18): a player's "texture pack is
  damaged" with the HD Texture Pack 4x Upscaled DDS v1 (six archives, no `GEA` folder). Packs
  without a `GEA` folder now import, and each import adds to what's imported (Remove clears all).
  Linux CLI only so far; Windows / Android / in game not checked. Workaround for v0.50.0 players:
  unpack all six into a folder named `GEA`, then Import a folder (desktop) or zip that folder and
  import the .zip. Release-note wording: "Fixed: texture packs without a GEA folder (such as the HD
  Texture Pack 4x Upscaled) were refused. Each import now adds to the textures already imported, so
  a pack in several archives can be imported one archive at a time."

## Shipped in v0.50.0-beta

**Released 2026-10-18** (the user's number and go-ahead: full approval to release, including signing
and device checks; *Released* at the top). Everything below went in; on 2026-10-18 the
`hud`, `datadir` and `hdtex` branches were merged in that order (rebased, patches regenerated,
`tools/check-patches.sh` green, Linux release builds and the CLI import / move tests passing).
- `b9f0157` Windows: the Fullscreen setting works at start (it needed Alt+Enter), no white border in
  fullscreen, sharp on high-DPI (4K at 150%) screens. Issue #2 / a Reddit report (HANDOFF-desktop
  *Status 2026-10-17, night*).
- `0e3a0cf` Controllers: the launcher finds the Steam Controller the game uses (it showed
  "#SettingsController_SteamController (not connected)" on SteamOS, issue #1), a controller moves
  through the launcher's home menu, and a profile with no controller buttons gets the default layout
  (HANDOFF-input *Steam Controller*). Release-note wording: the controller works in the launcher's
  menu; Steam Controller names; fullscreen fixes.
- `b1493b4` Game patch **ws23**: Continue from a memory card save crashed the first battle ("Invalid
  read from 0x00000004", black screen; ws20-ws22, so 0.9.0 to 0.10.0, both games, every platform),
  and save states made after such a Continue carried it. Checked on the Retroid in both games; desktop
  not run yet (HANDOFF-desktop). Release patches ws23 (USA output `018a9ffb`, European `8b556a56`);
  players' imports get repatched on update as for 0.10.0. Release-note wording: "Fixed: the first
  battle after Continue from a memory card save could crash (black screen)". Workaround for 0.10.0
  players until then: Continue from a save state, or start the game and load from the title menu.
  Draft notes for these: `build/notes-next.md` (local).
- **HD textures** (branch `hdtex`, merged 2026-10-18; the user lifted the hold): import a texture pack the player downloaded (.7z / .zip / folder) in Settings,
  Graphics, all three platforms; switch in game. Checked on Linux, the Retroid and Windows
  (HANDOFF-hdtextures). It was held for the world map's flight meter / compass "stretch", which
  turned out to be a Picture change made while the world map's menu was open (development-notes
  *Known issues*, left as is by the user). Release-note wording: "HD textures: import a texture pack you
  downloaded (Settings → Graphics); none is included. On Android take a pack's PNG download."
- **Match screen** (ultrawide; branch `aspect` merged into `dev` 2026-10-17 night): Picture gets a
  third choice that shapes the picture like the window or screen (4:3 to 32:9: 21:9 / 32:9
  ultrawides, 16:10, phones' 20:9), in the launcher, the in-game menu and Android Settings
  (`patches/recompcore-match-screen.patch`, development-notes *Match screen*). Checked on WSL,
  Windows (window resized through every shape) and the Retroid (16:9, and 20:9 via `wm size`); not
  looked at: the launcher's dropdown and the branded Android Settings row (same lists as the menu).
  Known: at 32:9 a few scenes show unbuilt scenery at the edges. Release-note wording: "Picture: Match screen fills any screen
  shape, ultrawide (21:9, 32:9) included; on an ultrawide raise Resolution for a sharp picture."
- **HUD placement** (branch `hud`, merged 2026-10-18): Settings → Graphics → HUD puts a wide picture's HUD at the screen edges
  (default), in a centred 21:9 or 16:9 area, or in the original 4:3 layout (`[Skies] Hud`, the game
  patch's `ws_hud_aspect`, `patches/recompcore-hud.patch`; development-notes *HUD placement*).
  Checked on WSL at 32:9 with a test DOL. The game patch is **ws24** (`patches/game/README.md`; USA
  output `c9da6ee4`, European `16241d84`, both release `.dolpatch` files regenerated 2026-10-18). Release-note wording: "HUD:
  on a wide picture, keep the HUD at the screen edges, in a 21:9 or 16:9 area, or in the original
  layout (Settings → Graphics → HUD)."
- **"Game files missing"** (same merge): a missing `g<ID>_recomp.dll` (partly extracted zip,
  antivirus) now says which file and what to do instead of "The game process exited with code 1";
  the runner exits with 20 for it and for the earlier-release DOL message, and the launcher shows no
  second box (development-notes *A missing game module*). Release-note wording: "A missing game file
  now says which one and how to fix it."
- **Data folder** (branch `datadir`, merged 2026-10-18): Settings → Disc → Data folder moves the imported game, saves and
  settings to any folder or drive (desktop: folder picker; Android 11+: internal storage, SD card or
  another folder, with All files access), Move Back to the Default, portable `portable.txt` beside
  the programs; a moved folder that isn't there is asked about at start. Checked on Linux, Windows
  (C: to D: and back) and the Retroid; SD cards untested (HANDOFF-datadir). Release-note
  wording: "Data folder: keep your game, saves and settings anywhere (Settings → Disc → Data folder),
  or portable beside the program (an empty portable.txt). On Android 11 and later the data can leave
  the app's storage, so it survives an uninstall."

## Linux on other distributions (shipped in v0.51.0-beta)

The v0.50.0-beta launcher didn't start on Bazzite: it linked the build machine's `libbz2.so.1.0`
(Debian's name; Fedora's is `libbz2.so.1`). `4c9dcdb` (written by the macOS session, built, checked
and pushed from Linux): `tools/build-release-linux.sh configure` sets `-DUSE_SYSTEM_LIBS=OFF`
(Dolphin's own bzip2, zstd, lzma, zlib and lz4 linked in; iconv stays glibc's), and
`build-release-linux.sh appimage VERSION` makes `SkiesOfArcadiaLegends-VERSION-linux-x86_64.AppImage`
from the packaged folder (pinned linuxdeploy `1-alpha-20251107-1`, its SHA-256 checked); validate
uploads it beside the tar.gz, `release.yml` unpacks it for the game-data check and publishes it.

Checked: validate run 39175810228 green (every job), and 39178782588 (with Language and Romhacks,
ws25, every module regenerated) too, its Linux build checked the same way (ws25 patches, CI AppImage
imported the USA RVZ and booted). The first one's artifact: the launcher and runner need no
bz2 / zstd / lzma / lz4 from the system; the modules need only libc and libm; glibc 2.38 and
`GLIBCXX_3.4.32` at most (built on ubuntu-24.04). The AppImage carries everything else but glibc,
libstdc++, the graphics drivers and ALSA (the zstd / lzma / lz4 inside are libsystemd's). Locally
(WSL), the AppImage through FUSE with a scratch HOME imported the USA RVZ, showed "Ready", booted the
opening; the release workflow's game-data check passes on both Linux packages. Not checked on Bazzite
or Fedora itself yet. The tar.gz still needs the system's `libbluetooth.so.3` (bluez-libs) and
PulseAudio client library; the AppImage has its own.

## Next

- Held by the user: the **nightly prerelease** (latest green `dev` on a fixed `nightly` tag, Android
  under its own app ID such as `com.skiesofarcadia.recomp.nightly`, no signing approval).
- European Android PGO profile: the European game runs 0.93x (55 fps) on the Retroid against 1.003 for
  the USA one (HANDOFF-android top); train `pgo/android-eu.profdata` on ws23, add it to the secrets
  repo (ask before using the device). Started 2026-10-17 night, paused after the ws23 states and the
  instrumented build (HANDOFF-android *European PGO profile*, Progress).
- The Retroid's app version: check HANDOFF-android before assuming (it ran the 0.9.1 release app with
  the user's data on 2026-10-17; release APKs install over each other, same key). Ask before updating it.
- The Japanese disc (branch `japan`, top of this file): pushed to `dev` 2026-10-20 after the
  0.60-follow-up run; a release still to be cut.
- Unscheduled requests (dual audio, ...): `docs/backlog.md`.

Other sessions on this repo (2026-10-17): DC music + European disc (shares the main checkout; commit
with explicit paths only), keyboard / remapping / input (own worktree, branch `input`), HD
texture packs (own worktree, branch `hdtex`), Match screen (own worktree `~/projects/skies-aspect`,
branch `aspect`, merged) and the data folder (own worktree, branch `datadir`).

Decided by the user (2026-10-11): first release **v0.9.0 beta**, **Windows + Android** (Linux later),
source in a **private repo for now**, Android app ID **`com.skiesofarcadia.recomp`** (like the user's
other ports). Hard rule: **no game files in anything we build or publish** (no disc, no DOL, no
extracted files); the player's own disc supplies them at import. Later: Linux joined for v0.9.0-beta, the repo
went public with that release, and v0.10.0-beta added the European disc.

## Status (2026-10-13): working towards the release

User: "let's work towards a prod release". Game patch ws20 committed (`feca39c`; battle freeze fix +
direct card Continue, Retroid-checked by the user); Windows zip and Linux tarball rebuilt locally
and smoke-tested (packaged runners: Continue -> card file 5 in 2.5 s).

The first CI run (validate, 2026-10-12, run 38308428665) failed in two jobs; codegen (1h49m) and
linux (30m) passed:
- **windows:** checkout died on SPIRV-Cross test files past Windows' 260-character path limit ->
  `git config --system core.longpaths true` before the checkout.
- **android:** `gradle/actions/wrapper-validation` rejected SDL's sample `android-project` wrapper jar
  (sha256 `2c23278a...`, not a released Gradle build; nothing references that project) -> the jar is
  deleted before validation instead of being allow-listed.
- Cost: codegen's encrypted output is now cached (key: DOL patch, DolRecomp pin + patches,
  `gen_module_tables.py`, LLVM apt version, secrets repo HEAD, a hash of the artifact key), and pushes
  to `main` no longer run validate (the tag's release run builds that commit anyway). Caches made on
  `dev` aren't visible to tag runs (GitHub scopes caches to the branch and the default branch), so a
  release still ran codegen once (until `177be84`: tags now reuse the validated codegen artifact).
- **Pushes don't start workflows on this repo** (the 10-12 and 10-13 pushes are recorded as
  PushEvents, no runs; Hybrid Heaven, public, runs on every push). validate is started by hand
  (`gh workflow run validate.yml --ref dev`); `release.yml` now also takes `workflow_dispatch` on the
  tag (the guard refuses any ref that isn't a tag), and `tools/release.sh` starts it that way when the
  tag push starts nothing within two minutes. Since the repo went public (2026-10-17) pushes to `dev` start validate again;
  pushes that touch only `**.md`, `.gitignore`, `.recomp.json` or the issue templates don't.

**CI security audit (2026-10-17, user: "are there any other ci security gaps"):** all `uses:` pinned
to SHAs, every checkout `persist-credentials: false`, no `${{ }}` inside `run:` scripts, token read-only
except publish, no `pull_request` / `pull_request_target` triggers; public logs hold no generated code
(no compiler warnings quote it; codegen's DolRecomp log stays on the runner). Fixed:
- Android's Gradle (unverified Maven dependencies; Dolphin has no `verification-metadata.xml`) ran in
  the job that decrypted the generated code, so a poisoned dependency could read that code or, with the
  runner's sudo, the artifact key from the runner's memory (the tj-actions attack). Now `android-module`
  (decrypt + NDK only) builds the module and `android` (Gradle) gets only that `.so`, no secrets
  (`tools/android-ws-build.sh` `MODULE_ONLY=1` / `JNILIBS=DIR`; JNILIBS path built locally, the module in
  the APK). Windows and Linux still decrypt and build in one job: their other code is pinned (vendored
  submodules, msvc-dev-cmd by SHA, Ubuntu-signed apt packages). **Green 2026-10-17 on `fe1e2a0`**
  (run 38996289115: codegen, windows, linux, android-module, android; android-sign skipped, tag only).
- `tools/setup-github-protection.sh` now also allows only GitHub's own actions plus the three
  third-party ones with SHA pinning required, turns on secret scanning + push protection, Dependabot
  alerts and private vulnerability reporting, and deletes every old artifact and cache (the encrypted
  codegen artifacts were downloadable by anyone once public; their key is replaced that day).
Left as is: the AES-CBC codegen artifact has no MAC (only produced and read inside one run); the
secrets repo token is a fine-grained token with an expiry, so CI's codegen fails when it lapses.

User decisions (2026-10-13): **v0.9.0-beta** (prerelease); **the repo goes public at release**;
clean-machine test in **Windows Sandbox** (the user enables it: admin + reboot); the **Retroid moves to
the release-signed APK** once CI makes it (backup, uninstall the debug-signed test app, install,
restore; ask before touching the device).

Release checklist from here:
1. **validate green 2026-10-17 on `a83b05e`** (run 38986860040; codegen 52 s from the cache, all four
   jobs passed). On the way: Windows `core.autocrlf false` (CRLF checkout broke the patches),
   setup-android `packages: platform-tools`, `tar --force-local` for the Windows LLVM archive. The
   published refs (`origin/main`, `origin/dev`) swept clean 2026-10-17 (the local clone also holds the
   draft history, which the plain history sweep flags; it isn't on this repo).
2. **Sandbox: done 2026-10-13** (`tools/sandbox/skies-sandbox.wsb` + `test.ps1`; the user enabled the
   feature). On a clean Windows (only the .NET copies of the VC runtime DLLs) the zip unpacks, the
   launcher runs, `--extract` imports the RVZ to DOL `f1ee9e8d`. Found and fixed (`5649cd9`): a console
   window beside the launcher (both exes were console programs; the release is a GUI program now), and
   no way past a missing Vulkan (automatic OpenGL fallback; development-notes *Known issues*). Sandbox
   has neither Vulkan nor OpenGL 3.3, so the game itself can't be shown there; the fallback was
   checked on the real GPU instead.
3. Going public (the user flips it): re-run the PII sweep (tree + history), then
   `tools/setup-github-protection.sh ogdanimal/SkiesOfArcadiaLegends-Recomp` (environments `ci` /
   `release`, rulesets for `v*` tags and `main`), the user runs
   `tools/set-release-secrets.sh --environments ogdanimal/SkiesOfArcadiaLegends-Recomp ogdanimal/SkiesOfArcadiaLegends-RecompSecrets`
   (hidden prompts; deletes the repo-level copies), then `environment: ${{ inputs.release && 'release' || 'ci' }}`
   on validate.yml's four jobs (B7 closed), validate green again.
   **Done 2026-10-17 (this session is primary for the release):** pre-public sweep of a fresh clone of
   the published repo clean (tree + history; 3000 high-entropy 24-byte chunks of the original DOL, raw
   and hex, found 0 times in all 189 published blobs, control found); the user made the repo public;
   `setup-github-protection.sh` ran (29 old artifacts/caches deleted); the user ran
   `set-release-secrets.sh --environments` (ci: SOAL_*, release: RELEASE_*, repo level empty);
   `environment:` lines added (ci on codegen/windows/linux/android-module, release on android-sign).
   `.recomp.json` added (user: status playable; original USA / GEAE8P / disc SHA-1 `46105320...` from
   `dolphin-tool verify`, no problems found; Wikidata Q2292272). Notes: `build/notes-0.9.0-beta.md`.
4. `tools/release.sh 0.9.0-beta SHA notes.md`; the `release` environment waits for the user's
   approval (Actions page) before the signed builds. **Done 2026-10-17** (see *Released* above).
5. Retroid onto the release APK. **Done 2026-10-17** (on 0.9.1-beta).

## Set up like the N64 recomps (2026-10-12, afternoon)

User: "prepare for release, set up like the N64 recomps" (Hybrid Heaven / Goemon). Decisions:
**Linux joins v0.9.0**; **repos the Hybrid Heaven way** (the full working history goes to a private
`SkiesOfArcadiaLegends-Recomp-draft`; `SkiesOfArcadiaLegends-Recomp` starts from ONE clean root
commit, noreply identity, branches `main` + `dev`, private until the user flips it); **the decomp
gets a private repo** (`SkiesOfArcadiaLegends-Decomp`, one clean root commit). Android GPU-driver
problems (Mali etc.) are handled through bug reports, no extra toggle (Dolphin's `DriverDetails`
already carries per-driver workarounds, Mali's readback one included).

Done:
- **Docs split like Hybrid Heaven's:** a player `README.md` (download table, getting started,
  quick fixes, controls, features, known issues, reporting), `BUILDING.md` (source, dependencies,
  codegen, per-platform builds, CI and releases), `docs/development-notes.md` (was `NOTES.md`) and
  `docs/handoffs/` (these files). `release/README.md` stays the copy shipped in every package.
- **Bug report template** `.github/ISSUE_TEMPLATE/bug_report.md` (Goemon's, for all three platforms:
  no game files, settings, Android *Copy device info*, desktop `Logs/ModernGekko.log`).
- **Linux in CI:** `tools/ci-codegen.sh` gains the `linux` target (x86_64-pc-linux-gnu, the desktop
  profile `pgo/windows.profdata`, which was trained on Linux anyway; `TARGETS=` limits a local run);
  `tools/module-linux.sh` links the module (system compiler, no DOL: prebuilt tables);
  `validate.yml` job `linux` on ubuntu-24.04 (older glibc than the 26.04 build machine, whose
  binaries need glibc 2.43) packages `SkiesOfArcadiaLegends-<ver>-linux.tar.gz`; `release.yml`
  downloads it, unpacks tarballs in the game-file check, wants each package's own module, sums it.
  Checked locally through the CI path (DolRecomp built as CI does, `TARGETS=linux`, module, package
  `v0.9.0-beta`): import of the RVZ with the packaged launcher (scratch HOME) gives DOL `39dd1e6c`,
  the runner boots into the opening in 16:9 with 0 fallbacks; the release check passes on the
  tarball; DOL byte sampling of the module 0/293 (control 13/13). 12 profile hash-mismatch warnings
  (the profile predates ws18; same for Windows). The runtime itself is not yet built on 24.04 (CI's
  first run is the check).
- **Fix:** an empty or relative `XDG_DATA_HOME` put the Linux user folder in the current directory;
  now it counts as unset (launcher + runner, `moderngekko-local.patch`, check-patches OK).
- `tools/release.sh VERSION SHA NOTES.md` (Hybrid Heaven's `hh-release` script, generic): refuses
  unless SHA is on `origin/dev` with a green validate run and `main` is behind it; fast-forwards
  `main`, tags, waits for the release, replaces its text with the notes.
- PII sweep script `~/projects/skies-pii-sweep.sh` (outside the repo: its patterns are PII; broad
  Hybrid Heaven patterns, `-i`, controls with planted strings incl. a PNG comment): recomp tree and
  decomp tree clean.

Next: commit; create the draft + decomp repos, push; clean root to `SkiesOfArcadiaLegends-Recomp`
(`main`, `dev`); check Actions is really enabled there (Hybrid Heaven's repo had it off despite the
API); first `validate` run; at go-public: rulesets on `main` / `v*`, secret scanning + Dependabot.

## Status (2026-10-11, late)

Done:

- **PII sweep** (the Hybrid Heaven recipe: tracked tree, every added line in history, commit
  messages, identities, filenames, tags, image metadata, the decomp repo, the shipped binaries; every
  pattern controlled with a planted string). Decomp repo clean. Recomp repo: local paths and the
  device serial removed from 7 tools and the Android handoff (`18a560e`); the APK's `libmain.so`
  carried the build machine's paths (~420 source-file strings): `tools/android-ws-build.sh` now passes
  `-ffile-prefix-map` (not rebuilt yet); the app ID that carried a personal domain is renamed
  (`0a4f289`). Windows release binaries, release images and the Windows zip: clean. **History still
  holds the old tool lines**: before the repo goes public, publish a fresh root commit or rewrite
  history (content AND messages), then re-sweep.
- **No game data in the binaries, checked:** 1,289 32-byte samples from all 8 sections of the
  original DOL (control: 13/13 planted samples found): 0 in the Windows module and the APK's module;
  the only hits anywhere (7, in `libmain.so` / the exes) are counting tables (`1b00 1c00 1d00 ...`).
  The module stores FNV hashes of the DOL's code ranges (`gen_module_tables.py`), not its bytes. The
  DOL patch (`release/skies-widescreen.dolpatch`, 27 KB) is our changes keyed to the original DOL's
  SHA-256, plus a few relocated game instructions around the hooks (as in ROM-hack patches).
- **Source reproduces the build:** fresh worktrees of the pinned ModernGekko / RecompCore /
  DolRecomp + `patches/` (README order) are byte-identical to the working copies (64 + 11 + 17
  files; the only extra is a stray `.profraw`). The three pinned commits exist on the public
  ExpansionPak remotes.
- **Licences:** `COPYING` (GPL-3.0, as ModernGekko / DolRecomp and the user's other ports),
  `release/THIRD-PARTY.md`, Dolphin's `Externals/licenses.md` shipped as `THIRD-PARTY-DOLPHIN.md`
  plus its `LICENSES/` texts. Fonts aren't bundled (system fonts).
- **Player README** `release/README.md` (requirements, getting started, features, controls, known
  issues, credits, licence).
- **Windows package:** `tools\build-release-win.bat package MODULE [VERSION]` makes
  `build\SkiesOfArcadiaLegends-v0.9.0-windows.zip` (named `SkiesRecomp-...` before the rename) + `.sha256` (one top-level folder: exes, module, `Sys`,
  `patches\skies-widescreen.dolpatch`, launcher art, app-local Visual C++ runtime, README, COPYING,
  notices; 56 MB). Checked: unzipped copy imports the RVZ (scratch `LOCALAPPDATA`) to DOL `7b4c416c`.
  Module: `C:\dev\SkiesRecomp\build\modules-llvm-pgo-ws16`. Not yet: a boot from the zip, a clean
  Windows machine (VM or Windows Sandbox).

**Later the same night (user: rename to "Skies of Arcadia Legends", "proceed"):**

- **Name:** Android app label, Windows launcher `Skies of Arcadia Legends.exe`, Windows user folder
  `%LOCALAPPDATA%\Skies of Arcadia Legends`, release files `SkiesOfArcadiaLegends-v<ver>-windows.zip` /
  `-android.apk`. The user's Windows folder was copied (robocopy, 5,580 files identical,
  `default-game.txt` re-pointed); the old `SkiesRecomp` folder is kept as a backup, and so is the
  old `SkiesRecomp.exe` in `build-release`.
- **Android shell** (`recompcore-skies-android-app.patch`, audit by a worker): Back menu cut to
  Pause / Overlay Controls / Exit (no Dolphin settings, its savestate slots, screenshot, disc swap),
  own boot hint, Dolphin's game list / TV UI / deep link not exported (`dolphinExported`), Files-app
  root with the app's icon, import and memory-card (`recompcore-skies-branding.patch`) messages
  without "Dolphin", licences in `assets/licenses`. Class names stay `org.dolphinemu.*` (invisible
  to players; renaming means every JNI symbol). `RELEASE=1 VERSION=x tools/android-ws-build.sh` builds
  a minified APK signed with the release key; checked with a throwaway key: signer, ID, label,
  `0.9.0-beta`, exports, no local paths, no DOL bytes. Not installed anywhere yet.
- **Windows zip** rebuilt under the new names; import from the unpacked zip checked again.
- **CI written** (`.github/workflows/validate.yml`, `release.yml`), not run yet (no repo). Helpers:
  `tools/apply-patches.sh` (verified: fresh worktrees + all patches == working copies),
  `tools/applydolpatch.py`, `tools/ci-codegen.sh` (the only step that sees the DOL),
  `-DMODULE_TABLES_INC` (`recompcore-module-tables-prebuilt.patch`), `PREGEN=` in
  `android-ws-build.sh`, the `.bat` files skip the local vcvars when MSVC is set up,
  `tools/make-release-key.sh`, `tools/set-release-secrets.sh`.

**Linux caught up (10-11, user request):** `tools/build-release-linux.sh configure|build|package
MODULE.so [VERSION]` (the bat's branding + EGL + `-ffile-prefix-map`) in
`lib/ModernGekko/build-release-linux`: launcher `SkiesOfArcadiaLegends` (no spaces, like the Hybrid
Heaven Linux binary), runner `skies-run`, user folder `~/.local/share/Skies of Arcadia Legends`,
`build/SkiesOfArcadiaLegends-v0.9.0-linux.tar.gz` (67 MB, module `build/modules-llvm-ws16`, not PGO).
Checked on WSLg from the unpacked tarball: import (patched DOL `7b4c416c`), the art home screen and
the Settings window (captures), boot to the opening in widescreen; no PII, no DOL bytes in the module.
No window icon on Linux (Windows embeds it; `icon.png` ships for a desktop entry). Not in CI; Linux
isn't in v0.9.0 (user decision), adding it = a third codegen target (x86_64 Linux) + a job.

**Device support (10-11, user: "2-4 now", from the Goemon / Hybrid Heaven review):** Android GPU
driver row + crash safeguard, About tab with Copy device info and Licences, the one-time 60 fps
prompt (NOTES *Slower devices and GPU drivers*), README *Reporting a problem*. Tools:
`patches/recompcore.series`, `tools/regen-recompcore-patch.sh`, `tools/check-patches.sh` (passes).
Other devices (Galaxy A15 Mali, Pixel 3 XL Adreno 630) are for later (user).

Open, in order:

1. **User steps for CI:** done 10-11 (secrets repo filled: main.dol + both profiles, checked by API;
   release key `~/skies-release-key/release.jks`, alias `skiesrecomp-release`; all six Actions secrets
   set on the port repo). Left: push the port repo (`skies-windows-android` as `main`, user's OK), then
   a first `workflow_dispatch` run of validate.
2. **Retroid (2026-10-12, user OK; done, HANDOFF-android top):** a debug-signed branded test build is on
   as the new app with the saves copied over (backup first: `C:\dev\SkiesRecomp\build\backup\retroid-before-rename-2026-10-12`;
   HANDOFF-android top). Claude can't build release-signed APKs locally (the key's password is a hidden
   prompt, by design), so the release-signed APK comes from CI (or the user's own terminal); installing it
   over the debug-signed test app needs an uninstall first, i.e. back up and restore the saves once more.
   The old-ID app stays until the user says to remove it.
3. Clean-machine tests (Windows Sandbox for the zip; Android fresh install), disc formats, a
   wrong-region disc refused, 4:3, both Continue paths, the Back menu, Import a save through the real
   pickers (a Dolphin `.gci` and `.raw`; done on the Retroid 2026-10-12, NOTES *Importing saves*).
4. **Review fixes (2026-10-12, [HANDOFF-review.md](HANDOFF-review.md)):** section A (9 items) before the
   tag: the Android driver-guard never runs after a crash (A1), disc import trusts FST file names on
   all platforms (A2), the stale ship-battle pointer in `qol.c` (A3), spare-EFB alloc failure (A4),
   card-save block count and state `uncompressed_size` unchecked at import (A5, A6), import failure
   can destroy a working install (A7), Activity recreation mid-import (A8), Dolphin Settings still
   reachable from the Back menu (A9). Section B's CI items (B7 guard-in-tagged-workflow, B8 publish
   check, B10 keystore check) before the first secret-bearing workflow run.
   **A1-A9 fixed 2026-10-12 (`0afcabc`; details and checks under each item in HANDOFF-review.md).** The game
   patch is now **ws17** (A3; `release/skies-widescreen.dolpatch` output `0fcdb786`), so the release
   builds need `configure` again. **ws18** later the same day (review section C's three game-patch
   items; output `39dd1e6c`; release builds reconfigured, ws18 APK built, not installed). The ws17 debug APK is on the Retroid (A2, A7, A8, A9 checked there;
   your ws16 auto-save Continues at 59.85 fps). Still open: A1 on a device (needs a marker in the
   app's private files; recipe under A1); B7 (needs Environments: a public repo or a paid plan;
   steps under B7). B8 and B10 done 2026-10-12; for B10, pin the release cert once (command under B10).
5. Changelog, tag `v0.9.0-beta` on main, the release workflow publishes.
6. Before going public: history scrub / fresh root, re-sweep, the source link in THIRD-PARTY.md.

## CI plan (like Hybrid Heaven / Goemon)

Same shape as `HybridHeavenRecomp/.github/workflows/{validate,release}.yml`:

- **Private secrets repo** (e.g. `<handle>/SkiesOfArcadiaLegends-RecompSecrets`) holding the
  original `main.dol` (SHA-256 `87c240ac...`, 3 MB; the only game input codegen needs) and the PGO
  profiles (`skies-android-gp.profdata`, 22 MB, and the Windows `merged.profdata`; function counters,
  no game bytes, but they're game-derived, so private). A read-only fine-grained token for it goes in
  the port repo's secrets; the clone is deleted right after copying (its `.git/config` holds the token).
- **`validate.yml`** (push to dev/main + `workflow_dispatch`, never `pull_request`, so forks never
  reach the secret; also `workflow_call` with `release: true` for tags):
  1. *codegen* (ubuntu): checkout with submodules, apply `patches/`, build `dolrecomp`, patch the DOL
     with the dolpatch (checked against its own `output-sha256`, ws18 `39dd1e6c...` since 2026-10-12 late), run DolRecomp (LLVM backend, PGO) for
     x86_64-windows and aarch64-android; encrypt the output with `ARTIFACT_KEY` and upload it
     short-lived (generated code is game-derived).
  2. *windows* (windows-latest): MSVC + LLVM 20, build the module (clang) and the launcher / runner
     (`build-release-win.bat configure|build|package`), upload the zip.
  3. *android* (ubuntu): NDK 29, module + `assembleRelease` (`-PskiesBrand`, `-ffile-prefix-map`),
     signed from `RELEASE_KEYSTORE_BASE64` etc. on release, verify the signature, upload the APK.
- **`release.yml`** (tag `v*` only): guard (tag on main), version from the tag (Android versionCode
  from it too, replacing minutes-since-2026), call validate, then **refuse anything game-shaped**
  (`*.dol`, `*.iso`, `*.gcm`, `*.rvz`, `*.gcz`, `*.wia`, `*.ciso`, `opening.bnr`, `*.elf`,
  generated chunks, any file whose SHA-256 matches the DOL) before `gh release create` (prerelease
  for `-beta`).
- Secrets: `SOAL_SECRETS_REPO_WITH_PAT`, `ARTIFACT_KEY`, `RELEASE_KEYSTORE_BASE64`,
  `RELEASE_STORE_PASSWORD`, `RELEASE_KEY_PASSWORD`, `RELEASE_KEY_ALIAS` (a new Skies release key,
  backed up like the Hybrid Heaven one; `~/projects/hh-make-release-key.sh` /
  `hh-public-secrets.sh` are the patterns: hidden input, each value proven before it is stored).
- Cost: on a private repo Actions minutes are metered (Windows at 2x); a cold Dolphin build on
  windows-latest is long, so cache with ccache / sccache keyed on the submodule pins + patches.

**User steps** (Claude can't create repos or enter secrets): create the private port repo and the
private secrets repo, push the DOL + profiles to the secrets repo, make the token, run the secrets
script, make the release key.

### User steps

1. (Created 10-11, private: `ogdanimal/SkiesOfArcadiaLegends-Recomp` and
   `ogdanimal/SkiesOfArcadiaLegends-RecompSecrets`, the user's `<Game>-Recomp` / `-RecompSecrets` naming.)
   Fill them: the port (push this repo's `skies-windows-android` branch as
   `main`; the release guard wants tags on main) and a secrets repo with the ORIGINAL `main.dol` at
   its root plus `pgo/windows.profdata` (`build/modules-llvm-pgo-gp/GEAE8P/pgo/merged.profdata`) and
   `pgo/android.profdata` (`build/android/pgo/skies-android-gp.profdata`).
2. A fine-grained token with read-only Contents access to the secrets repo only.
3. `tools/make-release-key.sh` (asks for a password; back the key up off this PC), then
   `tools/set-release-secrets.sh ogdanimal/SkiesOfArcadiaLegends-Recomp ogdanimal/SkiesOfArcadiaLegends-RecompSecrets` (asks for the token and the password).
4. Actions -> validate -> Run workflow.
