#!/usr/bin/env bash
TOOLS=$(dirname "$(readlink -f "$0")")  # before any cd
# Which buttons do something in a state? Loads STATE, waits PRE seconds, taps one button, waits, takes
# a screenshot; the same with no button is the baseline (runs from a savestate with no input are
# frame-exact), so a screenshot that differs means the button did something there.
# usage: tools/button-probe-win.sh STATE PRE INPUT... (INPUT: a drive.py tap name, tapped; k=v[,k=v],
#        e.g. c_x=1 or main_y=1,a=1, held for HOLD frames (default 60); "none" = baseline)
#        GAME / MODULE / HOLD in the environment pick the build (default: the unpatched game)
set -u
WIN_ROOT=${WIN_ROOT:-/mnt/c/dev/SkiesRecomp}
GAME=${GAME:-extracted\\SkiesOfArcadiaLegends} MODULE=${MODULE:-build\\modules-llvm-pgo-fix\\gGEAE8P_recomp.dll}
cd "$WIN_ROOT"
STATE=$1 PRE=$2; shift 2
U=build/user-probe A=build/auto-probe O=build/probe-$STATE
rm -rf "$U" "$A"; mkdir -p "$O" "$A/commands"; cp -r build/user-bt "$U"
cmd.exe /c "lib\\ModernGekko\\build\\moderngekko-run.exe --game $GAME --module $MODULE --user-dir build\\user-probe --automation-dir build\\auto-probe > build\\auto-probe\\run.log 2>&1" &
P=$!
D="python3 $TOOLS/drive.py"
AUTO=$A timeout 60 $D wait:6 >/dev/null
for b in "$@"; do
  if [ "$b" = none ]; then act="wait:0.2"; elif [[ $b == *=* ]]; then act="hold:$b:${HOLD:-60}"; else act="tap:$b:4"; fi
  AUTO=$A timeout 90 $D load:build/savestates/$STATE.sav wait:$PRE $act wait:1 shot:$(pwd)/$O/${b//[=,]/_}.png >/dev/null 2>&1
done
AUTO=$A timeout 30 $D stop >/dev/null 2>&1; wait $P
for b in "$@"; do [ "$b" = none ] && continue
  n=$(compare -metric AE "$O/none.png" "$O/${b//[=,]/_}.png" null: 2>&1 | awk '{print int($1)}'); echo "$b $n"; done
