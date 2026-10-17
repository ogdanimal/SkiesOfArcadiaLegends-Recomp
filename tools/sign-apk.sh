#!/bin/bash
# Sign an unsigned release APK with the release key, then refuse it unless it verifies as signed by
# exactly that key (and the pinned one, when release/android-signer.sha256 exists).
#   tools/sign-apk.sh UNSIGNED.apk SIGNED.apk
# Runs only the Android SDK's own zipalign / apksigner and the JDK's keytool, so in CI the signing
# secrets never share a process with Gradle, its plugins or the build's dependencies.
# SKIES_KEYSTORE (default ~/skies-release-key/release.jks), SKIES_KEY_ALIAS (default
# skiesrecomp-release); the passwords come from SKIES_STORE_PASS / SKIES_KEY_PASS (the key's defaults
# to the store's, as tools/make-release-key.sh makes them), or are asked for. They reach the tools
# through the environment, never on a command line.
set -eu
IN=${1:?usage: tools/sign-apk.sh UNSIGNED.apk SIGNED.apk} OUT=${2:?usage: tools/sign-apk.sh UNSIGNED.apk SIGNED.apk}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
export ANDROID_HOME=${ANDROID_HOME:-$HOME/Android/Sdk}
BT=$(ls -d "$ANDROID_HOME"/build-tools/*/ | sort -V | tail -1)
KS=${SKIES_KEYSTORE:-$HOME/skies-release-key/release.jks}
ALIAS=${SKIES_KEY_ALIAS:-skiesrecomp-release}
[ -f "$KS" ] || { echo "no release key at $KS (tools/make-release-key.sh)"; exit 1; }
if [ -z "${SKIES_STORE_PASS:-}" ]; then
  printf 'Release keystore password (hidden): '; IFS= read -rs SKIES_STORE_PASS; echo
fi
export SKIES_STORE_PASS SKIES_KEY_PASS=${SKIES_KEY_PASS:-$SKIES_STORE_PASS}

TMP=$(mktemp --suffix=.apk "${OUT%.apk}.XXXXXX")
trap 'rm -f "$TMP"' EXIT
"$BT/zipalign" -f -p 4 "$IN" "$TMP"
"$BT/apksigner" sign --ks "$KS" --ks-key-alias "$ALIAS" \
  --ks-pass env:SKIES_STORE_PASS --key-pass env:SKIES_KEY_PASS --out "$OUT" "$TMP"
rm -f "$OUT.idsig"

# Refuse an APK that isn't signed with the release key (a debug-signed one can't update players'
# installs): apksigner must verify it, and its signer's certificate digest must be the keystore's
# (and, when release/android-signer.sha256 exists, that pinned one too, so a substituted keystore in
# the CI secrets is caught). Exit codes are checked, not just matched text.
fail() { echo "$1"; rm -f "$OUT"; exit 1; }
CERTS=$("$BT/apksigner" verify --print-certs "$OUT") || fail "apksigner can't verify $OUT"
APK_SHA=$(printf '%s\n' "$CERTS" | sed -n 's/^Signer #1 certificate SHA-256 digest: //p' | tr 'A-F' 'a-f')
[ "$(printf '%s\n' "$CERTS" | grep -c '^Signer #[0-9]* certificate DN:')" = 1 ] && [ -n "$APK_SHA" ] ||
  fail "$OUT doesn't have exactly one signer"
LISTING=$(keytool -list -v -keystore "$KS" -storepass:env SKIES_STORE_PASS -alias "$ALIAS") ||
  fail "keytool can't open the release keystore"
KS_SHA=$(printf '%s\n' "$LISTING" | sed -n 's/^[[:space:]]*SHA256: //p' | head -1 | tr -d ':' | tr 'A-F' 'a-f')
[ -n "$KS_SHA" ] && [ "$APK_SHA" = "$KS_SHA" ] || fail "$OUT is not signed with the release key"
if [ -f "$ROOT/release/android-signer.sha256" ]; then
  [ "$APK_SHA" = "$(tr -d ' :\n' < "$ROOT/release/android-signer.sha256" | tr 'A-F' 'a-f')" ] ||
    fail "$OUT is signed with a key other than the pinned release key"
fi
echo "Signed with the release key ($APK_SHA): $OUT"
