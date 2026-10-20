#!/usr/bin/env bash
# Branded release build (Linux), the counterpart of tools/build-release-win.bat: the launcher
# (SkiesOfArcadiaLegends; no spaces in a Linux binary name, like the Hybrid Heaven port's), the runner
# skies-run and Sys in lib/ModernGekko/build-release-linux, configured for Skies (the USA disc GEAE8P
# the European GEAP8P and the Japanese GEAJ8P, each with its widescreen DOL patch applied to the player's own main.dol and
# its game ini; user directory ~/.local/share/Skies of Arcadia Legends). EGL on, as the WSL dev
# runtime (build-egl).
#   tools/build-release-linux.sh configure | build | package USA.so [EUROPE.so] [JAPAN.so] [VERSION] | appimage [VERSION]
# REGIONS="us eu jp" (configure) makes the release take the European and the Japanese disc too ("us eu":
# the European one); the default is the USA disc alone, and package then needs only the USA module.
# package: build/SkiesOfArcadiaLegends-VERSION-linux.tar.gz (+ .sha256), one top-level folder.
# appimage: build/SkiesOfArcadiaLegends-VERSION-x86_64.AppImage (+ .sha256) from the folder
# package made, with the libraries the binaries need from the build machine inside (linuxdeploy;
# glibc and the graphics drivers stay the player's own, so it needs a glibc as new as the builder's).
set -euo pipefail
cd "$(dirname "$0")/.."
B=${BUILD_DIR:-lib/ModernGekko/build-release-linux}
# Pinned linuxdeploy (its AppImage carries the AppImage output plugin)
LINUXDEPLOY_URL=https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage
LINUXDEPLOY_SHA256=c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
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
  # Dolphin's own copies of bzip2, zstd, lzma, zlib and the rest, linked in: the build machine's
  # would tie the release to its distribution (Debian's libbz2.so.1.0 isn't on Fedora, so the
  # launcher didn't start on Bazzite). Iconv stays glibc's.
  cmake -S lib/ModernGekko -B $B -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    -DUSE_SYSTEM_LIBS=OFF -DUSE_SYSTEM_ICONV=AUTO \
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
  MODULE=${2:?usage: tools/build-release-linux.sh package USA.so [EUROPE.so] [JAPAN.so] [VERSION]}
  MODULE_EU= MODULE_JP=
  # Optional European and Japanese modules (.so files, told apart by their names:
  # gGEAP8P_recomp.so, gGEAJ8P_recomp.so), then the version
  while [[ ${3:-} == *.so ]]; do
    if [[ $(basename "$3") == *GEAJ8P* ]]; then MODULE_JP=$3; else MODULE_EU=$3; fi
    shift
  done
  VERSION=${3:-v0.9.0}
  NAME=SkiesOfArcadiaLegends-$VERSION-linux
  P=build/$NAME
  rm -rf "$P" "$P.tar.gz"; mkdir -p "$P/patches" "$P/licenses"
  cp $B/SkiesOfArcadiaLegends $B/skies-run $B/launcher-background.png "$P/"
  strip --strip-debug -o "$P/gGEAE8P_recomp.so" "$MODULE"
  [ -n "$MODULE_EU" ] && strip --strip-debug -o "$P/gGEAP8P_recomp.so" "$MODULE_EU"
  [ -n "$MODULE_JP" ] && strip --strip-debug -o "$P/gGEAJ8P_recomp.so" "$MODULE_JP"
  cp -r $B/Sys "$P/"
  cp release/skies-widescreen.dolpatch "$P/patches/"
  [ -n "$MODULE_EU" ] && cp release/skies-widescreen-eu.dolpatch "$P/patches/"
  [ -n "$MODULE_JP" ] && cp release/skies-widescreen-jp.dolpatch "$P/patches/"
  cp release/README.md release/CREDITS.md release/THIRD-PARTY.md "$P/"
  cp release/skies-logo.png "$P/icon.png"
  cp COPYING "$P/COPYING.txt"
  cp lib/ModernGekko/vendor/dolphin/Externals/licenses.md "$P/THIRD-PARTY-DOLPHIN.md"
  cp -r lib/ModernGekko/vendor/dolphin/LICENSES/. "$P/licenses/"
  cp lib/libchdr/LICENSE.txt "$P/licenses/libchdr-LICENSE.txt"
  tar -C build -czf "$P.tar.gz" "$NAME"
  (cd build && sha256sum "$NAME.tar.gz" > "$NAME.tar.gz.sha256" && cat "$NAME.tar.gz.sha256")
  ;;
appimage)
  VERSION=${2:-v0.9.0}
  P=build/SkiesOfArcadiaLegends-$VERSION-linux
  [ -d "$P" ] || { echo "no $P: run package first" >&2; exit 1; }
  OUT=build/SkiesOfArcadiaLegends-$VERSION-x86_64.AppImage
  LD=build/tools/linuxdeploy-x86_64.AppImage
  if ! echo "$LINUXDEPLOY_SHA256  $LD" | sha256sum -c --status 2>/dev/null; then
    mkdir -p build/tools
    curl -fsSL -o "$LD" "$LINUXDEPLOY_URL"
    echo "$LINUXDEPLOY_SHA256  $LD" | sha256sum -c
    chmod +x "$LD"
  fi
  # The release folder as it is in usr/bin: the launcher finds the runner, the modules, Sys and the
  # DOL patches beside itself. The user directory stays ~/.local/share, outside the read-only image.
  A=build/AppDir
  # (the two programs are copied in by linuxdeploy, which points them at usr/lib)
  rm -rf "$A" "$OUT"; mkdir -p "$A/usr/bin"
  cp -r "$P/." "$A/usr/bin/"
  rm "$A/usr/bin/SkiesOfArcadiaLegends" "$A/usr/bin/skies-run"
  ICON=build/skies-of-arcadia-legends.png
  cp release/skies-logo.png "$ICON"
  cat > build/skies-of-arcadia-legends.desktop <<'EOF'
[Desktop Entry]
Type=Application
Name=Skies of Arcadia Legends
Comment=Skies of Arcadia Legends, recompiled
Exec=SkiesOfArcadiaLegends
Icon=skies-of-arcadia-legends
Categories=Game;
Terminal=false
EOF
  # The modules are loaded by the runner, not linked: their dependencies only
  MODULES=()
  for m in "$A"/usr/bin/g*_recomp.so; do MODULES+=(--deploy-deps-only "$m"); done
  # No FUSE on the CI runners; the AppImage made runs with the player's FUSE, or extracts itself
  APPIMAGE_EXTRACT_AND_RUN=1 ARCH=x86_64 LDAI_OUTPUT=$OUT "$LD" --appdir "$A" \
    --executable "$P/SkiesOfArcadiaLegends" --executable "$P/skies-run" \
    "${MODULES[@]}" --desktop-file build/skies-of-arcadia-legends.desktop --icon-file "$ICON" \
    --output appimage
  (cd build && sha256sum "${OUT#build/}" > "${OUT#build/}.sha256" && cat "${OUT#build/}.sha256")
  ;;
*)
  sed -n 2,14p "$0"; exit 1 ;;
esac
