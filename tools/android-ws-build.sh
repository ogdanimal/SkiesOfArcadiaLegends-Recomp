#!/bin/bash
# Android widescreen module + APK (the docs/handoffs/HANDOFF-android.md recipe, from a patched DOL made by
# tools/gamepatch.py). NAME (default ws) names the outputs: build/android/{llvm-gen,module}-NAME-<disc ID>, jniLibs-NAME
# and build/android/apk/skies-NAME-<date>.apk; DOL defaults to build/game/GEAE8P-NAME/sys/main.dol.
# PREGEN=DIR (CI): use DolRecomp output made elsewhere (DIR/generated with module_tables.inc, no DOL), so
# this machine never needs the game's DOL.
# BRAND=1 builds the branded app instead (Skies of Arcadia Legends, com.skiesofarcadia.recomp: first-run disc import
# with release/skies-widescreen.dolpatch, which must turn the original DOL into this DOL; no game list):
# build/android/apk/skies-recomp-NAME-<date>.apk (TEST_ID=1: the debug build as com.skiesofarcadia.recomp.test,
# beside the release app: skies-recomp-NAME-<date>-test.apk).
# The European game (GEAP8P) gets a module of its own beside the USA one when its DOL is there:
# DOL_EU (default build/game/GEAP8P-NAME/sys/main.dol) or, in CI, PREGEN_EU=DIR; no PGO until there is
# a European profile (build/android/pgo/skies-android-eu.profdata). BRAND=1 then also bundles
# release/skies-widescreen-eu.dolpatch and release/GEAP8P.ini.
# MODULE_ONLY=1 stops after the native module(s) (build/android/jniLibs-NAME); JNILIBS=DIR builds only the
# APK, around a module built that way (CI: the job that decrypts the generated code builds the module,
# and Gradle with its downloaded dependencies runs in a job of its own that never sees that code or the
# key that decrypts it).
set -eu
cd "$(dirname "$0")/.."
ROOT=$PWD
A=$PWD/build/android V=$PWD/lib/ModernGekko/vendor/dolphin
mkdir -p $A
export ANDROID_HOME=${ANDROID_HOME:-$HOME/Android/Sdk}
NDK=${NDK:-$ANDROID_HOME/ndk/29.0.14206865}
export PATH=/usr/lib/llvm-20/bin:$PATH
# Keep the build machine's absolute paths (home directory, user name) out of the shipped binaries.
PREFIX_MAP="-ffile-prefix-map=$PWD/="
NAME=${NAME:-ws}
DOL=${DOL:-build/game/GEAE8P-$NAME/sys/main.dol}
DOL_EU=${DOL_EU:-build/game/GEAP8P-$NAME/sys/main.dol}
EU=0
{ [ -n "${PREGEN_EU:-}" ] || [ -f "$DOL_EU" ]; } && EU=1
[ -n "${JNILIBS:-}" ] && { [ -f "$JNILIBS/arm64-v8a/libgGEAP8P_recomp.so" ] && EU=1 || EU=0; }
APK=$A/apk/skies-$NAME-$(date +%F).apk
BRAND_ARGS=()
if [ "${BRAND:-0}" = 1 ]; then
  APK=$A/apk/skies-recomp-$NAME-$(date +%F).apk
  B=$A/brand-$NAME
  rm -rf $B; mkdir -p $B/assets
  cp release/skies-widescreen.dolpatch $B/assets/skies-widescreen.dolpatch
  cp release/GEAE8P.ini $B/assets/skies-GEAE8P.ini
  if [ $EU = 1 ]; then
    cp release/skies-widescreen-eu.dolpatch $B/assets/skies-widescreen-eu.dolpatch
    cp release/GEAP8P.ini $B/assets/skies-GEAP8P.ini
  fi
  cp release/launcher-background.png $B/assets/skies-background.png
  # The GPL and the notices travel with the APK too
  mkdir -p $B/assets/licenses && cp COPYING release/README.md release/THIRD-PARTY.md $B/assets/licenses/ &&
    cp $V/Externals/licenses.md $B/assets/licenses/THIRD-PARTY-DOLPHIN.md && cp -r $V/LICENSES/. $B/assets/licenses/ &&
    cp lib/libchdr/LICENSE.txt $B/assets/licenses/libchdr-LICENSE.txt
  # The DOL the import must produce: the patch's output hash (PREGEN), or this DOL's, checked against it
  if [ -n "${PREGEN:-}" ] || [ -n "${JNILIBS:-}" ]; then
    sed -n 's/^output-sha256 //p' release/skies-widescreen.dolpatch > $B/dol-sha256.txt
  else
    sha256sum $DOL | cut -d' ' -f1 > $B/dol-sha256.txt
  fi
  if [ $EU = 1 ]; then
    if [ -n "${PREGEN_EU:-}" ] || [ -n "${JNILIBS:-}" ]; then
      sed -n 's/^output-sha256 //p' release/skies-widescreen-eu.dolpatch > $B/dol-sha256-eu.txt
    else
      sha256sum $DOL_EU | cut -d' ' -f1 > $B/dol-sha256-eu.txt
    fi
    grep -q "output-sha256 $(cat $B/dol-sha256-eu.txt)" $B/assets/skies-widescreen-eu.dolpatch ||
      { echo "release/skies-widescreen-eu.dolpatch doesn't make $DOL_EU"; exit 1; }
  fi
  # The launcher icon and banner (tools/make-skies-icon.sh); without them the app would show Dolphin's
  [ -f release/android-icon/mipmap-xxxhdpi/skies_launcher.png ] && [ -f release/android-icon/drawable-xhdpi/skies_banner.png ] ||
    { echo "release/android-icon is incomplete (tools/make-skies-icon.sh)"; exit 1; }
  cp -r release/android-icon $B/res
  grep -q "output-sha256 $(cat $B/dol-sha256.txt)" $B/assets/skies-widescreen.dolpatch ||
    { echo "release/skies-widescreen.dolpatch doesn't make $DOL"; exit 1; }
  BRAND_ARGS=(-PskiesBrand=$B)
  # TEST_ID=1: com.skiesofarcadia.recomp.test, installed beside the release app without touching it
  if [ "${TEST_ID:-0}" = 1 ]; then
    BRAND_ARGS+=(-PskiesAppIdSuffix=.test)
    APK=${APK%.apk}-test.apk
  fi
