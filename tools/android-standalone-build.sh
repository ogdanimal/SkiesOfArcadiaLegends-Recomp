#!/bin/bash
# The emulator-free host's Android test app, com.skiesofarcadia.standalone (docs/handoffs/HANDOFF-emulator-free.md):
# standalone/ cross-built for arm64 as libmain.so (SDL3 and Aurora linked in, Dawn's Android prebuilt), SDL's Java
# glue plus standalone/android, packed without Gradle (javac, d8, aapt2, zipalign, apksigner with the debug key).
#   tools/android-standalone-build.sh           -> build/standalone-apk/skies-standalone.apk
# MODULE= the arm64 recomp module(s) to bundle, space-separated (default: build/android/jniLibs-sa26's, made from the
# ws26 DOL with NAME=sa26 MODULE_ONLY=1 tools/android-ws-build.sh; each must match the DOL pushed to the device).
# The app loads the disc's own (libg<disc ID>_recomp.so) unless args.txt gives --module.
# AURORA_DEPS= Dawn's Android prebuilt and the abseil / sqlite3 / tracy / png sources (as android-ws-build.sh);
# SDL, fmt, xxhash and zstd come from the desktop build's FetchContent sources (build/standalone/_deps).
# No game files go into the APK: the game folder is pushed to the app's external files folder (see the handoff).
set -eu
cd "$(dirname "$0")/.."
SDK=${ANDROID_HOME:-$HOME/Android/Sdk}
NDK=${NDK:-$SDK/ndk/29.0.14206865}
BT=$SDK/build-tools/36.0.0
JAR=$SDK/platforms/android-36/android.jar
B=build/standalone-android
O=build/standalone-apk
D=$PWD/build/standalone/_deps
AD=${AURORA_DEPS:-$HOME/projects/skies-aurora/android-deps}
MODULE=${MODULE:-build/android/jniLibs-sa26/arm64-v8a/libgGEAE8P_recomp.so}
export PATH=/usr/lib/llvm-20/bin:$PATH
for m in $MODULE; do [ -f "$m" ] || { echo "no module at $m"; exit 1; }; done
[ -d "$D/sdl-src" ] || { echo "build the desktop standalone first (its FetchContent sources)"; exit 1; }

if [ ! -f $B/build.ninja ]; then
  cmake -S standalone -B $B -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 \
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON -DFETCHCONTENT_SOURCE_DIR_DAWN_PREBUILT=$AD/dawn-android-aarch64 \
    -DFETCHCONTENT_SOURCE_DIR_ABSEIL-CPP=$AD/src/abseil-cpp -DFETCHCONTENT_SOURCE_DIR_SQLITE3=$AD/src/sqlite3 \
    -DFETCHCONTENT_SOURCE_DIR_TRACY=$AD/src/tracy -DFETCHCONTENT_SOURCE_DIR_PNG=$AD/src/png \
    -DFETCHCONTENT_SOURCE_DIR_SDL=$D/sdl-src -DFETCHCONTENT_SOURCE_DIR_XXHASH=$D/xxhash-src \
    -DFETCHCONTENT_SOURCE_DIR_ZSTD=$D/zstd-src -DFETCHCONTENT_SOURCE_DIR_FMT=$D/fmt-src > $B-configure.log 2>&1 ||
    { tail -20 $B-configure.log; exit 1; }
fi
cmake --build $B -j8 --target skies-standalone > $B-build.log 2>&1 || { grep -E "error|FAILED" $B-build.log | head -20; exit 1; }

rm -rf "${O:?}"
mkdir -p $O/classes $O/dex $O/sdl-java
cp $D/sdl-src/android-project/app/src/main/java/org/libsdl/app/*.java $O/sdl-java/
# Aurora draws only while its surface is marked ready (window.cpp, Java_org_libsdl_app_SDLSurface_
# auroraNativeSetSurfaceReady), which stock SDL never calls: patch the copy of SDLSurface.java
python3 -I - "$O/sdl-java/SDLSurface.java" <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read()
edits = [
    ("        mIsSurfaceReady = false;\n        SDLActivity.onNativeSurfaceDestroyed();",
     "        auroraNativeSetSurfaceReady(false);\n        mIsSurfaceReady = false;\n"
     "        SDLActivity.onNativeSurfaceDestroyed();", 1),
    ("        /* Surface is ready */\n        mIsSurfaceReady = true;",
     "        /* Surface is ready */\n        mIsSurfaceReady = true;\n        auroraNativeSetSurfaceReady(true);", 1),
]
for old, new, count in edits:
    assert s.count(old) == count, old
    s = s.replace(old, new)
brace = s.index("{", s.index("public class SDLSurface"))
s = s[:brace + 1] + "\n    public static native void auroraNativeSetSurfaceReady(boolean ready);\n" + s[brace + 1:]
open(p, "w").write(s)
EOF
javac --release 11 -classpath $JAR -d $O/classes \
  $O/sdl-java/*.java \
  standalone/android/java/com/skiesofarcadia/standalone/*.java
$BT/d8 --release --min-api 28 --lib $JAR --output $O/dex $(find $O/classes -name '*.class')
$BT/aapt2 link -o $O/base.apk --manifest standalone/android/AndroidManifest.xml -I $JAR \
  --min-sdk-version 28 --target-sdk-version 34
STRIP=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip
mkdir -p $O/lib
$STRIP --strip-debug -o $O/lib/libmain.so $B/libmain.so
$STRIP --strip-debug -o $O/lib/libpng16.so $B/_deps/png-build/libpng16.so
# as the disc ID's library name (android-ws-build.sh's are named so already)
for m in $MODULE; do
  b=$(basename "$m"); case $b in lib*) ;; *) b=lib$b ;; esac
  cp "$m" "$O/lib/$b"
done
# dex and native libraries stored, not deflated
python3 -I - "$O/base.apk" "$O/dex/classes.dex" $O/lib/*.so <<'EOF'
import os, sys, zipfile
apk, dex, libs = sys.argv[1], sys.argv[2], sys.argv[3:]
with zipfile.ZipFile(apk, "a", zipfile.ZIP_STORED) as z:
    z.write(dex, "classes.dex")
    for lib in libs:
        z.write(lib, "lib/arm64-v8a/" + os.path.basename(lib))
EOF
$BT/zipalign -P 16 -f 4 $O/base.apk $O/aligned.apk
$BT/apksigner sign --ks $HOME/.android/debug.keystore --ks-pass pass:android --out $O/skies-standalone.apk $O/aligned.apk
rm -f $O/base.apk $O/aligned.apk
ls -la $O/skies-standalone.apk
