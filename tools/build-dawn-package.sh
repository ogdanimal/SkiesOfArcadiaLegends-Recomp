#!/bin/bash
# A Dawn package with our patches (patches/dawn/: the 2x / 8x MSAA sample counts, the Vulkan swapchain check): the
# install tree tarball Aurora's AURORA_DAWN_PROVIDER=package takes, built from encounter/dawn at Aurora's
# AURORA_DAWN_REF the way encounter's own release workflow builds the prebuilt ones (.github/workflows/release.yml
# and dawn-ci.cmake in that tree). The standalone then builds against it with
#   -DSKIES_DAWN_PROVIDER=package -DAURORA_DAWN_PACKAGE_URL=file:///<OUT>/dawn-<system>-<arch>.tar.gz
# (Windows: tools/build-dawn-package.bat).
#
# usage: tools/build-dawn-package.sh linux|android|darwin [OUT_DIR]   (default OUT_DIR: build/dawn-package)
#   DAWN_SRC=<tree>  an existing tree at AURORA_DAWN_REF with the patches applied (checked), instead of a clone
#                    into OUT_DIR/src (encounter/dawn, the patches applied with git am, dependencies fetched by
#                    Dawn's own configure step)
#   JOBS=N           parallel compiles (default 6: Dawn's biggest units take about 1 GB each)
#   ANDROID_NDK      default ~/Android/Sdk/ndk/29.0.14206865 (encounter's NDK version)
#   NO_X11=1         Linux without X11 (a machine without libx11-xcb-dev): for local tests only, so the package is
#                    named dawn-linux-x86_64-nox11.tar.gz and never stands in for a release one
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
TARGET=${1:?usage: tools/build-dawn-package.sh linux|android|darwin [OUT_DIR]}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$(mkdir -p "${2:-$ROOT/build/dawn-package}" && cd "${2:-$ROOT/build/dawn-package}" && pwd)
JOBS=${JOBS:-6}
AURORA_CMAKE=$ROOT/lib/ModernGekko/vendor/dolphin/GXRuntime/graphics/aurora/CMakeLists.txt
REF=$(sed -n 's/^_aurora_dependency_version(AURORA_DAWN_REF "\([0-9a-f]*\)".*/\1/p' "$AURORA_CMAKE")
[ -n "$REF" ] || { echo "AURORA_DAWN_REF not found in $AURORA_CMAKE" >&2; exit 1; }

if [ -n "${DAWN_SRC:-}" ]; then
  SRC=$(cd "$DAWN_SRC" && pwd)
else
  SRC=$OUT/src
  if [ ! -d "$SRC/.git" ]; then
    git clone --filter=blob:none https://github.com/encounter/dawn "$SRC"
    git -C "$SRC" checkout --detach "$REF"
    git -C "$SRC" -c user.name=build -c user.email=build@localhost am "$ROOT"/patches/dawn/*.patch
  fi
fi
# The patches must be in the tree: for each file a patch changes, the first line it adds there appears in it
for p in "$ROOT"/patches/dawn/*.patch; do
  checked=0
  while IFS=$'\t' read -r file line; do
    grep -qF -- "$line" "$SRC/$file" || { echo "$SRC lacks $(basename "$p") ($file)" >&2; exit 1; }
    checked=$((checked + 1))
  done < <(awk '/^\+\+\+ b\// { file = substr($0, 7); next }
                /^\+\+\+ / { file = ""; next }
                file != "" && /^\+[^+]/ { print file "\t" substr($0, 2); file = "" }' "$p")
  [ "$checked" -gt 0 ] || { echo "$(basename "$p") adds no line to check" >&2; exit 1; }
done

# dawn-ci.cmake reads CMAKE_SYSTEM_NAME, so the target options come first (as in release.yml); LATE ones after
# it override its values
ARGS=() LATE=()
case $TARGET in
  linux)
    NAME=dawn-linux-x86_64
    if [ "${NO_X11:-0}" = 1 ]; then
      LATE+=(-DDAWN_USE_X11=OFF)
      NAME+=-nox11
    elif ! pkg-config --exists x11-xcb; then
      echo "libx11-xcb-dev is missing (encounter's packages have X11); install it, or NO_X11=1 for a test package" >&2
      exit 1
    fi
    ;;
  android)
    NAME=dawn-android-aarch64
    NDK=${ANDROID_NDK:-$HOME/Android/Sdk/ndk/29.0.14206865}
    ARGS+=(-DCMAKE_SYSTEM_NAME=Android -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake"
           -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 -DCMAKE_SYSTEM_PROCESSOR=aarch64
           -DDAWN_BUILD_PROTOBUF=OFF)
    ;;
  darwin)
    NAME=dawn-darwin-arm64
    ARGS+=(-DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0)
    ;;
  *) echo "unknown target $TARGET (linux, android, darwin)" >&2; exit 1 ;;
esac

ARGS+=(-C "$SRC/.github/workflows/dawn-ci.cmake" -DCMAKE_BUILD_TYPE=Release)
BUILD=$OUT/build-$NAME INSTALL=$OUT/install-$NAME
cmake -S "$SRC" -B "$BUILD" -G Ninja "${ARGS[@]}" ${LATE[@]+"${LATE[@]}"}
cmake --build "$BUILD" --config Release -j "$JOBS"
rm -rf "${INSTALL:?}"
cmake --install "$BUILD" --config Release --prefix "$INSTALL"
if [ "$TARGET" = android ]; then
  "$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" --strip-debug "$INSTALL/lib/libwebgpu_dawn.a"
fi
grep -q DAWN_NATIVE_EXTRA_SAMPLE_COUNTS "$INSTALL/include/dawn/native/DawnNative.h" ||
  { echo "the installed headers lack DAWN_NATIVE_EXTRA_SAMPLE_COUNTS" >&2; exit 1; }
tar -czf "$OUT/$NAME.tar.gz" -C "$INSTALL" .
echo "$OUT/$NAME.tar.gz (encounter/dawn $REF + $(ls "$ROOT"/patches/dawn/*.patch | wc -l) patches)"
