#!/usr/bin/env bash
# Desktop PGO on Linux/WSL: `moderngekko-port pgo-run` builds an instrumented module, this script
# trains it on the four standard states (load, 16 s settle, 30 s; the same states and timing as the
# Android profile), stops the runtime cleanly so the profile is written, and pgo-run then merges it
# and builds the PGO module into OUT.
# usage: tools/pgo-wsl.sh [OUT] [extra pgo-run build options]   (OUT defaults to build/modules-llvm-pgo;
#        backend LLVM + state-in-memory unless overridden, e.g. `build/modules-clang-pgo --backend c`)
# clang, its profiling runtime and llvm-profdata must all be LLVM 20, the LLVM DolRecomp links
# (apt: clang-20 libclang-rt-20-dev); /usr/bin/clang is 21, so LLVM 20's bin goes first on PATH.
set -eu
cd "$(dirname "$0")/.."
OUT=${1:-build/modules-llvm-pgo}; shift $(( $# > 0 ? 1 : 0 ))
(( $# )) || set -- --backend llvm --state-in-memory
export PATH=/usr/lib/llvm-20/bin:$PATH AUTO=build/auto
STATES=(01-opening-cutscene 02-first-battle 03-valuan-ship-field 04-valuan-ship-later)
mkdir -p "$AUTO/commands"
rm -f "$AUTO/status.txt" "$AUTO/commands/"*
LOG=build/pgo-$(basename "$OUT").log
systemd-run --user --scope --quiet -p MemoryMax=10G -p MemorySwapMax=256M -- \
  lib/ModernGekko/build/moderngekko-port pgo-run extracted/SkiesOfArcadiaLegends "$@" \
  --llvm-profdata /usr/lib/llvm-20/bin/llvm-profdata --output "$OUT" --keep-work \
  -- --user-dir build/user-bench --automation-dir "$AUTO" --load-state "build/savestates/${STATES[0]}.sav" \
  > "$LOG" 2>&1 &
PORT=$!
# The instrumented build comes first (~8 min for LLVM).
until grep -q '^state=running' "$AUTO/status.txt" 2>/dev/null; do
  kill -0 $PORT 2>/dev/null || { echo "pgo-run ended before training; see $LOG"; exit 1; }
  sleep 5
done
steps=(wait:16 speed:30)
for state in "${STATES[@]:1}"; do steps+=("load:build/savestates/$state.sav" wait:16 speed:30); done
python3 tools/drive.py "${steps[@]}" stop
wait $PORT && echo "PGO module: $(cat "$OUT"/GEAE8P/active-module.txt)" || { echo "pgo-run failed; see $LOG"; exit 1; }
