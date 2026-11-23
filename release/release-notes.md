A round of fixes on top of 0.91: smoother 60 fps, ship battles that respect Invincible, clean menu text, a Cutscene bars setting and an Android wall flicker gone. Coming from 0.83? Everything the new engine brings is listed below too, from [New since 0.83](#new-since-083).

> [!IMPORTANT]
> **You need your own Skies of Arcadia Legends disc** (GameCube: USA `GEAE8P`, European `GEAP8P` or Japanese `GEAJ8P`) as RVZ, ISO, GCM, GCZ, WIA or CISO.
> Nothing here contains game data: the launcher imports your disc and patches it.

> [!WARNING]
> **Coming from 0.83, back up your saves first** (Settings → Saves → Back up saves). Your saves carry over, but save states made by the new engine don't load in 0.83.

## Since 0.91

**New**

- **Cutscene bars setting** (Settings → Graphics, widescreen only): hide the black letterbox bars in cutscenes such as the intro that plays at the title screen ([#34](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/34)).

**Fixed**

- **Smoother 60 fps and Display refresh:** torch flames, sprites and small objects no longer jump ahead of everything else, and two similar objects side by side no longer get mixed up ([#18](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/18), [#24](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/24)).
- **The FPS counter** shows the frames that actually reach your screen; at Display refresh it used to read too high ([#24](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/24)).
- **Menu text** no longer garbles at 60 fps, and the clouds behind the menu no longer turn blocky after the ship's start menu ([#37](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/37)).
- **Ship battles with Invincible on:** your ship can no longer be sunk, and a ship already left at 0 HP is refilled ([#36](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/36)).
- **Ship battle HUD:** the banners sit slightly in from the screen's edges, so big damage numbers next to the enemy's banner are no longer cut off ([#25](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/25)).
- **Android:** the walls of the Valuan throne room no longer flicker ([#34](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/34)).
- **Grand Fortress gate:** the turrets' shadows no longer make the wall see-through; their edges are a little sharper now ([#23](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/23)).

## New since 0.83

- **A new engine**
  - The recompiled game runs on its own GameCube graphics, sound and disc layer, drawing through Direct3D 12, Vulkan, Direct3D 11, Metal or OpenGL ES. Everything from 0.83 is here: all three discs, Continue, card saves, save states, widescreen, 60 fps, HD textures, the Dreamcast features and every setting.
  - Faster everywhere we measured, for example 1.46x instead of 0.88x on one low-power PC core, and 1.77x instead of 1.38x on a Retroid Pocket 5 (1.00x is full speed). The [first test build's notes](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.90.0-standalone.6) have the full table.
- **Renderer setting** (Settings → Graphics → Renderer): the game picks a graphics API by itself; choose another if it crashes or draws things wrong.
- **Frame rate: Display refresh** ([#20](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/20))
  - A picture for every refresh of your screen (120 Hz, 144 Hz, 240 Hz and up), with or without V-Sync or VRR.
  - The game still runs at its own speed; the pictures between its frames come a frame later than at 60 fps.
- **Shaders are prepared when the game starts** ([#34](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/34))
  - Nothing comes in late the first time you see a scene.
  - A "Preparing shaders" bar shows the first time (about 20 seconds on an AYN Thor, 10 on a desktop graphics card), a second or so after that. Any button, key or tap skips it.
- **Mouse camera** (PC and Mac; Settings → Gameplay): click the picture and the mouse turns, tilts and zooms the camera. Off unless you turn it on.
- **Controller light** (Settings → Controls): a DualSense, DualShock 4 or Switch Pro controller glows sky blue in the field and red in battle.
- **Android: touch controls of the game's own** (Settings → Controls → Touch controls), with opacity and size.
- **Cheat codes** ([#5](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/5)): Action Replay and Gecko codes that write memory, from `Cheats/<disc ID>.ini` in your data folder, with a **Cheats** switch in the in-game menu.
- **Ship battle HUD setting** (Settings → Graphics: Spread out, Bar + grid centred or Centred) ([#25](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/25)): the Item list no longer runs off the screen in widescreen, and attack names sit at the edges.
- **Tap or click the in-game menu**, and **F1 saves a state** on desktop ([#19](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/19)).
- **Drop files on the launcher** (PC and Mac): a disc image, a save, a texture pack or a Dreamcast disc goes where its Select button would take it.
- **Laptops with two graphics chips use the faster one** ([#32](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/32)).
- **Crash reports and a log on every platform**
  - The log is `Logs/skies-standalone.log` in your data folder; if the game crashes, it says where, and on Windows a `crash-….dmp` file is saved beside it.
  - Nothing is sent anywhere: attach them to an issue.

## Fixed since 0.83

- **Battle damage numbers showed "0"** on Apple M4 Macs and some newer Android phones ([#16](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/16)).
- **60 fps: faces no longer jump between expressions** ([#18](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/18)), and **characters no longer get squashed** for a frame when they turn quickly ([#35](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/35)).
- **Dreamcast music is level with the GameCube music**, with more headroom ([#19](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/19)).
- **Widescreen: the effect behind Drachma's victory pose** turns with the camera shake without shearing, at the picture's width ([#22](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/22)).
- **Widescreen: Sailors' Island's bridge keeps the floating rocks and the tower in the sky** ([#21](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/21)).
- **See-through textures no longer look blocky** at higher resolutions ([#35](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/35)).
- **Sharper textures on widescreen screens**: Resolution "window" now fills the window instead of stretching a 4:3 picture ([#35](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/35)).
- **No more thin lines in the sky** with anti-aliasing (MSAA) ([#35](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/35)).
- **Security: a crafted save state can no longer make the memory card write a file outside its folder.** Damaged or crafted save states, disc files and texture-pack images are refused instead of crashing.
- **Stick dead zone and rumble set in the launcher** now apply in the game.

## Not in this release

- **Android 7 and 8:** the new engine needs Android 9 or later; on 7 and 8 the app keeps using the Dolphin engine.
- **Dolphin's video backend settings** (the Renderer setting replaces them) and **netplay**.

## Installing and updating

Your data folder (moved or portable too), settings, memory card saves and save states carry over, from 0.91, 0.83 and the test builds; 0.83's save states are converted when they load. Your disc is patched again the first time you start, coming from 0.91, 0.83 or a test build.

- **Windows / Linux / macOS:** unpack the new version and run it. macOS: allow it in System Settings → Privacy & Security the first time (not notarized).
- **Android:** install the APK over 0.91, 0.83 or a test build as an update.
- **Going back to 0.83:** load a memory card save (0.83's Continue may pick the new engine's newest save state and refuse it). On Android, going back means uninstalling first, which deletes the app's data unless you moved the data folder: back up your saves.

## Thanks

To everyone who tried 0.91 and the test builds and reported what they found, especially [Cawl87](https://github.com/Cawl87), [GLEclipseGaming](https://github.com/GLEclipseGaming), [doc-haz](https://github.com/doc-haz), [ghettowario](https://github.com/ghettowario), [workmax67-create](https://github.com/workmax67-create) and [chrisbrasington](https://github.com/chrisbrasington). Several features come from [Dusklight](https://github.com/TwilitRealm/dusklight), the Twilight Princess port on the same [Aurora](https://github.com/encounter/aurora) renderer: thank you to both projects.

---

Controls, settings and the rest are in the [README](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp#readme). Please [report problems](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/new/choose) with your platform, device and the log (`Logs/skies-standalone.log` in your data folder).
