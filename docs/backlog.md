# Backlog

Requests and ideas not scheduled yet. Move an item to a handoff when work on it starts.


## After the standalone test prerelease (v0.90.0-standalone.4) — 2026-11-18

- **#26 Dreamcast music stuck at one level** in some songs (Ramirez / Valua scenes, the Dark Rift's end): a
  constant -9830 for over a second, then a slow drift back; not clipping, not the reverb writing over samples
  (development notes, *Issues #26-#32*). Needs the reporter's save state; then the AICA DSP's state there.
  Both builds.
- **#28 fix to dev** (`recompcore-tz-fallback.patch`, ordered before the aurora patches) for a Dolphin-build
  release, if one ships before the standalone replaces it.
- **The `issues` branch** (#25 ShipHud, #19, #18, #24, #22; ws35 US only): its game patch for EU / JP, its 60 fps
  changes ported to `standalone/src/interp60.cpp`, then onto aurora-backend.
- **Patched Dawn** (MSAA 2x / 8x, the Windows swap chain fix): host the packages (`tools/build-dawn-package.sh`)
  or build them in CI.
- **By hand:** two controllers and remapping with a pad (standalone); the desktop launcher's Continue click.
- **Android:** touch controls (the standalone has none; phones without a pad can't play); custom GPU drivers
  aren't used by the standalone.
- The standalone's game clock is UTC (`std::time()`); the Dolphin build's was local time (card file dates).

## Dual audio (Japanese voices) — requested 2026-10-17

A player asked for dual audio: a choice of English or Japanese voices with the English text kept.
Skies has no fully voiced scenes; the voices are short clips (battle shouts, Special Move calls,
exclamations under story text boxes), so this swaps that set of clips.

- **Source:** the player's own Japanese disc, imported like the Dreamcast music (no game files in
  the build). Candidates: the Japanese GameCube release (*Eternal Arcadia Legends*) or the
  Dreamcast original (*Eternal Arcadia*).
- **To find out first:**
  1. How the US disc stores the voice clips (format, file names, banks), and whether the Japanese
     disc uses the same layout. Same layout → mostly a file redirect, like the DC music.
  2. Whether the clip lists match one to one (extra, missing or reordered lines in the Japanese
     release).
  3. Whether anything in the US game assumes English clip lengths (text box waits, lip flaps,
     battle timing).
- **Researched 2026-10-21 (not built; answers to the three questions):**
  1. **Layout: the same.** Every voice is in `sound/`, under the same 1799 names on the USA, European
     and Japanese GameCube discs: streams `B702NNNN` (battle lines), `KO00NN` (each spell's / S-move's
     voice and effects pre-mixed), `NNN` (victory lines), `f71000NN` (field voices), and the banks
     `STVnn` / `CRnnn` / `e6xNNN` (`.info` + `.samp`; decomp `docs/sound.md`).
  2. **Clip lists match**: every bank has the same sequence count except two where the Japanese one
     has an extra sound at the end (`e6h011`, `k8900001`), which the USA game never asks for.
  3. **Lengths**: 30 of the 66 `KO` streams differ in length (up to -1.6 / +1.2 s; the action waits
     for its stream, so a spell takes the Japanese length; check no animation is cut). Text boxes
     don't wait on clips.
- **How:** swap the **whole** `sound/` folder, not the voice files alone. The Japanese `.info` files
  of the banks that otherwise match (music, field effects) differ only in MusyX object IDs, numbered
  across the whole project, so a mix of USA and Japanese banks could collide; the music `.samp` data is
  byte-identical, so the whole folder changes nothing but the voices. The Japanese files mostly have
  other sizes (unlike the Dreamcast music's same-size conversion) and a save state holds the disc's
  file table and the banks loaded at boot (`tone`, `s1000000`, `b7000000`), so the setting needs a
  restart (as Language does) and save states kept per voice setting. The reverse (Japanese text,
  English voices) is the same swap; European text with Japanese voices works too, and the Japanese
  streams fit the port's 60 Hz (the European ones were made for 50 Hz, *European 50 Hz audio* in the
  development notes). Dreamcast voices: other formats, and Legends has voiced additions; not worth it.

## The European languages for the USA game — asked 2026-10-21 (issue #3 comment)

A player on the European disc saw the intro out of sync and asked whether the European release could
be imported only for its languages, like the Dreamcast music. The desync is fixed on branch `pal60`
(the European scripts' 50 Hz timing, its streams and enemy sound tempos; development notes *European
intro in sync with its music*, *European 50 Hz audio*), which makes the European disc play like the
USA one. Running the USA game with the European text would be much harder: the USA DOL has no code
for the European `.sot` text files or the language-suffixed graphics, and the European scripts
differ. Answer on #3 once the fix is released.

## Remember controllers by identity, not name — idea 2026-10-17

Controller 1 is saved as Dolphin's `SDL/<n>/<name>`, and a name can change with how the game was
started (Steam's virtual gamepad versus the pad itself; issue #1, HANDOFF-input *Steam Controller*).
Aurora's ports save each port's pad by SDL GUID + serial with a VID/PID fallback and only display the
name. Doing the same would mean our own field beside Dolphin's `Device` (the launcher and
`SkiesInput::Follow` resolve it to the current `SDL/<n>/<name>`). Not needed while the game follows
whichever pad is pressed and the launcher matches by label; worth it if more name reports come in.

## Retry the extracted game's rename on Windows — 2026-10-20

Once in about six Windows extractions (2026-10-20, issue #6 tests) the last step failed with "can't
publish extracted game: can't move the new game folder into place: Access is denied"; the same
extraction worked when run again. Likely a scanner (Defender) still holding the 1.4 GB of new files
when `ReplaceDirectory` renames the staging folder into `games/<id>`. A short retry loop (a few tries
over a few seconds) on that rename would hide it from players. Not reported by players yet.

## World map meter / compass keep the old shape after a Picture change in the world map menu — 2026-10-17

First reported as "widescreen stretches the world map's flight meter and compass"; re-diagnosed the
same night (development-notes *Known issues*): in normal play they match the unpatched game in every
picture. Only after changing Picture while the world map's own menu (Start) is open do they keep the
previous picture's shape, until that menu closes (they're replayed from display lists recorded before
the change). Cosmetic and transient; low priority.

## Romhacks that change the game's code — 2026-10-18

The release takes romhacks that change only data (HANDOFF-romhacks, development notes *Romhacks*);
one that changes code is refused at import, because the module runs the code it was compiled from (a
changed instruction in a chunk reached by a native call silently runs as the old code). Ways in, if
a wanted romhack needs it:

- **Known changes:** add its code change to `patches/game/<disc ID>.known` (`tools/dolknown.py`) and
  give the release an option of its own for what it does, as for hold-B.
- **A module per romhack:** build one from the romhacked DOL (about 8 minutes and 150 MB per platform,
  its own PGO run); modules are found by disc ID only today, so they'd need a key like the code hash.
- **The runtime:** check every chunk against the loaded DOL at boot and send calls into changed ones
  through the dispatcher (needs DolRecomp's direct calls to check a per-chunk flag); unknown cost.

## Windows: Smart App Control blocks the unsigned exes — found 2026-10-26

A fresh Windows 11 with Smart App Control on (state 1 under `HKLM\SYSTEM\CurrentControlSet\Control\CI\Policy`
`VerifiedAndReputablePolicyState`) refuses to start the unsigned game exe: "An Application Control policy has
blocked this file", from a mapped folder or a local copy alike. Seen in Windows Sandbox (build 26100.9711), whose
image now has it on; the 2026-10-13 sandbox test of the release zip didn't hit it. It applies to the released
Dolphin build's exes as much as the standalone's. Smart App Control is on only for some clean installs (it can't
be turned back on once off), so most players won't see it, but those who do get no way past it but turning it off
for good. The fix is code signing (a certificate with reputation, e.g. Azure Trusted Signing); the user's call
(cost). Meanwhile the release README could say what the message means.

Standalone sandbox test (`C:\dev\SkiesStandalone\sandbox`, `skies-sandbox-sa.wsb` + `in\test.ps1`, which turns
Smart App Control off with `CiTool --refresh` first): D3D11 draws the game; D3D12 crashes the sandbox's kernel
while Dawn enumerates adapters (a Windows GPU-PV bug); Vulkan's crash was Dawn calling a missing
`vkCreateSwapchainKHR`, fixed by `patches/dawn/0002` (a source-built Dawn only). Details in
`HANDOFF-feature-gaps.md`, *Items 8-10*.

## macOS release: signing, notarization — 2026-10-18

The macOS build (HANDOFF-macos) is built by CI (validate.yml `macos`, since 2026-10-19) and signed ad
hoc, so Gatekeeper asks once ("Open Anyway"). A smoother release needs an Apple Developer ID (paid
account), `codesign --options runtime` with a timestamp, and `notarytool` submission and stapling in
that job. The hardened runtime would also need checking against Dolphin's needs (no JIT is used by the recomp, but
the fallback interpreter path and MAP_JIT should be confirmed). A macOS PGO profile is a separate,
smaller step.

## Uncensored, what's left — requested 2026-10-20

The Uncensored setting (development notes, *Uncensored*; HANDOFF-uncensor) covers the art that is a
straight asset swap. Still to do, from the same Eternal Arcadia discs:

- **The drunk sailor model** Legends remade (`tc0102` in `A004AB05`).
- **Script work:** the drunk sailor in the Sailors' Island tavern (entry `0xc21` only on the
  Japanese disc; his English lines are still in `field/me004a.sct`), the Hamachou party (disc 2),
  Ramirez's blood (`ME131I.SCT`), and the text ("loqua" for rum in about 104 lines across 21 scripts,
  Salam, the Belleza and Ramirez lines). New English is needed for some; agree the scope first.
