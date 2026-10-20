#!/usr/bin/env bash
# Branded release build (macOS, Apple Silicon), the counterpart of tools/build-release-linux.sh: the
# launcher, the runner skies-run and Sys in lib/ModernGekko/build-release-macos, configured for Skies
# (the USA disc GEAE8P, the European GEAP8P and the Japanese GEAJ8P, each with its widescreen DOL patch
# applied to the player's own main.dol and its game ini), packaged as "Skies of Arcadia Legends.app". Metal is the
# graphics backend (Vulkan would need MoltenVK, which needs a full Xcode to build); OpenGL is the
# fallback.
#   tools/build-release-macos.sh configure | build | package USA.dylib [EUROPE.dylib] [JAPAN.dylib] [VERSION]
# REGIONS="us eu jp" (configure) makes the release take the European and the Japanese disc too ("us eu":
# the European one); the default is the USA disc alone, and package then needs only the USA module.
# package: build/SkiesOfArcadiaLegends-VERSION-macos.zip (+ .sha256), the .app and the documents.
set -euo pipefail
cd "$(dirname "$0")/.."
B=lib/ModernGekko/build-release-macos
APP_NAME="Skies of Arcadia Legends"
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
  if [[ " ${REGIONS:-us} " == *" jp "* ]]; then
    IDS+=";GEAJ8P" SHAS+=";$(sha release/skies-widescreen-jp.dolpatch)"
    PATCHES+=";$PWD/release/skies-widescreen-jp.dolpatch" INIS+=";$PWD/release/GEAJ8P.ini"
  fi
  # Keep the build machine's paths (home directory, user name) out of the binaries
  MAP="-ffile-prefix-map=$PWD/="
  cmake -S lib/ModernGekko -B $B -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
    -DDOLRECOMP_ENABLE_LLVM=OFF -DENABLE_VULKAN=OFF -DUSE_BUNDLED_MOLTENVK=OFF \
    -DMACOS_CODE_SIGNING=OFF -DENABLE_QT=OFF \
    -DUSE_SYSTEM_ZSTD=OFF -DUSE_SYSTEM_LZ4=OFF -DUSE_SYSTEM_LZO=OFF -DUSE_SYSTEM_LIBUSB=OFF \
    "-DCMAKE_C_FLAGS=$MAP" "-DCMAKE_CXX_FLAGS=$MAP" "-DCMAKE_OBJCXX_FLAGS=$MAP" \
    "-DMODERNGEKKO_FRONTEND_NAME=$APP_NAME" -DMODERNGEKKO_LAUNCHER_OUTPUT_NAME=SkiesOfArcadiaLegends \
    -DMODERNGEKKO_RUNNER_OUTPUT_NAME=skies-run "-DMODERNGEKKO_USER_DIRECTORY_NAME=$APP_NAME" \
    "-DMODERNGEKKO_REQUIRED_DISC_ID=$IDS" "-DMODERNGEKKO_REQUIRED_DOL_SHA256=$SHAS" \
    "-DMODERNGEKKO_DOL_PATCH_MANIFEST=$PATCHES" "-DMODERNGEKKO_GAME_SETTINGS_OVERRIDE=$INIS" \
    "-DMODERNGEKKO_DEFAULT_WINDOW_TITLE=$APP_NAME" -DMODERNGEKKO_GAMECUBE_CONTROLLERS=ON
  ;;
build)
  cmake --build $B --target moderngekko-run moderngekko-launcher
  cp release/launcher-background.png release/CREDITS.md $B/
  ;;
