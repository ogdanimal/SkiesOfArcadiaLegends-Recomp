#!/usr/bin/env bash
# Regenerate one RecompCore patch (patches/recompcore-NAME.patch) from the working copy: a temporary
# worktree gets the patches before it (tools/apply-patches.sh order), then the working copy's
# versions of the patch's own files plus any EXTRA files (new ones it should now own), and the diff
# becomes the patch. Patches that share a file stay separate: each holds only its own changes.
#   tools/regen-recompcore-patch.sh NAME [EXTRA_FILE...]   (paths relative to vendor/dolphin)
# Check afterwards that the whole chain still reproduces the working copy (docs/handoffs/HANDOFF-release.md).
set -euo pipefail
cd "$(dirname "$0")/.."
NAME=$1; shift
RC=lib/ModernGekko/vendor/dolphin
PATCH=$PWD/patches/recompcore-$NAME.patch
ORDER=$(grep -v '^#' patches/recompcore.series)
W=$(mktemp -d "$PWD/build/regen-XXXXXX")
trap 'git -C "$RC" worktree remove --force "$W/rc" >/dev/null 2>&1 || true; rm -rf "$W"' EXIT
git -C "$RC" worktree add -q --detach "$W/rc" HEAD
for p in $ORDER; do
  [ "$p" = "$NAME" ] && break
  git -C "$W/rc" apply "$PWD/patches/recompcore-$p.patch"
done
git -C "$W/rc" add -A
git -C "$W/rc" -c user.name=regen -c user.email=regen@invalid commit -q --allow-empty -m base
FILES=$( { [ -f "$PATCH" ] && grep '^diff --git' "$PATCH" | awk '{print $3}' | sed 's#^a/##'; printf '%s\n' "$@"; } | sort -u)
for f in $FILES; do
  mkdir -p "$W/rc/$(dirname "$f")"
  cp "$RC/$f" "$W/rc/$f"
done
git -C "$W/rc" add -A
git -C "$W/rc" diff --cached HEAD > "$PATCH"
echo "$PATCH: $(grep -c '^diff --git' "$PATCH") files"
