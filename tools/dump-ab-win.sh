#!/usr/bin/env bash
# Frame-by-frame A/B on Windows: run a state with every real picture dumped, then compare two runs
# picture by picture (same frame index = same game frame: from a savestate with no input, a run is
# deterministic; two runs of the unpatched game match byte for byte).
#
# usage: tools/dump-ab-win.sh run NAME GAME MODULE STATE [SECONDS] [WIDESCREEN]
#          GAME/MODULE as Windows paths under C:\dev\SkiesRecomp, e.g.
#          extracted\SkiesOfArcadiaLegends build\modules-llvm-pgo-fix\gGEAE8P_recomp.dll (unpatched)
#          build\game\GEAE8P-ws build\modules-llvm-pgo-ws\gGEAE8P_recomp.dll (widescreen DOL)
#          WIDESCREEN = [Skies] Widescreen (0 = 4:3, the default here; 1 = 16:9)
#        tools/dump-ab-win.sh cmp NAME_A NAME_B
#          prints how many common pictures are identical, and the differing ones with pixel counts
#
# Runs use a copy of build\user-bt (1x, 640x528) in single core with interpolation forced on
# (adaptive off), which is what dumps the pictures (build\dump-ab-NAME\f<frame>-real.png from
# frame 0). States made with the unpatched DOL (01-06) are the only fair reference: a state saved
# by a widescreen build carries its widened camera into an unpatched run (07 does).
# Windows paths ignore case: don't use names that differ only in case.
# Check (2026-10-10, widescreen DOL with the 4:3 setting vs the unpatched game, state 02):
#   tools/dump-ab-win.sh run orig extracted\\SkiesOfArcadiaLegends build\\modules-llvm-pgo-fix\\gGEAE8P_recomp.dll 02-first-battle
#   tools/dump-ab-win.sh run ws43 build\\game\\GEAE8P-ws build\\modules-llvm-pgo-ws\\gGEAE8P_recomp.dll 02-first-battle
#   tools/dump-ab-win.sh cmp orig ws43     -> identical=165 different=0
set -u
WIN_ROOT=${WIN_ROOT:-/mnt/c/dev/SkiesRecomp}
cd "$WIN_ROOT"

if [ "${1:-}" = cmp ]; then
  A=build/dump-ab-$2 B=build/dump-ab-$3 same=0 diff=0 list=""
  for f in $(ls "$A" | grep -- '-real.png' | sort -V); do
    [ -f "$B/$f" ] || continue
    n=$(compare -metric AE "$A/$f" "$B/$f" null: 2>&1 | awk '{print int($1)}')
    if [ "$n" = 0 ]; then same=$((same + 1)); else diff=$((diff + 1)); list="$list ${f%-real.png}:$n"; fi
  done
  echo "identical=$same different=$diff"
  [ -n "$list" ] && echo "$list" | tr ' ' '\n' | head -40 | tr '\n' ' ' && echo
  exit 0
fi

[ "${1:-}" = run ] || { sed -n '2,25p' "$0"; exit 1; }
NAME=$2 GAME=$3 MODULE=$4 STATE=$5 SECS=${6:-16} WS=${7:-0}
U=build/user-ab-$NAME A=build/auto-ab-$NAME D=build/dump-ab-$NAME
rm -rf "$U" "$A" "$D"
cp -r build/user-bt "$U"
sed -i '/^CPUThread/d' "$U/GameSettings/GEAE8P.ini"
printf '\n[Skies]\nWidescreen = %s\nFrameMode = 1\n' "$WS" >> "$U/Config/Dolphin.ini"
mkdir -p "$A/commands" "$D"
win() { echo "$1" | tr / '\\'; }
cmd.exe /c "set RECOMP_FRAME_INTERPOLATION=1&& set RECOMP_FRAME_INTERPOLATION_ADAPTIVE=0&& set RECOMP_FRAME_INTERPOLATION_DUMP=$(win "$D")&& set RECOMP_FRAME_INTERPOLATION_DUMP_FRAME=0&& set RECOMP_FRAME_INTERPOLATION_DUMP_COUNT=400&& lib\\ModernGekko\\build\\moderngekko-run.exe --game $GAME --module $MODULE --user-dir $(win "$U") --automation-dir $(win "$A") --load-state build\\savestates\\$STATE.sav > $(win "$D")\\run.log 2>&1" &
P=$!
AUTO=$A timeout 120 python3 tools/drive.py wait:"$SECS" "peek:0x80002A00:12" stop
wait $P
echo "$NAME: $(ls "$D"/*-real.png 2>/dev/null | wc -l) real pictures"