- **Android import of GDI zips:** Android takes only CHDs (a GDI's track files can't be opened beside
  a picked document); the TOSEC Eternal Arcadia dumps are zipped GDIs, so reading a GDI inside a zip
  would save players a chdman step.
- **The @barai edition** (v1.003, HDR-0119): not checked; its files fail the SHA-1 check if they
  differ, which leaves those files out.

## Super attacks out of sync with their sound at battle speed — reported 2026-10-21, done (ws27)

**Done 2026-10-21 (game patch ws27, `dev` `c18ddc4`):** spells and special attacks (party and enemies)
play at normal speed, always on (the user's choice); development notes *Spells and special attacks at
normal speed*. Ship battles followed in ws28 (2026-10-21, branch `smove`): their magic, crew specials,
enemy special attacks and the S-Cannon at normal speed too, and frame interpolation stays on for all of
these at speed (they showed at 30 fps before); development notes *Ship battles' spells and special
attacks*. Not seen live yet: the S-Cannon, crew specials and enemy special attacks in a ship battle.

Issue #7 (bobbleyaga) and on Reddit (r/recomps v0.60.0-beta thread, Unleaded_95): every super attack,
the party's and the enemies', plays its sound past the animation, or the game waits for the sound,
"worse when increasing battle speeds", with or without V-Sync. Battle speed skips the game's
end-of-frame waits, so the animation runs 2-4x while the sound hardware keeps real time. #7 asks, in
order: Normal speed during a special attack (a toggle), the sound sped up, or the sound cut when the
animation ends.

**Measured (2026-10-21, Windows test copy, Cutlass Fury, audio dumps + game state):** a spell's or an
S-move's sound is one pre-mixed stereo stream, `SOUND/KO00NN_L/R.dsp` (NN = the skill id; voice and
effects together, no sound effects started during it), and the action's last step waits for the stream
to end. The animation after the stream starts: 10.1 s at 1x, 5.0 s at 2x, 3.3 s at 3x, 2.5 s at 4x; the
stream is 11.6 s, so at 4x the battle sits about 8.6 s with the animation over (the Reddit "I'll still
have to wait until the audio finishes"). No drift at 1x. So "the sound sped up" can only mean the whole
stream faster (higher pitch, or a time-stretch), and "cut when the animation ends" loses most of it at
4x. What runs it: the party's dispatcher `fn_80086C68` puts `fn_80085064` on the unit's task
(`lbl_80309700[unit]`) for command type 1 (magic) and 2 (S-move) in `lbl_80309174[unit]` (0x20 each:
+0 type, +6 skill id; 3 = attack, `fn_80086308`); the enemies' `fn_8008B9E0` puts `fn_80087074` there for
type 1 (their magic, rule codes 500+) and 0xC (their specials, `fn_8008A424`); `lbl_80347334` is the
unit acting. At 3x / 4x the stream used to stall and freeze the battle (the overclock; fixed, development
notes *A battle can freeze*).

## Texture filtering and lower resolutions — requested 2026-10-21

Reddit (r/recomps v0.60.0-beta thread, Unleaded_95): texture filtering options, and resolutions below
the lowest 16:9 one offered (640x528 shown on a 4K display) for an original-resolution look on 16:9:
640x360 or 768x432. **Done on branch `gfx`** (development notes, *Texture filtering, anisotropic
filtering, scaling*): 640x528 is already the original, so Sharp scaling answers the second part.

## AppImage update information — suggested 2026-10-21

The AppImage catalog's test (AppImage/appimage.github.io#10524, since merged) passed on v0.80.0-beta (glibc 2.34),
with two warnings: no update information (embed it with `appimagetool -u "gh-releases-zsync|ogdanimal|SkiesOfArcadiaLegends-Recomp|latest|SkiesOfArcadiaLegends-*-x86_64.AppImage.zsync"`
and publish the `.zsync` beside the AppImage, so AppImageUpdate works), and a "mostly dark" window: its
screenshot shows the launcher without the background art, which loads within 3 s under Xvfb here
(software GL), so likely the bot's sandbox; the launcher's own dark look is by design.

## Drachma's victory pose squeezed into 4:3 — issue #9, fixed on dev (ws31), 2026-10-22

Sandbowl, against v0.80.0-beta, with a video: in 16:9 Drachma's victory pose "reverts back to 4:3". It
copies the screen and draws it back on a plane in front of the camera (a shake), sized to the game's
own field of view, so the whole picture was squeezed into the middle 4:3 with black sides. Game patch
ws31 draws that plane at the game's field of view (development notes *Battle screen plane*). Checked on
WSL, the Retroid Pocket Nova and the Retroid Pocket 5; not released yet.

## Both landscape orientations on Android — issue #12, done on dev, 2026-10-22

MK13X (Pixel): the game is locked to one landscape; wanted the reverse one too. The game screen now
follows the device between both landscapes, unless the system's rotation lock is on
(`recompcore-android-orientation.patch`). On a Retroid the firmware's own landscape overlay
(`com.rp.settings`) decides the orientation, so it can't be seen there; the Activity's request was
checked with `dumpsys` on the Nova. Needs a phone to see it turn.

## ~~Ship battle command grid stretched in the first tutorial~~ — issue #11, FIXED in v0.81.0-beta (ws30)

Sandbowl, against v0.80.0-beta: "The input command screen is still stretched in different aspect
ratios. This only happens during the first tutorial." While Drachma explains the grid ("a 3X3 grid
will appear"), the game shows it as a sprite picture across the whole screen, which the patch drew
full width; the real grid at command input was fine. Game patch ws30 keeps that picture at the HUD's
4:3 shape (development notes *Ship battle tutorial grid*). Released in v0.81.0-beta; #11 closed.

## Cutscene shots that show past their set pieces in widescreen — issue #10, 2026-10-21

Renatodep: "Is there a way to edit models positions for the wide screen cutscenes? I.e. opening with
Fina where you see the valuan ship on the side?" Branch `cutscene` frames listed shots (development
notes *Cutscene shots framed for widescreen*); the opening's two are listed (ws29), and since ws32 two more from GLEclipseGaming's comments: the opening's horizon shot and Aika and Vyse leaving the lookout on the Moon Stone night. The rest of the game's
cutscenes (255 scripts, 5,493 distinct camera commands) aren't surveyed: list shots as players report
them, or survey a scene at a time with the 16:9 / 4:3 shot log. Data-only alternative players can use
today: SPICE (github.com/SoAModTools/SPICE) edits `.sct` camera poses, lens and object placement, and
the port plays such a romhack (it changes the 4:3 picture too).

