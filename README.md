# Skies of Arcadia Legends: Recompiled

A native port of **Skies of Arcadia Legends** (GameCube) to **Windows, Linux, macOS and Android**, made by
static recompilation: the game's PowerPC code is translated ahead of time into native code, and the
GameCube's graphics, sound and input come from [Dolphin](https://dolphin-emu.org/)'s hardware code.
It adds widescreen, 60 fps, camera control, save states and quality-of-life options.

**This is a beta (v0.51).** Please [report](../../issues/new/choose) what you run into.

No build contains the game. You need your own copy of the disc.

## Download

Get a build from the **[Releases](../../releases)** page:

| Platform | File |
|---|---|
| Windows | `SkiesOfArcadiaLegends-<version>-windows.zip` |
| Linux | `SkiesOfArcadiaLegends-<version>-linux-x86_64.AppImage` (or `-linux.tar.gz`) |
| macOS (Apple Silicon) | `SkiesOfArcadiaLegends-<version>-macos.zip` (from the release after v0.51.0-beta) |
| Android | `SkiesOfArcadiaLegends-<version>-android.apk` |

Versions ending in `-beta` (or `-rc1`, `-rc2`, ...) are published as prereleases.

## Getting Started

1. Install it:
   - **Windows:** unzip, and run `Skies of Arcadia Legends.exe` from inside the folder.
   - **Linux:** make the `.AppImage` executable (`chmod +x`, or **Allow executing** in the file's
     Properties) and run it; it has the libraries it needs inside. Or extract the `.tar.gz` and run
     `./SkiesOfArcadiaLegends` from inside the folder.
   - **macOS:** unzip, and move **Skies of Arcadia Legends** to Applications if you like. The app isn't
     notarized by Apple, so the first time macOS says it can't check it: open **System Settings →
     Privacy & Security** and choose **Open Anyway** (or Control-click the app and choose **Open**).
   - **Android:** install the APK and open **Skies of Arcadia Legends**.
2. Choose **Select Disc** and pick your disc image. The launcher checks it, imports it (a few seconds)
   and patches it for this port; you only do this once.
3. Press **Start Game**. Later, **Continue** starts from your newest save.

**Updating from an earlier version:** your saves, save states and settings carry over. On Windows and
Linux, unpack the new version and run it; if the launcher asks for your disc once more, choose
**Select Disc** and pick the same disc image, which updates your earlier import in a few seconds. On
Android, install the new APK over the old one; the app updates the imported disc by itself.

**The disc is the USA or the European GameCube release** (game ID `GEAE8P` or `GEAP8P`, the original
release), as RVZ, ISO, GCM, GCZ, WIA or CISO. The European disc has the text in English, German,
French and Spanish (the **Language** tab in Settings and in the in-game menu, which can restart the
game in the new language at once) and runs at 60 Hz like the USA one. The Japanese release isn't
supported. One disc is installed at a time: **Change Disc** replaces it.

**Requirements:**

| | |
|---|---|
| **Windows** | 64-bit Windows 10 or 11 |
| **Linux** | 64-bit (x86_64), with a Vulkan or OpenGL driver |
| **macOS** | macOS 13 or later on an Apple Silicon Mac (M1 or newer) |
| **Android** | 7.0+, a 64-bit (`arm64-v8a`) device |

