# Handoff: player settings, in-game menu, save states, save import, launcher, camera and options

## Current state (2026-10-12; input 2026-10-17)

> **Since then (2026-10-17, v0.10.0-beta):** the menus gained Language (European disc), Music
> (Dreamcast music) and Button mapping; settings keys and menu tabs are in NOTES *The player build*;
> the work is in HANDOFF-dcmusic-eu and HANDOFF-input. Branch `hdtex` (not merged yet) adds
> **HD textures** to the launchers' and the menu's Graphics tab (HANDOFF-hdtextures).

Everything below the history line is the session log; this is the summary. What a player gets and how it
works is in NOTES *The player build* (controls, settings keys, menu, camera, options, launcher).

- **HUD placement (2026-10-17 night, branch `hud`, not merged; NOTES *HUD placement*):** `[Skies] Hud`
  2 = edges (default), 3 = 21:9 area, 1 = 16:9 area, 0 = original 4:3 layout; Graphics tab row after
  Picture in the menu, the launcher (`skies_video_rows` now 0..3) and Android Settings.

- **Match screen (2026-10-17 late night, on `dev`, not released; NOTES *Match screen*):** Picture has
  three values, `[Skies] Widescreen` 1 = 16:9, 2 = Match screen (the window's shape, 4:3..32:9,
  applied live as the window resizes; Dolphin's custom ratio is set in the current layer only), 0 =
  4:3; the menu steps 16:9 / Match screen / 4:3. Entries below that say Picture is 16:9 / 4:3
  predate this.

- **Keyboard, button mapping, menu hotkey (2026-10-17, shipped in 0.10.0-beta; HANDOFF-input.md, NOTES
  *Keyboard and button mapping*):** the keyboard plays on Windows and Linux (the game adds its layout to
  controller 1's profile); every button and key can be changed in the launcher's Settings → Controls
  (a table replacing the old "controller profile" choice) and in the menu's **Controls → Button mapping**
  (all three platforms); **Esc or a controller's Back / Select** opens and closes the menu besides Z+Start;
  Invincible's L3 / R3 is a hotkey that can be rebound; controller 1 moves to the SDL gamepad in use
  (desktop) and is "Always Connected". Older entries below that say the menu opens only with Z+Start, or
  that controls aren't in the menu, predate this.

- **Battle freeze from older states, and Continue straight into a card save (2026-10-12 evening, game
  patch ws19 / ws20; patches/game/README.md, NOTES *Memory card auto-load*):** the user's every-attack
  freeze came from the battle handler table entry their states carried from ws15 / ws16 (`qol_damage`'s
  old address, now inside other patch code); `qol_frame` rewrites it every frame (verified on Windows
  from the user's state: two battles to the end, then on the Retroid by the user). Continue from a card
  save now loads it in the game patch itself (`autoload.c`), no logos / intro / title / load screen:
  ~6 s from launch on Windows, RAM identical to a menu load; Retroid ws20 APK checked by the user ("all
  good on retroid"). **Correction (2026-10-17):** it wasn't identical: skipping the logos also skipped
  the resident files' load, and the first battle after such a Continue crashed (ws20-ws22, all
  platforms); fixed in ws23 (`b1493b4`, development-notes *Memory card auto-load*). Files 1-99 (the old button-driving refused files past 3), and the launchers fall back to the
  newest state when the newest card save can't be auto-loaded.

- **Import a save (2026-10-12, user request: "same behavior as goemon and hybrid heaven", card saves
  and states, the launchers' Saves tab on all three platforms; NOTES *Importing saves*):** shared
  `lib/ModernGekko/tools/save_import.cpp` (desktop launcher; Android through JNI). Prompt before the
  picker with Cancel focused; `.gci` / `.gcs` / `.sav` card saves, `.raw` / `.gcp` card images and
  Dolphin states, told apart by contents and checked like their loaders; replaced card saves kept as
  `.pre-import.bak`, staged writes; imports dated now so Continue takes them. Verified: 16 cases on WSL
  (`tools/save-import-test.py`), the Windows release exe, and on the Retroid (installed with the user's
  OK: Continue auto-loaded an imported card file in game, and an imported state; test imports removed,
  the user's saves md5-identical to the backup). Windows / Linux packages rebuilt with it. Not
  committed yet. Not clicked by hand: the desktop file dialog itself (SDL; the import behind it is the
  tested `--import-save` path).

- **Release prep (2026-10-11/12, HANDOFF-release.md):** everything player-facing is named "Skies of
  Arcadia Legends" (launchers, Android app, user folders, packages). New settings: Android Graphics
  **Renderer** (Vulkan / OpenGL ES; OpenGL ES checked on the Retroid) and **GPU driver** (Adreno, with
  a crash safeguard), an **About** tab (device info, licences); on every platform the menu opens **once**
  on Frame rate when 60 fps stays under 90% speed (`[Skies] SlowFrameRateNoted`). NOTES *Slower devices
  and GPU drivers*. Linux has the same branded launcher as Windows (`tools/build-release-linux.sh`).

- **In-game menu restyled like the launchers (2026-10-11 late night, user request; NOTES *The in-game
  menu*):** tabs (Saves / Graphics / Gameplay / Sound / Controls, L / R; Up from the first setting
  reaches the tab bar, the user's follow-up), label + value box + description rows, Segoe UI / Roboto,
  no Close row; checked on Windows (dev play build: tabs, the tab bar, a change saved on close; the dev
  `user-ws` volume put back to 0). Windows dev and release runners rebuilt with it. Android: on the
  Retroid the D-pad / stick / L / R did nothing at first: the app's controller profile, not the menu
  (NOTES *Known issues*; fixed, HANDOFF-android.md). Installed: `skies-recomp-ws16-tabs` (20:44, still
  with a temporary input log); user 10-11: the menu is "perfect". WSL runtime not rebuilt.
- **Launcher redesign (2026-10-11 late night, user requests; NOTES *Launcher and release*):** the art
  the user supplied as the background (`release/launcher-background.png`), laid out like the N64 recomps:
  title at the top left, menu at the bottom left (Select Disc with its hint tight under it; Continue /
  Start Game once a disc is in), and **Settings as a window inside the launcher** with tabs Graphics /
  Display / Gameplay / Sound / Controls / Saves / Disc, every in-game menu setting wired to the same keys,
  one-line descriptions, A- / A+ text size (default 120 %), Netplay removed from it. The user liked the
  look ("much better than before") and steered the details (hint placement, spacing, descriptions, text
  size, Resolution under Display, fullscreen as a Windowed / Fullscreen row). Built in the Windows release
  folder only (`lib\ModernGekko\build-release`), not packaged or committed when written; the user hasn't
  clicked through it yet (a setting written by the launcher, A- / A+). Android's branded app got the same
  home screen: `skies-recomp-ws16-2026-10-11.apk` (19:21), installed on the Retroid 19:52 (user OK).
  Then Settings on Android too (20:00, `SkiesSettingsPanel.kt`, HANDOFF-android.md): the user tried it
  on the device and their changes saved.
  Fixed after the user's first try: picking the disc over their old ws10 import failed ("existing
  extraction patching failed"; now `sys/` is refreshed from the disc, their install is on ws16), and the
  launcher froze for seconds at startup (it hashed all 1.4 GB of game files; now skipped). Disc formats
  checked through the real import: RVZ, ISO, GCM, GCZ, WIA, CISO all work; WBFS dropped (NOTES).
- **ws16 (`66da66de` / `7b4c416c`, WS_VERSION 10, 2026-10-11 late night, user requests):** ship battle
  ring lists 50 px lower, crew / ship panels midway between the Spirit gauge and the ring (dy -24).
  Menu rows **Stick dead zone** (0-30 %, both sticks of controller 1) and **Rumble** (motor range 100 % / 0),
  written to `GCPadNew.ini` on close (checked on Windows: 10 % / off saved). **Android disc import and the
  branded app** (`recompcore-skies-android-app.patch`, `BRAND=1 tools/android-ws-build.sh`: "Skies Recomp",
  `com.skiesofarcadia.recomp`, first-run picker -> extract + `skies-widescreen.dolpatch` + SHA-256 check in
  JNI, Play / Continue, pad A opens the picker, re-patch on update) installed on the Retroid with the ws16
  dev app: RVZ import ~11 s, Continue boots the saved battle at 1.01x. Windows play build + release on
  ws16 (release from the original DOL: 1.000x, 59.94 fps). WSL ws16 module + EGL runtime: Baltor 1.00x.
  App icon (2026-10-11 late night): Reckoner's logo (`release/skies-logo.png`) -> `tools/make-skies-icon.sh`
  -> `release/android-icon/` + `release/skies.ico`; the Windows release embeds it
  (`-DMODERNGEKKO_WINDOWS_ICON`, checked: both exes carry it); the Android icon APK is built.
  Spirit gauge digit: matches the 4:3 original in captures; the user dropped it (10-11).
  The user looked at the ws16 ship battle HUD on Windows (Baltor 17b) and approved it: "looks much cleaner".
- **Ship battle HUD in widescreen (ws15 `2d7b1f1d` / `fd91cf07`, 2026-10-11 night, user request):** left
  column (Spirit gauge, ring, lists, panels) at the left edge, command grid in the top right corner, the
  enemy's HP plate below it, plates' damage popups with their plates (NOTES *Ship battle HUD*). Also fixed:
  battle speed / Invincible ran patch code inside the retrace polling loop, so states made there (ws12-ws14)
  hang once the patch code moves (`tools/statecheck.py`; NOTES *Known issues*). Windows play build and
  release on ws15, run from the original DOL; 4:3 identical (state 02); speed / Invincible / rewards
  re-checked on ground and ship battles. Android: not built.
- **Ship battles (ws14, 2026-10-11 night):** battle speed, Invincible and the rewards reach
  ship-versus-ship battles (field mode 6 on maps 500-599). Savestates `17-ship-battle-baltor` /
  `17b-ship-battle-baltor-commands` (no save of ours gets there: `tools/shipbattle-state-win.sh` and the
  debug spec `patches/game/shipdebug.json` give the party the Little Jack and start `me500a`). Windows:
  1x-4x = 30 / 59 / 92 / 116 game frames per second (3x / 4x need the frontend's emulated-CPU overclock),
  Invincible holds the ship at 10000, won normally with 410 EXP / 300 gold (x2 at 200%); auto-save skips
  the ship battle and works after it (the game leaves the data pointer set, the old test would have
  blocked it for good). Release pinned to ws14 (`7a73cd6e` / `6fa0cdf6`) and run from the original DOL.
  Android: not built yet. NOTES *The player build*, decomp `docs/qol_cheats.md` *Ship battles*.
- **Game build:** DOL **ws14** (`build/game/GEAE8P-ws14`, SHA-1 `7a73cd6e`, SHA-256 `6fa0cdf6`; ws13 below plus
  ship battles). ws13 (`fa7b1af1`, `fe464ed3`):
  widescreen with the 4:3 setting identical to the original (ws10), camera control (ws11), QOL options
  and Invincible (ws12), the world map altitude gauge at the left edge like the original 4:3 (ws13; the
  -94 inset of ws9-ws12 was the user's earlier call, reversed once they saw 4:3 hugs the edge;
  `WS_VERSION` 7). Spec `patches/game/widescreen.json` (`widescreen.c`, `fullwidth.S`, `camera.c`, `qol.c`,
  `qol.S`). Committed: recomp `45a05fb` (ws13, the menu fix, docs), decomp `929ea72`.
- **Save states and auto-save (2026-10-11, committed `3cc0959`):** menu rows Save state (slot 1-5, A saves),
  Load state (newest auto-save or slot 1-5, A twice loads), Auto-save (`[Skies] AutoSave`, default every
  10 min; saved at the next quiet moment: field / world map, no ship battle, memory card idle 10 s; newest
  three kept). Files in `StateSaves` with ModernGekko's names (`state-skies-slotN.sav`,
  `recovery-skies-<date>-<time>.sav`), so the launcher lists them; its new **Continue** button boots the
  newest. Z+Start now opens the menu in battle too (before, Start had to come within 150 ms of Z outside
  the camera's places). NOTES *Save states and auto-save*; test `tools/states-test-win.sh`.
- **Settings:** `[Skies] Widescreen`, `FrameMode`, `CameraControl`, `Encounters`, `Exp`, `Gold`, `MagicExp`,
  `BattleSpeed`, `Invincible`, `AutoSave` in `Dolphin.ini`, plus Dolphin's resolution, MSAA, V-Sync, Show FPS, volume.
  All live, all in the in-game menu (hold Z, press Start), all settable by `drive.py config:`.
- **Menu pulse fixed (2026-10-11):** on the Retroid (1080p, where the 15-row menu has to shrink) the menu
  switched between two sizes every few frames. Cause (logged on the device with `__android_log_print`): the
  shrink factor was continuous but ImGui rounds font sizes to whole pixels, so the font flipped 35 <-> 34 px,
  the measured height jumped with it and the next frame's factor swung back. A first fix (measure the height
  from this frame's layout instead of `GetWindowHeight()`, which lags a frame) wasn't enough on its own. The
  fix in `SkiesMenu.cpp` (`recompcore-skies-settings.patch`): when shrinking, the font size is a whole
  pixel count, the height is this frame's layout, and a size found too tall is capped for that viewport
  height (`s_px_cap`). Device: settles at 34 px after one frame, the highlight bar steady over 242 recorded
  frames.
- **Windows:** `tools\play-ws.bat` runs ws13 with `build\modules-llvm-pgo-ws13`; dev runner and branded
  release rebuilt with the menu fix; the release (`tools\build-release-win.bat`, pinned to ws13, patch
  `release/skies-widescreen.dolpatch`) was run end to end from the original DOL (patched to ws13, gauge at
  the edge, 59.9 fps at 1.00x). The menu itself wasn't looked at on Windows after the fix (Dolphin's
  screenshots leave out the OSD, and the desktop isn't captured). The user's old release game root in
  `%LOCALAPPDATA%\SkiesRecomp\games\GEAE8P` holds ws10, which the new runner refuses: re-import the disc.
- **Retroid:** `build/android/apk/skies-ws13c-states-2026-10-11.apk` (save states; the user OK'd tests
  and installs on all platforms while away) + the ws13 DOL installed; before it `skies-ws13b-2026-10-11.apk`; previous
  DOLs backed up on the device in `files/Backup-2026-10-10/` (`main.dol.7ad26f22` ws12, `.54afdf92` ws11,
  `.7cb9de68` ws10, `.329c631f` ws9). Checked there: gauge at the edge (59.85 fps at 1.005x), menu steady and
  15 rows fit, D-pad selection; earlier builds: camera, battle speed (2x full speed; 3x / 4x step down to
  2x), Invincible.
- **WSL:** runtime in `lib/ModernGekko/build-egl` current (menu fix, save states); newest WSL module is
  ws10 (`build/modules-llvm-ws10`).
- **Invincible on screen (2026-10-11, user report "HP / MP didn't fill"):** checked on Windows in the deck
  battle state: turned on while commands are chosen, the unit records are refilled at once (and the Spirit
  gauge, which is drawn live), but the party status box keeps the HP / MP it showed when the turn began until
  the round plays out; after one round it reads full. Writing the battle HUD panel (`lbl_80302AB0[slot]`)
  didn't change the box during command input either, so an immediate refresh was dropped (a `ws14` test
  build exists in `build/game/GEAE8P-ws14` / `build\modules-llvm-pgo-ws14`, reverted, not used). Knocked-out
  members (HP 0) are deliberately not revived. Asked the user whether theirs stayed low after a round or a
  member was down. Decomp `docs/qol_cheats.md`.

**Not checked yet:** L3 / R3 on a real controller (the user, on the Retroid), MP refill after casting with
Invincible on, music at 2x-4x by ear,
ship battles other than Baltor's, L3 / R3 inside a ship battle on a real controller, the menu
on Windows after the pulse fix, the user's answer on Invincible, the launcher's Continue button clicked by
hand (its logic and `--load-state` were checked; for a card save the game-side auto-load was checked
with `AutoLoadCard` set by hand), the card auto-load on Android, auto-save in the user's own play on
the Retroid.

**Continue (10-11, user request):** both launchers pick the newer of the newest save state and the
newest memory card save, and a card save now loads by itself (the user chose that over "load it on
the title screen"): the runtime drives the title / load screens by their task states (NOTES *Memory
card auto-load*). Windows: cold boot to the save point in ~33 s with no input. Built: Windows release,
Android `skies-recomp-ws16-autoload-2026-10-11.apk` (installed on the Retroid 22:15, user OK). The launcher's click path
(Continue writing `AutoLoadCard`) isn't clicked by hand yet. **Open (tabled by the user 10-11):** the battle
freeze after mid-round speed changes (NOTES *Known issues*).

**Next, in order:** commit the save import (2026-10-12; patches already regenerated and checked); the
user tries Import a save through the desktop file dialog; the user tries Continue from a card save on the Retroid (APK installed 10-11);
the battle freeze after mid-round speed changes is tabled (user, 10-11; NOTES *Known issues*: split the
overclock from the interpolation, then the actual wait); (done 10-11: icon / banner everywhere on the Retroid after restarting Radikal;
menu and controls "work great now"; the shader compile at every start, fixed and verified on the
Retroid; the automatic controller-profile fix, verified from the SDL backup) a clean
Android build without the input log; whether the Retroid's launcher shows the new icon; the user's
click-through of the desktop launcher (change a setting, check the ini; A- / A+); the WSL runtime
(`build-egl`) with the new menu; dead zone / rumble on the Retroid's own pad; unlocked frame rate (design note under *Design* below); optionally reviving
knocked-out members with Invincible (user to decide); the recomp's own wording for save / load
messages (Dolphin's show file names).

## Before (2026-10-10 night): the switches this replaced

| Setting then | Where | Live? |
|---|---|---|
| Widescreen | which DOL + module is run (`GEAE8P-ws9` vs the original) | no, a different build |
| 60 fps | Android: empty `FrameInterpolation.on` in the app's user dir; Windows: `frame_interpolation=true` in `build\user-ws\config.ini` -> env `RECOMP_FRAME_INTERPOLATION` | read once at boot (`FrameInterpolator::detail::ReadEnabled`) |
| Dual core | `CPUThread = True` in `GameSettings/GEAE8P.ini` | boot only (Dolphin) |
| Picture aspect | `AspectRatio = 5` (custom 16:9) in `GFX.ini`; with Match screen (2026-10-17) the custom width / height are the window's, current layer only | live (Dolphin) |

The old switches still work as fallbacks when `FrameMode` isn't set (first boot migrates them).

---

History (newest last within each status block):

## Status (2026-10-10, late night): step 1 on Windows too; 4:3 identical to the unpatched game (ws10 DOL); Android APK built, not installed

- **Windows:** runtime rebuilt with the backend (`moderngekko-run.exe`; previous exe
  `moderngekko-run-1010-presettings.exe`). Checked on a copy of the play user dir (3x, dual core,
  interpolation on): first boot wrote `Widescreen = 1`, `FrameMode = 1`; live `Widescreen` 1 -> 0 -> 1
  and `FrameMode` 1 -> 0 -> 1 in the opening cutscene and in battle (state 02), full speed, 60 -> 30 -> 60
  fps, no errors. The ini gets a changed value when the backend saves (first boot, a picture change) or at
  a clean exit; a settings screen should call `Config::Save` itself after a change.
- **4:3 A/B against the unpatched DOL done** (NOTES *Widescreen 4:3 setting*): ws9's 4:3 had the SP gauge
  8 px high and invisible edge differences; both fixed in the game patch, now DOL `7cb9de68`
  (`build/game/GEAE8P-ws10`, `WS_VERSION` 6). 4:3 identical to the unpatched game on every picture of
  state 02, 16:9 identical to ws9. Tool: `tools/dump-ab-win.sh`. The runtime applies the setting at once
  after a state load now (`SkiesSettings::OnStateLoaded`).
- **Play builds on ws10:** Windows `tools\play-ws.bat` (DOL + `build\modules-llvm-pgo-ws`; previous DOL
  `main.dol.329c631f`, its module in `build\modules-llvm-pgo-ws9`); WSL module `build/modules-llvm-ws10`
  (state in memory, no PGO; WSL runtime rebuilt).
- **Android: installed and checked on the Retroid** (HANDOFF-android.md top status: first boot, live switches
  in battle and on the ladder with replay slices running, ladder clean). Original note:
  `jni/Automation.cpp` got `set_config` (in `recompcore-skies-settings.patch`). Built: `build/android/apk/skies-ws10-settings-2026-10-10.apk`, which needs
  the ws10 DOL pushed with it (`build/game/GEAE8P-ws10/sys/main.dol`, SHA-1 `7cb9de68`; module and DOL must
  match). On the device: first boot should derive `FrameMode = 1` from `FrameInterpolation.on`; then a live
  switch in dual core while a replay slice runs (`ADB_AUTO=... tools/drive.py config:...`).
  Earlier runtime-only APK (ws9 DOL): `skies-ws9-settings-2026-10-10.apk`.

**2026-10-11: the in-game menu (step 3) works on WSL, Windows and the Retroid (installed, user OK).**
Hold **Z, then press Start** on controller 1 (any platform; keyboard too through the GC pad mapping).
Rows: Picture (16:9 / 4:3), Frame rate (30 / 60), V-Sync, Volume (0-100 in 10s), Show FPS, Close.
D-pad or stick to choose, Left/Right or A to change (Up/Down repeat while held, Left/Right only on
Volume: a repeating toggle flips back), B / Start / "Close" to close; changes are staged and
applied + saved (`Config::Save`) on close. The game keeps running and sees a released controller while the
menu is open; the buttons that closed it are held back until released. A lone Z reaches the game 150 ms
late (held back to catch the combo). Drawn with ImGui in the on-screen UI, sized to the picture height; the window fits its text (the font
shrinks if that's wider than 90% of the screen).
Code: `VideoCommon/SkiesMenu.cpp`, hooks in `GCPad.cpp` and `OnScreenUI.cpp` (all in
`recompcore-skies-settings.patch`). Checked: WSL (llvmpipe) by automation + X window capture (menu drawn,
frame rate changed and applied, lone Z tap doesn't open it, 60 fps after close); Windows release build
(Picture 4:3 -> 16:9 staged while open, applied on close, saved, 1.00x throughout). Dolphin's screenshots
(`drive.py shot:`) don't include the on-screen UI; on WSL `import -window $(xdotool search --name Skies)`
does; on Android `adb exec-out screencap -p` does. Android: installed
`build/android/apk/skies-ws10-menu2-2026-10-11.apk` (same DOL `7cb9de68`, module `jniLibs-ws10`; rebuilt
with gradle only). Checked on the Retroid (state 03, automation pad): Z+Start opens, stick/D-pad move, A
toggles, a 1 s Left on Picture changes it once, Left held on Volume repeats, close applies (4:3 pillarbox,
`Widescreen = 0` saved) and 16:9 restored afterwards. The first APK (`skies-ws10-menu-...`, fixed-width
window, Left/Right repeat on toggles) overflowed the box at 1080p and flipped toggles back on a long hold.
Speed on the device was 23-24% that session for an unrelated reason: the kernel fuel gauge read 1% (3.49 V,
"Not charging" on USB) and the low-SoC limiter isolated cores 4-7 (`cpu-isolate4..7` cur_state 1), even on a
fresh boot; the user's battery indicator disagreed. Recheck 1.00x once the gauge reads normally.
**2026-10-11 (later): Resolution and Anti-aliasing rows.** Resolution steps through the launcher's EFB
scales (1, 2, 3, 4, 6x); Anti-aliasing through the backend's MSAA modes (`g_backend_info.AAModes`). Both
apply on close (`GFX_EFB_SCALE`, `GFX_MSAA`) and are saved in `GFX.ini`. Desktop: the runner sets the
resolution from `config.ini` at boot, so the runtime now calls `GraphicsSettings::
on_internal_resolution_changed` from a config callback and `moderngekko_run` writes the new
`resolution=` back. The launcher has an Anti-aliasing choice (`GFX.ini [Settings] MSAA`). Checked on the
Windows release build (deck battle, `16-deck-battle-ws10`): 1x -> 2x + 2x MSAA from the menu, picture
853x480 -> 1707x960, `config.ini` 1280x720, `GFX.ini` InternalResolution 2 / MSAA 2, 59.9 fps 1.00x;
both kept after a reboot. Android: `build/android/apk/skies-ws10-menu3-2026-10-11.apk` built, not
installed (there GFX.ini is the only copy). Not in the menu: controls (the launcher's controller profile).

**2026-10-11 (evening): camera control, DOL ws11 (`build/game/GEAE8P-ws11`, sha1 `54afdf92`, sha256
`1d628342`).** Hold Z: C-stick up/down tilts the camera, R / L triggers zoom out / in; on the world map
C-stick left/right also turns the camera around the ship (it eases back behind when let go). A Z tap
still reaches the game on release (minimap, world-map menu); Z+Start is still the settings menu. Towns
and dungeons keep the camera between the game's own camera and Vyse (no camera collision in the game);
the world map gets a wider range. Game side `patches/game/camera.c` (decomp `docs/camera.md`), input in
`VideoCommon/SkiesMenu.cpp` (namespace `camera`), setting `[Skies] CameraControl` (menu row Camera).
Windows: module `build\modules-llvm-pgo-ws11`, release patch `release/skies-widescreen.dolpatch`
regenerated and `build-release-win.bat` pinned to ws11; checked with `tools/camera-test-win.sh` (world
map, ship cabin, first person untouched) and end to end with the release runner (original DOL patched
to ws11, camera works, 1.00x). Android: `NAME=ws11 tools/android-ws-build.sh` -> `build/android/apk/skies-ws11-2026-10-11.apk`,
**installed on the Retroid with the ws11 DOL (user OK)**; the ws10 DOL is backed up on the device as
`files/Backup-2026-10-10/main.dol.7cb9de68`. Checked there: world map (tilt, zoom, orbit and its
return, Z tap -> the game's menu) and the ship cabin (tilt within the room, turn, zoom in, Z tap ->
minimap), 59.9 fps at 1.00x throughout; the menu shows the Camera row.

**2026-10-11 (late night): QOL options in the menu, DOL ws12 (`build/game/GEAE8P-ws12`, sha1 `7ad26f22`,
sha256 `3047bc0b`; it now includes Invincible, below).** Menu rows Encounters (Off, 1/4, 1/2, Normal, 2x, 4x), EXP / Gold / Magic EXP (Off,
1/2, Normal, 2x, 3x, 4x) and Battle speed (Normal, 2x, 3x, 4x); a Z tap in battle cycles the battle
speed (on-screen message, saved). `[Skies] Encounters / Exp / Gold / MagicExp / BattleSpeed`. Game side
`patches/game/qol.c` + `qol.S` (whole-call hooks: the encounter roll 0x800C207C, the ground-battle
member count before the rewards 0x8006F5B8, ship battle results 0x8015C6B4, the end-of-frame
retrace waits 0x801DC498 / 0x801DC4D8), input block `qol_input` 0x80002EA0 written every poll by
`VideoCommon/SkiesMenu.cpp` (namespace `qol`, which also turns frame interpolation off while a battle
runs fast). Checked on Windows with `tools/qol-test-win.sh` (ws12 module `build\modules-llvm-pgo-ws12`):
first battle Vyse EXP / gold / magic EXP 3 / 10 / 2+4 at Normal and 12 / 40 / 8+16 at 4x; world map
first battle after 29 s at 1/4, 20 s Normal, 8 s 4x, none in 60 s Off; deck battle 30 / 61 / 92 / 122
game frames per second at Normal / 2x / 3x / 4x via Z taps (and back to Normal), emulation 1.00x and
59.9 fps throughout, pictures fine. Release patch and `build-release-win.bat` pinned to ws12. Android:
`build/android/apk/skies-ws12-2026-10-11.apk` built, not installed (needs the ws12 DOL pushed with it).

**Invincible (L3 / R3, menu row Invincible, `[Skies] Invincible`).** In battle the party's HP and MP
(members still standing) and the Spirit gauge are refilled every frame (so magic and S-moves cost
nothing) and damage to the party is zeroed: `qol_damage` wraps the damage function `fn_8002DC14`
(its 13 calls, plus entry 0 of the action handler table `lbl_802DBB10`, rewritten with gamepatch's new
`redirect_pointers`). Turning it off leaves the stats as they are. L3 / R3 aren't GameCube buttons:
the runtime reads them from controller 1's real device ("Thumb L/R" on desktop, "Button L3/R3" on
Android) and shows a message. Checked on Windows (`qol-test-win.sh invincible`): first battle HP
374/420 -> 420/420 and Spirit 4/8 -> 8/8 at once; deck battle with it off Vyse drops to 0, with it on
he stays at 589/589 all battle. Not checked yet: the stick clicks themselves (automation presses the
emulated pad only), MP after casting.

**On the Retroid (2026-10-11, user OK): `skies-ws12b-2026-10-11.apk` + ws12 DOL `7ad26f22` installed**
(ws11 DOL backed up on the device as `Backup-2026-10-10/main.dol.54afdf92`). The 15-row menu fits (the
menu now sizes itself from its measured height). Battle speed in the deck battle: 30 / 60 game frames
per second at Normal / 2x at 1.00x, but 3x / 4x only reach ~79 / 75 with the emulation at 0.85x /
0.65x: the Retroid tops out near 2.5x. So the runtime now steps a fast battle down (to 2x at the
lowest) when the emulation stays under 95% for 1.5 s, with a message, until the battle ends: asked
for 4x it settled at 2x, 60 game frames per second, 1.00x. Invincible in the deck battle: HP 589/589
all battle. `drive.py peek:` now works over adb too.

**Later the same night:** the widescreen patch now reaches the player's own `main.dol` through the
launcher (whole-file DOL patch), the launcher has the Skies options, and a branded Windows release build
runs end to end from the USA RVZ (section *Launcher and options* below).

(Superseded by *Current state* at the top: the overlay, V-Sync, MSAA and volume are done since; Android
disc import and packaging are still open; disc 2 was a mistake, the game is one disc.)

**2026-10-11 (afternoon / evening): ws13, the menu pulse, Invincible's status box.**
- Altitude gauge: the user saw that in 4:3 the world map altitude gauge hugs the left edge, so the -94
  anchor they had asked for (as far in as the compass) was wrong; `WS_HUD_ANCHORS` now -100 for
  `fn_8016B434`, `WS_VERSION` 6 -> 7 (the defaults changed, so states made before reset the data).
  DOL ws13 `fa7b1af1` / `fe464ed3`. Windows: `tools/module-llvm-win.sh llvm-pgo-ws13` (5 min), 16:9 and 4:3
  screenshots from `10-worldmap-android` both ~2 px from the edge; release patch regenerated, bat pinned,
  release reconfigured / rebuilt and run from the original DOL in a fresh game root
  (`build\game\reltest-ws13`): patched to ws13, 59.9 fps at 1.00x. Android: `NAME=ws13
  tools/android-ws-build.sh` (4.5 min) -> `skies-ws13-2026-10-11.apk`, installed with the DOL (user OK; ws12
  DOL backed up as `main.dol.7ad26f22`): gauge at the edge, 59.85 fps at 1.005x.
- Menu pulse (user report): measured from `adb shell screenrecord` (the highlight bar's extents per frame:
  two sizes, 500-1419 vs 514-1405, every ~5 frames), then logged with `__android_log_print` / `adb logcat
  -s`: `u` drifted 1.17-1.19 and the font flipped 35 / 34 px. First fix (this frame's layout height) built
  and installed but didn't help; second fix (whole-pixel font sizes, per-viewport cap) settled at 34 px.
  APK `skies-ws13b-2026-10-11.apk` (gradle only, `-PrecompJniLibs=build/android/jniLibs-ws13`; the native
  part rebuilds incrementally in ~5 s), installed (user OK). Patch section regenerated; `git apply --check -R`
  passes on the vendor tree. Windows `tools\build-win.bat build` and `build-release-win.bat build` rebuilt.
  The automation opens the menu with `press:z=1 wait:0.5 press:z=1,start=1 wait:0.3 release` (a `tap:start`
  during `press:z` lets go of Z, which then reaches the game as a Z tap); `adb exec-out screencap -p` shows
  the OSD, Dolphin's `shot:` doesn't.
- Invincible's status box (user report: "the bar at the top filled up but neither party member's HP or MP"):
  Windows, deck battle state, Invincible off until Vyse is at 235/589 and Aika knocked out, then on: memory
  589/589 at once, the box still 235/589 for the rest of the command input, 589/589 after one round. RAM
  dumps (24 x 1 MB `peek`s; one 24 MB peek fails) found the battle HUD panel `lbl_80302AB0[slot]` (from
  `fn_8002969C`: shown HP +0x20 float, target +0x28, max +0x2C; the game's own sync is `fn_8002942C(slot,
  2)`, and `BattleHudDraw` follows unit HP only in panel states 3-8). Poking +0x20 / +0x28 changed neither
  the digits nor the bar during command input, so a `qol_hud_hp` sync (ws14) was built, tested, and reverted.
  The command-input box isn't redrawn from these fields; what it is drawn from wasn't found (not
  `fn_801B0838`, which prints a field status box; `fn_80022F74` only lays out sprites). Aika stays down:
  knocked-out members are skipped on purpose.

**2026-10-11 (late evening, user away with tests OK on every platform): save states and auto-save.**
- Design agreed with the user: menu save / load, timed auto-save, Continue in the launcher.
- Found on the way: Dolphin keeps the memory card's contents out of states unless a movie runs
  (`CEXIMemoryCard::DoState`), so loading never rolls back card saves (an earlier worry); but a state
  doesn't hold a card transfer in progress either, hence the 10 s card-idle rule
  (`SecondsSinceLastAccess`, counted in `TransferByte` / `DMARead` / `DMAWrite`).
- First version used Dolphin's numbered slots (`.s01`-`.s08`); switched to ModernGekko's named `.sav`
  layout because the launcher lists only `.sav` (and deliberately not numbered slots); `List` now skips
  Dolphin's `lastState.sav` (the "undo save" file it leaves when a save overwrites one).
- Saves / loads go through `Core::QueueHostJob` (host thread, `RunOnCPUThread` pauses at a safe point).
  The first Windows run seemed to do nothing: the save in battle never happened because the menu didn't
  open there (Start must have come within 150 ms of Z; fixed), and the auto-save never ran because the
  menu stayed open after a load of an empty slot (auto-save polls only while it's closed).
- Results: see NOTES *Save states and auto-save*. Retroid auto-save: one second at 0.935 / 0.974 / 0.986x
  per save, caught up the next second. The device's StateSaves was cleaned and AutoSave reset after the
  tests (the user's ini has no AutoSave key, so the default 10 min applies); the app was left stopped.

## Status (2026-10-10 night): step 1 built and verified on WSL; Windows and Android not yet built (superseded)

Done: `patches/recompcore-skies-settings.patch` (new `Core/SkiesSettings`, `Core/Config/SkiesSettings`, hooks
in `Core.cpp`, `State.cpp`, `BPStructs.cpp`, `FrameInterpolator.cpp/.h`) and the regenerated
`patches/moderngekko-local.patch` (automation `set_config`, `tools/drive.py config:`). Settings: `[Skies]`
`Widescreen` (1 = 16:9, 0 = 4:3) and `FrameMode` (0 = 30, 1 = 60, 2 reserved) in the user dir's
`Config/Dolphin.ini`; -1 / absent = first boot derives them (DOL default 16:9; the frame mode from
`RECOMP_FRAME_INTERPOLATION` / `FrameInterpolation.on`) and saves. WSL ws9 module built:
`build/modules-llvm-ws9` (DOL `c608dea9`, no PGO), so WSL can run the current DOL again.

Verified (WSL, llvmpipe 1x, state 01, `build/user-settings-test`, `build/auto-settings/{wide,std,wide2}.png`):
`ws_config.display_aspect` 1.778 -> 0 -> 1.778 on `config:Dolphin.Skies.Widescreen=0/1` with the picture
following (640x527 4:3 vs 853x480 16:9, correct framing in both); `FrameMode=1/0` logs
`[interp] frame interpolation on/off (setting)` and the game stays at 1.00x; the ini gets the keys at boot.

Not done: Windows tree sync + runtime rebuild (copy the ten patched files + `tools/automation_protocol.*`,
`src/runtime/dolphin_runtime.cpp`, `tools/drive.py` to `C:\dev\SkiesRecomp`, `tools\build-win.bat build`),
Android runtime rebuild (`Automation.cpp` on Android has its own command parser: add `set_config` there
too), a 4:3-vs-original-DOL A/B (battle-entry shatter, HUD edges) and a check that switching modes while
a replay job is mid-slice in dual core is clean (WSL test was single core). The interpolation switch is
applied at the game's XFB copy on the GPU thread, so it never lands mid-frame.

## Launcher and options, after looking at the N64 recomps (2026-10-10, late night)

**What Goemon64Recomp, HybridHeavenRecomp and Quest64-Recomp have** (all the same N64Recomp/RecompUI
template, RmlUi drawn by RT64; `assets/config_menu.rml`, `assets/config_menu/*.rml`, `src/game/config.cpp`):
- Tabs: General, Controls, Graphics, Sound, Mods, plus per game Cheats / Saves / Debug, and on Android
  Touch and GPU Driver. Tab indices never shift (new tabs go last).
- Graphics: Resolution (Original, 2x-8x, Auto), Downsampling, Aspect Ratio (Original / Expand), Window
  Mode, Framerate (Original / Display / Manual + value), MSAA (None / 2x / 4x; Android default None,
  measured on the Retroid), Framebuffer Effects, V-Sync (HH), HUD Placement (Q64: Original / 16:9 /
  Expand). Per-device defaults (Steam Deck fullscreen, Android lower resolution and no MSAA).
- General: rumble strength, stick deadzone, background input, analog camera + invert + sensitivity,
  autosave (with a "Saved" toast), game-specific gameplay toggles. Sound: master plus per-channel volume.
  Controls: rebinding per input, reset to defaults, the menu button itself rebindable.
- Opened in game with Esc or a bound pad button; pad, keyboard and mouse navigation; Esc closes;
  graphics changes staged and confirmed (Apply / Discard) on close; four JSON files written through a
  backup file on close, missing keys fall back to defaults.
- Launcher: title + Select ROM (until a valid one is registered) / Start Game, Controls, Settings, Mods,
  Exit, version text; native file dialog (SAF on Android); a hash check with specific error messages
  (HH names the required dump and its SHA-1); the ROM is remembered. No update check, one region only.

**What the Skies frontend already has:** ModernGekko's own launcher (`tools/moderngekko_launcher.cpp`,
ImGui over SDL, built as `ModernGekko`): Browse for ISO / WBFS / RVZ -> extract with progress -> apply the
bundled DOL patch -> require disc ID + DOL SHA-256 -> Play; graphics backend, fullscreen, internal
resolution, FPS in title, controller profile, savestate to start from, netplay. Branding is CMake options
(`MODERNGEKKO_FRONTEND_NAME`, `..._REQUIRED_DISC_ID`, `..._REQUIRED_DOL_SHA256`, `..._DOL_PATCH_MANIFEST`,
`..._GAME_SETTINGS_OVERRIDE`, output names). So the N64-style flow exists; Skies needed two things:
- **The widescreen patch on the player's own DOL** (done, this session): the bundled patch format could
  only replace words in existing code, and the widescreen patch adds sections. `tools/dol_patch.cpp`
  now also takes a whole-file patch (`moderngekko-dol-file-patch 1`: input and output SHA-256, size,
  hex runs), made by `tools/mkdolpatch.py ORIGINAL PATCHED OUT` (`release/skies-widescreen.dolpatch`,
  19 KB: 535 runs, 7.5 KB of our code, data and call sites). Applies only to the USA DOL, checks its
  output, leaves an already patched DOL alone. Tested: original -> ws10 byte for byte, rerun unchanged,
  ws9 DOL refused, word manifests still work.
- **Skies options in the launcher** (done): "Picture" (Widescreen 16:9 / Original 4:3) and "Frame rate"
  (30 fps / 60 fps interpolated), shown for disc GEAE8P, written to `[Skies]` in the user dir's
  `Config/Dolphin.ini`.
- Release build: `tools\build-release-win.bat configure|build|package MODULE` (separate
  `lib\ModernGekko\build-release`: the release runner patches the DOL of any game root it starts).
  The runner finds `gGEAE8P_recomp.dll` beside itself or in `<user>/StaticRecompModules`. Outputs:
  `SkiesRecomp.exe` (launcher), `skies-run.exe`, `Sys/` (with `GameSettings/GEAE8P.ini` from
  `release/GEAE8P.ini`: idle skip + dual core), `patches/skies-widescreen.dolpatch`; user dir
  `%LOCALAPPDATA%\SkiesRecomp`.
- **Tested end to end on Windows (2026-10-10, late night):** `SkiesRecomp.exe --extract build\disc\skies.rvz`
  (the launcher's own extraction path, headless) extracted the USA RVZ in 18 s to
  `%LOCALAPPDATA%\SkiesRecomp\games\GEAE8P`, patched its DOL to ws10 (SHA-1 `7cb9de68`) and validated
  it; `skies-run.exe` booted it with the bundled module at 1.00x in 16:9; with `[Skies] Widescreen = 0`,
  `FrameMode = 1` (what the launcher writes) it booted in 4:3 at 59.9 fps. The launcher window showed the
  two options with the stored values. Found and fixed on the way: the runtime never created `Config/` in
  a fresh user dir, so no setting was ever saved there (`UICommon::CreateDirectories()` at startup).
  Not done: the launcher's Browse and Play buttons by hand (GUI; after the file dialog it's the same
  code as `--extract`), Linux and Android packaging. (Disc 2 was listed here too: Legends is a single
  disc, unlike the Dreamcast original; corrected 2026-10-11.)
  Screenshots of the launcher from WSL: a plain `PrintWindow` gives black and a screen copy catches other
  windows; `tools/capture-window-win.ps1` (DPI-aware, `PrintWindow` flag 3) works (2026-10-11).

**Options worth adding next (mapped to what already exists):** V-Sync (`GFX.ini [Hardware] VSync`),
MSAA (`[Settings] MSAA`), volume (`Dolphin.ini [DSP] Volume`; per-channel needs game-side work),
background input (`[Input] BackgroundInput`, already in the runtime), deadzone and rumble (controller
profile). The in-game overlay (step 3) should show the same options, staged and applied on close like
the N64 menus, and open from a bindable pad button. Android: same keys; a hash-checked disc import
before native boot like the N64 Android launchers.

## What the research found

- **Widescreen does not need a DOL swap.** `patches/game/widescreen.c` keeps `ws_config` at a
  fixed address (`0x80002A00`: magic `0x57534346` 'WSCF', `version`, then `float display_aspect`
  at `+8`) so "a frontend can change the target aspect at runtime with a memory write;
  `ws_frame_end` applies it every frame". `display_aspect = 0` is the original 4:3 (HUD anchors,
  full-width effects and the unwide transitions all follow `factor`). So the ws9 DOL is the only
  DOL to ship, and 4:3 is a 4-byte write (on the CPU thread: `Core::RunOnCPUThread`).
- **Interpolation can be toggled at a frame boundary.** `s_enabled` is a cached `s8`; the frame
  machinery resets cleanly in `FrameInterpolator::Shutdown()`. Turning it off = set `s_enabled = 0`
  and `Shutdown()` right after the frame end (`OnXFBCopy` path); turning it on = `s_enabled = 1`
  (recording starts with the next frame). Dual core can stay on in both modes.
- **Dolphin's config system reaches every platform.** A `[Skies]` section in `Dolphin.ini`
  (`Config::Info<float> SKIES_DISPLAY_ASPECT`, `Config::Info<int> SKIES_FRAME_MODE`) is readable
  from C++ everywhere and from the Android settings UI with no JNI work: `BooleanSetting.kt` /
  `IntSetting.kt` entries are just (file, section, key, default) and `NativeConfig.cpp` gets/sets
  arbitrary locations. `Config::AddConfigChangedCallback` fires on every change, including from the
  Android settings activity while the game runs.
- **Android already has an in-game Settings entry** (`EmulationActivity` `MENU_ACTION_SETTINGS`
  -> `SettingsActivity.launch(this, MenuTag.SETTINGS)`), so a "Skies of Arcadia" page added to
  `SettingsFragmentPresenter.addTopLevelSettings` is reachable from the game's pause menu.
- **Desktop has no UI at all** (`moderngekko_run.cpp` is window + SDL pads; Dolphin's ImGui
  `OnScreenUI` is compiled in and draws the OSD/debug text, with `SetKey`/`SetMousePos` plumbing
  that nothing feeds yet).

## Design (three steps, each shippable)

Status: step 1 done (2026-10-10), step 3 done (2026-10-11, and grown into the full menu), step 2 not
needed (the overlay works on Android too).

1. **Backend in the runtime (all platforms).** New `VideoCommon/SkiesSettings.{h,cpp}` (or
   under Core): declares the two `[Skies]` infos, registers a config-changed callback, and applies:
   - aspect: if RAM at `0x80002A00` holds 'WSCF', write `display_aspect` (16/9 or 0) at `+8` via
     `RunOnCPUThread`; also set `GFX_ASPECT_RATIO` (custom 16:9 vs 4:3) so the window letterboxes
     to match. Re-apply on boot and after a savestate load (states restore RAM, which carries the
     old value; `ws_check` keeps the versioned struct valid).
   - frame mode: 0 = 30 fps (interpolation off), 1 = 60 fps (interpolation on). Replaces the
     `FrameInterpolation.on` file and the `frame_interpolation` env on first run (migrate: if the
     file/env is present and the key is unset, write the key). Applied at the next frame end.
   - unlocked (later, see below).
   Patch goes into `patches/recompcore-frame-interpolation.patch` (or a new
   `recompcore-skies-settings.patch`), Windows tree synced by hand as usual.
2. **Android page.** `MenuTag.SKIES`, `addSkiesSettings`: a SingleChoice "Picture" (Widescreen
   16:9 / Original 4:3) and a SingleChoice "Frame rate" (30 / 60), plus strings. Reachable from
   the main settings and the in-game menu. No JNI changes.
3. **Desktop + parity: an in-game overlay.** A small ImGui window in `OnScreenUI` (opened by a pad
   combo or a key), navigated with the pad (`Pad::GetStatus(0)`), writing the same config keys.
   Works on Android too, which gives one screen on all three platforms; the Android page from
   step 2 stays as the no-controller fallback.

Scope note on **unlocked framerate**: the game's logic runs at 30 Hz; interpolation makes one
in-between per real frame, so 60 is the ceiling of the current replay design. "Unlocked" means
either N in-betweens per frame for 90/120 Hz panels (replay cost scales with N; the Retroid's
headroom after one in-between is ~10-28%, so 120 Hz is out on it) or a game patch that runs the
logic at the display rate, which is a separate research project in the decomp. Plan the setting
as a third enum value from the start so the config doesn't change later.

## Order of work (revised for the shipping goal, 2026-10-10)

The user wants to ship a port where players supply their own disc. That changes two things:
the overlay (step 3) comes before the Android page (step 2), because a shipped app hides
Dolphin's settings UI and the overlay is the one screen that works on all three platforms; and
the widescreen patch should be applied by the runtime to the player's own `main.dol` at boot
(same patch data, in memory) instead of shipping a pre-patched DOL, which also makes the
"one DOL, live aspect switch" design the natural one. So: step 1 (backend), then runtime patch
application, then step 3 (overlay); step 2 only as a dev convenience. (Done in that order; the runtime
patch became the launcher's whole-file DOL patch of the player's own `main.dol`.) Test states: 01 (cutscene),
15 (ladder, interpolation stress), and a 4:3 check of the battle-entry transition and HUD after
a live switch.

Other shipping work, separate from settings: first-run disc import with hash check (RVZ boots;
only savestates differ from the folder), rebranded Android app (package name, no game list,
boots the module directly, GameSettings ini + module in the APK), a desktop launcher with the
same first-run flow, GPL-3 source release (ModernGekko, Dolphin). (Desktop launcher with the first-run
flow: done, *Launcher and options*; the rest open.)
