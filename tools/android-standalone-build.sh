#!/bin/bash
# The emulator-free host's Android test app, com.skiesofarcadia.standalone (docs/handoffs/HANDOFF-emulator-free.md):
# standalone/ cross-built for arm64 as libskies.so (SDL3 and Aurora linked in, Dawn's Android prebuilt), SDL's Java
# glue plus standalone/android, packed without Gradle (javac, d8, aapt2, zipalign, apksigner with the debug key).
#   tools/android-standalone-build.sh           -> build/standalone-apk/skies-standalone.apk
# MODULE= the arm64 recomp module(s) to bundle, space-separated (default: build/android/jniLibs-sa26's, made from the
# ws26 DOL with NAME=sa26 MODULE_ONLY=1 tools/android-ws-build.sh; each must match the DOL pushed to the device).
# The app loads the disc's own (libg<disc ID>_recomp.so) unless args.txt gives --module.
# DAWN_SRC= a Dawn source tree to build instead of the prebuilt (the extra MSAA sample counts patch,
# patches/dawn/; its third_party fetched), into B= / O= (default build/standalone-android-dawnsrc,
# build/standalone-apk-dawnsrc) so the prebuilt build stays as it is.
# DAWN_PACKAGE= a Dawn package tarball instead (tools/build-dawn-package.sh android: the prebuilt's layout with
# patches/dawn/ applied), into B= / O= (default build/standalone-android-dawnpkg, build/standalone-apk-dawnpkg).
# AURORA_DEPS= Dawn's Android prebuilt and the abseil / sqlite3 / tracy / png sources (as android-ws-build.sh);
# SDL, fmt, xxhash and zstd come from the desktop build's FetchContent sources (build/standalone/_deps).
# LIB_ONLY=1 (tools/android-ws-build.sh STANDALONE=1): only O/lib/libskies.so + libpng16.so and O/app-java (SDL's
# patched Java glue + standalone/android/app/java: the branded app's game screen), no module and no APK.
# No game files go into the APK: the game folder is pushed to the app's external files folder (see the handoff).
set -eu
cd "$(dirname "$0")/.."
SDK=${ANDROID_HOME:-$HOME/Android/Sdk}
NDK=${NDK:-$SDK/ndk/29.0.14206865}
BT=$SDK/build-tools/36.0.0
JAR=$SDK/platforms/android-36/android.jar
AD=${AURORA_DEPS:-$HOME/projects/skies-aurora/android-deps}
DAWN_PREBUILT=$AD/dawn-android-aarch64
if [ -n "${DAWN_PACKAGE:-}" ]; then
  B=${B:-build/standalone-android-dawnpkg}
  O=${O:-build/standalone-apk-dawnpkg}
  DAWN_PREBUILT=$PWD/$B-dawn
  if [ ! -f "$DAWN_PREBUILT/lib/cmake/Dawn/DawnConfig.cmake" ]; then
    mkdir -p "$DAWN_PREBUILT"
    tar -xzf "$DAWN_PACKAGE" -C "$DAWN_PREBUILT"
  fi
  DAWN_ARGS=""
elif [ -n "${DAWN_SRC:-}" ]; then
  B=${B:-build/standalone-android-dawnsrc}
  O=${O:-build/standalone-apk-dawnsrc}
  DAWN_ARGS="-DSKIES_DAWN_PROVIDER=vendor -DFETCHCONTENT_SOURCE_DIR_DAWN=$DAWN_SRC -DDAWN_BUILD_PROTOBUF=OFF"
else
  B=${B:-build/standalone-android}
  O=${O:-build/standalone-apk}
  DAWN_ARGS=""
fi
D=${STANDALONE_DEPS:-$PWD/build/standalone/_deps}
MODULE=${MODULE:-build/android/jniLibs-sa26/arm64-v8a/libgGEAE8P_recomp.so}
export PATH=/usr/lib/llvm-20/bin:$PATH
if [ "${LIB_ONLY:-0}" != 1 ]; then
  for m in $MODULE; do [ -f "$m" ] || { echo "no module at $m"; exit 1; }; done
