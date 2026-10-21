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
