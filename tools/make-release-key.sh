#!/bin/bash
# Creates the Skies Recomp Android release signing key ONCE (the Hybrid Heaven recipe) and,
# given the port repo (owner/name), stores the four RELEASE_* Actions secrets on it.
# The password is read silently and passed to keytool and gh through the environment / stdin:
# never printed, never on a command line, never written to disk. Refuses to run if a key exists.
#
#   tools/make-release-key.sh [OWNER/REPO]     no repo: make the key only (upload later with
#                                              tools/set-release-secrets.sh)
set -u
PORT_REPO=${1:-}
KEYDIR="${SKIES_KEYDIR:-$HOME/skies-release-key}"
KS="$KEYDIR/release.jks"
ALIAS=skiesrecomp-release
# Embedded in every published APK and readable by anyone -- so no real name.
DNAME="CN=Skies of Arcadia Legends Recompiled"

if [ -e "$KS" ]; then
    echo "A key already exists at $KS -- refusing to touch it."
    echo "Replacing a release key strands every installed copy. Nothing changed."
    exit 1
fi

printf 'Choose a keystore password (hidden, at least 8 characters): '
IFS= read -rs PW1; echo
printf 'Type it again: '
IFS= read -rs PW2; echo
[ "$PW1" = "$PW2" ] || { echo "They do not match. Nothing changed."; exit 1; }
[ ${#PW1} -ge 8 ] || { echo "Too short. Nothing changed."; exit 1; }
export SKIES_PW="$PW1"; unset PW1 PW2

mkdir -p "$KEYDIR" && chmod 700 "$KEYDIR"
echo "Generating a 4096-bit RSA key (takes a few seconds)..."
keytool -genkeypair -storetype PKCS12 -keystore "$KS" -alias "$ALIAS" \
    -keyalg RSA -keysize 4096 -validity 10000 -dname "$DNAME" \
    -storepass:env SKIES_PW -keypass:env SKIES_PW >/dev/null 2>&1 \
    || { echo "keytool failed. Nothing was uploaded."; rm -f "$KS"; exit 1; }
chmod 600 "$KS"

# Prove the file opens with that password before anything depends on it.
# keytool's own exit status decides (a pipe would only test its last command).
LISTING=$(keytool -list -v -keystore "$KS" -storepass:env SKIES_PW -alias "$ALIAS" 2>/dev/null) &&
    printf '%s\n' "$LISTING" | grep -q 'SHA256:' ||
    { echo "The new keystore did not verify. Nothing was uploaded."; exit 1; }
printf '%s\n' "$LISTING" | grep -E 'Owner:|SHA256:' | head -2
# The certificate's digest is public (it's in every APK): pin it so the build can tell this key from
# any other (tools/android-ws-build.sh, release mode).
printf '%s\n' "$LISTING" | sed -n 's/^[[:space:]]*SHA256: //p' | head -1 | tr -d ':' | tr 'A-F' 'a-f' \
    > "$(dirname "$0")/../release/android-signer.sha256"
echo "Pinned the certificate digest in release/android-signer.sha256 (commit it)."

if [ -n "$PORT_REPO" ]; then
    echo "Uploading the four secrets to $PORT_REPO ..."
    base64 -w0 "$KS" | gh secret set RELEASE_KEYSTORE_BASE64 --repo "$PORT_REPO" \
     && printf '%s' "$SKIES_PW" | gh secret set RELEASE_STORE_PASSWORD --repo "$PORT_REPO" \
     && printf '%s' "$SKIES_PW" | gh secret set RELEASE_KEY_PASSWORD   --repo "$PORT_REPO" \
     && printf '%s' "$ALIAS"    | gh secret set RELEASE_KEY_ALIAS      --repo "$PORT_REPO" \
     || { unset SKIES_PW; echo "Uploading failed. The key is still safe at $KS -- re-run the gh steps, do NOT re-run this script."; exit 1; }
    gh secret list --repo "$PORT_REPO" | grep RELEASE_
fi
unset SKIES_PW

cat <<EOF

Done. The key is at:  $KS
BACK IT UP NOW, together with its password -- e.g. the file into your password
manager as an attachment, or onto a drive that is not this PC. This WSL folder
disappears if the distro is ever reset, and a lost key cannot be recovered:
every player would have to uninstall (losing their saves) to take an update.
EOF
