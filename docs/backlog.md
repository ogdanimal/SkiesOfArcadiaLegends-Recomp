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

## Widescreen: the world map's flight meter and compass are stretched — found 2026-10-17

In 16:9 the world map's flight meter comes out ~1.33x as wide as in 4:3 and the compass ~1.4x (same
picture height; Windows, `10-worldmap-android.sav`; development-notes *Known issues*). The rest of the
HUD keeps its shape through the widescreen patch's HUD anchors; these two don't go through them (the
compass is a tilted 3D object). A game patch fix (ws24). The user is waiting for it before the HD
textures branch (`hdtex`, HANDOFF-hdtextures) is merged: the texture pack's meter is the 4:3 one,
right once 16:9 keeps the meter's shape.
