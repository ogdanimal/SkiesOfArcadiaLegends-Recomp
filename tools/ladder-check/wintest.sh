#!/usr/bin/env bash
# usage: wintest.sh AUTODIR STATE LOG [ENV=VAL ...]  -- launches a ws test instance on Windows in background
set -eu
AD=$1 ST=$2 LOG=$3; shift 3
cd /mnt/c/dev/SkiesRecomp
rm -rf build/$AD; mkdir -p build/$AD/commands
SETS=""
for kv in "$@"; do SETS+="set $kv&& "; done
cmd.exe /c "cd /d C:\\dev\\SkiesRecomp&& ${SETS}lib\\ModernGekko\\build\\${RUNTIME:-moderngekko-run.exe} --game ${WGAME:-build\\game\\GEAE8P-ws} --module ${WMODULE:-build\\modules-llvm-pgo-ws\\gGEAE8P_recomp.dll} --user-dir build\\${USERDIR:-user-ws-test} --automation-dir build\\$AD $( [ "$ST" = - ] || echo "--load-state build\\savestates\\$ST.sav" ) > build\\$LOG 2>&1"
