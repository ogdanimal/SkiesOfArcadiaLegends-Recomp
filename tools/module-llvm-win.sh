#!/usr/bin/env bash
# LLVM-backend module for Windows, cross-generated from WSL: the Windows tools are built without
# LLVM, but the pinned DolRecomp emits COFF objects for a Windows triple (the same route as the
# Android module). DolRecomp runs here, then tools\module-win.bat links the objects on Windows.
# usage: tools/module-llvm-win.sh NAME [extra dolrecomp args, e.g. --profile-use FILE.profdata]
#   -> $WIN_ROOT/build/modules-NAME/gGEAE8P_recomp.dll  (WIN_ROOT defaults to /mnt/c/dev/SkiesRecomp)
#   DOL=<path> builds from another DOL, e.g. build/game/GEAE8P-ws/sys/main.dol (tools/gamepatch.py)
set -eu
cd "$(dirname "$0")/.."
NAME=$1; shift
WIN_ROOT=${WIN_ROOT:-/mnt/c/dev/SkiesRecomp}
DOL=${DOL:-extracted/SkiesOfArcadiaLegends/sys/main.dol}
GEN=build/win-llvm/$NAME
rm -rf "$GEN"; mkdir -p "$(dirname "$GEN")"
DOLRECOMP_LLVM_TARGET=x86_64-pc-windows-msvc lib/ModernGekko/build/dolrecomp -j"$(nproc)" \
  --backend=llvm --state-in-memory "$@" --cpu gekko --gamecube "$DOL" "$GEN" > "$GEN.log" 2>&1 ||
  { echo "dolrecomp failed; see $GEN.log"; exit 1; }
cp "$DOL" "$GEN/generated/"
OUT=$WIN_ROOT/build/modules-$NAME
rm -rf "$OUT"; mkdir -p "$OUT"
cp -r "$GEN/generated" "$OUT/"
W=$(wslpath -w "$WIN_ROOT")
cmd.exe /c "$W\\tools\\module-win.bat $W\\build\\modules-$NAME\\generated $W\\build\\modules-$NAME\\module-build" \
  > "$OUT/build.log" 2>&1 || { echo "module build failed; see $OUT/build.log"; exit 1; }
cp "$OUT/module-build/gGEAE8P_recomp.dll" "$OUT/"
echo "module: build\\modules-$NAME\\gGEAE8P_recomp.dll"