## The emulator-free path (Aurora + standalone GXRuntime) — decided 2026-10-22

Goal: the port runs without Dolphin's core: the recompiled game on GXRuntime, ModernGekko's standalone
runtime, with Aurora drawing; the Dolphin build ships until the standalone one reaches parity. Plan,
gates and the survey in [handoffs/HANDOFF-emulator-free.md](handoffs/HANDOFF-emulator-free.md). Hard
gaps: audio (Skies mixes with MusyX 2.0 on the DSP and GXRuntime has no DSP emulation: a DSP emulator or
a native MusyX engine such as AxioDL's amuse), the 60 fps interpolator (built into Dolphin's video layer),
a new Android shell, and about 25,000 lines of port features written against Dolphin's APIs. Status
(2026-10-25): the standalone host plays on desktop (WSL, Windows D3D12) with no Dolphin core: audio
bit-exact (Skies' MusyX ucode is Dolphin's AX, ported as HLE), card saves, the in-game menu, save states;
same-machine baseline: faster than the Dolphin build on CPU (battle too: 3.78x vs 3.25x, 1.86x vs 1.27x on
one E-core, 0.35 vs 0.49 cores at full speed), and on GPU about 44% less GPU time per frame (GPU-bound on an
iGPU at 6x: 24.4 against ~43.5 ms, 1.30x against 0.77x, after forks 0032-0035; at full speed there the
standalone holds 33 ms frames, Dolphin is late on every one; the first GPU number, "more GPU time", came
from a Dolphin run at the wrong resolution). Goal set by the user: better than the Dolphin build, weaker
hardware included. Android: a test app (`tools/android-standalone-build.sh`,
com.skiesofarcadia.standalone) plays on the Retroid Pocket 5: 22-25% faster uncapped than the Dolphin build
on the same DOL and module (1.68x vs 1.38x, battle 2.04x vs 1.63x), and since the pacing fix (presents scheduled
on the throttle's clock, fork 0036, with an adaptive delay; the emulation thread on the prime core: schedutil
had clocked the sleeping thread down) smoother at full speed at ~0.4-0.5 W less (p99 34.2-34.4 vs 37.7 ms in
battle, 34.7-35.0 vs 36.2 at the save point; Windows p99 39.8 -> 35.0 too); at 6x it holds full speed where
Dolphin can't (GPU-bound 1.24x vs 0.87x at the save point; fork 0037 dropped the present resample pass). The
shell is immersive and survives background / foreground. Since then (feature-gap items 0-6, 2026-10-25/26): the
European and Japanese discs, Continue, the graphics options with MSAA up to 8x (a Dawn patch; Pocket 5 2x, RTX 5080
8x), input (rumble, keyboard layout, Button mapping, controller follow), Language, HD textures (pack import; on
the Pocket 5 as fast as without the pack, half the Dolphin build's memory, BC7 skipped there; fork 0045 fixed
a loader-thread hitch) and 60 fps (frame interpolation), and the Japanese memory card fixed. Aurora now runs on
a video thread beside the game (fork 0046, `HANDOFF-emulator-free.md`, *The video thread*): Pocket 5 uncapped
1.77x / 1.70x at the save point at 30 / 60 fps (Dolphin 1.38x / 1.41x), 2.15x / 2.05x in battle (1.63x /
1.69x); 60 fps at full speed p99 ~17.7 ms against Dolphin's 24.5-25.9, at less power. Ahead as an engine, not
yet a replacement (features 7-10 missing, not played through; `HANDOFF-emulator-free.md`, *Where it stands*).
Item 7 since (2026-10-26): the Dreamcast features (Uncensored, the Dreamcast music and streams, the
European 60 Hz audio) from the Dolphin build's modules copied as they are behind shimmed Dolphin headers;
every output byte-identical to the Dolphin build's. Item 8 (2026-10-26): the Dolphin build's save states load
(converted), the launcher patch, controller Device; Windows' Vulkan and D3D11 fallbacks fixed (fork 0049), a
dialog when no graphics API starts; no console window on Windows; the Language restart on Android (Pocket 5,
European disc). Open: the Android launcher and packaging (items 9-10, waiting for dev to be
merged into the branch, on hold by the user); a PC where D3D12 really fails and D3D11's speed on a weak GPU; the EFB
passes on the tiler; the built-in pad and audio on the device (by hand), size changes on return. The plan for
the features: [handoffs/HANDOFF-feature-gaps.md](handoffs/HANDOFF-feature-gaps.md).

The Aurora renderer inside Dolphin (`--graphics Aurora`, `AURORA=1`), the groundwork for it: the picture
matches Dolphin on both Retroids, Windows and WSL (fork fixes: the XF viewport decoded per register;
intermediate EFB passes no longer skipped under the 60 fps replay or before a kept-picture copy); 60 fps
at full speed on both Retroids with Vulkan's on-screen cadence; the Picture option applies; the in-game
menu and OSD draw through a Dear ImGui overlay (fork 0026). Inside Dolphin it is at parity with Vulkan, not
ahead, and Dolphin's HD packs don't apply under it, so it stays opt-in there. Committed as patches on the
local branch `aurora-backend`, kept local on purpose (not rebased, merged or pushed).
