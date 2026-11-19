# Handoff: dropping the Dolphin engine (plan, 2026-11-19)

**Decision (the owner, 2026-11-19):** "At some point we need to drop dolphin support since supporting both isn't
really feasible." The port moves to the new engine (the standalone host: the recompiled game on GXRuntime, drawn by
Aurora; `standalone/`) alone. Until then every feature is written twice (the settings rows four times: the
standalone's `menu.cpp`, the Dolphin build's `SkiesMenu.cpp`, the desktop launcher, the Android panel; review Q1), and
each engine has its own bugs and security surface (the 2026-11-19 card-path flaw was in both: `REVIEW-standalone-2026-11.md`
S1). Until the cut-over, no new Dolphin-engine-only features; a fix that needs both engines says so.

Two separate things, done in this order:
1. **Drop the Dolphin engine**: the game never runs inside Dolphin. Removes the Dolphin runner, Dolphin's video /
   audio / input / timing for the game, the RecompCore patches that host the game in Dolphin, the Dolphin-side
   in-game menu, and Android's Dolphin emulation screen.
2. **Drop Dolphin code**: the launchers stop using Dolphin's libraries. Optional and later: today the desktop
   launcher links Dolphin's `discio core uicommon` (`lib/ModernGekko/CMakeLists.txt`, `moderngekko-launcher`), and the
   Android app *is* Dolphin's Android app (Kotlin launcher, settings, imports, `libmain.so` in the launcher process;
   the game runs in its own `:game` process on `libskies.so`).

## Gates before the cut-over (proposed by Claude, not yet agreed)

- **A play-through on the new engine** (at least the USA disc; the European and Japanese ones by spot checks).
  Checked so far: sweeps (47 starts on WSL, 31 on the Pocket 5, a 5-start A/B run after the review fixes), bench
  scenes, routes, short runs on Windows / macOS / Android. Each new scene has found something before.
- **Testers' reports** from one or two opt-in test builds (`v0.90.0-standalone.5`, a `.6` with their fixes) with
  nothing serious open.
- **Performance re-measured** on the current builds (the "ahead of Dolphin" numbers in `HANDOFF-emulator-free.md`
  are from 2026-10-25/27).
- **The owner's call on Android 7 / 8**: the new engine needs Android 9 (API 28). Those players keep the last
  Dolphin release; the cut-over APK raises `minSdk` or keeps a notice.
- Already decided: the Windows File menu and netplay are not carried over; cheats are memory writes only (no
  encrypted Action Replay, conditionals or code-patching Gecko types: the code is native).

## Phases

