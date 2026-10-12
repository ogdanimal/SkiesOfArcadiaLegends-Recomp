#!/usr/bin/env bash
# Run the built module memory-capped, with the automation directory enabled.
#
# usage: tools/play.sh [LOG] [extra moderngekko-run args, e.g. --load-state FILE]
#   AUTO=<dir>    automation directory (default build/auto); drive it with tools/drive.py
#   MODULE=<so>   module to run (default: the active WSL module, LLVM + PGO in build/modules-llvm-pgo-gp)
#   RUNTIME=<exe> runtime binary, kept in lib/ModernGekko/build/ beside Sys/ (default moderngekko-run)
#   USER_DIR=<dir> user directory (default build/user; build/user-bench has the frame limiter off)
#   GAME=<dir>   game root (default extracted/SkiesOfArcadiaLegends; patched ones: tools/gamepatch.py)
#
# The cap turns a runaway run into "the game dies alone" instead of taking the WSL VM down
# (the HybridHeavenRecomp lesson). Rendering stays on Vulkan/llvmpipe on purpose; see docs/development-notes.md
# for why dzn and OGL aren't used.
set -u
cd "$(dirname "$0")/.."
LOG=${1:-build/play.log}; shift $(( $# > 0 ? 1 : 0 ))
AUTO=${AUTO:-build/auto}
mkdir -p "$AUTO"
rm -f "$AUTO/status.txt" "$AUTO/commands/"*
exec systemd-run --user --scope --quiet -p MemoryMax=8G -p MemorySwapMax=256M -- \
  timeout -k 5 -s TERM 3600 stdbuf -o0 -e0 \
  "${RUNTIME:-lib/ModernGekko/build/moderngekko-run}" --game "${GAME:-extracted/SkiesOfArcadiaLegends}" \
  --module "${MODULE:-$(cat build/modules-llvm-pgo-gp/GEAE8P/active-module.txt)}" --user-dir "${USER_DIR:-build/user}" \
  --automation-dir "$AUTO" "$@" > "$LOG" 2>&1
