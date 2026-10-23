#!/usr/bin/env bash
# Prove patches/ reproduces the submodules' working copies: fresh worktrees of the pinned ModernGekko,
# RecompCore and DolRecomp get every patch (patches/recompcore.series order, as CI applies them),
# then each changed or new file is compared with the working copy. Prints any difference.
set -euo pipefail
cd "$(dirname "$0")/.."
MG=lib/ModernGekko RC=$MG/vendor/dolphin DR=$RC/DolRecomp
R=$(mktemp -d "$PWD/build/check-patches-XXXXXX")
cleanup() {
  git -C "$DR" worktree remove --force "$R/mg/vendor/dolphin/DolRecomp" >/dev/null 2>&1 || true
  git -C "$RC" worktree remove --force "$R/mg/vendor/dolphin" >/dev/null 2>&1 || true
  git -C "$MG" worktree remove --force "$R/mg" >/dev/null 2>&1 || true
  rm -rf "$R"
}
trap cleanup EXIT
git -C "$MG" worktree add -q --detach "$R/mg" HEAD
git -C "$RC" worktree add -q --detach "$R/mg/vendor/dolphin" HEAD
git -C "$DR" worktree add -q --detach "$R/mg/vendor/dolphin/DolRecomp" HEAD
P=$PWD/patches
git -C "$R/mg" apply "$P/moderngekko-local.patch"
git -C "$R/mg" apply "$P/moderngekko-macos.patch"
git -C "$R/mg" apply "$P/moderngekko-launcher-signal.patch"
for p in $(grep -v '^#' "$P/recompcore.series"); do git -C "$R/mg/vendor/dolphin" apply "$P/recompcore-$p.patch"; done
for p in mtmsr-entry gather-pipe llvm-cpu win64-sjlj-frame coff-uwtable halfword-byterev macho-sections fast-fp; do
  git -C "$R/mg/vendor/dolphin/DolRecomp" apply "$P/dolrecomp-$p.patch"
done
status=0
changed() { (cd "$1" && { git diff HEAD --name-only; git ls-files --others --exclude-standard; } | sort -u); }
for pair in "$MG:$R/mg" "$RC:$R/mg/vendor/dolphin" "$DR:$R/mg/vendor/dolphin/DolRecomp"; do
  W=${pair%%:*} C=${pair#*:}
  changed "$W" > "$R/w"; changed "$C" > "$R/c"
  only_w=$(comm -23 "$R/w" "$R/c" | grep -v '\.profraw$' || true)
  only_c=$(comm -13 "$R/w" "$R/c" || true)
  differ=$(comm -12 "$R/w" "$R/c" | while read -r f; do [ -d "$W/$f" ] || cmp -s "$W/$f" "$C/$f" || echo "$f"; done)
  echo "$(basename "$W"): $(wc -l < "$R/w") changed files"
  [ -z "$only_w$only_c$differ" ] || { status=1; echo "  only in working copy: $only_w"; echo "  only from patches: $only_c"; echo "  differ: $differ"; }
done
[ $status = 0 ] && echo "patches reproduce the working copies"
exit $status