Tested on a **Retroid Pocket 5** (Android 13, Snapdragon 865), which runs it at full speed at 60 fps,
and on Windows. Slower Android devices may not keep 60 fps (see [Slower devices](#slower-devices)).

## Something's Wrong? Quick Fixes

| Problem | Try this |
|---|---|
| **macOS:** the game opens Apple's **Game Overlay** and stops responding to the controller | That's the controller's PS button: macOS uses it for its own overlay. Press it again (or Esc) to close the overlay, and use **Create** for the game's settings menu. |
| **macOS:** "can't be opened" / "Apple could not verify" at the first start | The app isn't notarized: **System Settings → Privacy & Security → Open Anyway** (once). |
| **Windows / Linux:** "Failed to create Vulkan device" or a similar message at start | Your graphics driver can't run Vulkan. The game switches to OpenGL by itself and keeps it (**Settings → Display → Graphics backend**). If neither starts, install the latest driver for your graphics card. |
| **Android:** the game crashes or draws things wrong | **Settings → Graphics → Renderer** switches between Vulkan and OpenGL ES. On Adreno devices, **Settings → Graphics → GPU driver** can load another driver (see [GPU driver](#gpu-driver-android)). |
| **Windows / Linux / macOS:** "Game files missing" when the game starts | A file of the download (`gGEAE8P_recomp.dll` or the European `gGEAP8P_recomp.dll`; `.so` on Linux; inside the app on macOS) isn't beside the game. Extract the whole zip again and keep its files together; if the file disappears again, your antivirus removed it (allow the folder). |
| The import refuses your disc | It must be the USA (`GEAE8P`) or European (`GEAP8P`) disc, the original release. The Japanese disc isn't supported. |
| Battle freezes after changing the battle speed | A known issue: change the speed while you choose commands, not while a round plays out. |
| You want saves from Dolphin, a GameCube or another device | **Settings → Saves → Import a save**; see [Importing a save](#importing-a-save). |
| **Android:** installing an update fails or asks you to uninstall first | It was signed with a different key (a self-built APK). Uninstalling **deletes your saves**; back them up first with **Settings → Saves → Back up saves**. |
| **SteamOS / Linux through Steam:** the Steam Controller works in menus but not in the game | Steam may have it on its desktop (mouse and keyboard) layout. Hold Start, or pick a gamepad layout in the game's controller settings in Steam. |
| Still stuck | [Open an issue](../../issues/new/choose) with your platform, device and what happened. |

## Controls

On Windows, Linux and macOS the keyboard and a controller both work, together or on their own. The game
uses whichever controller you press, so a controller plugged in later or a second one needs no setup
(started from Steam with Steam Input, Steam's controller works the same way; the Steam Controller
shows as "Steam Controller"). On Android the built-in controller, or
the first one you connect, is set up for you; without one, the game shows touch controls (the in-game
**Back** menu turns them on and off).

| GameCube | Controller (Xbox / PlayStation) | Keyboard |
|---|---|---|
| A / B | A / B (Cross / Circle) | Space / Left Shift |
| X / Y | X / Y (Square / Triangle) | F / R |
| Z | RB (R1) | Tab |
| L / R | LT / RT (L2 / R2) | Q / E |
| Start | Start (Options) | Enter |
| Control stick | Left stick | W A S D |
| C-stick | Right stick | I J K L |
| D-pad | D-pad | Arrow keys |

The face buttons go by name, not position: Xbox A is GameCube A. Every button and key can be
changed: **Settings → Controls** in the launcher, or **Controls → Button mapping** in the in-game
menu (on Android too). The controller's A and B can be changed but not cleared, so the menu always
works. On Linux the keyboard needs an X11 session or XWayland (what most desktops run games in). On
macOS, Control-Command-F switches full screen.

The game's own controls are unchanged; on top of them:

| Input | What it does |
|---|---|
| Esc, the controller's Back / View (Create) / Select, or hold Z and press Start | The settings menu (L / R switch tabs; B, Start or Esc close it). Its Saves tab saves and loads states |
| Hold Z + C-stick up / down | Tilt the camera |
| Hold Z + R / L | Zoom out / in |
| Tap Z in battle | Battle speed: Normal, 2x, 3x, 4x |
| L3 or R3 (a stick click) | Invincible on / off (a key for it can be set in Button mapping) |

Every setting is also in the launcher's **Settings** window, with a short description of each.
**Settings → Controls** has the controller, every button and key, the stick dead zone and rumble.

## What It Adds

Most of these are set in game from the settings menu (**Esc, the controller's Back, or hold Z and
press Start**), or in the launcher's **Settings** window.

- **Widescreen** 16:9 with the HUD at the screen edges, **Match screen** for any other shape
  (ultrawide 21:9 up to 32:9, 16:10, a phone's 20:9), or the original 4:3 (identical to the
  original game). On an ultrawide, a higher Resolution keeps the picture sharp across the width.
- **HUD placement** on a wide picture: at the screen edges, in a centred 21:9 or 16:9 area (easier to
  take in on an ultrawide), or the original 4:3 layout in the middle.
- **60 fps** by frame interpolation, or the original 30.
- Internal resolution and anti-aliasing.
- **Camera control:** hold Z and use the C-stick to tilt, and R / L to zoom (towns, dungeons and the
  world map; on the world map Z + C-stick left / right also turns around the ship).
- **Battle speed** up to 4x: tap Z during a battle.
- **Options:** random encounter rate (off to 4x), Hold B for no random battles, EXP / gold / magic
  EXP (off to 4x), and Invincible.
- **Romhacks** that change the game's data, such as Maeson's rebalance (see [Romhacks](#romhacks)).
- **Keyboard and button mapping:** play with the keyboard on Windows and Linux, and change any
  button or key (see [Controls](#controls)). On desktop, any controller you press is picked up without setup.
- **Dreamcast music** (optional): the Dreamcast version's soundtrack, from your own Dreamcast disc
  (see [Dreamcast music](#dreamcast-music)).
- **HD textures** (optional): a texture pack you downloaded, imported in Settings (see
  [HD textures](#hd-textures)).
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
- **A backup** made with **Settings → Saves → Back up saves** on Android (`.zip`): every save in it.
  Make one before uninstalling the app, which deletes its saves.

A memory card save replaces the file with the same number; the old one is kept beside it as
`.pre-import.bak`. A save has to be from the same version as your disc: USA saves with the USA
disc, European saves with the European disc. Saves from the Japanese version can't be used.

### Dreamcast music

The Dreamcast release of Skies of Arcadia plays the same soundtrack on the Dreamcast's own sound
chip, and it sounds different from the GameCube version. To hear it here:

1. In the launcher, **Settings → Sound → Import a Dreamcast disc...** and pick your own Skies of
   Arcadia Dreamcast disc: a **GDI** (with its track files beside it) or a **CHD**. On Android, use a
   CHD. Either disc has most of the music; import both for all of it.
2. Set **Music** to **Dreamcast** (launcher **Settings → Sound**, or the settings menu's Sound tab in
   game; it switches mid-song).

The port runs the Dreamcast's sound driver on an emulated sound chip and plays the songs from your
disc, in place of the GameCube music. Two of the streamed songs differ on the Dreamcast, the intro's
(disc 1) and a story scene's (disc 2), and play the disc's recordings too (converted once, the first
time they're needed, and kept in the music folder); the other two (the title and staff roll, the
ending) are the same recordings on both. **Intro music** (below the import) picks the intro's: the
Dreamcast recording is a different cut, so it doesn't line up with this intro's scenes; choose
GameCube to keep them in step. Music imported with an earlier version has no streamed songs: import
the disc again. The two discs can come from different regions. Where both have a song, the USA or
Japanese copy is kept, because the European discs play the story scenes' music slower (made for 50 Hz).
**Remove** deletes the imported music. Only the sound driver and
the music are copied from the disc.

### HD textures

Texture packs made for Dolphin work here, for example **Skies of Arcadia HD** by Bighead and
frozenwings (on the Dolphin forums; it redoes the three leads' faces and costumes, the skies, the
menus, text and some effects). None is included: download one yourself, then:

1. In the launcher, **Settings → Graphics → Import a texture pack...** and pick the `.7z` or `.zip`
   you downloaded (on desktop, **Import a folder...** takes one you already unpacked).
2. **HD textures** turns on by itself. Switch it in the launcher or in game from the settings menu's
   Graphics tab.

Only the pack's textures for this game are copied: its `GEA` folder (used by both discs) when it has
one, leaving out its optional extras (French text, the other flight meter), else every texture in
it. Each import adds to what is already imported, so a pack that comes as several archives (the
**HD Texture Pack 4x Upscaled** by Shoopkid comes as six) can be imported one archive at a time, or
all at once: pick them all in the Android picker, or put them in one folder or `.zip` / `.7z` and
import that; a texture in two packs is the last import's. **Remove** deletes everything imported.
On Android, take a pack's **PNG** download where there is one: most phones can't show BC7 DDS
textures (with their own driver; a Turnip driver can), and the app shows how many of them stay as
they were. BC1 and BC3 DDS textures work.

### Romhacks

A romhack of the game that changes its **data** plays like the original disc: patch your own disc
image with the romhack's patch (DeltaPatcher for an `.xdelta`, Lunar IPS / Floating IPS for an
`.ips`), then pick the patched image with **Select Disc** (Android: **Change Disc**). The launcher
shows *modified* beside the game's name. Enemy and item stats, encounter tables, scripts, text,
textures and models all count as data; so do the tables inside `main.dol`.

A romhack that changes the game's **code** can't run on this port's recompiled code, and the import
says so. One code change is recognised and replaced by an option of the port's own: the **hold B for
no random encounters** patch (cleartonic's, also built into **Maeson's rebalance**, which therefore
plays) becomes **Settings → Gameplay → Hold B**.

A romhack's save states are kept apart from the original game's (a state brings back the game's
item and magic tables, so they mustn't mix): Continue, the menu's slots and the auto-saves only see
the disc you play. Its memory card saves sit beside the original game's (the romhack keeps the disc
ID), which the romhack's author says is fine.

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
| **macOS** | `~/Library/Application Support/Skies of Arcadia Legends` |
| **Android** | The app's own storage. Uninstalling deletes it: back up first (**Settings → Saves → Back up saves**), or move it out. |

To keep them somewhere else, open **Settings → Disc → Data folder → Change...** and pick a folder or
drive: everything moves there (into a folder named *Skies of Arcadia Legends*) and the launcher starts
again on it. **Move Back to the Default** returns it. Picking a folder that already holds this port's
data (from another install, or after reinstalling) uses it as it is. If the folder isn't there at
start (a drive not connected), the launcher asks: try again or use the default folder.

- **Portable (Windows and Linux):** an empty file named `portable.txt` beside the program keeps
  everything in a `User` folder beside it, for example on a USB drive. Copy that folder along when you
  update to a new version. (Linux: the `.tar.gz` only; the AppImage can't hold a file beside it.)
- **Android (11 or later):** Change offers internal storage, an SD card or another folder, and asks for **All files
  access** first (the game reads its files by name, which Android allows outside the app's own storage
  only with it). Moved out, the data stays when the app is uninstalled and a file manager can reach it.

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
- The Dreamcast music plays through Neill Corlett's
  [Highly Theoretical](https://github.com/kode54/Highly_Theoretical) (the Dreamcast sound chip,
  maintained by kode54) and reads CHD images with Romain Tisserand's
  [libchdr](https://github.com/rtissera/libchdr).
- HD texture packs in `.7z` files are read with Igor Pavlov's
  [LZMA SDK](https://www.7-zip.org/sdk.html).
- Started from [ModernGekko-Template](https://github.com/ExpansionPak/ModernGekko-Template).

Skies of Arcadia Legends is © SEGA. This project isn't affiliated with or endorsed by SEGA.

## License

GPL-3.0 (see [`COPYING`](COPYING)). Third-party components and their licences are listed in
[`release/THIRD-PARTY.md`](release/THIRD-PARTY.md).