fi
if [ -n "${JNILIBS:-}" ]; then
  [ -f "$JNILIBS/arm64-v8a/libgGEAE8P_recomp.so" ] || { echo "no $JNILIBS/arm64-v8a/libgGEAE8P_recomp.so"; exit 1; }
  mkdir -p $A && rm -rf $A/jniLibs-$NAME && cp -r "$JNILIBS" $A/jniLibs-$NAME
else
# module ID DOL PREGEN PROFILE: the module for disc ID into jniLibs-NAME (from DOL, or PREGEN's
# DolRecomp output; PROFILE "" or missing: no PGO)
module() {
  local id=$1 dol=$2 pregen=$3 profile=$4 gen tables=() pgo=()
  if [ -n "$pregen" ]; then
    gen=$pregen/generated
    tables=(-DMODULE_TABLES_INC=$gen/module_tables.inc)
  else
    [ -n "$profile" ] && [ -f "$profile" ] && pgo=(--profile-use "$profile")
    rm -rf $A/llvm-gen-$NAME-$id
    DOLRECOMP_LLVM_CPU=cortex-a77 DOLRECOMP_LLVM_TARGET=aarch64-unknown-linux-android24 lib/ModernGekko/build/dolrecomp -j$(nproc) \
      --backend=llvm --state-in-memory --targets=aarch64 "${pgo[@]}" \
      --cpu gekko --gamecube $dol $A/llvm-gen-$NAME-$id > $A/llvm-gen-$NAME-$id.log 2>&1
    cp $dol $A/llvm-gen-$NAME-$id/generated/
    gen=$A/llvm-gen-$NAME-$id/generated
  fi
  cmake -G Ninja -S $V/module-template -B $A/module-$NAME-$id \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-24 -DGENERATED_DIR=$gen -DGXRUNTIME_DIR=$V/GXRuntime \
    -DCHASSIS_ABI_DIR=$V/Source/Core/Core/PowerPC/StaticRecomp -DGAME_ID=$id -DCMAKE_BUILD_TYPE=Release \
    -DRECOMPCORE_MODULE_ENABLE_IPO=ON -DRECOMPCORE_MODULE_OPT_LEVEL=2 "-DCMAKE_C_FLAGS=-mtune=cortex-a77 $PREFIX_MAP" \
    -DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384 "${tables[@]}" > $A/module-$NAME-$id.cmake.log 2>&1
  ninja -C $A/module-$NAME-$id > $A/module-$NAME-$id.log 2>&1
  mkdir -p $A/jniLibs-$NAME/arm64-v8a
  $NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip --strip-debug \
    -o $A/jniLibs-$NAME/arm64-v8a/libg${id}_recomp.so $A/module-$NAME-$id/g${id}_recomp.so
}
rm -rf $A/jniLibs-$NAME
module GEAE8P "$DOL" "${PREGEN:-}" $A/pgo/skies-android-gp.profdata
if [ $EU = 1 ]; then
  module GEAP8P "$DOL_EU" "${PREGEN_EU:-}" $A/pgo/skies-android-eu.profdata
