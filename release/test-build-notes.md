The fifth test build of the new engine: sharper pictures on widescreen screens, no more pieces loading in late on phones, smoother 60 fps, and faster European and Japanese games.

> [!WARNING]
> **This is a test build.** [v0.83.0-beta](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.83.0-beta) stays the recommended release. **Back up your saves first** (Settings → Saves → Back up saves): save states made by the new engine don't load in 0.83.

> [!IMPORTANT]
> **You need your own Skies of Arcadia Legends disc** (GameCube: USA `GEAE8P`, European `GEAP8P` or Japanese `GEAJ8P`) as RVZ, ISO, GCM, GCZ, WIA or CISO.
> Nothing here contains game data: the launcher imports your disc and patches it.

New to the new engine? The [first test build's notes](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.90.0-standalone.6) cover what changed from 0.83 and how much faster it runs.

## Fixed

- **Shaders are prepared when the game starts** ([#34](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/34))
  - The first time the game saw a scene, it drew without the objects whose shaders were still compiling, so on phones and handhelds pieces of a scene (the Valuan gate in the opening) came in late. The game now prepares the shaders it knows it will need before it starts, with a "Preparing shaders" bar: about 10-20 seconds the first time on an AYN Thor, a second or so after that. Any button, key or tap skips it.
- **Soft textures on widescreen screens** ([#35](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/35))
  - With Resolution "window", the picture was drawn at 4:3 inside the window and stretched sideways. It now covers the window, as Dolphin's Auto does: on a 2560×1440 screen textures are as sharp as in the Dolphin build.
- **Lines in the sky with anti-aliasing (MSAA)** ([#35](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/35))
  - Thin dotted lines along the edges of cloud layers (Tenkou Island's sky, for one) are gone.
- **60 fps: characters squashed when turning** ([#35](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/35))
  - A character or enemy turning quickly could flatten for one in-between picture, which looked like warping or flickering. They now stay whole.

## Faster

- **After Fina's flashback** ([#35](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/35))
  - The scenes after the flashback slowed down on some PCs (about 10 ms of work every frame); that work is gone.
- **The European and Japanese games**
  - They now get the same profile-guided optimisation as the USA game: 14-21% less CPU in our tests.

Thanks to [Cawl87](https://github.com/Cawl87) and [GLEclipseGaming](https://github.com/GLEclipseGaming) for the detailed reports and videos.

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
