#!/usr/bin/env bash
# CI code generation: the only step that ever sees the game's DOLs. From the original main.dol of each
# region and the PGO profiles (the private secrets repo), make the patched DOL with the region's
# release/*.dolpatch, run DolRecomp for each target in tools/codegen-targets.txt (Windows x86_64 COFF,
# Linux x86_64 ELF, Android aarch64, macOS arm64 Mach-O) and precompute each module's
# module_tables.inc so the build jobs never need the DOL (-DMODULE_TABLES_INC).
#   tools/ci-codegen.sh SECRETS_DIR OUT_DIR
#     SECRETS_DIR: main.dol (USA, GEAE8P), main-eu.dol (European, GEAP8P), main-jp.dol (Japanese,
#                  GEAJ8P), and the targets' profiles pgo/PROFILE.profdata, pgo/PROFILE-eu.profdata,
#                  pgo/PROFILE-jp.profdata (codegen-targets.txt names them)
#     OUT_DIR:     USA: NAME/generated (windows/generated, ...); European: NAME-eu/generated;
#                  Japanese: NAME-jp/generated
#                  (no DOL in any; checked)
# pgo/windows.profdata is the desktop profile (trained on Linux with tools/pgo-wsl.sh), used for both
# x86_64 targets. A build without its profile has no PGO (the profiles are keyed by code addresses,
# so the USA ones don't fit the European DOL). DOLRECOMP defaults to lib/ModernGekko/build/dolrecomp;
# TARGETS (default: all of codegen-targets.txt) and REGIONS (default: "us" and each of "eu" / "jp"
# whose main-eu.dol / main-jp.dol the secrets have) limit the run. CI runs it once per target and region.
# The repository files the output depends on (for a cache key), besides the secrets, the target's
# line in tools/codegen-targets.txt and the region's release/skies-widescreen*.dolpatch:
# CACHE_INPUTS: tools/ci-codegen.sh tools/applydolpatch.py lib/ModernGekko/vendor/dolphin/module-template/gen_module_tables.py patches/dolrecomp-*.patch
set -euo pipefail
cd "$(dirname "$0")/.."
SECRETS=$(cd "$1" && pwd) OUT=$2
rm -rf "$OUT"; mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
# GNU coreutils on Linux and in CI, the BSD tools on macOS
NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu)
sha256() { if command -v sha256sum >/dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }
DOLRECOMP=${DOLRECOMP:-lib/ModernGekko/build/dolrecomp}
TABLES=lib/ModernGekko/vendor/dolphin/module-template/gen_module_tables.py
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

gen() {  # gen DOL NAME PROFILE [env...] -- [dolrecomp args...]  (PROFILE "" or missing: no PGO)
  local dol=$1 name=$2 prof=$3; shift 3
  local envs=() pgo=(); while [ "$1" != -- ]; do envs+=("$1"); shift; done; shift
  if [ -n "$prof" ] && [ -f "$prof" ]; then pgo=(--profile-use "$prof"); else echo "($name: no profile, no PGO)"; fi
  echo "== $name"
  env "${envs[@]}" "$DOLRECOMP" -j"$NPROC" --backend=llvm --state-in-memory "$@" \
    ${pgo[@]+"${pgo[@]}"} --cpu gekko --gamecube "$dol" "$OUT/$name" > "$OUT/$name.log" 2>&1 ||
    { rc=$?  # not the log itself: its lines carry game addresses and code, and CI logs go public
      echo "dolrecomp failed for $name (exit $rc, $(grep -ciE 'error|fatal' "$OUT/$name.log") error lines);" \
        "rerun tools/ci-codegen.sh locally to read the log"; exit 1; }
  local g=$OUT/$name/generated
  python3 -I "$TABLES" "$g/generated.h" "$g/generated_smc.txt" "$dol" "$g/module_tables.inc"
}

region() {  # region ORIGINAL_DOL DOLPATCH SUFFIX
  local original=$1 patch=$2 suffix=$3
  local dol=$WORK/main$suffix.dol name prof cpu triple args envs
  python3 -I tools/applydolpatch.py "$original" "$patch" "$dol"
  while read -r name prof cpu triple args; do
    [[ $TARGETS == *" $name "* ]] || continue
    envs=("DOLRECOMP_LLVM_TARGET=$triple")
    [ "$cpu" = - ] || envs+=("DOLRECOMP_LLVM_CPU=$cpu")
    # shellcheck disable=SC2086  # args: words
    gen "$dol" "$name$suffix" "$SECRETS/pgo/$prof$suffix.profdata" "${envs[@]}" -- $args < /dev/null
  done < "$WORK/targets"
  return 0
}
grep -v '^#' tools/codegen-targets.txt | grep . > "$WORK/targets"
TARGETS=" ${TARGETS:-$(cut -d" " -f1 "$WORK/targets" | tr "\n" " ")} "
if [ -z "${REGIONS:-}" ]; then
  REGIONS=us
  [ -f "$SECRETS/main-eu.dol" ] && REGIONS+=" eu" || echo "(no main-eu.dol: no European game)"
  [ -f "$SECRETS/main-jp.dol" ] && REGIONS+=" jp" || echo "(no main-jp.dol: no Japanese game)"
fi
REGIONS=" $REGIONS "
for t in $TARGETS; do
  grep -q "^$t " "$WORK/targets" || { echo "no target $t in tools/codegen-targets.txt"; exit 1; }
done
[[ $REGIONS == *" us "* ]] && region "$SECRETS/main.dol" release/skies-widescreen.dolpatch ""
[[ $REGIONS == *" eu "* ]] && region "$SECRETS/main-eu.dol" release/skies-widescreen-eu.dolpatch -eu
[[ $REGIONS == *" jp "* ]] && region "$SECRETS/main-jp.dol" release/skies-widescreen-jp.dolpatch -jp
# No copy of a DOL (original or patched) may leave this step.
# (Listings go to files first: `find | grep -q` under pipefail can fail on find's SIGPIPE.)
find "$OUT" -type f -size +1M -exec sh -c 'if command -v sha256sum >/dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi' sh {} + > "$WORK/sums"
for dol in "$SECRETS"/main*.dol "$WORK"/main*.dol; do
  [ -f "$dol" ] || continue
  if grep -q "^$(sha256 "$dol" | cut -d' ' -f1) " "$WORK/sums"; then
    echo "a DOL ended up in $OUT"; exit 1
  fi
done
find "$OUT" -iname '*.dol' > "$WORK/dols"
[ -s "$WORK/dols" ] && { echo "a .dol file ended up in $OUT"; exit 1; }
echo "codegen done: $OUT"