fi
fi
if [ "${MODULE_ONLY:-0}" = 1 ]; then
  ls -la $A/jniLibs-$NAME/arm64-v8a/
  exit 0
fi
# RELEASE=1 (with BRAND=1): a minified release APK, built unsigned by Gradle and then signed with the
# release key by tools/sign-apk.sh (SKIES_KEYSTORE etc. there; the password is asked for or comes
# from SKIES_STORE_PASS, never a command line). SIGN=0 stops at the unsigned APK (CI signs it in a
# job of its own, away from Gradle). VERSION names it (e.g. 0.9.0-beta).
TASK=assembleDebug OUT=app/build/intermediates/apk/debug/app-debug.apk
SIGN_ARGS=()
if [ "${RELEASE:-0}" = 1 ]; then
  [ "${BRAND:-0}" = 1 ] || { echo "RELEASE=1 needs BRAND=1"; exit 1; }
  TASK=assembleRelease OUT=
  APK=$A/apk/SkiesOfArcadiaLegends-v${VERSION:-$NAME}-android.apk
  rm -rf $V/Source/Android/app/build/intermediates/apk/release
fi
[ -n "${VERSION:-}" ] && SIGN_ARGS+=(-PskiesVersionName=$VERSION)
# The injected ABI builds arm64 only, but on its own it also marks the APK android:testOnly, which
# only adb install -t accepts (players' installs failed with "App not installed"); testOnly=false undoes that
cd $V/Source/Android && ./gradlew :app:$TASK \
  -Pandroid.injected.build.abi=arm64-v8a -Pandroid.injected.testOnly=false -PrecompJniLibs=$A/jniLibs-$NAME "-PrecompNativeFlags=-mno-outline-atomics $PREFIX_MAP" "${BRAND_ARGS[@]}" "${SIGN_ARGS[@]}" > $A/apk-$NAME.log 2>&1
if [ "${RELEASE:-0}" = 1 ]; then
  # No keystore property, so Gradle leaves it unsigned (app-release-unsigned.apk)
  UNSIGNED=(app/build/intermediates/apk/release/*.apk)
  [ ${#UNSIGNED[@]} = 1 ] && [ -f "${UNSIGNED[0]}" ] || { echo "expected one release APK, found: ${UNSIGNED[*]}"; exit 1; }
  mkdir -p $A/apk && cp "${UNSIGNED[0]}" ${APK%.apk}-unsigned.apk
  if [ "${SIGN:-1}" = 1 ]; then
    "$ROOT/tools/sign-apk.sh" ${APK%.apk}-unsigned.apk $APK && rm ${APK%.apk}-unsigned.apk
  else
    APK=${APK%.apk}-unsigned.apk
  fi
else
  mkdir -p $A/apk && cp $OUT $APK
fi
# The Gradle + Kotlin daemons keep ~2.5 GB resident for hours; on the 12 GB WSL VM that
# starved later game runs into a global OOM (2026-10-11). Release them after each build.
./gradlew --stop >/dev/null 2>&1 || true; pkill -u "$USER" -f KotlinCompileDaemon || true
unzip -l $APK | grep recomp
echo DONE
