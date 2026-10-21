# Backlog

Requests and ideas not scheduled yet. Move an item to a handoff when work on it starts.

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
- Not started. **The Japanese GameCube disc is on hand since 2026-10-20** (the user's ISO,
  `../GameCube-Research/targets/SkiesOfArcadiaLegends/jp`; the port now runs it as a game of its own,
  HANDOFF-japan). First facts for question 1: its files have the USA names, and the ones that differ
  in content are the voice and sound banks (`sound/cr*` 52 files, `e*` 509, `stv*` 144, `k*` / `ko*`
  100, `b*` 165, `f*` 58, plus 114 `.dsp` streams and clips), while the music banks (`m0231`-`m0465`)
  match the USA's; so the clips are likely the same layout under the same names. Not yet compared
  bank by bank (sample counts, lengths).

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

**Done 2026-10-21 (game patch ws27, branch `smove`):** spells and special attacks (party and enemies)
play at normal speed, always on (the user's choice); development notes *Spells and special attacks at
normal speed*.

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

The AppImage catalog's test (AppImage/appimage.github.io#10524) passed on v0.80.0-beta (glibc 2.34),
with two warnings: no update information (embed it with `appimagetool -u "gh-releases-zsync|ogdanimal|SkiesOfArcadiaLegends-Recomp|latest|SkiesOfArcadiaLegends-*-x86_64.AppImage.zsync"`
and publish the `.zsync` beside the AppImage, so AppImageUpdate works), and a "mostly dark" window: its
screenshot shows the launcher without the background art, which loads within 3 s under Xvfb here
(software GL), so likely the bot's sandbox; the launcher's own dark look is by design.
