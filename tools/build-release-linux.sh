#!/usr/bin/env bash
# Branded release build (Linux), the counterpart of tools/build-release-win.bat: the launcher
# (SkiesOfArcadiaLegends; no spaces in a Linux binary name, like the Hybrid Heaven port's), the runner
# skies-run and Sys in lib/ModernGekko/build-release-linux, configured for Skies (the USA disc GEAE8P
# and the European GEAP8P, each with its widescreen DOL patch applied to the player's own main.dol and
# its game ini; user directory ~/.local/share/Skies of Arcadia Legends). EGL on, as the WSL dev
# runtime (build-egl).
#   tools/build-release-linux.sh configure | build | package USA.so [EUROPE.so] [VERSION]
# REGIONS="us eu" (configure) makes the release take the European disc too; the default is the USA
# disc alone, and package then needs only the USA module.
# package: build/SkiesOfArcadiaLegends-VERSION-linux.tar.gz (+ .sha256), one top-level folder.
set -euo pipefail
cd "$(dirname "$0")/.."
B=lib/ModernGekko/build-release-linux
case "${1:-}" in
configure)
  # Per region: disc ID, pinned DOL (the patch's output), DOL patch, game ini
  sha() { sed -n 's/^output-sha256 //p' "$1"; }
  IDS=GEAE8P SHAS=$(sha release/skies-widescreen.dolpatch)
  PATCHES=$PWD/release/skies-widescreen.dolpatch INIS=$PWD/release/GEAE8P.ini
  if [[ " ${REGIONS:-us} " == *" eu "* ]]; then
    IDS+=";GEAP8P" SHAS+=";$(sha release/skies-widescreen-eu.dolpatch)"
    PATCHES+=";$PWD/release/skies-widescreen-eu.dolpatch" INIS+=";$PWD/release/GEAP8P.ini"
  fi
  # Keep the build machine's paths (home directory, user name) out of the binaries
  MAP="-ffile-prefix-map=$PWD/="
  cmake -S lib/ModernGekko -B $B -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    -DDOLRECOMP_ENABLE_LLVM=OFF -DMODERNGEKKO_ENABLE_EGL=ON \
    "-DCMAKE_C_FLAGS=$MAP" "-DCMAKE_CXX_FLAGS=$MAP" \
    "-DMODERNGEKKO_FRONTEND_NAME=Skies of Arcadia Legends" -DMODERNGEKKO_LAUNCHER_OUTPUT_NAME=SkiesOfArcadiaLegends \
    -DMODERNGEKKO_RUNNER_OUTPUT_NAME=skies-run "-DMODERNGEKKO_USER_DIRECTORY_NAME=Skies of Arcadia Legends" \
    "-DMODERNGEKKO_REQUIRED_DISC_ID=$IDS" "-DMODERNGEKKO_REQUIRED_DOL_SHA256=$SHAS" \
    "-DMODERNGEKKO_DOL_PATCH_MANIFEST=$PATCHES" "-DMODERNGEKKO_GAME_SETTINGS_OVERRIDE=$INIS" \
    "-DMODERNGEKKO_DEFAULT_WINDOW_TITLE=Skies of Arcadia Legends" -DMODERNGEKKO_GAMECUBE_CONTROLLERS=ON
  ;;
build)
  cmake --build $B --target moderngekko-run moderngekko-launcher
  cp release/launcher-background.png release/CREDITS.md $B/
  ;;
package)
  MODULE=${2:?usage: tools/build-release-linux.sh package USA.so [EUROPE.so] [VERSION]}
  MODULE_EU=
  # An optional European module (a .so), then the version
  if [[ ${3:-} == *.so ]]; then MODULE_EU=$3; shift; fi
  VERSION=${3:-v0.9.0}
  NAME=SkiesOfArcadiaLegends-$VERSION-linux
  P=build/$NAME
  rm -rf "$P" "$P.tar.gz"; mkdir -p "$P/patches" "$P/licenses"
  cp $B/SkiesOfArcadiaLegends $B/skies-run $B/launcher-background.png "$P/"
  strip --strip-debug -o "$P/gGEAE8P_recomp.so" "$MODULE"
  [ -n "$MODULE_EU" ] && strip --strip-debug -o "$P/gGEAP8P_recomp.so" "$MODULE_EU"
  cp -r $B/Sys "$P/"
  cp release/skies-widescreen.dolpatch "$P/patches/"
  [ -n "$MODULE_EU" ] && cp release/skies-widescreen-eu.dolpatch "$P/patches/"
  cp release/README.md release/CREDITS.md release/THIRD-PARTY.md "$P/"
  cp release/skies-logo.png "$P/icon.png"
  cp COPYING "$P/COPYING.txt"
  cp lib/ModernGekko/vendor/dolphin/Externals/licenses.md "$P/THIRD-PARTY-DOLPHIN.md"
  cp -r lib/ModernGekko/vendor/dolphin/LICENSES/. "$P/licenses/"
  cp lib/libchdr/LICENSE.txt "$P/licenses/libchdr-LICENSE.txt"
  tar -C build -czf "$P.tar.gz" "$NAME"
  (cd build && sha256sum "$NAME.tar.gz" > "$NAME.tar.gz.sha256" && cat "$NAME.tar.gz.sha256")
  ;;
*)
  sed -n 2,8p "$0"; exit 1 ;;
esac
