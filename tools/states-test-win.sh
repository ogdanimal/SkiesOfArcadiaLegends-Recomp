#!/usr/bin/env bash
# Save states in the Skies menu and auto-save (VideoCommon/SkiesMenu.cpp), on Windows with the dev
# runner: menu save in a battle (slot 1), no auto-save during a battle, menu load from the world map
# brings the battle back, auto-saves every minute on the world map with the newest three kept (and
# the speed around them), overwriting a slot (lastState.sav), one A on Load does nothing, two load.
# usage: tools/states-test-win.sh [GAME] [MODULE]   (Windows paths under C:\dev\SkiesRecomp; ws13)
# Takes about 7 minutes. States land in build\user-sstest\StateSaves.
set -u
TOOLS=$(dirname "$(readlink -f "$0")")
GAME=${1:-build\\game\\GEAE8P-ws13}
MODULE=${2:-build\\modules-llvm-pgo-ws13\\gGEAE8P_recomp.dll}
cd "${WIN_ROOT:-/mnt/c/dev/SkiesRecomp}"
U=build/user-sstest A=build/auto-sstest
rm -rf "$U" "$A"; mkdir -p "$A/commands"; cp -r build/user-bt "$U"; rm -rf "$U/StateSaves"
printf '\n[Skies]\nAutoSave = 1\n' >> $U/Config/Dolphin.ini
cmd.exe /c "lib\\ModernGekko\\build\\moderngekko-run.exe --game $GAME --module $MODULE --user-dir build\\user-sstest --automation-dir build\\auto-sstest > build\\auto-sstest\\run.log 2>&1" &
D="python3 $TOOLS/drive.py"
d() { AUTO=$A timeout 300 $D "$@" 2>/dev/null | grep -v "^$"; }
word() { d peek:$1:4 | awk '{print strtonum("0x"$2$3$4$5)}'; }
menu() { d press:z=1 wait:0.5 press:z=1,start=1 wait:0.3 release wait:0.6 >/dev/null; }
states() { ls -l --time-style=+%T $U/StateSaves/ 2>/dev/null | awk 'NR>1{print $6, $7}' | tr '\n' ' '; echo; }
d wait:6 >/dev/null
d load:build/savestates/16-deck-battle-ws10.sav wait:3 >/dev/null
echo "== in battle (mode $(word 0x803475CC)): menu Save state (slot 1)"
menu; d tap:a:4 wait:4 >/dev/null; echo "  $(states)"
echo "== battle 80 s with auto-save every minute: no auto-save expected"
d wait:77 >/dev/null; echo "  mode $(word 0x803475CC)  $(states)"
echo "== world map; menu Load state slot 1 (Right, A, A): battle back?"
d load:build/savestates/10-worldmap-android.sav wait:2 >/dev/null
menu; d tap:dpad_down:4 wait:0.3 tap:dpad_right:4 wait:0.3 tap:a:4 wait:0.4 tap:a:4 wait:5 >/dev/null
echo "  mode $(word 0x803475CC) (7 = yes)"
echo "== world map 4 x 62 s: auto-saves, newest three kept, speed"
d load:build/savestates/10-worldmap-android.sav >/dev/null
for i in 1 2 3 4; do d speed:62 2>/dev/null | grep -i "speed" | sed 's/^/  /'; echo "  $(states)"; done
echo "== overwrite slot 1 (menu save again): lastState.sav appears, not offered by the launcher"
menu; d tap:a:4 wait:4 >/dev/null; echo "  $(states)"
echo "== one A on Load (auto-save) does nothing; two A load the newest auto-save from a battle"
menu; d tap:dpad_down:4 wait:0.3 tap:a:4 wait:0.5 tap:b:4 wait:2 >/dev/null; echo "  mode after one A: $(word 0x803475CC)"
d load:build/savestates/16-deck-battle-ws10.sav wait:2 >/dev/null
menu; d tap:dpad_down:4 wait:0.3 tap:a:4 wait:0.4 tap:a:4 wait:5 >/dev/null; echo "  mode after two A from battle: $(word 0x803475CC) (6 = loaded)"
d stop >/dev/null; wait
