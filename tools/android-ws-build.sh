#!/bin/bash
# Android widescreen module + APK (the docs/handoffs/HANDOFF-android.md recipe, from a patched DOL made by
# tools/gamepatch.py). NAME (default ws) names the outputs: build/android/{llvm-gen,module,jniLibs}-NAME
# and build/android/apk/skies-NAME-<date>.apk; DOL defaults to build/game/GEAE8P-NAME/sys/main.dol.
# PREGEN=DIR (CI): use DolRecomp output made elsewhere (DIR/generated with module_tables.inc, no DOL), so
# this machine never needs the game's DOL.
# BRAND=1 builds the branded app instead (Skies of Arcadia Legends, com.skiesofarcadia.recomp: first-run disc import
# with release/skies-widescreen.dolpatch, which must turn the original DOL into this DOL; no game list):
# build/android/apk/skies-recomp-NAME-<date>.apk.
set -eu
cd "$(dirname "$0")/.."
ROOT=$PWD
A=$PWD/build/android V=$PWD/lib/ModernGekko/vendor/dolphin
export ANDROID_HOME=${ANDROID_HOME:-$HOME/Android/Sdk}
NDK=${NDK:-$ANDROID_HOME/ndk/29.0.14206865}
export PATH=/usr/lib/llvm-20/bin:$PATH
# Keep the build machine's absolute paths (home directory, user name) out of the shipped binaries.
PREFIX_MAP="-ffile-prefix-map=$PWD/="
NAME=${NAME:-ws}
DOL=${DOL:-build/game/GEAE8P-$NAME/sys/main.dol}
APK=$A/apk/skies-$NAME-$(date +%F).apk
BRAND_ARGS=()
if [ "${BRAND:-0}" = 1 ]; then
  APK=$A/apk/skies-recomp-$NAME-$(date +%F).apk
  B=$A/brand-$NAME
  rm -rf $B; mkdir -p $B/assets
  cp release/skies-widescreen.dolpatch $B/assets/skies-widescreen.dolpatch
  cp release/GEAE8P.ini $B/assets/skies-GEAE8P.ini
  cp release/launcher-background.png $B/assets/skies-background.png
  # The GPL and the notices travel with the APK too
  mkdir -p $B/assets/licenses && cp COPYING release/README.md release/THIRD-PARTY.md $B/assets/licenses/ &&
    cp $V/Externals/licenses.md $B/assets/licenses/THIRD-PARTY-DOLPHIN.md && cp -r $V/LICENSES/. $B/assets/licenses/
  # The DOL the import must produce: the patch's output hash (PREGEN), or this DOL's, checked against it
  if [ -n "${PREGEN:-}" ]; then
    sed -n 's/^output-sha256 //p' release/skies-widescreen.dolpatch > $B/dol-sha256.txt
  else
    sha256sum $DOL | cut -d' ' -f1 > $B/dol-sha256.txt
  fi
  # The launcher icon and banner (tools/make-skies-icon.sh); without them the app would show Dolphin's
  [ -f release/android-icon/mipmap-xxxhdpi/skies_launcher.png ] && [ -f release/android-icon/drawable-xhdpi/skies_banner.png ] ||
    { echo "release/android-icon is incomplete (tools/make-skies-icon.sh)"; exit 1; }
  cp -r release/android-icon $B/res
  grep -q "output-sha256 $(cat $B/dol-sha256.txt)" $B/assets/skies-widescreen.dolpatch ||
    { echo "release/skies-widescreen.dolpatch doesn't make $DOL"; exit 1; }
  BRAND_ARGS=(-PskiesBrand=$B)
fi
TABLES=()
if [ -n "${PREGEN:-}" ]; then
  GEN=$PREGEN/generated
  TABLES=(-DMODULE_TABLES_INC=$GEN/module_tables.inc)
else
  rm -rf $A/llvm-gen-$NAME
  DOLRECOMP_LLVM_CPU=cortex-a77 DOLRECOMP_LLVM_TARGET=aarch64-unknown-linux-android24 lib/ModernGekko/build/dolrecomp -j$(nproc) \
    --backend=llvm --state-in-memory --targets=aarch64 --profile-use $A/pgo/skies-android-gp.profdata \
    --cpu gekko --gamecube $DOL $A/llvm-gen-$NAME > $A/llvm-gen-$NAME.log 2>&1
  cp $DOL $A/llvm-gen-$NAME/generated/
  GEN=$A/llvm-gen-$NAME/generated
fi
cmake -G Ninja -S $V/module-template -B $A/module-$NAME \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 -DGENERATED_DIR=$GEN -DGXRUNTIME_DIR=$V/GXRuntime \
  -DCHASSIS_ABI_DIR=$V/Source/Core/Core/PowerPC/StaticRecomp -DGAME_ID=GEAE8P -DCMAKE_BUILD_TYPE=Release \
  -DRECOMPCORE_MODULE_ENABLE_IPO=ON -DRECOMPCORE_MODULE_OPT_LEVEL=2 "-DCMAKE_C_FLAGS=-mtune=cortex-a77 $PREFIX_MAP" \
  -DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384 "${TABLES[@]}" > $A/module-$NAME.cmake.log 2>&1
