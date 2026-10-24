#!/usr/bin/env bash
# Benchmark a Windows module from WSL with the docs/development-notes.md protocol: load the standard state, wait for
# the load + 16 s, average `speed` over 30 s (plus the slowest and fastest second), screenshot, stop cleanly.
# usage: tools/bench-win.sh NAME MODULE [STATE]   (MODULE relative to the Windows checkout, e.g.
#        'build\modules-clang\GEAE8P\<hash>\gGEAE8P_recomp.dll'; STATE defaults to 01-opening-cutscene)
#   WIN_ROOT defaults to /mnt/c/dev/SkiesRecomp. RUNTIME / USER_DIR are passed to run-win.bat
#   (Windows paths, e.g. USER_DIR='build\user-bench' for the frame limiter off).
#   GAME another game root (run-win.bat), e.g. build\game\GEAE8P-ws.
set -eu
NAME=$1 MODULE=$2 STATE=${3:-01-opening-cutscene}
WIN_ROOT=${WIN_ROOT:-/mnt/c/dev/SkiesRecomp}
cd "$WIN_ROOT"
WINDIR=$(wslpath -w "$WIN_ROOT")
mkdir -p build/shots
SETS="set MODULE=$MODULE&& "
[ -n "${RUNTIME:-}" ] && SETS+="set RUNTIME=$RUNTIME&& "
[ -n "${USER_DIR:-}" ] && SETS+="set USER_DIR=$USER_DIR&& "
[ -n "${GAME:-}" ] && SETS+="set GAME=$GAME&& "
cmd.exe /c "$SETS$WINDIR\\tools\\run-win.bat build\\bench-$NAME.log --load-state build\\savestates\\$STATE.sav" &
RUN=$!
sleep 3
until grep -q '^state=running' build/auto/status.txt 2>/dev/null; do sleep 1; done
sleep 16
samples=()
for _ in $(seq 30); do samples+=("$(grep '^speed=' build/auto/status.txt | cut -d= -f2)"); sleep 1; done
AUTO=build/auto python3 tools/drive.py "shot:build/shots/bench-$NAME.png" stop > /dev/null
rc=0; wait $RUN || rc=$?
printf '%s\n' "${samples[@]}" | awk -v n="$NAME" -v rc=$rc '{s+=$1; if(NR==1||$1<lo)lo=$1; if($1>hi)hi=$1}
  END {printf "%s speed=%.3f min=%.3f max=%.3f exit=%d\n", n, s/NR, lo, hi, rc}'
