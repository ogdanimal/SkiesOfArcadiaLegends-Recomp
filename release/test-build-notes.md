## How the new engine differs from v0.83

**The same as v0.83:** the USA, European and Japanese discs, Continue, memory card saves, save states (older
ones are converted when they load), widescreen and Match screen, the HUD options, 60 fps, HD texture packs, the
Dreamcast music and Uncensored, Language, field and battle speed, the gameplay options, the camera, controls and
button mapping, and the graphics options (resolution, anti-aliasing, texture filtering, anisotropic filtering,
scaling).

**New or better:**
- **Faster and smoother**, most of all on weaker hardware (see *Expected performance*).
- **Android touch controls of its own** (v0.83 used Dolphin's): Settings → Controls → Touch controls (Auto,
  Always, Off), with opacity and size.
- **Renderer** setting: the engine tries the graphics APIs in order (Windows: Direct3D 12, Vulkan, Direct3D 11;
  Linux and Android: Vulkan, OpenGL ES; macOS: Metal) and you can pick the one tried first.
- **The log names your graphics card and driver**, which makes reports easier to act on.

**Different or not (yet) there:**
- Dolphin's video backend settings are gone; the Renderer setting takes their place.
- **Cheat codes:** Action Replay and Gecko codes that write memory work; codes that patch the game's code are
  skipped (the game's code runs natively, not emulated).
- **Android 9 or later.** On Android 7 and 8 the app keeps using the Dolphin engine.
- **Save states made by this build don't load in v0.83.** Memory card saves work in both.
- No netplay.

## Expected performance

Measured with the same game files on both engines, at the same scenes (the opening dungeon's
save point, a battle). The speeds are uncapped: how fast the game *could* run, where 1.00x is full speed. In
normal play the game is held at full speed, so the extra headroom shows up as steadier frame times, lower CPU
and battery use, and room for a higher resolution or 60 fps on hardware that couldn't manage it before.

| Hardware | New engine | v0.83 (Dolphin engine) |
|---|---|---|
| Desktop CPU (Intel i7-12700K), uncapped | **2.91x** | 2.33x |
| CPU used at full speed | **0.45 cores** | 0.60 cores |
| One low-power core (an i7-12700K E-core) | **1.46x** | 0.88x (below full speed) |
| Integrated GPU (Intel UHD 770) at 6x resolution | **1.30x** (full speed) | 0.77x (below full speed) |
| Retroid Pocket 5 (Snapdragon 865), save point | **1.77x** | 1.38x |
| Retroid Pocket 5, battle | **2.15x** | 1.63x |
| Retroid Pocket 5 at 60 fps, save point / battle | **1.70x / 2.05x** | 1.41x / 1.69x |
| Retroid Pocket 5 at 60 fps, the slowest 1% of frames | **17.7 ms** (a steady 60) | 24.5-25.9 ms |
| Retroid Pocket 5 at 6x resolution | **1.24x** (full speed) | 0.87x (below full speed) |

Also on the Pocket 5: 0.4-0.9 W less power at full speed, and with an HD texture pack about half the memory.
A fast PC that already runs v0.83 at full speed will mostly notice steadier frame pacing; laptops with
integrated graphics, low-power CPUs and Android handhelds gain the most. Reports with your device
and the log are welcome.

**Recent changes** (this is the first public test build of the new engine; the list is what changed in the last
rounds of internal testing):

- **Android: touch controls** for phones and tablets without a controller: stick, C-stick, D-pad,
  A / B / X / Y, L / R, Z, Start and a menu button. They step aside when a controller is used and come
  back when you touch the screen. Settings → Controls → Touch controls (also in the in-game menu): Auto,
  Always or Off, with their opacity (25-100%) and size (75-110%).
- **Ship battle HUD** (#25): the Item list no longer runs off the bottom of the screen in widescreen, and a
  new setting (Settings → Graphics → Ship battle HUD) keeps the game's own centred layout if you prefer it.
- **60 fps** (#18): parts of a character that change texture between frames (a face changing expression)
  now move with the rest of the model instead of jumping.
- **Dreamcast music** (#19): a little quieter, level with the GameCube music now that it has its reverb. One
  of the sound chip's reverb calculations now wraps the way the hardware does; it was looked at for the
  music breaking up (#26) but isn't confirmed as its cause: please report if it still happens.
- **Battle screen effects in widescreen** (#22): the full-screen effect behind some poses (Drachma's victory
  pose) now turns with the camera shake without shearing. Not yet seen in game: tell us how it looks.
- **Cheat codes** (#5): Action Replay / Gecko codes that write memory, from `Cheats/<disc ID>.ini` in the data
  folder (Dolphin's format: a Dolphin cheat file works as it is; on Android, Settings → Gameplay → Cheat file), with a **Cheats** switch in the in-game
  menu's Gameplay tab. See README.md, *Cheat codes*.
- **The Menu and Invincible hotkeys can be rebound** (Controls → Button mapping, last two rows), as in the
  Dolphin build.
- **F1 saves a state** (desktop, #19) to the slot chosen in the in-game menu (slot 1 by default), unless F1 is
  bound to a game button.
- **Stick dead zone and Rumble** set in the launcher now apply in the game (they only worked from the
  in-game menu before).
- **Renderer** (desktop): Settings → Graphics → Renderer picks the graphics API (Auto, Direct3D 12 / Vulkan /
  Direct3D 11 on Windows, Vulkan / OpenGL ES on Linux) if the automatic one crashes or draws things wrong.
- **Android: custom GPU drivers** (Mesa Turnip on Adreno, Settings → Graphics → GPU driver) now work with the
  new engine, with the same safeguard: if the game doesn't start with it, the app goes back to the device's
  own driver.
- **Anti-aliasing 2x and 8x** (Settings → Graphics → Anti-aliasing) beside Off and 4x, as in the Dolphin build
  (8x where the graphics chip has it).
- **Laptops with two graphics chips** (#32): the game asks for the faster one on every graphics API, and the
  log (`Logs/skies-standalone.log`) names the graphics card, API and driver it used: include it in reports.
- **Security fixes:** a save state (either engine's) can no longer make the memory card write a file outside
  its folder; damaged or crafted save states, disc file tables and texture-pack PNGs are refused instead of
  crashing or filling memory; on Android, test files in the app's folder are ignored by this build and other
  apps can't close the game. The log writes your home folder as `~`, so it's safe to attach to a report.
- **Fixes:** a keyboard key bound to Invincible now works; cheat codes that patch the game's code (which runs
  natively here) are listed as skipped in the log, and Action Replay float writes work; touch controls show on
  phones that report a button-only "gamepad" and let go when a dialog opens; the game clock is right around a
  daylight-saving change.
- The European and Japanese discs get the same game update as the USA one; your discs are patched again
  the first time you start this build.
