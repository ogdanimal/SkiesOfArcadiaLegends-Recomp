#!/usr/bin/env bash
# Apply patches/ to freshly checked-out submodules (git submodule update --init --recursive), in the
# order patches/README.md lists them. CI runs it; locally the working copies already carry them.
#   tools/apply-patches.sh [--check]    --check: only test that every patch applies
set -euo pipefail
cd "$(dirname "$0")/.."
P=$PWD/patches
MG=lib/ModernGekko RC=lib/ModernGekko/vendor/dolphin DR=lib/ModernGekko/vendor/dolphin/DolRecomp
CHECK=()
[ "${1:-}" = --check ] && CHECK=(--check)
apply() { git -C "$1" apply ${CHECK[@]+"${CHECK[@]}"} "$P/$2" || { echo "patch failed: $2"; exit 1; }; echo "ok: $2"; }

apply $MG moderngekko-local.patch
apply $MG moderngekko-macos.patch
apply $MG moderngekko-launcher-signal.patch
apply $MG moderngekko-aurora.patch
apply $MG moderngekko-standalone-launcher.patch
apply $MG moderngekko-ship-hud.patch
apply $MG moderngekko-cheats.patch
apply $MG moderngekko-renderer.patch
apply $MG moderngekko-display-rate.patch
for p in $(grep -v '^#' "$P/recompcore.series"); do
  apply $RC recompcore-$p.patch
done
for p in mtmsr-entry gather-pipe llvm-cpu win64-sjlj-frame coff-uwtable halfword-byterev macho-sections fast-fp; do
  apply $DR dolrecomp-$p.patch
done
# Highly Theoretical (the Dreamcast music's AICA), both builds; when it's checked out (CI's codegen jobs take
# only ModernGekko, RecompCore and DolRecomp)
if [ -f lib/HighlyTheoretical/Core/yam.c ]; then
  apply lib/HighlyTheoretical highlytheoretical-dsp-shift-wrap.patch
fi
