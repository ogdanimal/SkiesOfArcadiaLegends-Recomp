#!/usr/bin/env bash
# Log the device's clocks, GPU load and temperatures twice a second while a benchmark runs, so a
# slow second in drive.py's SPEED_TRACE (its wall= column) can be checked against throttling.
# The device loop uses only shell builtins (no forks), so it costs next to nothing.
# It also records key/button/touch input (getevent) into LOCAL_FILE.input: a press on the device
# during a run (someone holding it) changes what the savestate plays, so `stop` warns about it.
# Usage: tools/android-sampler.sh start | stop LOCAL_FILE
# Columns: epoch cpu7_khz cpu4_khz gpu_hz gpu_busy cpu7_mC gpu_mC skin_mC
set -euo pipefail
# Windows' adb.exe from WSL (WSL's own adb server can't see the device); SERIAL picks one of several.
ADB=${ADB:-adb.exe}
SERIAL=${SERIAL:-}
LOG=/data/local/tmp/skies-sampler.txt
PID=/data/local/tmp/skies-sampler.pid
INPUT=/data/local/tmp/skies-input.txt
IPID=/data/local/tmp/skies-input.pid
adb() { "$ADB" ${SERIAL:+-s "$SERIAL"} "$@"; }
case ${1:?usage: android-sampler.sh start | stop LOCAL_FILE} in
start)
  adb shell "for p in $PID $IPID; do [ -f \$p ] && kill \$(cat \$p) 2>/dev/null; done; rm -f $LOG $PID $INPUT $IPID; true"
  adb shell "setsid nohup sh -c 'echo \$\$ > $IPID; exec getevent -lt' > $INPUT 2>/dev/null < /dev/null &"
  # The thermal zone numbers are the Retroid Pocket 5's (cpu-1-7-usr, gpuss-0-usr, skin-therm-usr).
  adb shell "setsid nohup sh -c 'echo \$\$ > $PID; C=/sys/devices/system/cpu; K=/sys/class/kgsl/kgsl-3d0; T=/sys/class/thermal
    while :; do read a < \$C/cpu7/cpufreq/scaling_cur_freq; read b < \$C/cpu4/cpufreq/scaling_cur_freq
      read g < \$K/gpuclk; read u p < \$K/gpu_busy_percentage
      read t7 < \$T/thermal_zone14/temp; read tg < \$T/thermal_zone15/temp; read ts < \$T/thermal_zone84/temp
      echo \$EPOCHREALTIME \$a \$b \$g \$u \$t7 \$tg \$ts; sleep 0.5; done' > $LOG 2>&1 < /dev/null &"
  ;;
stop)
  adb shell "[ -f $PID ] && kill \$(cat $PID); rm -f $PID; cat $LOG; rm -f $LOG" > "${2:?stop needs LOCAL_FILE}"
  adb shell "[ -f $IPID ] && kill \$(cat $IPID); rm -f $IPID; grep -E 'EV_KEY|BTN_TOUCH' $INPUT; rm -f $INPUT" > "$2.input" || true
  presses=$(grep -c 'DOWN' "$2.input" || true)
  (( presses == 0 )) || echo "WARNING: $presses button/touch presses on the device during this run ($2.input)" >&2
  ;;
esac
