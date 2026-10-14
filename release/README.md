# Skies of Arcadia Legends: Recompiled

A native port of **Skies of Arcadia Legends** (GameCube) for Windows, Linux and Android, made by static
recompilation. It doesn't contain the game: you supply your own copy of the disc, and the game's
data is read from it.

This is a **beta** (v0.9.0). Please report what you run into.

## What you need

- Your own dump of the **USA** GameCube disc (game ID `GEAE8P`). Other regions aren't supported yet;
  the import checks the game and refuses any other version.
- As RVZ, ISO, GCM, GCZ, WIA or CISO.
- **Windows:** 64-bit Windows 10 or 11.
- **Linux:** 64-bit (x86_64), with a Vulkan or OpenGL driver.
- **Android:** a 64-bit (arm64) device with Android 7 or newer. Tested on a Retroid Pocket 5
  (Snapdragon 865), which runs it at full speed at 60 fps. Slower devices may not.

## Getting started

**Windows:** unzip, run `Skies of Arcadia Legends.exe`, choose **Select Disc** and pick your disc image. The
launcher imports it (a few seconds), patches it and starts the game. After that, **Start Game** or
**Continue** start it directly.

**Linux:** extract, then run `./SkiesOfArcadiaLegends` from inside the folder; the rest is as on
Windows.

**Android:** install the APK, open **Skies of Arcadia Legends** and pick your disc image when asked.

Your saves and settings are kept in `%LOCALAPPDATA%\Skies of Arcadia Legends` (Windows),
`~/.local/share/Skies of Arcadia Legends` (Linux) or in the app's own storage (Android). Uninstalling the Android app deletes them, so copy them out first if you want to keep them.

### Bringing your saves

**Settings → Saves → Import a save** takes a save you already have and puts it where the game finds
it. It asks first, then opens a file picker. It takes:

- **A memory card save** from Dolphin or a real GameCube: `.gci` (Dolphin's GCI folders, e.g.
  `GC/USA/Card A`), `.gcs` or `.sav` (memory card tools such as GameShark or MaxDrive).
- **A whole memory card** from Dolphin (`MemoryCardA.USA.raw`) or a card dump (`.gcp`): its Skies of
  Arcadia Legends saves are taken, nothing else.
- **A save state** from this port (`.sav` from the `StateSaves` folder), e.g. from your other device.
  Save states only work in the same version of the port; memory card saves work in every version.

A memory card save replaces the file with the same number here; the one it replaces is kept beside
it as a `.pre-import.bak` file (`.pre-import.2.bak` and so on if you import over it again). A save state is added to the ones you have. **Continue** then starts
from what you imported. Saves from the European or Japanese version of the game can't be used: this
port plays the USA version.

## What it adds

- **Widescreen** 16:9 with the HUD at the screen edges, or the original 4:3 (identical to the
  original game).
- **60 fps** by frame interpolation, or the original 30.
- Internal resolution and anti-aliasing.
- **Camera control:** hold Z and use the C-stick to tilt, and R / L to zoom out / in (towns, dungeons
  and the world map; on the world map Z + C-stick left / right also turns around the ship).
- **Battle speed** up to 4x: tap Z during a battle.
- **Options:** random encounter rate (off to 4x), EXP / gold / magic EXP (off to 4x), and Invincible
  (press L3 or R3 to toggle).
- **Save states:** five slots, plus an auto-save every 10 minutes on the field (the newest three are
  kept). Your memory card saves are never touched by a state.
- **Continue** in the launcher starts from your newest save state or memory card save, whichever is
  newer. A memory card save loads straight into the game, without the intro or title screen.
- **Import a save** from Dolphin, a GameCube memory card or another device (see *Bringing your
  saves*).

## Controls

The game's own controls are unchanged. On top of them:

| Input | What it does |
|---|---|
| Hold Z, press Start | the settings menu (L / R switch tabs; B or Start close it). Its Saves tab saves and loads states |
| Hold Z + C-stick up / down | tilt the camera |
| Hold Z + R / L | zoom out / in |
| Tap Z in battle | battle speed: Normal, 2x, 3x, 4x |
| L3 or R3 | Invincible on / off |

Every setting is also in the launcher's **Settings** window, with a short description of each.

## Known issues

- **Changing the battle speed while a round plays out can freeze the battle.** Change it while you
  choose commands instead.
- On Android, battles top out at about 2.5x speed on a Snapdragon 865; 3x and 4x settle at 2x.
- An auto-save can cause a brief hitch on Android.

## Reporting a problem

Please say what happened and where in the game. On Android, open **Settings → About → Copy device
info** and paste it into the report: it names the phone, its chip and the graphics driver.

**Android GPU drivers:** on Adreno devices, **Settings → Graphics → GPU driver** can load a driver
package (for example Mesa Turnip) when the device's own driver crashes or draws things wrong. This is
unsupported; if the game doesn't close normally with a new driver, the app goes back to the device's
own driver by itself, and after the first normal session it asks whether to keep the new one.
On any Android device, **Settings → Graphics → Renderer** can switch from Vulkan to OpenGL ES if the
game crashes or draws things wrong with Vulkan.

**Windows / Linux graphics:** when the graphics driver can't start Vulkan ("Failed to create Vulkan
device" or a similar message), the game switches to OpenGL by itself and keeps it (**Settings →
Display → Graphics backend**). If neither starts, install the latest driver for your graphics card.

**60 fps on slower devices:** if 60 fps can't keep full speed for a while, the settings menu opens
once by itself on Frame rate, so you can switch to 30 fps. Nothing changes unless you choose it.

## Credits

- Launcher background art: **I_LIKE_SONIC**
- App icon art: **Reckoner**
- Built on [ModernGekko](https://github.com/ExpansionPak/ModernGekko) and
  [DolRecomp](https://github.com/ExpansionPak/DolRecomp), which build on the
  [Dolphin](https://dolphin-emu.org/) emulator's hardware code.

Skies of Arcadia Legends is (c) SEGA. This project isn't affiliated with or endorsed by SEGA.

## License

GPL-3.0 (see `COPYING`). Third-party components and their licences are listed in `THIRD-PARTY.md`.
