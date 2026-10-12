#!/usr/bin/env bash
# devladder.sh TAG: boot the Retroid build, load the ladder state, record 16 s while holding down, pull + score
set -u; cd ~/projects/SkiesOfArcadiaLegends-Recomp; S=$(cd "$(dirname "$0")" && pwd)
eval "$(sed -n 5,9p tools/android-boot.sh)"; export ADB SERIAL
export ADB_AUTO=/sdcard/Android/data/$PKG/files/Automation
tools/android-boot.sh || exit 1
sleep 3
python3 tools/drive.py load:build/savestates/15-ladder.sav wait:3 >/dev/null 2>&1
(adb shell screenrecord --time-limit 16 /sdcard/lad-$1.mp4 &)
sleep 1
python3 tools/drive.py hold:main_y=-1:600 >/dev/null 2>&1
sleep 18
OUT=build/android/recordings-2026-10-10/check-ladder-${PFX:-tmem}-$1.mp4
adb pull /sdcard/lad-$1.mp4 "$(wslpath -w $OUT)" >/dev/null && adb shell rm /sdcard/lad-$1.mp4
python3 tools/drive.py stop >/dev/null 2>&1
echo "== run $1: $(ffprobe -v error -select_streams v -show_entries stream=nb_frames -of csv=p=0 $OUT) frames"
python3 -I $S/outlier.py $OUT; python3 -I $S/greenfind.py $OUT | cut -c1-80
