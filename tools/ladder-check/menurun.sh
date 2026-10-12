#!/usr/bin/env bash
# menurun.sh TAG [RUNTIME]: field state, open the main menu (X), move in it, close (B B); dump frames 20..320
S=$(cd "$(dirname "$0")" && pwd)
TAG=$1; cd /mnt/c/dev/SkiesRecomp; D=build/dump-$TAG; rm -rf $D; mkdir -p $D
(RUNTIME=${2:-moderngekko-run.exe} USERDIR=user-bt $S/wintest.sh auto-$TAG 03-valuan-ship-field $TAG.log RECOMP_FRAME_INTERPOLATION=1 RECOMP_FRAME_INTERPOLATION_DUMP=build\\dump-$TAG RECOMP_FRAME_INTERPOLATION_DUMP_FRAME=20 RECOMP_FRAME_INTERPOLATION_DUMP_COUNT=300 &)
sleep 10
AUTO=build/auto-$TAG timeout 120 python3 ~/projects/SkiesOfArcadiaLegends-Recomp/tools/drive.py tap:x wait:2 tap:dpad_down wait:1 tap:dpad_down wait:1 tap:a wait:2 tap:b wait:1 tap:b wait:2 >/dev/null 2>&1
for i in $(seq 60); do [ $(ls $D 2>/dev/null | wc -l) -ge 590 ] && break; sleep 2; done
AUTO=build/auto-$TAG timeout 30 python3 ~/projects/SkiesOfArcadiaLegends-Recomp/tools/drive.py stop >/dev/null 2>&1; sleep 3
tasklist.exe | grep -qi moderngekko && taskkill.exe /F /IM moderngekko-run.exe >/dev/null
echo "$TAG dumped: $(ls $D | wc -l)"; python3 -I $S/dumpscore.py $D