fi
# Local sources when they are there (offline rebuilds); otherwise (CI) CMake fetches them, Dawn's stock package too
SRC_ARGS=()
if [ -d "$D/sdl-src" ]; then
  SRC_ARGS+=(-DFETCHCONTENT_SOURCE_DIR_SDL=$D/sdl-src -DFETCHCONTENT_SOURCE_DIR_XXHASH=$D/xxhash-src
    -DFETCHCONTENT_SOURCE_DIR_ZSTD=$D/zstd-src -DFETCHCONTENT_SOURCE_DIR_FMT=$D/fmt-src)
fi
if [ -d "$AD/src" ]; then
  SRC_ARGS+=(-DFETCHCONTENT_SOURCE_DIR_ABSEIL-CPP=$AD/src/abseil-cpp -DFETCHCONTENT_SOURCE_DIR_SQLITE3=$AD/src/sqlite3
    -DFETCHCONTENT_SOURCE_DIR_TRACY=$AD/src/tracy -DFETCHCONTENT_SOURCE_DIR_PNG=$AD/src/png)
fi
if [ -d "$DAWN_PREBUILT" ] && [ -z "${DAWN_SRC:-}" ]; then
  SRC_ARGS+=(-DFETCHCONTENT_SOURCE_DIR_DAWN_PREBUILT=$DAWN_PREBUILT)
fi
[ -d "$D/sdl-src" ] && [ -d "$AD/src" ] && SRC_ARGS+=(-DFETCHCONTENT_FULLY_DISCONNECTED=ON)

if [ ! -f $B/build.ninja ]; then
  # No host pkg-config files: a cross build would take the build machine's libzstd (CI: zstd.h not found)
  PKG_CONFIG_LIBDIR=/nonexistent PKG_CONFIG_PATH= cmake -S standalone -B $B -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 \
    "-DCMAKE_C_FLAGS=-ffile-prefix-map=$PWD/=" "-DCMAKE_CXX_FLAGS=-ffile-prefix-map=$PWD/=" \
    $DAWN_ARGS "${SRC_ARGS[@]}" ${STANDALONE_CMAKE_ARGS:-} > $B-configure.log 2>&1 ||
    { tail -20 $B-configure.log; exit 1; }
fi
# As many jobs as cores (CI runners have 4; -j8 there can run out of memory); the end of the log on any failure
cmake --build $B -j$(nproc) --target skies-standalone > $B-build.log 2>&1 ||
  { grep -E "error|FAILED" $B-build.log | head -20; echo "--- end of $B-build.log:"; tail -30 $B-build.log; exit 1; }

rm -rf "${O:?}"
mkdir -p $O/classes $O/dex $O/sdl-java
SDL_SRC=$D/sdl-src
[ -d "$SDL_SRC" ] || SDL_SRC=$B/_deps/sdl-src
cp $SDL_SRC/android-project/app/src/main/java/org/libsdl/app/*.java $O/sdl-java/
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
STRIP=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip
mkdir -p $O/lib
$STRIP --strip-debug -o $O/lib/libskies.so $B/libskies.so
$STRIP --strip-debug -o $O/lib/libpng16.so $B/_deps/png-build/libpng16.so
# the custom GPU driver shim (standalone/src/android_vkshim.c)
if [ -f $B/libskiesvk.so ]; then $STRIP --strip-debug -o $O/lib/libskiesvk.so $B/libskiesvk.so; fi
if [ "${LIB_ONLY:-0}" = 1 ]; then
  mkdir -p $O/app-java/org/libsdl/app
  cp $O/sdl-java/*.java $O/app-java/org/libsdl/app/
  cp -r standalone/android/app/java/. $O/app-java/
  ls -la $O/lib
  exit 0
fi
javac --release 11 -classpath $JAR -d $O/classes \
  $O/sdl-java/*.java \
  standalone/android/java/com/skiesofarcadia/standalone/*.java
$BT/d8 --release --min-api 28 --lib $JAR --output $O/dex $(find $O/classes -name '*.class')
$BT/aapt2 link -o $O/base.apk --manifest standalone/android/AndroidManifest.xml -I $JAR \
  --min-sdk-version 28 --target-sdk-version 34
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
