#!/usr/bin/env bash
# Exit test for a Windows module from WSL: COUNT times, load STATE, play SECONDS after the load, stop
# with drive.py and record the exit code and whether StaticRecompCore::Shutdown ran (its log line).
# usage: tools/exit-test-win.sh NAME MODULE [STATE] [COUNT] [SECONDS]
#   (MODULE relative to the Windows checkout, as for tools/bench-win.sh; STATE defaults to
#   01-opening-cutscene, COUNT to 10, SECONDS to 20). WIN_ROOT, RUNTIME, USER_DIR as for bench-win.sh.
#   STOP=auto (default: drive.py stop), close (the title bar's X) or altf4 (key presses); close and altf4
#   take keyboard focus, as does FOCUS=1 with auto (tools/focus-win.ps1, which also reports whether the
#   window really had focus). Ask before running those on a desktop someone is using.
#   Exit codes seen through WSL are the low byte: 9 is 0xC0000409 (abort), 5 is 0xC0000005.
set -eu
NAME=$1 MODULE=$2 STATE=${3:-01-opening-cutscene} COUNT=${4:-10} SECONDS_PLAYED=${5:-20}
STOP=${STOP:-auto} FOCUS=${FOCUS:-0}
WIN_ROOT=${WIN_ROOT:-/mnt/c/dev/SkiesRecomp}
cd "$WIN_ROOT"
WINDIR=$(wslpath -w "$WIN_ROOT")
focus() { powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$WINDIR\\tools\\focus-win.ps1" -Action "$1" | tr -d '\r' || true; }
SETS="set MODULE=$MODULE&& "
[ -n "${RUNTIME:-}" ] && SETS+="set RUNTIME=$RUNTIME&& "
[ -n "${USER_DIR:-}" ] && SETS+="set USER_DIR=$USER_DIR&& "
clean=0
for i in $(seq "$COUNT"); do
  LOG=build/exit-$NAME-$i.log
  cmd.exe /c "$SETS$WINDIR\\tools\\run-win.bat ${LOG//\//\\} --load-state build\\savestates\\$STATE.sav" &
  RUN=$!
  sleep 3
  until grep -q '^state=running' build/auto/status.txt 2>/dev/null; do sleep 1; done
  sleep "$SECONDS_PLAYED"
  focused=-
  if [ "$STOP" = auto ]; then
    [ "$FOCUS" = 1 ] && focused=$(focus focus)
    AUTO=build/auto python3 tools/drive.py stop > /dev/null
  else
    focused=$(focus "$STOP")
  fi
  rc=0; wait $RUN || rc=$?
  shutdown=no; grep -q '^\[staticrecomp\] shutdown' "$LOG" && shutdown=yes
  [ "$rc" = 0 ] && [ $shutdown = yes ] && clean=$((clean + 1))
  echo "$NAME $STATE stop=$STOP run $i: exit=$rc shutdown=$shutdown focused=${focused#focused=}"
done
echo "$NAME $STATE stop=$STOP: $clean/$COUNT clean"