package)
  MODULE=${2:?usage: tools/build-release-macos.sh package USA.dylib [EUROPE.dylib] [JAPAN.dylib] [VERSION]}
  MODULE_EU= MODULE_JP=
  # Optional European and Japanese modules (.dylib files, told apart by their names:
  # gGEAP8P_recomp.dylib, gGEAJ8P_recomp.dylib), then the version
  while [[ ${3:-} == *.dylib ]]; do
    if [[ $(basename "$3") == *GEAJ8P* ]]; then MODULE_JP=$3; else MODULE_EU=$3; fi
    shift
  done
  VERSION=${3:-v0.50.0-beta}
  NAME=SkiesOfArcadiaLegends-$VERSION-macos
  P=build/$NAME
  APP="$P/$APP_NAME.app"
  rm -rf "$P" "$P.zip"; mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources/patches" "$P/licenses"
  # Programs and game modules in Contents/MacOS; Sys, the patches and the background in
  # Contents/Resources (where Dolphin looks for Sys and the launcher and runner for their data)
  cp $B/SkiesOfArcadiaLegends $B/skies-run "$APP/Contents/MacOS/"
  # Only system libraries: the GitHub runner has Homebrew's zstd, lz4, lzo and libusb, which
  # Dolphin's CMake prefers to the bundled ones (USE_SYSTEM_*=OFF above), and a player's Mac doesn't
  if otool -L $B/SkiesOfArcadiaLegends $B/skies-run | grep -E '/opt/homebrew|/usr/local'; then
    echo "error: the programs link libraries outside macOS (above)" >&2; exit 1
  fi
  strip -S -x -o "$APP/Contents/MacOS/gGEAE8P_recomp.dylib" "$MODULE"
  [ -n "$MODULE_EU" ] && strip -S -x -o "$APP/Contents/MacOS/gGEAP8P_recomp.dylib" "$MODULE_EU"
  [ -n "$MODULE_JP" ] && strip -S -x -o "$APP/Contents/MacOS/gGEAJ8P_recomp.dylib" "$MODULE_JP"
  cp -R $B/Sys "$APP/Contents/Resources/"
  cp release/launcher-background.png release/CREDITS.md "$APP/Contents/Resources/"
  cp release/skies-widescreen.dolpatch "$APP/Contents/Resources/patches/"
  [ -n "$MODULE_EU" ] && cp release/skies-widescreen-eu.dolpatch "$APP/Contents/Resources/patches/"
  [ -n "$MODULE_JP" ] && cp release/skies-widescreen-jp.dolpatch "$APP/Contents/Resources/patches/"
  # The icon, from the logo
  ICONSET=$(mktemp -d)/skies.iconset; mkdir -p "$ICONSET"
  for size in 16 32 128 256 512; do
    sips -z $size $size release/skies-logo.png --out "$ICONSET/icon_${size}x${size}.png" >/dev/null
    sips -z $((size * 2)) $((size * 2)) release/skies-logo.png \
      --out "$ICONSET/icon_${size}x${size}@2x.png" >/dev/null
  done
  iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/skies.icns"
  SHORT_VERSION=$(echo "${VERSION#v}" | sed -E 's/^([0-9]+(\.[0-9]+)*).*/\1/')
  cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleDevelopmentRegion</key><string>en</string>
  <key>CFBundleDisplayName</key><string>$APP_NAME</string>
  <key>CFBundleName</key><string>$APP_NAME</string>
  <key>CFBundleExecutable</key><string>SkiesOfArcadiaLegends</string>
  <key>CFBundleIconFile</key><string>skies.icns</string>
  <key>CFBundleIdentifier</key><string>io.github.ogdanimal.SkiesOfArcadiaLegends</string>
  <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$SHORT_VERSION</string>
  <key>CFBundleVersion</key><string>$SHORT_VERSION</string>
  <key>LSApplicationCategoryType</key><string>public.app-category.role-playing-games</string>
  <key>LSMinimumSystemVersion</key><string>13.0</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>GCSupportsControllerUserInteraction</key><true/>
</dict>
</plist>
EOF
  # Ad-hoc signature: Apple Silicon runs nothing unsigned. Inside out, the bundle last.
  for f in "$APP"/Contents/MacOS/*; do codesign --force --sign - "$f"; done
  codesign --force --sign - "$APP"
  cp release/README.md release/CREDITS.md release/THIRD-PARTY.md "$P/"
  cp COPYING "$P/COPYING.txt"
  cp lib/ModernGekko/vendor/dolphin/Externals/licenses.md "$P/THIRD-PARTY-DOLPHIN.md"
  cp -R lib/ModernGekko/vendor/dolphin/LICENSES/. "$P/licenses/"
  cp lib/libchdr/LICENSE.txt "$P/licenses/libchdr-LICENSE.txt"
  # ditto keeps the bundle's permissions; no resource forks or extended attributes (AppleDouble ._
  # files: the build machine's quarantine and provenance data), which the signature doesn't need
  ditto -c -k --norsrc --noextattr --noqtn --keepParent "$P" "$P.zip"
  (cd build && shasum -a 256 "$NAME.zip" > "$NAME.zip.sha256" && cat "$NAME.zip.sha256")
  ;;
*)
  sed -n 2,10p "$0"; exit 1 ;;
esac
