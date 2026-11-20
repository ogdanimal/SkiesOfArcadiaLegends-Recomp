The fourth test build of the new engine: a much faster character menu on Windows, less CPU use in battle, and save states that no longer pause the game.

> [!WARNING]
> **This is a test build.** [v0.83.0-beta](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.83.0-beta) stays the recommended release. **Back up your saves first** (Settings → Saves → Back up saves): save states made by the new engine don't load in 0.83.

> [!IMPORTANT]
> **You need your own Skies of Arcadia Legends disc** (GameCube: USA `GEAE8P`, European `GEAP8P` or Japanese `GEAJ8P`) as RVZ, ISO, GCM, GCZ, WIA or CISO.
> Nothing here contains game data: the launcher imports your disc and patches it.

New to the new engine? The [first test build's notes](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.90.0-standalone.6) cover what changed from 0.83 and how much faster it runs.

## Faster

- **The character menu and other text-heavy screens** ([#35](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/35))
  - The menu's text made the new engine create dozens of new textures every frame. On Direct3D (Windows) each one is slow, so the menu ran far below full speed on some PCs. It now reuses them: on an Intel UHD 770 the menu runs about 2.5× faster, and Direct3D 12 is no longer slower than Direct3D 11.
- **Less CPU in battle and elsewhere**
  - The game's own wait for the next frame no longer burns processor time: up to a third less CPU in battles on PC (about 14% on a Retroid Pocket 5), a little less elsewhere. On handhelds that means less battery and heat at the same speed.
- **Save states no longer pause the game**
  - A save state (and the auto-save) used to hold the game for a moment while it was compressed and written; that now happens in the background.
- Thanks to [Cawl87](https://github.com/Cawl87) for the detailed reports.

## Not in this build

- **Android 7 and 8:** the new engine needs Android 9 or later; on 7 and 8 the app keeps using the Dolphin engine.
- **Dolphin's video backend settings** (the Renderer setting replaces them) and **netplay**.

## Installing and updating

Your data folder (moved or portable too), settings, memory card saves and save states carry over, from the earlier test builds and from 0.83 (0.83's save states are converted when they load).

- **Windows / Linux / macOS:** unpack the new version and run it. macOS: allow it in System Settings → Privacy & Security the first time (not notarized).
- **Android:** install the APK over an earlier test build or 0.83 as an update.
- **Going back to 0.83:** load a memory card save (0.83's Continue may pick the new engine's newest save state and refuse it). On Android, going back means uninstalling first, which deletes the app's data unless you moved the data folder: back up your saves.

---

Controls, settings and the rest are in the [README](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp#readme). Please [report problems](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/new/choose) with your platform, device and the log (`Logs/skies-standalone.log` in your data folder).
