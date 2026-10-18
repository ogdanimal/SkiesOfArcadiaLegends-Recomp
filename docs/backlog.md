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
- Not started; no Japanese disc on hand yet.

## Remember controllers by identity, not name — idea 2026-10-17

Controller 1 is saved as Dolphin's `SDL/<n>/<name>`, and a name can change with how the game was
started (Steam's virtual gamepad versus the pad itself; issue #1, HANDOFF-input *Steam Controller*).
Aurora's ports save each port's pad by SDL GUID + serial with a VID/PID fallback and only display the
name. Doing the same would mean our own field beside Dolphin's `Device` (the launcher and
`SkiesInput::Follow` resolve it to the current `SDL/<n>/<name>`). Not needed while the game follows
whichever pad is pressed and the launcher matches by label; worth it if more name reports come in.

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

