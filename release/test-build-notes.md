The first public test build of the new engine: the game no longer runs inside Dolphin. Faster and smoother, most of all on weaker PCs, laptops and Android handhelds.

> [!WARNING]
> **This is a test build.** [v0.83.0-beta](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/releases/tag/v0.83.0-beta) stays the recommended release. **Back up your saves first** (Settings → Saves → Back up saves): save states made by this build don't load in 0.83.

> [!IMPORTANT]
> **You need your own Skies of Arcadia Legends disc** (GameCube: USA `GEAE8P`, European `GEAP8P` or Japanese `GEAJ8P`) as RVZ, ISO, GCM, GCZ, WIA or CISO.
> Nothing here contains game data: the launcher imports your disc and patches it.

## New

- **A new engine**
  - The recompiled game runs on its own GameCube graphics, sound and disc layer, drawing through Direct3D 12, Vulkan, Direct3D 11, Metal or OpenGL ES, instead of inside Dolphin. Everything you use in 0.83 is here: all three discs, Continue, card saves, save states, widescreen, 60 fps, HD textures, the Dreamcast features and every setting.
- **Renderer setting** (Settings → Graphics → Renderer)
  - The game tries the graphics APIs in order and falls back by itself; pick the one tried first if the automatic choice crashes or draws things wrong.
- **Android: touch controls of the game's own** (Settings → Controls → Touch controls: Auto, Always or Off), with opacity and size.
- **Cheat codes** ([#5](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/5))
  - Action Replay and Gecko codes that write memory, from `Cheats/<disc ID>.ini` in your data folder (a Dolphin cheat file works as it is), with a **Cheats** switch in the in-game menu. Codes that patch the game's code are skipped: the code runs natively here.
  - Thanks to [doc-haz](https://github.com/doc-haz) for asking for it.
- **Ship battle HUD setting** (Settings → Graphics) ([#25](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/25))
  - The Item list no longer runs off the bottom of the screen in widescreen; **Centred** keeps the game's own layout.
  - Thanks to [Cawl87](https://github.com/Cawl87) for reporting it.
- **F1 saves a state on desktop** to the slot chosen in the in-game menu, unless F1 is bound to a game button ([#19](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/19)).
- **Laptops with two graphics chips use the faster one** ([#32](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/32))
  - The log (`Logs/skies-standalone.log` in your data folder) names the graphics card and driver: please attach it to reports. It writes your home folder as `~`.
  - Thanks to [workmax67-create](https://github.com/workmax67-create) for reporting it.

## Expected performance

The same game files on both engines, at the same scenes. Uncapped speed shows how fast the game *could* run (1.00x is full speed); in normal play the game is held at full speed, so the headroom shows up as steadier frames, lower CPU and battery use, and higher settings on hardware that couldn't manage them before.

| Hardware | New engine | 0.83 |
|---|---|---|
| Desktop CPU (Intel i7-12700K) | **2.91x** | 2.33x |
| CPU used at full speed | **0.45 cores** | 0.60 cores |
| One low-power core (an i7-12700K E-core) | **1.46x** | 0.88x |
| Integrated graphics (Intel UHD 770) at 6x resolution | **1.30x** | 0.77x |
| Retroid Pocket 5 (Snapdragon 865), field / battle | **1.77x / 2.15x** | 1.38x / 1.63x |
| Retroid Pocket 5 at 60 fps, field / battle | **1.70x / 2.05x** | 1.41x / 1.69x |
| Retroid Pocket 5 at 60 fps, slowest 1% of frames | **17.7 ms** | 24.5-25.9 ms |
| Retroid Pocket 5 at 6x resolution | **1.24x** | 0.87x |

On the Pocket 5 it also uses 0.4-0.9 W less at full speed, and about half the memory with an HD texture pack. A fast PC that already runs 0.83 at full speed mostly gains steadier frame pacing.

## Fixed

- **60 fps: a face changing expression no longer jumps** while the rest of the model moves ([#18](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/18))
  - Thanks to [ghettowario](https://github.com/ghettowario) for reporting it.
- **Dreamcast music is level with the GameCube music** now that it has its reverb ([#19](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/19))
  - One of the sound chip's reverb steps now wraps as the hardware does. It may help with music breaking up ([#26](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/26)), which isn't confirmed: please tell us if you still hear it.
  - Thanks to [Cawl87](https://github.com/Cawl87) for reporting both.
- **Widescreen: the effect behind Drachma's victory pose turns with the camera shake** without shearing ([#22](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/22))
  - Not yet seen in game: tell us how it looks. Thanks to [Cawl87](https://github.com/Cawl87) for reporting it.
- **Security: a crafted save state can no longer make the memory card write a file outside its folder.** Damaged or crafted save states, disc files and texture-pack images are refused instead of crashing or filling memory.
- **Stick dead zone and rumble set in the launcher apply in the game** (they only worked from the in-game menu).

## Not in this build

- **Android 7 and 8:** the new engine needs Android 9 or later; on 7 and 8 the app keeps using the Dolphin engine.
- **Dolphin's video backend settings** (the Renderer setting replaces them) and **netplay**.

## Installing and updating from 0.83

Your data folder (moved or portable too), settings, memory card saves and save states carry over; 0.83's save states are converted when they load. The European and Japanese discs are patched again the first time you start this build.

- **Windows / Linux / macOS:** unpack the new version and run it. macOS: allow it in System Settings → Privacy & Security the first time (not notarized).
- **Android:** install the APK over 0.83 as an update.
- **Going back to 0.83:** load a memory card save (0.83's Continue may pick this build's newest save state and refuse it). On Android, going back means uninstalling first, which deletes the app's data unless you moved the data folder: back up your saves.

---

Controls, settings and the rest are in the [README](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp#readme). Please [report problems](https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues/new/choose) with your platform, device and the log.
