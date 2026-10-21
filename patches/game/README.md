# Game patches

Changes to the game itself, written in C (and a little assembly) and built into a copy of
`main.dol` by `tools/gamepatch.py`. The shipping spec is `widescreen.json` (widescreen, camera
control and the QOL options together); the current build is **ws28** (`build/game/GEAE8P-ws28`, DOL
SHA-1 `f1e39fc0`, SHA-256 `7af78727`; the European one `GEAP8P-ws28`, SHA-1 `836ab729`, SHA-256
`caaf2dd9`; the Japanese one `GEAJ8P-ws28`, SHA-1 `c95115f8`, SHA-256 `74f0fedc`; ws28 = ws27 + ship battles' spells and special attacks at normal speed and `qol_normal_speed` for the frontend, branch `smove`; ws27 = ws26 + spells and special attacks at normal speed, branch `smove`; ws26 = ws25 + field speed, branch `speed`; ws25 = ws24 + Hold B, branch `romhack`), and `release/skies-widescreen.dolpatch` (`skies-widescreen-eu.dolpatch`, `skies-widescreen-jp.dolpatch`) turns a player's own
`main.dol` into it (`tools/mkdolpatch.py`). Because they're part of the DOL, the recompiled module runs them
as ordinary game code: desktop and Android alike, with no mod loader and no runtime cost beyond the
code itself. The research behind each one is in the decomp's `docs/` (`../SkiesOfArcadiaLegends-Decomp`).

| Spec | What it does |
|---|---|
| `widescreen.json` | 16:9 (or any aspect, set at runtime in `ws_config`; the frontend's Match screen sets the window's own aspect, 4:3 to 32:9, `recompcore-match-screen.patch`): wider 3D view, HUD at its original shape, full-screen effects kept full width, the battle-entry transitions' 3D tiles drawn at the game's own field of view (`UNWIDE` in `fullwidth.S`), and the HUD's side pieces at the wide edges (`WS_HUD_ANCHORS`), or at the edges of a narrower centred area the frontend sets in `ws_hud_aspect` (`0x80002E9C`; < 0 = the original layout). With `display_aspect` 0 (4:3) the hooks step aside (`ORIGINAL_IN_43` in `fullwidth.S`) and the picture is identical to the unpatched game's (`tools/dump-ab-win.sh`, NOTES *Widescreen 4:3 setting*). Decomp `docs/widescreen.md`. |
| `widescreen.json` (camera.c) | Player camera control: wraps the camera manager's call to the shake function (`0x8022D910`) and turns the eye around the field camera's look-at point by the tilt, zoom and turn the frontend writes to `cam_input` (`0x80002EC0`, hold Z in game; `VideoCommon/SkiesMenu.cpp`). Towns, dungeons and the world map; the game's camera object isn't touched. Decomp `docs/camera.md`. |
| `widescreen.json` (qol.c, qol.S) | Quality-of-life options from `qol_input` (`0x80002EA0`, the in-game menu): random encounter rate (remaps the encounter roll; Off also clears the frames-since-battle counter), EXP / gold / magic EXP scaled in the battle data once per battle before the results read it (ground and ship battles), battle speed (skips the end-of-frame retrace waits in game mode 7, but not while a spell or special attack runs, ws27, ship battles' too, ws28, which `qol_normal_speed` at `0x80002BDC` tells the frontend; `qol_wait_retrace` tail-calls `VIWaitForRetrace` for savestates), and the same in the field (game mode 6: cutscenes, towns, dungeons, the world map) when the frontend sets `QOL_SPEED_FIELD` in the speed word, at a speed in quarters (ws26), Invincible (party HP / MP / Spirit refilled every battle frame, damage to the party zeroed by `qol_damage` around `fn_8002DC14`, including the action handler table entry via `redirect_pointers`, written again every frame since ws19 because states keep the old one; in ship battles, field mode 6 on maps 500-599, the ship's HP, Spirit and crew MP, and `qol_ship_phase` around `fn_8014419C`). Decomp `docs/qol_cheats.md`. |
| `widescreen.json` (dcmusic.c) | The Dreamcast music (ws22; checked in game on WSL 2026-10-17): the sound layer's music start (`sndSeqPlayEx` at `0x8021882C`, the only one) goes through `dcm_seq_play`, which starts the sequence with every track muted (`SND_PLAYPARA_TRACKMUTE`, mask 0) when the frontend says it has Dreamcast data for that (bank slot, data number); the sequence still runs, so the play list, its end, pause and continue are unchanged. Every `sndSeqVolume` / `Pause` / `Continue` / `Stop` / `SetMidiCtrl` call goes through a `dcm_*` wrapper (`redirect_calls`) that posts the music sequence's events to a 12-entry ring; a paused song (`MusicPause`, the field music during a battle) is kept beside the current one, so its continue (with its pair) and stop are posted too; controller 7 (the towns' adaptive channel volumes, set every frame) goes to a per-channel table instead (channels 0-15; the game's channel 17 has no Dreamcast counterpart); `dcm_frame` (from `qol_frame`) mutes or unmutes a running song when the option changes. Everything lives in `struct dcm_box` at `0x8031A890` (MetroTRK's buffers after trace2d's table, before shipdebug's word; Europe `0x8031DE50`); the frontend (`Core/SkiesMusic.cpp`) writes the magic `'DCMU'`, `enabled` and per-slot allow bits (with an FNV-1a hash of the slot's file name) and reads the ring. Decomp `docs/sound.md`. |
| `widescreen.json` (autoload.c) | Continue from a memory card save without the logos, intro, title or load screen: the frontend writes `load_request` (`0x80002EF0`: `'LOAD'`, file 1-99, status, error; `VideoCommon/SkiesMenu.cpp` namespace `autoload`) on the first frame; the title's per-frame call at `0x801DC350` goes through `dl_title`, which holds title state 1 while it pumps the font's loading, reads card A's file with the load screen's own card functions on a stand-in task, checks the XOR and version, applies the save and starts the field the way the title's Continue does; before the card it also loads the resident files the second logo would have (`fn_8022A854`; without them the first battle crashed, ws20-ws22), and `qol_frame` loads them in the field for a state made without them (ws23). A failed load (no file, read error, bad data) frees everything and lets the title go on. Decomp `docs/ui.md` *Loading a save without the load screen*. |
| `shipdebug.json` | Debug only, combined with `widescreen.json`: gives the party a ship on request (`'SHP' << 8 \| ship` written to `0x8031AA24`, the last word of MetroTRK's buffers, so it combines with `trace2d.json`; the field main's call at `0x801DC358` goes through `dbg_field`, which tail-calls it so states made with this build load in play builds). Used by `tools/shipbattle-state-win.sh` to make the Baltor ship battle states. |
| `trace2d.json` | Debug only, combined with `widescreen.json`: records the ten-deep call chain of every projection load (`trace2d.c`, table in MetroTRK's buffers at `0x80319078`); read it with `AUTO=... tools/readtrace.py 0x80319078 [reset] [2d]`. |

## Building

### The European release (GEAP8P)

`gamepatch.py` recognises the European `main.dol` by its SHA-1 (`295a9fc9...`, SHA-256 `d0e9fc91...`)
and builds the same sources for it: every game address in the sources, specs and `cave.ld`
(`0x80xxxxxx`, `fn_80xxxxxx`, `lbl_80xxxxxx`) is translated through `GEAP8P.addresses.json` before
compiling, and an address missing from the table is an error. Cave addresses map to themselves, so
the patch's own data (`qol_input`, `cam_input`, `load_request`, `ws_config`) is at the same place in
both builds. The MetroTRK code area is `0x80257024`-`0x802597B0` there.

The table comes from `tools/dolmatch.py` (stdlib only, ~12 s): it matches the USA functions to the
European ones (7108 of 7117: masked instruction signatures, aligned `bl` call sites, order between
anchors, pointer tables), maps data through the matched functions' references, checks every
redirect site is a `bl` to the mapped target and every struct offset the patches use is unchanged,
and writes `build/eu/dolmap.json`, `build/eu/address-map.md` (the table with method and confidence,
plus a Checks section) and `build/eu/address-map.json`. After adding a game address to a patch, run
it again and copy the addresses the sources use into `GEAP8P.addresses.json` (keep only those: the
tool also scans the table file itself). Known differences it found: `fn_8014419C`'s ship-sunk branch
stores its two reward words from 104-byte enemy rows in Europe (120 in the USA; dolmatch reports "one
word", its aligner pairing reordered instructions: the scaling is right, checked by reading both),
the title's state table has rewritten handlers in slots 2 and 10-13 (the same 19 states: autoload's
1 and 0x12 hold), and the European game has 23 more
`TextPrint` callers (caught by `redirect_calls` like the rest).

### The Japanese release (GEAJ8P)

The same for the Japanese `main.dol` (*Eternal Arcadia Legends*, SHA-1 `74d8ff5d...`, SHA-256
`4aa0f3dc...`, 3,165,120 bytes): `GEAJ8P.addresses.json`, made by `tools/dolmatch.py --target GEAJ8P`
(it writes `build/jp/`, the table itself as `build/jp/GEAJ8P.addresses.json`: every address the
sources use, comments included because `gamepatch.py` translates those too, with a confident match;
run on the European DOL the same way it gives exactly the 209 entries of `GEAP8P.addresses.json`).
The Japanese build is much closer to the USA one than the European: 7104 of 7117 functions matched
(5474 by signature), data shifted by about -0x560 to -0x590, the text in the DOL as in the USA. Every
redirect target has the same number of callers except `VIWaitForRetrace` (6, USA 7; the patched site
in `fn_801DC420` is there), every struct field the patches use keeps its offset, and the functions
they use match instruction for instruction but two: `fn_801A4070` (the card read: Japan also probes
card files `EA_LEGEND_EXTRA #00`-`#02` at mount, `fn_801A3BB4`, into the task's work +0x270) and
`fn_801A4354` (the save's data into the game's: shorter, called the same way from the load screen
with the payload at +0x1440). After that call the Japanese load screen sets or clears three event
flags (2852, 1096, 2850) by which of the three extra files the card has (promotional bonus saves);
the patch's direct load (`autoload.c`) skips that step, so a Continue keeps the flags the save holds.
The card file is `EA_LEGEND.%03d` (USA `SA_LEGENDS.%03d`), 0x6000 bytes (3 blocks) as in the USA.
MetroTRK's handlers are at `0x80254398`-`0x80256B24`; the idle PC `0x8023772C` (`release/GEAJ8P.ini`).

```sh
tools/gamepatch.py patches/game/widescreen.json --dol <european disc>/sys/main.dol \
  --out-root build/game/GEAP8P-ws28
tools/gamepatch.py patches/game/widescreen.json --dol <japanese disc>/sys/main.dol \
  --out-root build/game/GEAJ8P-ws28
```

```sh
tools/gamepatch.py patches/game/widescreen.json --out-root build/game/GEAE8P-ws28
DOL=build/game/GEAE8P-ws28/sys/main.dol tools/module-llvm-win.sh llvm-pgo-ws28 \
  --profile-use build/modules-llvm-pgo-gp/GEAE8P/pgo/merged.profdata      # Windows module
NAME=ws28 tools/android-ws-build.sh                                       # Android module + APK
python3 tools/mkdolpatch.py --fst extracted/SkiesOfArcadiaLegends/sys/fst.bin \
  extracted/SkiesOfArcadiaLegends/sys/main.dol build/game/GEAE8P-ws28/sys/main.dol \
  release/skies-widescreen.dolpatch patches/game/GEAE8P.known             # release patch
python3 tools/mkdolpatch.py --fst <european disc>/sys/fst.bin <european disc>/sys/main.dol \
  build/game/GEAP8P-ws28/sys/main.dol release/skies-widescreen-eu.dolpatch   # European release patch
python3 tools/mkdolpatch.py --fst <japanese disc>/sys/fst.bin <japanese disc>/sys/main.dol \
  build/game/GEAJ8P-ws28/sys/main.dol release/skies-widescreen-jp.dolpatch   # Japanese release patch
```

The Android build picks up the European game from `DOL_EU` (default `build/game/GEAP8P-NAME/sys/main.dol`)
and the Japanese one from `DOL_JP` (`build/game/GEAJ8P-NAME/sys/main.dol`).

`mkdolpatch.py` also writes both DOLs' code hashes (header and text sections), the original's data
hash and (with `--fst`) its file table's hash (a romhack's save states get a folder of their own:
`RomhackTag`), and the original bytes of any game data the patch changes (`data-original`; ws25: the battle action table's first entry), so
the frontend can take a romhack that changes only the game's data (development notes, *Romhacks*);
`patches/game/GEAE8P.known` lists romhacks' code changes the import undoes (`tools/dolknown.py`
makes the lines from a romhacked `main.dol`).

The release builds take the DOL's SHA-256 from the patch's `output-sha256` line
(`tools/build-release-win.bat` and `build-release-linux.sh` since ws17, the Android app through
`tools/android-ws-build.sh`), so after `mkdolpatch.py` run `configure` and `build` there again. The release runner refuses any other DOL (a data romhack of the original aside), so test a new build
with the dev runner (`lib\ModernGekko\build\moderngekko-run.exe`).

`gamepatch.py` checks that the input is the USA Rev 0 `main.dol` (or, with `--dol`, the European or the Japanese one), compiles the sources with clang for
the Gekko (`-mcpu=750`, `-Os`, no small-data), links them into the low-memory gap at `0x80001900`
(`cave.ld`: code and constants up to `0x80002A00`, data from there to `0x80002F00`, with
`ws_hud_aspect` fixed at `0x80002E9C`, `qol_input` at `0x80002EA0`, `cam_input` at `0x80002EC0` and `load_request` at `0x80002EF0` for the frontend), rewrites the listed call sites and adds the code and data as new DOL sections. That gap is nearly full (ws13: code 0xFD0 of 0x1100 bytes, data 0x4E4 of 0x500), so there's a second code area: functions marked `section(".trkcave")` go to `0x802548E8`-`0x80257074` (10 KB), over MetroTRK's message handlers, which only run with a debugger attached (`__start` starts MetroTRK only when `0x800030E8` is 2 or 3; the handlers are reached only through its dispatch table). `gamepatch.py` writes that code into the DOL's text in place; it's code only (data still goes in the gap). It
writes a game root: `sys/` with the patched DOL, `files/` linked to the original (on Windows, make
it a junction: `mklink /J build\game\GEAE8P-ws24\files extracted\SkiesOfArcadiaLegends\files`), plus
`patches.txt` (symbols and patched sites) and `patch.elf` (for disassembly). Run it with
`GAME=build/game/GEAE8P-ws24` (`tools/play.sh`) or `GAME=build\game\GEAE8P-ws24` (`tools\run-win.bat`)
and a module built from the same DOL.

| Build | DOL SHA-1 | What it added |
|---|---|---|
| ws8 | `a4f9ef7c` | battle-entry transitions at full width |
| ws9 | `329c631f` | HUD side pieces at the wide edges, the MetroTRK code area |
| ws10 | `7cb9de68` | the 4:3 setting identical to the unpatched game |
| ws11 | `54afdf92` | camera control (`camera.c`) |
| ws12 | `7ad26f22` | QOL options and Invincible (`qol.c`, `qol.S`), `redirect_pointers` |
| ws13 | `fa7b1af1` | world map altitude gauge at the left edge like the original 4:3 (side -100, was -94), `WS_VERSION` 7 |
| ws14 | `7a73cd6e` | QOL options in ship battles: `qol_ship_phase` at `0x80144E3C`, ship HP / Spirit / crew MP refill, speed on maps 500-599 (an earlier ws14 test build of 2026-10-11, a reverted status-box sync, was never used) |
| ws15 | `2d7b1f1d` | ship battle HUD at the edges (grid top right, enemy plates below it; anchor entries now 12 bytes with `dx` and a kind, `WS_VERSION` 9, `TextPrintf` wrapped); battle speed / Invincible work moved out of the retrace polling loop (`qol_frame` from `ws_frame_end`), so states made there survive later builds |
| ws16 | `66da66de` | ship battle spacing asked for by the user: ring lists dy +50, crew / ship panels dy -24 (midway between the Spirit gauge and the ring), `WS_VERSION` 10 |
| ws17 | `910d9504` | ship battle QOL only while the battle's data is live (code review A3, 2026-10-12): the game frees `lbl_8034727C`'s data in `fn_80144E60` but never clears the pointer, so from the second ship battle on Invincible wrote through a stale pointer between the map's load and the new allocation. `qol_ship_live` is set by `qol_ship_alloc` (new hook at `0x8015C6DC`, `fn_8015C6C8`'s call of `fn_801453D0`) and by `qol_ship_phase`, cleared after `qol_ship_results`; it sits in `.late_bss` (`cave.ld`) after every older variable, so states from earlier builds keep their data where this build looks (they read 0 there: off until the battle's next phase) |
| ws18 | `6a401968` | code review section C (2026-10-12): the camera keeps the game's own tilt when its eye sits below the look-at point by more than `rise` (the height limit went negative and put the eye on the far side); `ws_check` restarts the unwide depth for an older build's state and `ws_unwide_end` never takes it below 0 (either would leave every later unwide pair off); `in_ram` accepts only word-aligned addresses inside the 24 MB. No data moved (all 19 data symbols at their ws17 addresses), so ws17 states load as they are. The debug `shipdebug.c` request word moved to `0x8031AA24` so it combines with `trace2d.json` (debug builds only) |
| ws19 | `2185d3d4` | every attack froze from older states (2026-10-12): a state restores code but not data, so the battle action handler table entry `0x802DBB10` (rewritten by `redirect_pointers`) kept the address `qol_damage` had in the build that made the state, and after ws17 / ws18 moved the code that address was inside `qol_ship_results` ("Invalid write to 0x0000001e"); auto-saves carried it forward. `qol_frame` now writes the entry every frame. Also: `qol_ram` checks the pointers it follows, battle QOL only in battle states 4-7 (`0x8034737C`), ship results only while `qol_ship_live`, the camera only in game mode 6. Only that one patch-code pointer lives in game data (audit: no other code pointers in the patch's data, no callbacks) |
| ws20 | `e78a04d9` | Continue from a card save loads it directly (`autoload.c`, `load_request` at `0x80002EF0`, `dl_title` at the title's call `0x801DC350`): ~6 s from launch to the field on Windows, RAM identical to a load through the menus but for the uptime counter; a missing file falls back to the intro and title. Replaces the frontend's button-driving (30-40 s). Released in v0.9.0-beta and v0.9.1-beta |
| ws21 | `49979e26` (Europe `6d2620e5`) | the Dreamcast music hooks (`dcmusic.c`, `dcm_frame` called at the end of `qol_frame`); 663 call sites (Europe 685). Built 2026-10-17, never released |
| ws22 | `ac40e5cf` (Europe `55ab3d55`) | `dcmusic.c`: the paused song tracked (its continue and stop posted), controller 7 as a table instead of ring events (`struct dcm_box` grew to 380 bytes). `fullwidth.S`: the ortho matrix's [0][0] through the spec's symbol `ws_ortho_m00` instead of a literal `0x8034` / `0x63C0`, which gamepatch doesn't translate: the European build's full-width wrappers wrote another word, so its screen-copy effects (the red alert room's pulse, afterimages) were drawn back squeezed, a double image in widescreen (the USA DOL is unchanged). Checked in game on WSL 2026-10-17 (both games); `release/skies-widescreen.dolpatch` (output `e58354e5`) and `release/skies-widescreen-eu.dolpatch` (`30904d88`) are ws22 since 2026-10-17; released in v0.10.0-beta |
| ws23 | `271b1ccd` (Europe `2fb3e9d9`) | the first battle after a Continue from a card save crashed ("Invalid read from 0x00000004", ws20-ws22, both games): the direct load skipped the second logo, which loads 16 resident files into ARAM and lists them in a checksummed table (`fn_8022A854`, `lbl_803165C8`) that the battle's loading reads unchecked. `dl_title` now steps that loader to the end before the card; `qol_resident` (from `qol_frame`) steps it once a frame in the field when it never ran or the table's sum is wrong, with random encounters held back meanwhile, so states made in such a session work too. Checked on the Retroid 2026-10-17 (both games). `release/skies-widescreen.dolpatch` (output `018a9ffb`) and `skies-widescreen-eu.dolpatch` (`8b556a56`) were ws23 |
| ws24 | `0569daa3` (Europe `bb683272`) | HUD placement: `ws_hud_aspect`, a float fixed at `0x80002E9C` (`.ws_hud` in `cave.ld`, outside `ws_config` so older states' variables don't move), read by the anchor scaling in `widescreen.c`: 0 = the HUD at the picture's edges (as ws23), an aspect = the HUD in a centred area of that shape (16:9, 21:9 as 64:27), below 0 = the original layout; the frontend writes it with the picture (`recompcore-hud.patch`, `[Skies] Hud`). The European address table got `0x80002E9C` (identity, like the other fixed addresses). `release/skies-widescreen.dolpatch` (output `c9da6ee4`) and `skies-widescreen-eu.dolpatch` (`16241d84`) were ws24 |
| ws25 | `f4293d72` (Europe `409d12c1`) | Hold B (2026-10-18, romhacks): `qol_input.hold_b` (`0x80002EBC`, the struct's padding word until now, so no data moved) makes `qol_encounter_rand` act as Encounters Off while controller 1's B is held, read from the game's `PADStatus` buffer (`*0x8034763C`, Europe `*0x8034AD5C`: the pointer at `r13 - 0x70E4` / `- 0x7084` that the game swaps with a second buffer every frame); the frontend writes `[Skies] HoldB`. It stands in for cleartonic's hold-B romhack, whose code the import undoes (`GEAE8P.known`). The patches now carry code hashes and `data-original` (`tools/mkdolpatch.py`). `release/skies-widescreen.dolpatch` (output `31e43962`) and `skies-widescreen-eu.dolpatch` (`54adb1a4`) are ws25 |
| ws26 | `357c1821` (Europe `3b973331`) | Field speed (2026-10-20): `QOL_SPEED_FIELD` (0x100) in `qol_input.battle_speed` makes `qol_fast` true in game mode 6 too, with the low byte in quarters (`QOL_QUARTERS`; whole steps without the flag), and `qol_frame` paces each frame at 2, 1 or 0 retraces from a budget (`qol_frame_retraces` in `.late_bss`; `qol_skip_wait` skips VIWaitForRetrace at 0), so cutscenes, towns, dungeons and the world map run at the frontend's field speed, 1.25x and 1.5x included (the battle's whole steps pace as before); no earlier variable moved and no new game address. The frontend sets the flag only outside battle, so ws25 and earlier, reading the whole word, see only the battle speed in a battle |
| ws27 | `f62abf95` (Europe `b9f1c990`, Japan `6f644635`) | Spells and special attacks at normal speed (2026-10-21, issue #7): `qol_fast` is false in a ground or deck battle (mode 7, battle state 4-7) while any unit's action task (`lbl_80309700[0..11]`, the task's first word) runs `fn_80085064` (the party's spell / S-move action, command types 1 and 2) or `fn_80087074` (the enemies' spell / special action, types 1 and 0xC). Each spell and S-move plays one pre-mixed stream (`KO00NN`) and waits for its end, so at speed the animation ended seconds before the sound. Measured on Windows: Cutlass Fury's animation 10.1 s at 1x, 2x and 4x (was 5.0 s / 2.5 s at 2x / 4x), its stream ending 1.7 s after it as at 1x; Antonio's special 7.3 s at 1x and 4x; plain attacks unchanged (an enemy attack 1.0 s at 4x, 4.2-6.3 s at 1x). Ship battles keep the speed. No new data: states from ws26 load. |
| ws28 | `f1e39fc0` (Europe `836ab729`, Japan `c95115f8`) | Ship battles' spells and special attacks at normal speed (2026-10-21): a ship battle turn plays its phases on a frame timeline (`lbl_803472E0`; `lbl_803472DC` 1.0 while it plays), each phase's record (`lbl_803472A4`) giving the frames of the side acting first (+0xC..+0x14) and second (+0x14..end); `lbl_80306E38` holds the two sides' actions in that order (0x90 each, entries of 0x10: +0 kind, +1 side, +5 weapon slot, +8 id). `qol_fast` is false while the acting side has magic (kind 5), a crew special (7), an enemy special attack (8, ids 3-7: Hull Ram, Red Ray, Fist of Grendel, Boulder Throw, Stomp) or the party fires an S-Cannon (kind 4 from the weapon slot holding the Harpoon Cannon 0x1AE or the MoonStone Cannon 0x1AF, `0x803063F0 + 0x298 + slot * 0x22 + 0x12`). `qol_normal_speed` (`0x80002BDC`, its own `.late_bss_ws28` section after `.late_bss`, so ws27's variables keep their addresses) is 1 while a battle at speed plays one of these, ground or ship; the frontend keeps frame interpolation on then (`recompcore-battle-specials.patch`). Measured on Windows from a random encounter (`me550a`), Aika's Sacri: 29.8-30.1 game frames a second in its window at 2x and 4x whichever side acted first, the enemy's Focus in the same phase 60 (2x) / 121 (4x); 1x unchanged; ground battles unchanged (Cutlass Fury at 4x held normal speed for its whole action). No new data in older variables: states from ws27 load. |

A module only matches the DOL it was built from: an unpatched module with a patched DOL (or the
reverse) fails verification for the patched code and runs it in the interpreter, but only where the
runtime enters a chunk through the dispatcher: a chunk reached by a native call from another chunk
isn't checked again, so a changed instruction there silently runs as the module's old code
(2026-10-18: a romhack's one-word branch inside the encounter function never ran; development notes,
*Romhacks*). Savestates made
before a patch carry the old code; `recompcore-restore-boot-dol.patch` puts the boot DOL's code back
when such a state is loaded. Don't test a patch change with a module from an earlier patched DOL: in one such run
the new code most likely never ran, because the module's compiled callers came from the old DOL (NOTES *Widescreen*).

## Savestates across patch builds

A state restores RAM, so it brings back the old build's patch *data* (the boot DOL's code is put
back) and whatever return addresses were on the stacks. Rules that keep states loadable:

- The game must never be waiting inside patch code when a state is taken. Wrappers around a function
  the game waits in (the end-of-frame retrace wait) tail-call it (`ws_frame_end` in `fullwidth.S`).
- Constants belong in `.text` (`cave.ld` puts `.rodata` there). Mutable data is versioned:
  `ws_check` resets it to `ws_defaults` when `ws_config.version` isn't `WS_VERSION`; bump that with
  any change to the data layout or the defaults.
- States made with widescreen builds before late 2026-10-07 don't load in later ones (their stack
  returns into moved patch code).

## Debug hooks

`gamepatch.py` leaves weak undefined symbols at 0, so a spec can offer an optional hook: `widescreen.c`
calls `trace_projection` only when `trace2d.c` is linked in.

## Writing a spec

```json
{
  "sources": ["widescreen.c", "fullwidth.S"],
  "symbols": {"njClipZ": "0x80299188"},
  "redirect_sites": [{"sites": ["0x801DCD68"], "target": "fn_801DC420", "to": "ws_frame_end"}],
  "redirect_calls": [{"target": "fn_80290ED4", "to": "ws_poly"}],
  "redirect_pointers": [{"address": "0x802DBB10", "target": "fn_8002DC14", "to": "qol_damage"}]
}
```

- Game functions are `fn_XXXXXXXX` (resolved from the name) or listed in `symbols`.
- `redirect_pointers` rewrites function pointers in data (a handler table) at the given addresses
  and fails if one doesn't hold the target; use it where `redirect_calls` warns about a pointer.
- `redirect_sites` rewrites `bl target` to `bl to` at the given addresses and fails if one doesn't
  hold that call; `redirect_calls` rewrites every direct call in the DOL and warns about pointers to
  the target in data (indirect calls would bypass the patch).
- Patches hook whole calls, never the middle of a function, so DolRecomp sees ordinary functions.
  A wrapper that must pass arbitrary arguments through is written in assembly (`fullwidth.S`).
- When the code to change is in the middle of a function, wrap a call next to it instead: the camera
  wraps the shake call right before the view is built, the reward scaling wraps the member count
  right before the rewards are read (qol.c).
- Settings from the frontend go in a fixed-address block in the data section (`cave.ld`) with a magic
  word the frontend writes; with no frontend the magic stays 0 and the hook does nothing. The
  frontend checks that the DOL has the hook (a call site no longer holds the original `bl`) before
  writing.
- The gap is shared with Dolphin: it writes a stub at `0x80001800`, hooks `0x800018A8` and
  `0x80002FFC`, and enabling Gecko codes installs the code handler over the whole range.
