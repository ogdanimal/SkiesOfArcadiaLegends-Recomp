#!/usr/bin/env bash
TOOLS=$(dirname "$(readlink -f "$0")")  # before any cd
# Makes the ship battle savestates (no save of ours reaches one): from the world map
# (10-worldmap-android, before the game hands out the Little Jack) the debug build
# (patches/game/shipdebug.json on top of widescreen.json) gives the party the Little Jack, the test
# equips its Main Cannon and 3"Cannon and changes the map to me500a (Baltor's Blackbeard, the first
# ship battle), as fn_80100270 does. Saves 17-ship-battle-baltor (the battle's opening scene) and
# 17b-ship-battle-baltor-commands (Drachma's tutorial done, Aika choosing a weapon).
# usage: tools/shipbattle-state-win.sh [GAME] [MODULE]   (Windows paths under C:\dev\SkiesRecomp)
set -u
GAME=${1:-build\\game\\GEAE8P-ws15-shipdbg}
MODULE=${2:-build\\modules-llvm-pgo-ws15-shipdbg\\gGEAE8P_recomp.dll}
cd "${WIN_ROOT:-/mnt/c/dev/SkiesRecomp}"
U=build/user-sbstate A=build/auto-sbstate
rm -rf "$U" "$A"; mkdir -p "$A/commands"; cp -r build/user-bt "$U"
cmd.exe /c "lib\\ModernGekko\\build\\moderngekko-run.exe --game $GAME --module $MODULE --user-dir build\\user-sbstate --automation-dir build\\auto-sbstate > build\\auto-sbstate\\run.log 2>&1" &
D="python3 $TOOLS/drive.py"
d() { AUTO=$A timeout 120 $D "$@" 2>/dev/null | grep -v "^$"; }
d wait:6 load:build/savestates/10-worldmap-android.sav wait:2 >/dev/null
# shipdebug.c: 'SHP' << 8 | 0 -> fn_8021BD7C(0), the Little Jack, on the next field frame
d poke:0x8031AA24:53485000 wait:0.5 >/dev/null
# the ship's weapon slots (lbl_8030B7F4 + 0x1568 + 0x18, item IDs): Main Cannon 0x190, 3"Cannon 0x19A
d poke:0x8030CD74:0190019a wait:0.3 >/dev/null
d peek:0x8030CD5C:0x20
d poke:0x80305CF0:$(printf 'me500a.sct\0\0' | xxd -p) poke:0x80311AEC:0000000f wait:8 >/dev/null
echo "map $(d peek:0x80311AC4:4)"; echo "ship $(d peek:0x80306628:0x1C)"
d save:build/savestates/17-ship-battle-baltor.sav >/dev/null
for i in $(seq 1 23); do d tap:a:4 wait:1.0 >/dev/null; done
d save:build/savestates/17b-ship-battle-baltor-commands.sav shot:build/sbstate.png >/dev/null
d stop >/dev/null; wait
ls -la build/savestates/17*.sav
