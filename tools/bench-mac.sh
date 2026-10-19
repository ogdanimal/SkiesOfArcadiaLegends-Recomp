#!/usr/bin/env bash
# The standard measurement on macOS, as tools/bench-wsl.sh / bench-win.sh do: launch skies-run with the state,
# wait for the load + 16 s, average `speed` over 30 s (plus the slowest and fastest second), screenshot, stop cleanly.
# usage: tools/bench-mac.sh NAME USER_DIR [STATE]   (STATE defaults to 01-opening-cutscene)
#   USER_DIR: a bench user directory (Config/Dolphin.ini [Core] EmulationSpeed = 0 for the limiter off,
#   config.ini resolution= for the scale). APP defaults to the release bundle in build/; GAME to the
#   user's imported USA disc. HOME is pointed at a scratch folder so nothing touches the player's data.
# The game window must stay visible: macOS throttles Metal drawables of occluded windows (~0.67x).
set -eu
cd "$(dirname "$0")/.."
NAME=$1 USER_DIR=$2 STATE=${3:-01-opening-cutscene}
APP=${APP:-"build/SkiesOfArcadiaLegends-v0.50.0-beta-macos/Skies of Arcadia Legends.app"}
GAME=${GAME:-"$HOME/Library/Application Support/Skies of Arcadia Legends/games/GEAE8P"}
export AUTO=${AUTO:-build/auto}
mkdir -p build/shots "$AUTO" build/bench-home
rm -f "$AUTO/status.txt" "$AUTO/commands/"* 2>/dev/null || true
HOME=$PWD/build/bench-home "$APP/Contents/MacOS/skies-run" --game "$GAME" --user-dir "$USER_DIR" \
  --automation-dir "$AUTO" ${EXTRA_ARGS:-} --load-state "$PWD/build/savestates/$STATE.sav" > "build/bench-$NAME.log" 2>&1 &
RUN=$!
sleep 3
until grep -q '^state=running' "$AUTO/status.txt" 2>/dev/null; do
  kill -0 $RUN 2>/dev/null || { echo "$NAME: runner exited early (build/bench-$NAME.log)"; exit 1; }
  sleep 1
done
sleep 16
# A refused state boots the disc instead (no error on stdout): the standard states start at frame ~3,850.
fc=$(grep '^frame_count=' "$AUTO/status.txt" | cut -d= -f2)
samples=()
for _ in $(seq 30); do samples+=("$(grep '^speed=' "$AUTO/status.txt" | cut -d= -f2)"); sleep 1; done
python3 -I tools/drive.py "shot:$PWD/build/shots/bench-$NAME.png" stop > /dev/null
rc=0; wait $RUN || rc=$?
printf '%s\n' "${samples[@]}" | awk -v n="$NAME" -v rc=$rc -v fc="$fc" '$1>0 {s+=$1; c++; if(c==1||$1<lo)lo=$1; if($1>hi)hi=$1}
  END {printf "%s speed=%.3f min=%.3f max=%.3f n=%d exit=%d frame=%s%s\n", n, s/c, lo, hi, c, rc, fc, (fc < 3000 ? " STATE-NOT-LOADED" : "")}'
