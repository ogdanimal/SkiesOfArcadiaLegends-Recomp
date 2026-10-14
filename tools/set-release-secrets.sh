#!/bin/bash
# Enters the Actions secrets on the port repo (the Hybrid Heaven recipe):
#   SOAL_SECRETS_REPO_WITH_PAT  the secrets repo's clone URL with a read-only token
#   SOAL_ARTIFACT_KEY           a fresh random key (encrypts the codegen artifact; nobody needs it)
#   RELEASE_*                   the release key, when tools/make-release-key.sh has made one
# Nothing typed is printed or written to disk; each value is proven before it is stored, and a
# failure changes nothing.
#   tools/set-release-secrets.sh [--environments] OWNER/PORT_REPO OWNER/SECRETS_REPO
# --environments (code review B7, once tools/setup-github-protection.sh has made them): the SOAL_*
# secrets go into the environments ci and release, the RELEASE_* ones into release only, and the
# repo-level copies are deleted afterwards.
set -u
ENVS=
[ "${1:-}" = --environments ] && { ENVS=1; shift; }
PORT_REPO=${1:?usage: tools/set-release-secrets.sh OWNER/PORT_REPO OWNER/SECRETS_REPO}
SECRETS_REPO=${2:?usage: tools/set-release-secrets.sh OWNER/PORT_REPO OWNER/SECRETS_REPO}
DOL_SHA256=87c240acb746c5a09e95dd84021f78643eaa6721d8edc96ed87d3b6b3bcbae54
KS="${SKIES_KEYDIR:-$HOME/skies-release-key}/release.jks"
ALIAS=skiesrecomp-release

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# --- 1. Secrets-repo token ----------------------------------------------------
printf 'Token that can READ %s (hidden), then Enter: ' "$SECRETS_REPO"
IFS= read -rs TOKEN; echo
TOKEN="${TOKEN//[[:space:]]/}"
[ -n "$TOKEN" ] || { echo "No token entered -- nothing changed."; exit 1; }
URL="https://x-access-token:${TOKEN}@github.com/${SECRETS_REPO}.git"
# As CI does: the token as an HTTP header through the environment, not in git's arguments
AUTH=$(printf 'x-access-token:%s' "$TOKEN" | base64 -w0)
unset TOKEN
echo "Testing: cloning the secrets repo with this token, as CI does..."
if ! GIT_TERMINAL_PROMPT=0 GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=http.https://github.com/.extraheader \
    GIT_CONFIG_VALUE_0="AUTHORIZATION: basic $AUTH" \
    git clone -q --depth 1 "https://github.com/${SECRETS_REPO}.git" "$TMP/s" 2>"$TMP/err"; then
    sed -E 's#//[^@]*@#//***@#g' "$TMP/err"
    echo "FAILED -- nothing was changed. 'Repository not found' means the token's repository"
    echo "access doesn't include $SECRETS_REPO."
    exit 1
fi
echo "$DOL_SHA256  $TMP/s/main.dol" | sha256sum -c --quiet - 2>/dev/null ||
    { echo "The clone worked, but main.dol is missing or isn't the original USA DOL -- nothing changed."; exit 1; }
for f in pgo/windows.profdata pgo/android.profdata; do
    [ -s "$TMP/s/$f" ] || { echo "The clone worked, but $f is missing -- nothing changed."; exit 1; }
done
echo "OK: the token reads the DOL (hash matches) and both profiles."

# --- 2. Release key (optional) ------------------------------------------------
STOREPASS=
if [ -f "$KS" ]; then
    printf 'Release keystore password for %s (hidden), then Enter: ' "$KS"
    IFS= read -rs STOREPASS; echo
    export STOREPASS
    keytool -list -keystore "$KS" -storepass:env STOREPASS -alias "$ALIAS" >/dev/null 2>&1 ||
        { echo "That password doesn't open $KS -- nothing changed."; exit 1; }
    echo "OK: the keystore opens."
elif [ -n "$ENVS" ]; then
    echo "No release key at $KS: the release environment needs it (tools/make-release-key.sh) -- nothing changed."
    exit 1
else
    echo "No release key at $KS: skipping the RELEASE_* secrets (tools/make-release-key.sh)."
fi

# --- 3. Store -----------------------------------------------------------------
set -e
if [ -n "$ENVS" ]; then
    for e in ci release; do
        gh api "repos/$PORT_REPO/environments/$e" >/dev/null 2>&1 ||
            { echo "No environment $e on $PORT_REPO: run tools/setup-github-protection.sh first -- nothing changed."; exit 1; }
    done
    SOAL_ENVS=(--env ci --env release) RELEASE_ENVS=(--env release)
else
    SOAL_ENVS=("") RELEASE_ENVS=("")
fi
put() {  # put NAME VALUE-ON-STDIN, once per target (the repo, or each environment)
    local name=$1; shift; local value; value=$(cat)
    if [ "$1" = "" ]; then printf '%s' "$value" | gh secret set "$name" --repo "$PORT_REPO"; return; fi
    while [ $# -gt 0 ]; do printf '%s' "$value" | gh secret set "$name" --repo "$PORT_REPO" "$1" "$2"; shift 2; done
}
printf '%s' "$URL" | put SOAL_SECRETS_REPO_WITH_PAT "${SOAL_ENVS[@]}"
unset URL AUTH
openssl rand -base64 48 | tr -d '\n' | put SOAL_ARTIFACT_KEY "${SOAL_ENVS[@]}"
if [ -n "$STOREPASS" ]; then
    base64 -w0 "$KS"          | put RELEASE_KEYSTORE_BASE64 "${RELEASE_ENVS[@]}"
    printf '%s' "$STOREPASS"  | put RELEASE_STORE_PASSWORD  "${RELEASE_ENVS[@]}"
    printf '%s' "$STOREPASS"  | put RELEASE_KEY_PASSWORD    "${RELEASE_ENVS[@]}"
    printf '%s' "$ALIAS"      | put RELEASE_KEY_ALIAS       "${RELEASE_ENVS[@]}"
fi
unset STOREPASS
if [ -n "$ENVS" ]; then
    # Everything is in the environments now; the repo-level copies would reach any ref
    for n in SOAL_SECRETS_REPO_WITH_PAT SOAL_ARTIFACT_KEY RELEASE_KEYSTORE_BASE64 RELEASE_STORE_PASSWORD \
             RELEASE_KEY_PASSWORD RELEASE_KEY_ALIAS; do
        gh secret delete "$n" --repo "$PORT_REPO" 2>/dev/null || true
    done
    for e in ci release; do echo "environment $e:"; gh secret list --repo "$PORT_REPO" --env "$e"; done
    echo "repo level (should be empty):"
fi
gh secret list --repo "$PORT_REPO"
