#!/usr/bin/env bash
# (Re)start Skies in the Android build and wait until the automation directory reports
# a running core. Then drive it with ADB_AUTO=... tools/drive.py.
set -euo pipefail
# Windows' adb.exe from WSL (WSL's own adb server can't see the device); SERIAL picks one of several.
ADB=${ADB:-adb.exe}
SERIAL=${SERIAL:-}
PKG=${PKG:-org.dolphinemu.dolphinemu.debug}
GAME=${GAME:-GEAE8P}  # GEAP8P: the European game (its folder in the app's ISOPath list)
AUTO=/sdcard/Android/data/$PKG/files/Automation
adb() { "$ADB" ${SERIAL:+-s "$SERIAL"} "$@"; }
# Wake the screen first: launching the activity while the device sleeps never reaches state=running.
adb shell input keyevent KEYCODE_WAKEUP
adb shell wm dismiss-keyguard
adb shell "am force-stop $PKG; mkdir -p $AUTO; rm -f $AUTO/status.txt $AUTO/commands/*"
adb logcat -c
adb shell am start -n "$PKG/org.dolphinemu.dolphinemu.activities.AppLinkActivity" \
  -d dolphinemu://app/play/0/$GAME >/dev/null
for _ in $(seq 120); do
  adb shell "grep -q state=running $AUTO/status.txt 2>/dev/null" && exit 0
  sleep 1
done
echo "boot timed out" >&2
exit 1
