#!/usr/bin/env bash
# CI: Dawn with patches/dawn/ (MSAA 2x / 8x, the Vulkan swapchain check) as the package Aurora's
# AURORA_DAWN_PROVIDER=package takes, for one platform, into OUT_DIR. .github/workflows/standalone-host.yml runs it behind
# a cache and builds the standalone against the package (DAWN_PACKAGE); a release fails without it, other runs fall
# back to the stock prebuilt (anti-aliasing Off / 4x only).
#   tools/ci-dawn.sh linux|android|darwin|windows OUT_DIR
# linux / android / darwin: tools/build-dawn-package.sh (it clones encounter/dawn at AURORA_DAWN_REF and applies
# the patches). windows (Git Bash, MSVC set up by the job): the same tree prepared here, the DXC DLLs taken from
# encounter's stock package of the same version (AURORA_DAWN_VERSION), then tools\build-dawn-package.bat.
# Prints the package's path last.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
TARGET=${1:?usage: tools/ci-dawn.sh linux|android|darwin|windows OUT_DIR}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "${2:?}"
OUT=$(cd "$2" && pwd)
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu)
export JOBS
AURORA_CMAKE=$ROOT/lib/ModernGekko/vendor/dolphin/GXRuntime/graphics/aurora/CMakeLists.txt
REF=$(sed -n 's/^_aurora_dependency_version(AURORA_DAWN_REF "\([0-9a-f]*\)".*/\1/p' "$AURORA_CMAKE")
VERSION=$(sed -n 's/^_aurora_dependency_version(AURORA_DAWN_VERSION "\([^"]*\)".*/\1/p' "$AURORA_CMAKE")
[ -n "$REF" ] && [ -n "$VERSION" ] || { echo "AURORA_DAWN_REF / VERSION not found in $AURORA_CMAKE" >&2; exit 1; }

case $TARGET in
  linux | android | darwin)
    "$ROOT/tools/build-dawn-package.sh" "$TARGET" "$OUT"
    ;;
  windows)
    SRC=$OUT/src
    if [ ! -d "$SRC/.git" ]; then
      git clone --filter=blob:none https://github.com/encounter/dawn "$SRC"
      git -C "$SRC" checkout --detach "$REF"
      git -C "$SRC" -c user.name=build -c user.email=build@localhost am "$ROOT"/patches/dawn/*.patch
    fi
    # The .bat configures with DAWN_FETCH_DEPENDENCIES=OFF (a local tree has its third_party/ already); a fresh
    # clone gets it here, with the script that option would run
    PY=$(command -v python3 || command -v python)
    "$PY" "$SRC/tools/fetch_dawn_dependencies.py" --directory "$SRC"
    # DXC isn't built: Dawn loads dxcompiler.dll / dxil.dll at run time, the stock package's
    STOCK=$OUT/stock
    mkdir -p "$STOCK"
    curl -fsSL -o "$STOCK/dawn-windows-amd64.tar.gz" \
      "https://github.com/encounter/dawn/releases/download/$VERSION/dawn-windows-amd64.tar.gz"
    tar -xzf "$STOCK/dawn-windows-amd64.tar.gz" -C "$STOCK"
    DXC=$(dirname "$(find "$STOCK" -iname dxcompiler.dll | head -1)")
    [ -f "$DXC/dxil.dll" ] || { echo "the stock package has no dxcompiler.dll / dxil.dll" >&2; exit 1; }
    cmd //c "$(cygpath -w "$ROOT/tools/build-dawn-package.bat")" "$(cygpath -w "$SRC")" "$(cygpath -w "$DXC")" \
      "$(cygpath -w "$OUT")"
    ;;
  *) echo "unknown target $TARGET" >&2; exit 1 ;;
esac
ls "$OUT"/dawn-*.tar.gz