ninja -C $A/module-$NAME > $A/module-$NAME.log 2>&1
mkdir -p $A/jniLibs-$NAME/arm64-v8a
$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip --strip-debug -o $A/jniLibs-$NAME/arm64-v8a/libgGEAE8P_recomp.so $A/module-$NAME/gGEAE8P_recomp.so
# RELEASE=1 (with BRAND=1): a minified release APK signed with the release key
# (tools/make-release-key.sh; SKIES_KEYSTORE overrides the path, the password is asked for and passed
# to Gradle through ORG_GRADLE_PROJECT_* variables, never on a command line); VERSION names it
# (e.g. 0.9.0-beta).
TASK=assembleDebug OUT=app/build/intermediates/apk/debug/app-debug.apk
SIGN_ARGS=()
if [ "${RELEASE:-0}" = 1 ]; then
  [ "${BRAND:-0}" = 1 ] || { echo "RELEASE=1 needs BRAND=1"; exit 1; }
  KS=${SKIES_KEYSTORE:-$HOME/skies-release-key/release.jks}
  [ -f "$KS" ] || { echo "no release key at $KS (tools/make-release-key.sh)"; exit 1; }
  if [ -z "${ORG_GRADLE_PROJECT_storepass:-}" ]; then
    printf 'Release keystore password (hidden): '; IFS= read -rs ORG_GRADLE_PROJECT_storepass; echo
  fi
  # The key's own password when given (CI: RELEASE_KEY_PASSWORD), else the store's (make-release-key.sh
  # makes them the same)
  export ORG_GRADLE_PROJECT_storepass ORG_GRADLE_PROJECT_keypass=${ORG_GRADLE_PROJECT_keypass:-$ORG_GRADLE_PROJECT_storepass}
  export ORG_GRADLE_PROJECT_keystore=$KS ORG_GRADLE_PROJECT_keyalias=${SKIES_KEY_ALIAS:-skiesrecomp-release}
  TASK=assembleRelease OUT=app/build/intermediates/apk/release/app-release.apk
  APK=$A/apk/SkiesOfArcadiaLegends-v${VERSION:-$NAME}-android.apk
fi
[ -n "${VERSION:-}" ] && SIGN_ARGS+=(-PskiesVersionName=$VERSION)
cd $V/Source/Android && ./gradlew :app:$TASK \
  -Pandroid.injected.build.abi=arm64-v8a -PrecompJniLibs=$A/jniLibs-$NAME "-PrecompNativeFlags=-mno-outline-atomics $PREFIX_MAP" "${BRAND_ARGS[@]}" "${SIGN_ARGS[@]}" > $A/apk-$NAME.log 2>&1
mkdir -p $A/apk && cp $OUT $APK
if [ "${RELEASE:-0}" = 1 ]; then
  # Refuse a release APK that isn't signed with the release key (a debug-signed one can't update
  # players' installs): apksigner must verify it, and its signer's certificate digest must be the
  # keystore's (and, when release/android-signer.sha256 exists, that pinned one too, so a
  # substituted keystore in the CI secrets is caught). Exit codes are checked, not just matched text.
  APKSIGNER=$(ls -d $ANDROID_HOME/build-tools/*/apksigner | sort -V | tail -1)
  CERTS=$($APKSIGNER verify --print-certs $APK) || { echo "apksigner can't verify $APK"; exit 1; }
  APK_SHA=$(printf '%s\n' "$CERTS" | sed -n 's/^Signer #1 certificate SHA-256 digest: //p' | tr 'A-F' 'a-f')
  [ "$(printf '%s\n' "$CERTS" | grep -c '^Signer #[0-9]* certificate DN:')" = 1 ] && [ -n "$APK_SHA" ] ||
    { echo "$APK doesn't have exactly one signer"; exit 1; }
  LISTING=$(keytool -list -v -keystore "$ORG_GRADLE_PROJECT_keystore" \
    -storepass:env ORG_GRADLE_PROJECT_storepass -alias "$ORG_GRADLE_PROJECT_keyalias") ||
    { echo "keytool can't open the release keystore"; exit 1; }
  KS_SHA=$(printf '%s\n' "$LISTING" | sed -n 's/^[[:space:]]*SHA256: //p' | head -1 | tr -d ':' | tr 'A-F' 'a-f')
  [ -n "$KS_SHA" ] && [ "$APK_SHA" = "$KS_SHA" ] ||
    { echo "$APK is not signed with the release key"; exit 1; }
  if [ -f "$ROOT/release/android-signer.sha256" ]; then
    [ "$APK_SHA" = "$(tr -d ' :\n' < "$ROOT/release/android-signer.sha256" | tr 'A-F' 'a-f')" ] ||
      { echo "$APK is signed with a key other than the pinned release key"; exit 1; }
  fi
  echo "Signed with the release key ($APK_SHA)"
fi
# The Gradle + Kotlin daemons keep ~2.5 GB resident for hours; on the 12 GB WSL VM that
# starved later game runs into a global OOM (2026-10-11). Release them after each build.
./gradlew --stop >/dev/null 2>&1 || true; pkill -u "$USER" -f KotlinCompileDaemon || true
unzip -l $APK | grep recomp
echo DONE
