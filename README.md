# Skies of Arcadia Legends: Recompiled

A native port of **Skies of Arcadia Legends** (GameCube) to **Windows, Linux and Android**, made by
static recompilation: the game's PowerPC code is translated ahead of time into native code, and the
GameCube's graphics, sound and input come from [Dolphin](https://dolphin-emu.org/)'s hardware code.
It adds widescreen, 60 fps, camera control, save states and quality-of-life options.

**This is a beta (v0.9.0).** Please [report](../../issues/new/choose) what you run into.

No build contains the game. You need your own copy of the disc.

## Download

Get a build from the **[Releases](../../releases)** page:

| Platform | File |
|---|---|
| Windows | `SkiesOfArcadiaLegends-<version>-windows.zip` |
| Linux | `SkiesOfArcadiaLegends-<version>-linux.tar.gz` |
| Android | `SkiesOfArcadiaLegends-<version>-android.apk` |

Versions ending in `-beta` (or `-rc1`, `-rc2`, ...) are published as prereleases.

## Getting Started

1. Install it:
   - **Windows:** unzip, and run `Skies of Arcadia Legends.exe` from inside the folder.
   - **Linux:** extract, and run `./SkiesOfArcadiaLegends` from inside the folder.
   - **Android:** install the APK and open **Skies of Arcadia Legends**.
2. Choose **Select Disc** and pick your disc image. The launcher checks it, imports it (a few seconds)
   and patches it for this port; you only do this once.
3. Press **Start Game**. Later, **Continue** starts from your newest save.

**The disc is the USA GameCube release** (game ID `GEAE8P`), as RVZ, ISO, GCM, GCZ, WIA or CISO.
Other regions aren't supported yet; the import refuses them.

**Requirements:**

| | |
|---|---|
| **Windows** | 64-bit Windows 10 or 11 |
| **Linux** | 64-bit (x86_64), with a Vulkan or OpenGL driver |
| **Android** | 7.0+, a 64-bit (`arm64-v8a`) device |

