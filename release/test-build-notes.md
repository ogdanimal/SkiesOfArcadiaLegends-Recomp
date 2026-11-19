**New in this test build** (since v0.90.0-standalone.4):

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
- **Laptops with two graphics chips** (#32): the game asks for the faster one on every graphics API, and the
  log (`Logs/skies-standalone.log`) names the graphics card, API and driver it used: include it in reports.
- The European and Japanese discs get the same game update as the USA one; your discs are patched again
  the first time you start this build.
