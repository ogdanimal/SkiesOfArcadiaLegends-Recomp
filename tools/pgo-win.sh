#!/usr/bin/env bash
# Windows counterpart of tools/pgo-wsl.sh, run from WSL: `build-win.bat pgo` builds an instrumented
# clang C module and starts training; this drives the four standard states (load, 16 s, 30 s) and stops
# the runtime so the profile is written. The instrumented run then crashes on exit (an access violation
# after the profile is flushed, docs/development-notes.md), so pgo-run stops there; this script merges the raw profile
# and links build\modules-clang-pgo\use-build\gGEAE8P_recomp.dll with tools\module-win.bat itself.
# The savestates must be in the Windows tree's build/savestates. WIN_ROOT defaults to /mnt/c/dev/SkiesRecomp.
set -eu
WIN_ROOT=${WIN_ROOT:-/mnt/c/dev/SkiesRecomp}
cd "$WIN_ROOT"
WINDIR=$(wslpath -w "$WIN_ROOT")
export AUTO=build/auto
STATES=(01-opening-cutscene 02-first-battle 03-valuan-ship-field 04-valuan-ship-later)
for state in "${STATES[@]}"; do [ -f "build/savestates/$state.sav" ] || { echo "missing build/savestates/$state.sav"; exit 1; }; done
mkdir -p "$AUTO/commands"
rm -f "$AUTO/status.txt" "$AUTO/commands/"*
cmd.exe /c "$WINDIR\\tools\\build-win.bat pgo" > build/pgo-clang.log 2>&1 &
PORT=$!
until grep -q '^state=running' "$AUTO/status.txt" 2>/dev/null; do
  kill -0 $PORT 2>/dev/null || { echo "pgo-run ended before training; see build/pgo-clang.log"; exit 1; }
  sleep 5
done
steps=(wait:16 speed:30)
for state in "${STATES[@]:1}"; do steps+=("load:build/savestates/$state.sav" wait:16 speed:30); done
python3 tools/drive.py "${steps[@]}" stop
wait $PORT || true
WS=build/modules-clang-pgo/GEAE8P/pgo
RAW=$(ls -t $WS/raw/*.profraw | head -n 1)
LLVM=/mnt/c/dev/clang+llvm-20.1.8-x86_64-pc-windows-msvc/bin
"$LLVM/llvm-profdata.exe" merge -output=$WS/merged.profdata "$(wslpath -w "$RAW")"
"$LLVM/llvm-profdata.exe" show $WS/merged.profdata | grep 'Total functions'
GEN=$(ls -d $WS/gen/GEAE8P/*/dolrecomp-output/generated)
rm -rf build/modules-clang-pgo/use-build
cmd.exe /c "$WINDIR\\tools\\module-win.bat $(wslpath -w "$GEN") $WINDIR\\build\\modules-clang-pgo\\use-build $(wslpath -w $WS/merged.profdata)" \
  > build/pgo-clang-use.log 2>&1 || { echo "PGO-use build failed; see build/pgo-clang-use.log"; exit 1; }
echo "PGO module: build\\modules-clang-pgo\\use-build\\gGEAE8P_recomp.dll"
