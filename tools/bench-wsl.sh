#!/usr/bin/env bash
# The standard measurement on the Linux/WSL build, as tools/bench-win.sh does on Windows: load the
# state, wait for the load + 16 s, average `speed` over 30 s (plus the slowest and fastest second), screenshot, stop cleanly.
# usage: tools/bench-wsl.sh NAME MODULE [STATE]   (STATE defaults to 01-opening-cutscene)
#   RUNTIME / USER_DIR are passed to tools/play.sh (USER_DIR=build/user-bench: frame limiter off, 1×).
set -eu
cd "$(dirname "$0")/.."
NAME=$1 STATE=${3:-01-opening-cutscene}
export MODULE=$2 AUTO=build/auto
mkdir -p build/shots
tools/play.sh "build/bench-$NAME.log" --load-state "build/savestates/$STATE.sav" &
RUN=$!
sleep 3
until grep -q '^state=running' build/auto/status.txt 2>/dev/null; do sleep 1; done
sleep 16
samples=()
for _ in $(seq 30); do samples+=("$(grep '^speed=' build/auto/status.txt | cut -d= -f2)"); sleep 1; done
python3 tools/drive.py "shot:$PWD/build/shots/bench-$NAME.png" stop > /dev/null
rc=0; wait $RUN || rc=$?
printf '%s\n' "${samples[@]}" | awk -v n="$NAME" -v rc=$rc '{s+=$1; if(NR==1||$1<lo)lo=$1; if($1>hi)hi=$1}
  END {printf "%s speed=%.3f min=%.3f max=%.3f exit=%d\n", n, s/NR, lo, hi, rc}'
