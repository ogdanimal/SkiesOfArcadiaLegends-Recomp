#!/usr/bin/env bash
# The sweep (tools/sa-sweep.sh) on an Android device running the branded app on the standalone: each save state
# is pushed to the app's files folder, played for SECS guest seconds from the launcher's Continue with scripted
# presses, then its log and frames come back. env.txt (SKIES_EXTRA_ARGS: the state and --seconds, appended to
# the launcher's command line, with a throwaway user folder; presses, dumps, a pinned clock) is written for each
# run and removed at the end, so the app's own saves and settings aren't touched.
#   tools/sa-device-sweep.sh OUT SECS STATE...
# Environment: ADB (default adb; a Windows adb.exe from WSL works: files go through STAGE, default
# /mnt/c/Users/Public/sa-sweep), SERIAL, PKG (default com.skiesofarcadia.recomp.test), PAD (the built-in
# controller's input node, default /dev/input/event8 on a Retroid Pocket 5: its A starts Continue and quits).
# The app must have a game imported for the states' disc; an existing env.txt stops the sweep (not ours).
# SPDX-License-Identifier: GPL-3.0-or-later
set -u
OUT=$(realpath -m "${1:?usage: tools/sa-device-sweep.sh OUT SECS STATE...}") SECS=${2:?}
shift 2
ADB=${ADB:-adb} PKG=${PKG:-com.skiesofarcadia.recomp.test} PAD=${PAD:-/dev/input/event8}
A=("$ADB")
[ -n "${SERIAL:-}" ] && A+=(-s "$SERIAL")
FILES=/sdcard/Android/data/$PKG/files
WINDOWS=0
[[ $ADB == *.exe ]] && WINDOWS=1
STAGE=${STAGE:-/mnt/c/Users/Public/sa-sweep}
host_path() { if [ $WINDOWS = 1 ]; then wslpath -w "$1"; else echo "$1"; fi; }
d() { "${A[@]}" "$@"; }
key() { d shell "sendevent $PAD 1 $1 1; sendevent $PAD 0 0 0; sleep 0.15; sendevent $PAD 1 $1 0; sendevent $PAD 0 0 0"; sleep 0.6; }
running() { [ -n "$(d shell pidof "$PKG:game" | tr -d '\r')" ]; }

if d shell "ls $FILES/env.txt" > /dev/null 2>&1; then echo "$FILES/env.txt exists: not ours, stopping"; exit 1; fi
mkdir -p "$OUT" "$STAGE"
: > "$OUT/report.txt"
PRESS=$(python3 -c "
secs=float('$SECS'); out=[]; k=0
while k*6<secs:
    for at,n in ((0.5,'A'),(1.5,'DOWN'),(2.5,'A'),(3.5,'RIGHT'),(4.5,'A'),(5.5,'B')):
        a=k*6+at
        if a<secs: out.append('+%.2f:%s'%(a,n))
    k+=1
print(','.join(out))")
d shell "mkdir -p $FILES/sweep"
i=0
for state in "$@"; do
  i=$((i + 1))
  name=$(printf "%02d-%s" "$i" "$(basename "${state%.*}")")  # numbered: two states can share a file name
  cp "$state" "$STAGE/s.sav"
  d push "$(host_path "$STAGE/s.sav")" "$FILES/sweep/s.sav" > /dev/null
  d shell "rm -rf $FILES/sweep/frames; mkdir -p $FILES/sweep/frames"
  # A user folder of its own: the presses save to the memory card at save points, and must not touch the app's
  d shell "rm -rf $FILES/sweep/user; mkdir -p $FILES/sweep/user/StateSaves"
  printf 'SKIES_EXTRA_ARGS=--user $FILES/sweep/user --state-dir $FILES/sweep/user/StateSaves --load-state $FILES/sweep/s.sav --seconds +%s\nSKIES_RTC=847000789\nSKIES_PRESS=%s\nSKIES_DUMP_DIR=$FILES/sweep/frames\nSKIES_DUMP_EVERY=300\n' \
    "$SECS" "$PRESS" > "$STAGE/env.txt"
  d push "$(host_path "$STAGE/env.txt")" "$FILES/env.txt" > /dev/null
  d shell am start -n "$PKG/org.dolphinemu.dolphinemu.skies.SkiesLauncherActivity" > /dev/null 2>&1
  sleep 5
  key 304  # A: Continue (the launcher's first button)
  for _ in $(seq 1 $((SECS * 4 + 120))); do sleep 1; running || break; done
  running && { echo "$name: still running after the timeout"; }
  rm -rf "$OUT/$name"; mkdir -p "$OUT/$name"
  d exec-out cat "$FILES/log.txt" > "$OUT/$name/run.log"
  rm -rf "$STAGE/frames"
  d pull "$FILES/sweep/frames" "$(host_path "$STAGE/frames")" > /dev/null 2>&1 && mv "$STAGE/frames" "$OUT/$name/"
  errors=$(grep -c -i "fatal\|couldn't\|can't\|invalid\|unmapped\|bad access\|abort\|exception" "$OUT/$name/run.log")
  echo "$name frames=$(ls "$OUT/$name/frames" 2>/dev/null | wc -l) errors=$errors $(grep -a 'loaded\|\[host\] speed' "$OUT/$name/run.log" | tail -1 | cut -c1-80)" | tee -a "$OUT/report.txt"
done
d shell "rm -f $FILES/env.txt; rm -rf $FILES/sweep"
rm -rf "$STAGE"
echo "done" | tee -a "$OUT/report.txt"
