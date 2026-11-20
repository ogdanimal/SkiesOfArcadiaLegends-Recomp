The third test build of the new engine: textures back to their smooth look, the boss battle swirl fixed, and a frame counter that counts 60 fps.

> [!WARNING]
> **This is a test build.** [v0.83.0-beta](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.83.0-beta) stays the recommended release. **Back up your saves first** (Settings → Saves → Back up saves): save states made by the new engine don't load in 0.83.

> [!IMPORTANT]
> **You need your own Skies of Arcadia Legends disc** (GameCube: USA `GEAE8P`, European `GEAP8P` or Japanese `GEAJ8P`) as RVZ, ISO, GCM, GCZ, WIA or CISO.
> Nothing here contains game data: the launcher imports your disc and patches it.

New to the new engine? The [first test build's notes](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.90.0-standalone.6) cover what changed from 0.83 and how much faster it runs, and the [second's](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.90.0-standalone.7) what came after.

## Fixed

- **Blocky, low-detail textures** ([#35](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/35))
  - The new engine read one of the game's texture-filter settings backwards, so many textures (hair, railings, the save points) were drawn as hard pixels instead of smoothly, most visibly at higher resolutions.
- **The opening swirl of boss battles showed the boss on black** (seen at Antonio, [#35](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/35))
  - The screen picture the swirl stretches came out see-through, so most of the boss and the room vanished.
- **The frame counter showed 30 with 60 fps on** ([#35](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/35)): it now counts the in-between frames too.
- Thanks to [Cawl87](https://github.com/Cawl87) for the careful side-by-side comparisons.

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
