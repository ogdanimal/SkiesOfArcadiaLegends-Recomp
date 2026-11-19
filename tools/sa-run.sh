#!/usr/bin/env bash
# One unattended run of the standalone (Linux build) for tests: from a save state or a card Continue, for a
# number of guest seconds, unthrottled, with frame dumps and the log in OUT.
#   tools/sa-run.sh OUT START REGION SECS [EVERY]
#     OUT     a folder: frames (frame_NNNNNN.ppm every EVERY frames, default 300), run.log, user/ (a fresh user
#             folder unless USER_DIR is set)
#     START   a save state (the Dolphin build's .sav or the standalone's .sst), or card:N (Continue from card
#             file N; CARD_DIR=<folder of .gci files> copies a card in first)
#     REGION  us | eu | jp: the game folder and module (GAME_US / MODULE_US, ... below)
#     SECS    guest seconds after the start (--seconds +SECS)
# Presses and pokes are relative to the start: SKIES_PRESS="+1.5:A,+3:B", SKIES_POKE="@+2:8030A178|=40000000".
# SKIES_RTC (default 847000789) pins the clock so two runs match; RTC=wall uses the real one. Draws with
# Vulkan on the CPU (DOL_AURORA_VULKAN_CPU=1: deterministic) and caps the run at 6 GB (systemd-run), since
# several runs at once can take the WSL VM down. Prints one line: frames, error lines, the last speed.
# Defaults: the ws35 folders tools/gamepatch.py and tools/module-linux.sh make (docs: HANDOFF-emulator-free,
# Sweep 2026-11-19); BIN defaults to build/standalone/skies-standalone.
# SPDX-License-Identifier: GPL-3.0-or-later
set -u
cd "$(dirname "$0")/.."
OUT=$(realpath -m "${1:?usage: tools/sa-run.sh OUT START REGION SECS [EVERY]}") START=$2 REGION=$3 SECS=$4
EVERY=${5:-300}
BIN=${BIN:-build/standalone/skies-standalone}
case $REGION in
  us) GAME=${GAME_US:-build/game/GEAE8P-ws35} MODULE=${MODULE_US:-build/ws35/module/gGEAE8P_recomp.so} REG=USA ;;
  eu) GAME=${GAME_EU:-build/game/GEAP8P-ws35} MODULE=${MODULE_EU:-build/ws35/module-eu/gGEAP8P_recomp.so} REG=EUR ;;
  jp) GAME=${GAME_JP:-build/game/GEAJ8P-ws35} MODULE=${MODULE_JP:-build/ws35/module-jp/gGEAJ8P_recomp.so} REG=JAP ;;
  *) echo "REGION: us, eu or jp"; exit 2 ;;
esac
rm -rf "$OUT"; mkdir -p "$OUT/frames"
USER_FOLDER=${USER_DIR:-$OUT/user}
mkdir -p "$USER_FOLDER"
ARGS=(--game "$GAME" --module "$MODULE" --user "$USER_FOLDER" --seconds "+$SECS" --unthrottled)
if [[ $START == card:* ]]; then
  if [ -n "${CARD_DIR:-}" ]; then
    mkdir -p "$USER_FOLDER/GC/$REG/Card A" && cp "$CARD_DIR"/*.gci "$USER_FOLDER/GC/$REG/Card A/"
  fi
  ARGS+=(--continue "${START#card:}")
else
  ARGS+=(--load-state "$(realpath "$START")")
fi
[ "${RTC:-}" = wall ] || export SKIES_RTC=${SKIES_RTC:-847000789}
export DOL_AURORA_VULKAN_CPU=${DOL_AURORA_VULKAN_CPU:-1} AURORA_SYNC_PIPELINES=1
export SKIES_DUMP_DIR="$OUT/frames" SKIES_DUMP_EVERY=$EVERY
CAP=()
command -v systemd-run >/dev/null && CAP=(systemd-run --user --scope --quiet -p MemoryMax=6G --)
"${CAP[@]}" timeout $((SECS * 6 + 180)) "$BIN" "${ARGS[@]}" > "$OUT/run.log" 2>&1
RC=$?
ERRORS=$(grep -c -i "fatal\|couldn't\|can't\|invalid\|unmapped\|bad access\|abort\|exception" "$OUT/run.log")
echo "$(basename "$OUT") exit=$RC frames=$(ls "$OUT/frames" | wc -l) errors=$ERRORS $(grep '\[host\] speed' "$OUT/run.log" | tail -1)"
