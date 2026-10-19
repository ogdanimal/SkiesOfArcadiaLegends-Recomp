#!/usr/bin/env bash
# CI code generation: the only step that ever sees the game's DOLs. From the original main.dol of each
# region and the PGO profiles (the private secrets repo), make the patched DOL with the region's
# release/*.dolpatch, run DolRecomp for Windows (x86_64 COFF), Linux (x86_64 ELF), Android (aarch64)
# and macOS (arm64 Mach-O), and precompute each module's module_tables.inc so the build jobs never
# need the DOL (-DMODULE_TABLES_INC).
#   tools/ci-codegen.sh SECRETS_DIR OUT_DIR
#     SECRETS_DIR: main.dol (USA, GEAE8P), main-eu.dol (European, GEAP8P), pgo/windows.profdata,
#                  pgo/android.profdata, and optionally pgo/windows-eu.profdata, pgo/android-eu.profdata,
#                  pgo/macos.profdata, pgo/macos-eu.profdata
#     OUT_DIR:     USA: windows/generated, linux/generated, android/generated, macos/generated;
#                  European: windows-eu/generated, linux-eu/generated, android-eu/generated,
#                  macos-eu/generated
#                  (no DOL in any; checked)
# pgo/windows.profdata is the desktop profile (trained on Linux with tools/pgo-wsl.sh), used for both
# x86_64 targets. A region without a profile is built without PGO (the profiles are keyed by code
# addresses, so the USA ones don't fit the European DOL). DOLRECOMP defaults to
# lib/ModernGekko/build/dolrecomp; TARGETS (default "windows linux android") and REGIONS (default
# "us eu", or "us" when the secrets have no main-eu.dol) limit the run.
# The repository files the output depends on (for a cache key), besides the secrets:
# CACHE_INPUTS: tools/ci-codegen.sh tools/applydolpatch.py release/skies-widescreen.dolpatch release/skies-widescreen-eu.dolpatch lib/ModernGekko/vendor/dolphin/module-template/gen_module_tables.py patches/dolrecomp-*.patch
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

region() {  # region ORIGINAL_DOL DOLPATCH SUFFIX DESKTOP_PROFILE ANDROID_PROFILE MACOS_PROFILE
  local original=$1 patch=$2 suffix=$3 desktop=$4 android=$5 macos=$6
  local dol=$WORK/main$suffix.dol
  python3 -I tools/applydolpatch.py "$original" "$patch" "$dol"
  [[ $TARGETS == *" windows "* ]] &&
    gen "$dol" "windows$suffix" "$desktop" DOLRECOMP_LLVM_TARGET=x86_64-pc-windows-msvc --
  [[ $TARGETS == *" linux "* ]] &&
    gen "$dol" "linux$suffix" "$desktop" DOLRECOMP_LLVM_TARGET=x86_64-pc-linux-gnu --
  [[ $TARGETS == *" android "* ]] &&
    gen "$dol" "android$suffix" "$android" DOLRECOMP_LLVM_CPU=cortex-a77 \
      DOLRECOMP_LLVM_TARGET=aarch64-unknown-linux-android24 -- --targets=aarch64
  [[ $TARGETS == *" macos "* ]] &&
    gen "$dol" "macos$suffix" "$macos" DOLRECOMP_LLVM_CPU=apple-m1 \
      DOLRECOMP_LLVM_TARGET=arm64-apple-macosx13.0 -- --targets=aarch64
  return 0
}
TARGETS=" ${TARGETS:-windows linux android macos} "
if [ -z "${REGIONS:-}" ]; then
  REGIONS="us eu"
  [ -f "$SECRETS/main-eu.dol" ] || { REGIONS=us; echo "(no main-eu.dol: the USA game only)"; }
fi
REGIONS=" $REGIONS "
[[ $REGIONS == *" us "* ]] &&
  region "$SECRETS/main.dol" release/skies-widescreen.dolpatch "" \
    "$SECRETS/pgo/windows.profdata" "$SECRETS/pgo/android.profdata" "$SECRETS/pgo/macos.profdata"
[[ $REGIONS == *" eu "* ]] &&
  region "$SECRETS/main-eu.dol" release/skies-widescreen-eu.dolpatch -eu \
    "$SECRETS/pgo/windows-eu.profdata" "$SECRETS/pgo/android-eu.profdata" "$SECRETS/pgo/macos-eu.profdata"

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
