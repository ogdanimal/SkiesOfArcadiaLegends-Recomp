# Skies of Arcadia Legends: Recompiled

A native port of **Skies of Arcadia Legends** (GameCube) for Windows, Linux, macOS and Android, made by static
recompilation. It doesn't contain the game: you supply your own copy of the disc, and the game's
data is read from it.

This is a **beta** (v0.82). Please report what you run into.

## What you need

- Your own dump of the **USA**, the **European** or the **Japanese** GameCube disc (game ID `GEAE8P`,
  `GEAP8P` or `GEAJ8P`, the original release; the Japanese one is *Eternal Arcadia Legends*). The
  European disc has English, German, French and Spanish (the **Language** tab in Settings and in the
  in-game menu, which can restart the game in the new language at once); the Japanese disc is in
  Japanese (the port's own menus stay in English). The import checks the game.
- As RVZ, ISO, GCM, GCZ, WIA or CISO.
- **Windows:** 64-bit Windows 10 (version 1703 or later) or 11.
- **Linux:** 64-bit (x86_64) with glibc 2.35 or newer (Ubuntu 22.04, Debian 12, Fedora 36, SteamOS,
  Bazzite or later), and a Vulkan or OpenGL driver.
- **macOS:** macOS 13 or later on an Apple Silicon Mac (M1 or newer).
- **Android:** a 64-bit (arm64) device with Android 7 or newer. Tested on a Retroid Pocket 5
  (Snapdragon 865), which runs it at full speed at 60 fps. Slower devices may not.

## Getting started

**Windows:** unzip, run `Skies of Arcadia Legends.exe`, choose **Select Disc** and pick your disc image. The
launcher imports it (a few seconds), patches it and starts the game. After that, **Start Game** or
**Continue** start it directly.

**Linux:** the `.AppImage` is one file with the libraries it needs inside: make it executable
(`chmod +x`, or the file's Properties) and run it. Or extract the `.tar.gz` and run
`./SkiesOfArcadiaLegends` from inside the folder. The rest is as on Windows.

**macOS:** unzip, and open **Skies of Arcadia Legends** (move it to Applications if you like); the
rest is as on Windows. The app isn't notarized by Apple, so the first time macOS says it can't check
it: open **System Settings → Privacy & Security** and choose **Open Anyway** (or Control-click the app
and choose **Open**). If the keyboard does nothing in the game, allow it in **Privacy & Security →
Input Monitoring**.

**Android:** install the APK, open **Skies of Arcadia Legends** and pick your disc image when asked.

**Updating from an earlier version:** saves, save states and settings carry over. On Windows and
Linux, if the launcher asks for your disc once more, choose **Select Disc** and pick the same disc image
(it updates your earlier import in a few seconds). On Android, install the new APK over the old one;
the app updates the imported disc by itself.

Your saves and settings are kept in `%LOCALAPPDATA%\Skies of Arcadia Legends` (Windows),
`~/.local/share/Skies of Arcadia Legends` (Linux), `~/Library/Application Support/Skies of Arcadia
Legends` (macOS) or in the app's own storage (Android). Uninstalling the Android app deletes them, so make a backup first
(**Settings → Saves → Back up saves**), or move them out of the app's storage.

**Settings → Disc → Data folder → Change...** moves them to a folder or drive of your choice; the
launcher starts again on it. On Android (11 or later) it asks for All files access first (the game reads its files
by name there), and the data then stays when the app is uninstalled. On Windows and Linux, an empty
`portable.txt` beside the program keeps everything in a `User` folder beside it instead; the first start
in portable mode offers to move the data you already have into it.

### Bringing your saves

**Settings → Saves → Import a save** takes a save you already have and puts it where the game finds
it. It asks first, then opens a file picker. It takes:

- **A memory card save** from Dolphin or a real GameCube: `.gci` (Dolphin's GCI folders, e.g.
  `GC/USA/Card A`), `.gcs` or `.sav` (memory card tools such as GameShark or MaxDrive).
- **A whole memory card** from Dolphin (`MemoryCardA.USA.raw`) or a card dump (`.gcp`): its Skies of
  Arcadia Legends saves are taken, nothing else.
- **A save state** from this port (`.sav` from the `StateSaves` folder), e.g. from your other device.
  Save states only work in the same version of the port; memory card saves work in every version.
- **A backup** made with **Settings → Saves → Back up saves** on Android (`.zip`): every save in it.

A memory card save replaces the file with the same number here; the one it replaces is kept beside
it as a `.pre-import.bak` file (`.pre-import.2.bak` and so on if you import over it again). A save state is added to the ones you have. **Continue** then starts
from what you imported. A save has to be from the same version as your disc (USA, European or
Japanese).

## What it adds

- **Widescreen** 16:9 with the HUD at the screen edges, **Match screen** for any other shape
  (ultrawide 21:9 up to 32:9, 16:10, a phone's 20:9), or the original 4:3 (identical to the
  original game). On an ultrawide, a higher Resolution keeps the picture sharp across the width.
- **HUD placement** on a wide picture: at the screen edges, in a centred 21:9 or 16:9 area (easier to
  take in on an ultrawide), or the original 4:3 layout in the middle.
- **60 fps** by frame interpolation, or the original 30.
- Internal resolution and anti-aliasing; texture filtering (smooth or sharp pixels), anisotropic
  filtering, and **Scaling**: Sharp keeps the original resolution (640x528) crisp on a large screen.
- **Camera control:** hold Z and use the C-stick to tilt, and R / L to zoom out / in (towns, dungeons
  and the world map; on the world map Z + C-stick left / right also turns around the ship).
- **Battle speed** up to 4x: tap Z during a battle. Spells and special attacks, ship battles' too,
  still play at normal speed, so their sound stays with the picture.
- **Field speed** 1.5x, 2x or 3x for cutscenes, towns, dungeons and sailing: hold Z and press X
  outside battle. A desktop PC holds 2x everywhere; 3x needs a fast one. Where a device can't keep
  up, it steps down by itself to what that device holds (a Retroid Pocket 5: about 1.25x-1.5x).
- **Options:** random encounter rate (off to 4x), Hold B for no random battles, EXP / gold / magic
  EXP (off to 4x), and Invincible (press L3 or R3 to toggle).
- **Romhacks** that change the game's data, such as Maeson's rebalance: see below.
- **Keyboard and button mapping:** play with the keyboard on Windows and Linux, and change any
  button or key (see *Controls*). On desktop, any controller you press is picked up without setup.
- **Save states:** five slots, plus an auto-save every 10 minutes on the field (the newest three are
  kept). Your memory card saves are never touched by a state.
- **Continue** in the launcher starts from your newest save state or memory card save, whichever is
  newer. A memory card save loads straight into the game, without the intro or title screen.
- **Import a save** from Dolphin, a GameCube memory card or another device (see *Bringing your
  saves*).
- **Dreamcast music** (optional), from your own Dreamcast disc: see below.
- **Uncensored** (optional), the original Japanese models and textures, from your own Eternal
  Arcadia discs: see below.
- **HD textures** (optional), from a texture pack you downloaded: see below.

### Dreamcast music

**Settings → Dreamcast → Import a Dreamcast disc...** in the launcher takes your own Skies of Arcadia
Dreamcast disc as a **GDI** (with its track files beside it) or a **CHD** (Android: CHD). Either
disc has most of the music; import both for all of it (they can be from different regions). Then set **Music** to **Dreamcast**
(there, or in the settings menu's Dreamcast tab in game). The Dreamcast's sound driver plays the songs
from your disc on an emulated sound chip, in place of the GameCube music. The two streamed songs
that differ on the Dreamcast (the intro's, disc 1, and a story scene's, disc 2) play the disc's
recordings too, converted once the first time they're needed. **Intro music** (below the import)
picks the intro's: the Dreamcast recording is a different cut that doesn't line up with this intro's
scenes; GameCube keeps them in step. Music imported with an earlier version has no streamed songs:
import the disc again. **Remove** deletes the imported music and Eternal Arcadia files.

### Uncensored

The Western releases (and every GameCube version) changed some art; only the Japanese Dreamcast
release, **Eternal Arcadia**, has the originals. Import your own Eternal Arcadia **disc 1** and **disc
2** the same way (**Settings → Dreamcast**; v1.009, the regular release), then set **Uncensored** to
**On** there or in the settings menu's Dreamcast tab. It brings back Dyne's and Gilder's cigars, Don's
cigar and bottle, Bellena's outfit, the tavern sign, the red-faced drunks and their drinks, and
Gilder's victory pose with his smoke (one of his three) and Vigoro in Aika's cell, from the next
area or scene the game loads.
The models are converted for your GameCube disc into `DreamcastUncensor` in the data folder; each
file is checked first and keeps its original's size, so save states work either way. Not yet: the
cut or rewritten scenes (the drunk sailor, Ramirez's blood, "loqua").

### HD textures

Texture packs made for Dolphin work, for example **Skies of Arcadia HD** by Bighead and frozenwings
(Dolphin forums). None is included. **Settings → Graphics → Import a texture pack...** takes the
`.7z` or `.zip` you downloaded (on desktop, **Import a folder...** an unpacked one) and turns **HD
textures** on; switch it there or in game. Only the pack's `GEA` textures are copied when it has
that folder, without its optional extras; else all of its textures. Each import adds to what is
imported, so a pack in several archives (the **HD Texture Pack 4x Upscaled** comes as six) can be
imported one archive at a time, or all at once: pick them all in the Android picker, or put them in
one folder or `.zip` / `.7z` and import that. **Remove** deletes everything imported. On Android,
take a pack's **PNG** download where there is one: most phones can't show BC7 DDS textures (with
their own driver; a Turnip driver can). BC1 and BC3 DDS textures work.

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

## Controls

On Windows and Linux the keyboard and a controller both work, together or on their own. The game
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
works. On Linux the keyboard needs an X11 session or XWayland (what most desktops run games in).

The game's own controls are unchanged; on top of them:

| Input | What it does |
|---|---|
| Esc, the controller's Back / View (Create) / Select, or hold Z and press Start | the settings menu (L / R switch tabs; B, Start or Esc close it). Its Saves tab saves and loads states |
| Hold Z + C-stick up / down | tilt the camera |
| Hold Z + R / L | zoom out / in |
| Tap Z in battle | battle speed: Normal, 2x, 3x, 4x |
| Hold Z and press X outside battle | field speed (cutscenes, walking, sailing): Normal, 1.5x, 2x, 3x |
| L3 or R3 (a stick click) | Invincible on / off (a key for it can be set in Button mapping) |

Every setting is also in the launcher's **Settings** window, with a short description of each.

## Known issues

- On Android, battles top out at about 2.5x speed on a Snapdragon 865; 3x and 4x settle at 2x.
- On Android, the field speed tops out at about 1.25x-1.5x on a Snapdragon 865 (1.5x on foot, 1.25x on
  the world map and in cutscenes).
- An auto-save can cause a brief hitch on Android.
- If you change Picture while the world map's menu (Start) is open, the flight meter and compass keep
  the old shape until you close that menu.
- At the widest shapes (32:9), a few scenes show unfinished scenery at the far edges.
- **macOS: the PS button opens Apple's Game Overlay** and the game stops responding until it's
  closed (press PS or Esc again). Use **Create** for the game's settings menu.
- **"Game files missing" when the game starts (Windows / Linux / macOS):** extract the whole zip again and keep
  its files together; if the file named there disappears again, your antivirus removed it.

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
- The **Hold B** option (no random battles while B is held) is **cleartonic**'s idea: their "hold B
  for no random encounters" patch (romhacking.net 5314), redone as a setting of the port's own.
- Romhack support was made for, and tested with, **Maeson**'s rebalance (romhacking.net 2859).
- HD textures were tested with **Shoopkid**'s HD Texture Pack 4x Upscaled (not included: download
  it from its author).
- Built on [ModernGekko](https://github.com/ExpansionPak/ModernGekko) and
  [DolRecomp](https://github.com/ExpansionPak/DolRecomp), which build on the
  [Dolphin](https://dolphin-emu.org/) emulator's hardware code.
- The Dreamcast music plays through Neill Corlett's
  [Highly Theoretical](https://github.com/kode54/Highly_Theoretical) (the Dreamcast sound chip,
  maintained by kode54) and reads CHD images with Romain Tisserand's
  [libchdr](https://github.com/rtissera/libchdr).
- HD texture packs in `.7z` files are read with Igor Pavlov's
  [LZMA SDK](https://www.7-zip.org/sdk.html).

Skies of Arcadia Legends is (c) SEGA. This project isn't affiliated with or endorsed by SEGA.

## License

GPL-3.0 (see `COPYING`). Third-party components and their licences are listed in `THIRD-PARTY.md`.
