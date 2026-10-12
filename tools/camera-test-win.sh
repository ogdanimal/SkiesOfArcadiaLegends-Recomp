#!/usr/bin/env bash
TOOLS=$(dirname "$(readlink -f "$0")")  # before any cd
# Camera control check on Windows (hold Z: C-stick tilts / turns, triggers zoom; camera.c + SkiesMenu).
# usage: tools/camera-test-win.sh GAME MODULE STATE NAME   -> build/camtest-NAME/*.png and cam_input dumps
set -u
cd "${WIN_ROOT:-/mnt/c/dev/SkiesRecomp}"
GAME=$1 MODULE=$2 STATE=$3 NAME=$4
U=build/user-camtest A=build/auto-camtest O=$(pwd)/build/camtest-$NAME
rm -rf "$U" "$A" "$O"; mkdir -p "$A/commands" "$O"; cp -r build/user-bt "$U"
cmd.exe /c "lib\\ModernGekko\\build\\moderngekko-run.exe --game $GAME --module $MODULE --user-dir build\\user-camtest --automation-dir build\\auto-camtest > build\\auto-camtest\\run.log 2>&1" &
D="python3 $TOOLS/drive.py"
d() { AUTO=$A timeout 120 $D "$@" 2>&1 | grep -v "^shot" | grep -v "^$"; }
d wait:6 load:build/savestates/$STATE.sav wait:3 shot:$O/0-base.png peek:0x80002EC0:0x20
d press:z=1,c_y=1 wait:0.8 release wait:0.5 shot:$O/1-tilt-up.png peek:0x80002EC0:0x20
d press:z=1,l_analog=1 wait:0.8 release wait:0.5 shot:$O/2a-zoom-in.png press:z=1,r_analog=1 wait:0.8 release wait:0.5 shot:$O/2-zoom-out.png peek:0x80002EC0:0x20
d press:z=1,c_x=1 wait:0.6 shot:$O/3-turn-held.png release wait:1.5 shot:$O/4-turn-released.png peek:0x80002EC0:0x20
d press:z=1,c_y=-1,l_analog=1 wait:1.5 release wait:0.5 shot:$O/5-tilt-down-zoom-in.png peek:0x80002EC0:0x20
d tap:z:4 wait:1.5 shot:$O/6-z-tap.png
d stop; wait
