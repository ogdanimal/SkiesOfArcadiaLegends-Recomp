The fifth test build of the new engine: a frame rate that follows your screen (120 Hz, 144 Hz, 240 Hz), sharper pictures on widescreen screens, no more pieces loading in late on phones, smoother 60 fps, and faster European and Japanese games.

> [!WARNING]
> **This is a test build.** [v0.83.0-beta](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.83.0-beta) stays the recommended release. **Back up your saves first** (Settings → Saves → Back up saves): save states made by the new engine don't load in 0.83.

> [!IMPORTANT]
> **You need your own Skies of Arcadia Legends disc** (GameCube: USA `GEAE8P`, European `GEAP8P` or Japanese `GEAJ8P`) as RVZ, ISO, GCM, GCZ, WIA or CISO.
> Nothing here contains game data: the launcher imports your disc and patches it.

New to the new engine? The [first test build's notes](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.90.0-standalone.6) cover what changed from 0.83 and how much faster it runs.

## New

- **Frame rate: Display refresh** ([#20](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/20))
  - A third Frame rate choice beside 30 and 60 fps: an in-between picture for every refresh of your screen (120 Hz, 144 Hz, 240 Hz and up).
  - Works with or without V-Sync or VRR.
  - The game still runs at its own speed; the pictures between its frames come a frame later than at 60 fps.
  - When a scene is too heavy for every refresh, it draws on every second or third one instead.
  - Example, an RTX 5080 at 240 Hz: 240 pictures a second in the field and on the world map, 120 in the Antonio battle.
- **Mouse camera** (PC and Mac; Settings → Gameplay)
  - Click the picture and the mouse turns, tilts and zooms the camera in towns, dungeons and on the world map.
  - Esc or the menu lets the mouse go.
  - Off unless you turn it on.
- **Controller light** (Settings → Controls)
  - A DualSense, DualShock 4 or Switch Pro controller glows sky blue in the field and red in battle.
  - Off leaves the light alone.
- **Crash reports in the log**
  - If the game crashes, the log (`Logs/skies-standalone.log` in your data folder) now says where.
  - On Windows a `crash-….dmp` file is saved beside it: please attach both to an issue.
  - Nothing is sent anywhere.
  - The logs of your last three runs are kept too.
- **Drop files on the launcher** (PC and Mac): a disc image, a save, a texture pack or a Dreamcast disc dropped on the window goes where its Select button would take it.
- Settings → Graphics → Resolution now says the size the game draws at.

These ideas come from [Dusklight](https://github.com/TwilitRealm/dusklight), the Twilight Princess port on the same Aurora renderer: thank you to its developers, and to [Aurora](https://github.com/encounter/aurora)'s, whose upstream fixes this build also picks up (lighter shaders).

## Fixed

- **Shaders are prepared when the game starts** ([#34](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/34))
  - Before: the first time the game saw a scene, it drew without the objects whose shaders were still compiling.
  - On phones and handhelds, pieces of a scene came in late (the Valuan gate in the opening).
  - The game now prepares the shaders it knows it will need before it starts, with a "Preparing shaders" bar.
  - The first time takes about 20 seconds on an AYN Thor (10 on a desktop graphics card); after that, a second or so.
  - Any button, key or tap skips it.
- **Soft textures on widescreen screens** ([#35](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/35))
  - Before: with Resolution "window", the picture was drawn at 4:3 inside the window and stretched sideways.
  - It now covers the window, as Dolphin's Auto does.
  - On a 2560×1440 screen, textures are as sharp as in the Dolphin build.
- **Lines in the sky with anti-aliasing (MSAA)** ([#35](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/35))
  - Thin dotted lines along the edges of cloud layers (Tenkou Island's sky, for one) are gone.
- **60 fps: characters squashed when turning** ([#35](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/35))
  - A character or enemy turning quickly could flatten for one in-between picture, which looked like warping or flickering.
  - They now stay whole.

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
