#!/usr/bin/env bash
# bisect.sh LO HI: skip replay draws [LO,HI) on the device, one ladder run, print green count
cd ~/projects/SkiesOfArcadiaLegends-Recomp; S=$(cd "$(dirname "$0")" && pwd)
eval "$(sed -n 5,9p tools/android-boot.sh)"; U=/sdcard/Android/data/$PKG/files
adb shell "echo '$1 $2' > $U/FrameInterpolation.skip"
PFX=skip-$1-$2 $S/devladder.sh 1 >/dev/null 2>&1
adb shell "rm -f $U/FrameInterpolation.skip"
echo "skip [$1,$2): $(python3 -I $S/colorfind.py build/android/recordings-2026-10-10/check-ladder-skip-$1-$2-1.mp4 | head -1 | cut -c1-100)"
