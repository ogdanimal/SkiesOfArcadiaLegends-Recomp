# Handoff: data folder anywhere (2026-10-17)

Branch `datadir`, worktree `~/projects/skies-datadir` (submodules are worktrees of the main checkout's,
patches applied). Committed on `datadir` (feature, then docs); **merged into `dev` 2026-10-18** (rebased after `hud`:
`f273541` feature, then docs). How it works and what was tested:
[development notes, *Data folder*](../development-notes.md#data-folder-2026-10-17).

## What's in it

- `patches/moderngekko-local.patch`: `tools/user_directory.{hpp,cpp}` (lookup, move, pointer),
  `tests/user_directory_test.cpp`, launcher (Settings → Disc → Data folder, missing-folder box,
  in-process restart, `--move-data-folder`), runner (same lookup, refuses a missing folder), CMake.
- `patches/recompcore-data-folder.patch` (last in `recompcore.series`): Android `SkiesDataFolder.kt`,
  launcher activity flows, Settings rows, `DirectoryInitialization`, JNI in `SkiesImport.cpp`,
  `MANAGE_EXTERNAL_STORAGE`, branded manifest comment, gradle `-PskiesAppIdSuffix`.
- `tools/android-ws-build.sh TEST_ID=1`: the branded debug build as `com.skiesofarcadia.recomp.test`.
- README, release README, bug template, development notes.

`tools/check-patches.sh` passes.

## Merging

- Rebase onto `dev` after `hdtex` / `aspect` if they land first: all three regenerate
  `moderngekko-local.patch` and touch `tools/moderngekko_launcher.cpp`; regenerate the patch from the
  merged working copy, keep `data-folder` last in the series or reorder with `regen-recompcore-patch.sh`.
- With `hdtex` merged, the Data folder section's `busy` check should also cover the texture pack
  import thread.
- Release notes: the draft line is in `build/notes-next.md` (main checkout, local), in an HTML
  comment until the branch is merged; HANDOFF-release *Next* has the wording.

## Open

- SD card folders on Android: no card was in the Retroid.
- Linux folder picker needs xdg-desktop-portal or zenity (SDL); not tried on a real Linux desktop.
- Windows test tree `C:\dev\SkiesDataDir` (`build\start.ps1`, `click.ps1`, `keys.ps1`, `clickabs.ps1`;
  scratch data in `build\la`).
