#!/usr/bin/env bash
# A/B of the hot code layout (issue #13, dolrecomp-coff-function-sections.patch) on Windows, from WSL:
# two modules from the same DolRecomp, DOL and profile, without (DOLRECOMP_FUNCTION_SECTIONS=0) and with
# a section per function, then tools/bench-win.sh on 01/02/04: one discarded warmup each, ROUNDS
# interleaved rounds (the order flips every round), limiter off. Results in build/ab-layout.txt.
#   tools/ab-layout-win.sh PROFDATA [ROUNDS]     (ROUNDS default 3)
#   GAME_DIR: the game root both sides use, relative to the checkouts (default
#   extracted/SkiesOfArcadiaLegends; build/game/GEAE8P-ws for the game-patched one); its sys/main.dol is
#   recompiled, so the module matches the game the states run. USER_DIR (Windows path) default
#   build\user-bench. WIN_ROOT as for tools/module-llvm-win.sh.
# Needs both patches in the working copies: the DolRecomp one in this checkout (then rebuild
# lib/ModernGekko/build/dolrecomp), recompcore-module-layout.patch in the Windows checkout.
set -eu
cd "$(dirname "$0")/.."
PROF=$(realpath "${1:?usage: tools/ab-layout-win.sh PROFDATA [ROUNDS]}")
ROUNDS=${2:-3}
WIN_ROOT=${WIN_ROOT:-/mnt/c/dev/SkiesRecomp}
GAME_DIR=${GAME_DIR:-extracted/SkiesOfArcadiaLegends}
STATES=(01-opening-cutscene 02-first-battle 04-valuan-ship-later)
DR=lib/ModernGekko/vendor/dolphin/DolRecomp
grep -q DOLRECOMP_FUNCTION_SECTIONS $DR/src/backend/llvm/target.cpp ||
  { echo "apply patches/dolrecomp-coff-function-sections.patch to $DR and rebuild dolrecomp"; exit 1; }
[ lib/ModernGekko/build/dolrecomp -nt $DR/src/backend/llvm/target.cpp ] ||
  { echo "lib/ModernGekko/build/dolrecomp is older than the patch: rebuild it"; exit 1; }
grep -q NOICF "$WIN_ROOT/lib/ModernGekko/vendor/dolphin/module-template/CMakeLists.txt" ||
  { echo "apply patches/recompcore-module-layout.patch in $WIN_ROOT/lib/ModernGekko/vendor/dolphin"; exit 1; }

DOL=$GAME_DIR/sys/main.dol
export DOL
DOLRECOMP_FUNCTION_SECTIONS=0 tools/module-llvm-win.sh layout-off --semantics fast --profile-use "$PROF"
tools/module-llvm-win.sh layout-on --semantics fast --profile-use "$PROF"
# The two must differ only in the layout: hot sections in the new objects, none in the old
objdump=$(command -v llvm-objdump-20 || command -v llvm-objdump || echo objdump)
hot() { for o in build/win-llvm/$1/generated/chunks/*.o; do $objdump -h "$o"; done | grep -c 'text\$hot' || true; }
off=$(hot layout-off) on=$(hot layout-on)
echo "hot sections: old layout $off, new layout $on"
[ "$off" = 0 ] && [ "$on" -gt 0 ] || { echo "unexpected: the builds don't differ in their layout"; exit 1; }

export GAME=${GAME_DIR//\//\\} USER_DIR=${USER_DIR:-'build\user-bench'}
OUT=build/ab-layout.txt
: > $OUT
run() { tools/bench-win.sh "$1-$2-$3" "build\\modules-layout-$1\\gGEAE8P_recomp.dll" "$2" | tee -a $OUT; }
for v in off on; do run $v ${STATES[0]} warmup > /dev/null; done
for r in $(seq "$ROUNDS"); do
  order=(off on); (( r % 2 )) || order=(on off)
  for s in "${STATES[@]}"; do for v in "${order[@]}"; do run $v "$s" "$r"; done; done
done
grep -v -- '-warmup ' $OUT | awk '{split($1, n, "-"); v=n[1]; s=n[2]; for (i=2; i<=NF; i++) if ($i ~ /^speed=/) {
    sub("speed=", "", $i); sum[s, v]+=$i; cnt[s, v]++; states[s]=1 } }
  END { for (s in states) printf "%s: old %.3f  new %.3f  (%+.1f%%)\n", s, sum[s, "off"]/cnt[s, "off"],
    sum[s, "on"]/cnt[s, "on"], 100*(sum[s, "on"]/cnt[s, "on"]/(sum[s, "off"]/cnt[s, "off"])-1) }' | sort | tee -a $OUT