**Phase 0: the last Dolphin-engine release.** v0.83.1 = v0.83.0 + `recompcore-gci-state-paths.patch` (the card-path
security fix), on the local dev-based branch `gci-state-fix` (`f427c65`, worktree `~/projects/skies-gcifix`; it
applies on dev's 41-patch chain). Push and release need the owner's OK. After it the Dolphin build is frozen:
another release only for a severe security issue.

**Phase 1: test builds until the gates pass.** `v0.90.0-standalone.N` drafts from `aurora-backend`
(`HANDOFF-release.md`, first section). Never publish `.4` (it has S1).

**Phase 2: the cut-over release** (a major version, e.g. v1.0.0): the new engine as the normal release.
- Branches: `aurora-backend` already holds everything on dev (v0.83.0 merged 2026-11-18); it becomes the main line
  (merge into dev and main), and `release.yml`'s guard goes back to plain tags on main.
- The release notes: back up saves first; card saves carry over; the Dolphin engine's states convert when loaded
  (one way: the new engine's states don't load in v0.83); Android 7 / 8 stay on v0.83.1; what's gone (the File
  menu, netplay, Dolphin's video backends and their settings).
- Android: the APK keeps Dolphin's app shell (Phase 4) but no longer offers Dolphin's emulation
  (`SkiesGame.standalone` always true, `EmulationActivity` paths removed or unreachable).
- Desktop: the packages stop building `moderngekko-run` only to overwrite it (`tools/build-release-*`), and the
  launcher's `MODERNGEKKO_STANDALONE_RUNNER` becomes the only mode.

**Phase 3: delete the Dolphin-engine code** (after the cut-over has shipped and settled). The inventory below.
Afterwards `Q1` (one settings table) gets simpler: two menus fewer.

**Phase 4 (optional): the launchers without Dolphin.** Desktop: move what the launcher takes from Dolphin's core
(disc reading for the import (DiscIO: RVZ, ISO, GCZ, WIA, CISO), card saves (`GCMemcard`), Dolphin state headers
for Import a save, the Dreamcast import and the texture-pack import) into a small library, or onto the copies the
standalone already has (`standalone/src/dreamcast/`, its disc reader). Android: a launcher of its own (big: the
Kotlin launcher, settings panel, imports, GPU driver installer are Dolphin's app). Worth it only if Dolphin's core
keeps costing build time or patch upkeep.

## Inventory (2026-11-19, `aurora-backend` at `b6560ae`)

### RecompCore patches (`patches/recompcore.series`, applied to `lib/ModernGekko/vendor/dolphin`)

| Goes with the engine (Phase 3) | Stays while the launchers use Dolphin (Phase 4) | Stays (the new engine, codegen) |
|---|---|---|
| `staticrecomp-speed`, `win32-exit`, `restore-boot-dol`, `frame-interpolation`, `vulkan-pipeline-cache-save`, `skies-branding`, `input`, `win32-fullscreen`, `controller-names`, `match-screen`, `hud`, `field-speed`, `japan`, `romhack`, `language`, `menu-close`, `overclock-timebase`, `battle-specials`, `pal60`, `dcmusic-fx`, `dcmusic-level`, `ship-hud`, `f1-state`, `interp-pacing`, `android-pad-follow`, `tz-fallback`, `gci-state-paths`, `skies-settings` (its `SkiesMenu` and live hooks; the `[Skies]` key list may move to the launchers), the Dolphin-side half of `aurora` (Aurora as a Dolphin video backend) | Android app shell: `android`, `skies-android-app`, `android-lifecycle`, `data-folder`, `state-folders`, `android-orientation`, `touch-setting`, `touch-style`, `cheats-file`, `standalone-app`, `standalone-driver`; launcher imports: `hd-textures`, `hd-textures-nested` (`TexturePackImport`), `dcmusic-streams`, `uncensor`, `skies-dcmusic-eu` (the Dreamcast import and conversion), the launcher parts of `review-d` and `macos` | `gxruntime-region-query`, `gxruntime-gather-pipe`, the GXRuntime / Aurora half of `aurora`, `aurora-vulkan-dir`, `png-limits`; `module-tables-prebuilt`, `module-fma` (the module build) |

Mixed patches (`skies-settings`, `aurora`, `review-d`, `macos`, `skies-dcmusic-eu`, `hd-textures`) are split when
their engine half goes: regenerate by hand (apply the earlier patches, the patch, then copy only the files that keep
changes; `tools/regen-recompcore-patch.sh` re-diffs every file a patch owns and absorbs later patches' changes).

### Other patches

- `moderngekko-local`, `-aurora`, `-launcher-signal`, `-macos`, `-renderer`, `-cheats`, `-ship-hud`,
  `-standalone-launcher`: the launcher parts stay; the runner parts (`src/runtime/dolphin_runtime.cpp`,
  `tools/moderngekko_run.cpp`, `tools/netplay_session.cpp`, the automation protocol's Dolphin side) go in Phase 3.
- `dolrecomp-*`: the codegen; stay. `highlytheoretical-dsp-shift-wrap`: the Dreamcast music on the new engine; stays.
  `patches/game/` (the game patch, ws35) and `patches/dawn/`: stay.

### CI and tools

- `validate.yml`: the codegen jobs and `standalone-host.yml` stay; the platform jobs stop building the Dolphin
  runner (`tools/build-release-*`); dev's own validate (the Dolphin build) stays only for a Phase 0 hotfix.
- Tools that drive the Dolphin runner (`tools/drive.py` and the automation directory, the Dolphin-runner test
  scripts in `HANDOFF-desktop.md` / `HANDOFF-android.md`) go with it; the standalone's (`tools/sa-*.sh`) stay.
- Docs: README / release README sections on Dolphin's settings (Dolphin video backends, the Dolphin engine's
  GPU driver path, netplay), `NOTES`-style Dolphin-build sections, the handoffs of Dolphin-only work, marked
  historical.

### What the new engine still takes from the Dolphin tree (stays)

`standalone/CMakeLists.txt` builds from `lib/ModernGekko/vendor/dolphin`: GXRuntime (it lives there), the
StaticRecomp ABI header, DolRecomp's decoder, `Externals/` (zstd, lz4, imgui, minizip-ng, picojson,
libadrenotools), `Data/Sys/GC/dsp_coef.bin`. The Dreamcast features are copies of the Dolphin build's modules
behind shimmed headers (`standalone/src/dreamcast/`, `src/dolphin_shim/`). So the `vendor/dolphin` submodule stays;
only its Dolphin-engine patches and its role as the game's runtime go.
