#!/usr/bin/env bash
# The standard measurement on the Android build: for each savestate, a fresh boot
# (tools/android-boot.sh), load, 16 s settle, then the speed averaged over 30 s
# (drive.py speed:30), plus a screenshot to check the picture.
# A fresh boot per state matches the desktop protocol. The app must boot the
# extracted disc folder, like desktop: the states hold that layout's file offsets,
# and on the RVZ every scene transition raises "Invalid read/write" (docs/development-notes.md, Android).
# Usage: tools/bench-android.sh NAME [STATE...]   (default: the four standard states)
set -euo pipefail
cd "$(dirname "$0")/.."
NAME=${1:?usage: bench-android.sh NAME [STATE...]}; shift
STATES=("$@")
(( ${#STATES[@]} )) || STATES=(01-opening-cutscene 02-first-battle 03-valuan-ship-field 04-valuan-ship-later)
export ADB_AUTO=${ADB_AUTO:-/sdcard/Android/data/org.dolphinemu.dolphinemu.debug/files/Automation}
OUT=build/android/bench/$NAME
mkdir -p "$OUT"
# Each state also keeps its per-second trace ($state.log, wall= column) and the device's clocks
# and temperatures ($state.sampler.txt, tools/android-sampler.sh) to explain slow seconds.
for state in "${STATES[@]}"; do
  tools/android-boot.sh
  tools/android-sampler.sh start
  SPEED_TRACE=1 tools/drive.py load:build/savestates/$state.sav wait:16 speed:30 \
    shot:$OUT/$state.png > "$OUT/$state.log"
  tools/android-sampler.sh stop "$OUT/$state.sampler.txt"
  echo "$NAME $state $(grep '^speed' "$OUT/$state.log")" | tee -a "$OUT/results.txt"
done
