#!/usr/bin/env bash
# The Java methods the emulator-free game's native code needs are still in an APK's dex: those SDL's
# JNI_OnLoad registers (its JNINativeMethod tables), those its C code looks up by name
# (GetStaticMethodID / GetMethodID), and the host's own (main.cpp: JNIEXPORT Java_..._NAME natives, GetMethodID).
# R8 removes Java methods only native code uses; a missing one aborted the v0.90.0-standalone.6 / .7 APKs at
# Start Game on every device (issue #34). The release workflow runs it on the APK; locally:
#   tools/check-apk-jni.sh APK [SOURCE_ROOT]   (SOURCE_ROOT: the repository, default this one)
# An APK without the emulator-free host (no SkiesGameActivity) passes: it has none of this.
set -euo pipefail
export LC_ALL=C
APK=$(realpath "$1")
ROOT=$(realpath "${2:-$(dirname "$0")/..}")
SDL=$ROOT/lib/ModernGekko/vendor/dolphin/Externals/SDL/SDL/src/core/android/SDL_android.c
HOST=$ROOT/standalone/src/main.cpp
[ -f "$SDL" ] && [ -f "$HOST" ] || { echo "check-apk-jni: no $SDL or $HOST"; exit 2; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
unzip -q -o "$APK" 'classes*.dex' -d "$W"
{
  # { "name", "(sig)", ... } entries of the JNINativeMethod tables
  grep -oE '\{ *"[A-Za-z_][A-Za-z0-9_]*", *"\(' "$SDL" | sed -E 's/\{ *"([^"]*)".*/\1/'
  # Get(Static)MethodID(env, class, "name", ...)
  grep -ohE 'Get(Static)?MethodID\([^"]*"[A-Za-z_][A-Za-z0-9_]*"' "$SDL" "$HOST" | sed -E 's/.*"([^"]*)"$/\1/'
  # The host's natives: Java_org_dolphinemu_dolphinemu_skies_CLASS_NAME
  grep -oE 'Java_org_dolphinemu_dolphinemu_skies_[A-Za-z]+_[A-Za-z0-9]+' "$HOST" | sed -E 's/.*_//'
} | grep -vx '<init>' | sort -u > "$W/needed"
# Every dex's string pool (string_ids: offsets of ULEB128-length-prefixed MUTF-8 strings) holds the method
# names and class descriptors; compared in Python (a few strings have newlines in them)
python3 -I - "$APK" "$W/needed" "$W"/classes*.dex <<'PY'
import os, struct, sys
apk, needed_path, dexes = sys.argv[1], sys.argv[2], sys.argv[3:]
pool = set()
for path in dexes:
    d = open(path, 'rb').read()
    size, off = struct.unpack_from('<II', d, 0x38)
    for i in range(size):
        (at,) = struct.unpack_from('<I', d, off + 4 * i)
        while d[at] & 0x80:  # the length (UTF-16 units), unneeded
            at += 1
        at += 1
        pool.add(d[at:d.index(b'\0', at)].decode('utf-8', 'replace'))
name = os.path.basename(apk)
if 'Lorg/dolphinemu/dolphinemu/skies/SkiesGameActivity;' not in pool:
    print('check-apk-jni: no emulator-free host in %s; nothing to check' % name)
    sys.exit(0)
needed = [n for n in open(needed_path).read().split() if n]
missing = [n for n in needed if n not in pool]
if missing:
    print('check-apk-jni: %s lacks Java methods its native code registers or calls:' % name)
    for n in missing:
        print('  ' + n)
    print('(R8 removed them: keep them in Source/Android/app/proguard-rules.pro, '
          'patches/recompcore-android-standalone-r8.patch)')
    sys.exit(1)
print('check-apk-jni: all %d Java methods the native code needs are in %s' % (len(needed), name))
PY
