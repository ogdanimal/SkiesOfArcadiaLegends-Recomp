The second test build of the new engine: fixes from the first one's reports, a tappable in-game menu, and a game that recovers by itself from a graphics driver that crashes at start.

> [!WARNING]
> **This is a test build.** [v0.83.0-beta](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.83.0-beta) stays the recommended release. **Back up your saves first** (Settings → Saves → Back up saves): save states made by the new engine don't load in 0.83.

> [!IMPORTANT]
> **You need your own Skies of Arcadia Legends disc** (GameCube: USA `GEAE8P`, European `GEAP8P` or Japanese `GEAJ8P`) as RVZ, ISO, GCM, GCZ, WIA or CISO.
> Nothing here contains game data: the launcher imports your disc and patches it.

New to the new engine? The [first test build's notes](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.90.0-standalone.6) cover what changed from 0.83 and how much faster it runs.

## New

- **Tap or click the in-game menu**
  - Tabs, rows and values take a tap (Android) or a click (desktop): the left or right of a value changes it, a tap outside the window closes the menu. The controller and keyboard work as before.
- **The game recovers from a graphics driver that crashes at start**
  - If the game closes while its graphics start, the next start switches **Settings → Graphics → Renderer** to the next one by itself (Android: Vulkan → OpenGL ES; Windows: Direct3D 12 → Vulkan → Direct3D 11; Linux: Vulkan → OpenGL ES) and says so in the log. It may be the cause of the crash at Start Game on an AYN Thor ([#34](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/34)).
- **The log is in your data folder on every platform:** `Logs/skies-standalone.log`, the file to attach to reports (before, only Windows wrote it). It writes your home folder as `~`.

## Fixed

- **Battle damage numbers all showed "0"** on Apple M4 Macs (likely other recent Apple chips too) and some newer Android phones, such as the Exynos 1580 ([#16](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/16))
  - These processors have a floating-point mode the engine switched on for the game's own maths, and in it the game's text-to-number routine read every damage number as 0.
  - Thanks to [OrbitingSpartan](https://github.com/OrbitingSpartan) and [sdsarmiento](https://github.com/sdsarmiento) for reporting it.
- **Dreamcast music: more headroom.** The game's own sound carries a constant offset, as on the console, whose audio output filters it out; it's filtered out here too now, so the Dreamcast music played on top of it has its full range. Not confirmed to be the music breaking up in some scenes ([#26](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/26)): if you still hear it, the log now has a line about it, please attach it.
  - Thanks to [Cawl87](https://github.com/Cawl87) for the report and the clips.
- **Android: settings changed in the game's menu were undone** when you then changed a setting in the app's own Settings.
- **The READMEs** describe the new engine, and the downloads include the licences of its components.

## Not in this build

- **Android 7 and 8:** the new engine needs Android 9 or later; on 7 and 8 the app keeps using the Dolphin engine.
- **Dolphin's video backend settings** (the Renderer setting replaces them) and **netplay**.

## Installing and updating

Your data folder (moved or portable too), settings, memory card saves and save states carry over, from the first test build and from 0.83 (0.83's save states are converted when they load).

- **Windows / Linux / macOS:** unpack the new version and run it. macOS: allow it in System Settings → Privacy & Security the first time (not notarized).
- **Android:** install the APK over the first test build or 0.83 as an update.
- **Going back to 0.83:** load a memory card save (0.83's Continue may pick the new engine's newest save state and refuse it). On Android, going back means uninstalling first, which deletes the app's data unless you moved the data folder: back up your saves.

---

Controls, settings and the rest are in the [README](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp#readme). Please [report problems](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/new/choose) with your platform, device and the log.