Tested on a **Retroid Pocket 5** (Android 13, Snapdragon 865), which runs it at full speed at 60 fps,
and on Windows. Slower Android devices may not keep 60 fps (see [Slower devices](#slower-devices)).

## Something's Wrong? Quick Fixes

| Problem | Try this |
|---|---|
| **Windows / Linux:** "Failed to create Vulkan device" or a similar message at start | Your graphics driver can't run Vulkan. The game switches to OpenGL by itself and keeps it (**Settings → Display → Graphics backend**). If neither starts, install the latest driver for your graphics card. |
| **Android:** the game crashes or draws things wrong | **Settings → Graphics → Renderer** switches between Vulkan and OpenGL ES. On Adreno devices, **Settings → Graphics → GPU driver** can load another driver (see [GPU driver](#gpu-driver-android)). |
| The import refuses your disc | It must be the USA disc (`GEAE8P`). European and Japanese discs aren't supported yet. |
| Battle freezes after changing the battle speed | A known issue: change the speed while you choose commands, not while a round plays out. |
| You want saves from Dolphin, a GameCube or another device | **Settings → Saves → Import a save**; see [Importing a save](#importing-a-save). |
| **Android:** installing an update fails or asks you to uninstall first | It was signed with a different key (a self-built APK). Uninstalling **deletes your saves**; export them first. |
| Still stuck | [Open an issue](../../issues/new/choose) with your platform, device and what happened. |

## Controls

The game's own controls are unchanged. On top of them:

| Input | What it does |
|---|---|
| Hold Z, press Start | The settings menu (L / R switch tabs; B or Start close it). Its Saves tab saves and loads states |
| Hold Z + C-stick up / down | Tilt the camera |
| Hold Z + R / L | Zoom out / in |
| Tap Z in battle | Battle speed: Normal, 2x, 3x, 4x |
| L3 or R3 | Invincible on / off |

Every setting is also in the launcher's **Settings** window, with a short description of each.
**Settings → Controls** has the controller profile, the stick dead zone and rumble.

## What It Adds

Most of these are set in game from the settings menu (**hold Z, press Start**), or in the launcher's
**Settings** window.

- **Widescreen** 16:9 with the HUD at the screen edges, or the original 4:3 (identical to the
  original game).
- **60 fps** by frame interpolation, or the original 30.
- Internal resolution and anti-aliasing.
- **Camera control:** hold Z and use the C-stick to tilt, and R / L to zoom (towns, dungeons and the
  world map; on the world map Z + C-stick left / right also turns around the ship).
- **Battle speed** up to 4x: tap Z during a battle.
- **Options:** random encounter rate (off to 4x), EXP / gold / magic EXP (off to 4x), and Invincible.
- **Save states:** five slots, plus an auto-save every 10 minutes on the field (the newest three are
  kept). Your memory card saves are never touched by a state.
- **Continue** in the launcher starts from your newest save state or memory card save, whichever is
  newer. A memory card save loads straight into the game, without the intro or title screen.

### Importing a save

**Settings → Saves → Import a save** takes a save you already have and puts it where the game finds
it:

- **A memory card save** from Dolphin or a real GameCube: `.gci`, `.gcs` or `.sav`.
- **A whole memory card** from Dolphin (`MemoryCardA.USA.raw`) or a card dump (`.gcp`): its Skies of
  Arcadia Legends saves are taken, nothing else.
- **A save state** from this port, e.g. from your other device. Save states only work in the same
  version of the port; memory card saves work in every version.

A memory card save replaces the file with the same number; the old one is kept beside it as
`.pre-import.bak`. Saves from the European or Japanese version can't be used.

### GPU driver (Android)

On Adreno devices, **Settings → Graphics → GPU driver** can load a driver package (for example Mesa
Turnip) when the device's own driver crashes or draws things wrong. This is unsupported; if the game
doesn't close normally with a new driver, the app goes back to the device's own driver by itself, and
after the first normal session it asks whether to keep the new one.

### Slower devices

If 60 fps can't keep full speed for a while, the settings menu opens once by itself on Frame rate, so
you can switch to 30 fps. Nothing changes unless you choose it.

## Known Issues

- **Changing the battle speed while a round plays out can freeze the battle.** Change it while you
  choose commands instead.
- On Android, battles top out at about 2.5x speed on a Snapdragon 865; 3x and 4x settle at 2x.
- An auto-save can cause a brief hitch on Android.

## Reporting a Problem

[Open an issue](../../issues/new/choose) and say what happened and where in the game. On Android, open
**Settings → About → Copy device info** and paste it into the report: it names the device, its chip
and the graphics driver.

## Disc and Storage

Your imported game, saves and settings are kept in:

| | |
|---|---|
| **Windows** | `%LOCALAPPDATA%\Skies of Arcadia Legends` |
| **Linux** | `~/.local/share/Skies of Arcadia Legends` |
| **Android** | The app's own storage. Uninstalling deletes it, so copy your saves out first. |

Nothing from the game is in this repository or in any build: the launcher reads your disc, and the
port's changes to the game are applied to your copy as a patch.

## Building

See [BUILDING.md](BUILDING.md). How the port works, with measurements and history, is in
[`docs/development-notes.md`](docs/development-notes.md).

## Credits

- Launcher background art: **I_LIKE_SONIC**
- App icon art: **Reckoner**
- Built on [ModernGekko](https://github.com/ExpansionPak/ModernGekko) and
  [DolRecomp](https://github.com/ExpansionPak/DolRecomp), which build on the
  [Dolphin](https://dolphin-emu.org/) emulator's hardware code.
- Started from [ModernGekko-Template](https://github.com/ExpansionPak/ModernGekko-Template).

Skies of Arcadia Legends is © SEGA. This project isn't affiliated with or endorsed by SEGA.

## License

GPL-3.0 (see [`COPYING`](COPYING)). Third-party components and their licences are listed in
[`release/THIRD-PARTY.md`](release/THIRD-PARTY.md).
