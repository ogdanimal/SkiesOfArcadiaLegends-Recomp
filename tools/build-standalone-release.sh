#!/usr/bin/env bash
# The emulator-free host (standalone/) as a release's runner (Linux and macOS; Windows:
# tools\build-standalone-release.bat): Release, the three discs' DOL patches as its release table (each
# patch's output-sha256 is the DOL it pins, as the Dolphin runner's MODERNGEKKO_REQUIRED_DOL_SHA256), the
# stock Dawn package. The release scripts then ship it as skies-run (STANDALONE=DIR there).
#   tools/build-standalone-release.sh [BUILD_DIR]     (default build/standalone-release)
set -euo pipefail
cd "$(dirname "$0")/.."
B=${1:-build/standalone-release}
IDS="GEAE8P;GEAP8P;GEAJ8P"
PATCHES="$PWD/release/skies-widescreen.dolpatch;$PWD/release/skies-widescreen-eu.dolpatch;$PWD/release/skies-widescreen-jp.dolpatch"
# Keep the build machine's paths (home directory, user name) out of the binary
MAP="-ffile-prefix-map=$PWD/="
# libpng fetched and linked in, never the build machine's (the macOS runner's is Homebrew's, which a player's
# Mac doesn't have: v0.90.0-standalone.1 linked /opt/homebrew/opt/libpng)
EXTRA=(-DCMAKE_DISABLE_FIND_PACKAGE_PNG=ON)
PKG=()
if [ "$(uname -s)" = Linux ]; then
  # Dawn's prebuilt needs GCC 13's libstdc++, linked in (CXX=g++-13 on Ubuntu 22.04: CMakeLists.txt)
  EXTRA+=(-DSKIES_STATIC_LIBSTDCXX=ON)
fi
if [ "$(uname -s)" = Darwin ]; then
  # Nothing from Homebrew (v0.90.0-standalone.3 linked its libzstd: Aurora's find_package / pkg-config found it):
  # its prefixes ignored and no .pc files (Linux keeps pkg-config: SDL3 finds X11 / Wayland through it). Ninja
  # by its path, which the ignored prefixes would otherwise hide.
  PKG=(env PKG_CONFIG_LIBDIR=/nonexistent PKG_CONFIG_PATH=)
  EXTRA+=("-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew;/usr/local" "-DCMAKE_MAKE_PROGRAM=$(command -v ninja)")
  EXTRA+=(-DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 "-DCMAKE_OBJCXX_FLAGS=$MAP")
fi
# DAWN_PACKAGE=<dawn-*.tar.gz>: Dawn with patches/dawn/ (tools/ci-dawn.sh; MSAA 2x / 8x) instead of the stock prebuilt
if [ -n "${DAWN_PACKAGE:-}" ]; then
  EXTRA+=("-DAURORA_DAWN_PACKAGE_URL=file://$(cd "$(dirname "$DAWN_PACKAGE")" && pwd)/$(basename "$DAWN_PACKAGE")")
fi
${PKG[@]+"${PKG[@]}"} cmake -S standalone -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  "-DSKIES_RELEASE_DISC_IDS=$IDS" "-DSKIES_RELEASE_DOL_PATCHES=$PATCHES" \
  "-DCMAKE_C_FLAGS=$MAP" "-DCMAKE_CXX_FLAGS=$MAP" "${EXTRA[@]}"
cmake --build "$B" --target skies-standalone
ls -la "$B/skies-standalone"
