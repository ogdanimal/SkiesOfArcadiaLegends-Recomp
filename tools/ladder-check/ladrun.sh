#!/usr/bin/env bash
# ladrun.sh TAG USERDIR [ENV...]: ladder state, hold down, dump ~500 pictures, stop; prints green counts
S=$(cd "$(dirname "$0")" && pwd)
TAG=$1 UD=$2; shift 2
cd /mnt/c/dev/SkiesRecomp
D=build/dump-$TAG; rm -rf $D; mkdir -p $D
(USERDIR=$UD $S/wintest.sh auto-$TAG 15-ladder $TAG.log RECOMP_FRAME_INTERPOLATION=1 RECOMP_FRAME_INTERPOLATION_DUMP=build\\dump-$TAG RECOMP_FRAME_INTERPOLATION_DUMP_FRAME=20 RECOMP_FRAME_INTERPOLATION_DUMP_COUNT=520 RECOMP_FRAME_INTERPOLATION_TRACE=1 "$@" &)
sleep 10
AUTO=build/auto-$TAG timeout 120 python3 ~/projects/SkiesOfArcadiaLegends-Recomp/tools/drive.py hold:main_y=-1:600 >/dev/null 2>&1
for i in $(seq 120); do [ $(ls $D 2>/dev/null | wc -l) -ge 1030 ] && break; sleep 2; done
AUTO=build/auto-$TAG timeout 30 python3 ~/projects/SkiesOfArcadiaLegends-Recomp/tools/drive.py stop >/dev/null 2>&1; sleep 3
tasklist.exe | grep -qi moderngekko && { echo "still running, killing"; taskkill.exe /F /IM moderngekko-run.exe >/dev/null; }
echo "dumped: $(ls $D | wc -l) files"
python3 -I $S/dumpscore.py $D
python3 -I $S/clouddump.py $D
