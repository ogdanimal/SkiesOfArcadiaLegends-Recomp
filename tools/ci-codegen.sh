#!/usr/bin/env bash
# CI code generation: the only step that ever sees the game's DOL. From the original main.dol and the
# PGO profiles (the private secrets repo), make the patched DOL with release/skies-widescreen.dolpatch,
# run DolRecomp for Windows (x86_64 COFF), Linux (x86_64 ELF) and Android (aarch64), and precompute
# each module's module_tables.inc so the build jobs never need the DOL (-DMODULE_TABLES_INC).
#   tools/ci-codegen.sh SECRETS_DIR OUT_DIR
#     SECRETS_DIR: main.dol, pgo/windows.profdata, pgo/android.profdata
#     OUT_DIR:     windows/generated, linux/generated, android/generated (no DOL in any; checked)
# pgo/windows.profdata is the desktop profile (trained on Linux with tools/pgo-wsl.sh), used for both
# x86_64 targets. DOLRECOMP defaults to lib/ModernGekko/build/dolrecomp; TARGETS (default
# "windows linux android") limits the run, for local checks.
set -euo pipefail
cd "$(dirname "$0")/.."
SECRETS=$(realpath "$1") OUT=$(realpath -m "$2")
DOLRECOMP=${DOLRECOMP:-lib/ModernGekko/build/dolrecomp}
TABLES=lib/ModernGekko/vendor/dolphin/module-template/gen_module_tables.py
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

python3 -I tools/applydolpatch.py "$SECRETS/main.dol" release/skies-widescreen.dolpatch "$WORK/main.dol"
rm -rf "$OUT"; mkdir -p "$OUT"

gen() {  # gen NAME PROFILE [env...] -- [dolrecomp args...]
  local name=$1 prof=$2; shift 2
  local envs=(); while [ "$1" != -- ]; do envs+=("$1"); shift; done; shift
  echo "== $name"
  env "${envs[@]}" "$DOLRECOMP" -j"$(nproc)" --backend=llvm --state-in-memory "$@" \
    --profile-use "$prof" --cpu gekko --gamecube "$WORK/main.dol" "$OUT/$name" > "$OUT/$name.log" 2>&1 ||
    { rc=$?  # not the log itself: its lines carry game addresses and code, and CI logs go public
      echo "dolrecomp failed for $name (exit $rc, $(grep -ciE 'error|fatal' "$OUT/$name.log") error lines);" \
        "rerun tools/ci-codegen.sh locally to read the log"; exit 1; }
  local g=$OUT/$name/generated
  python3 -I "$TABLES" "$g/generated.h" "$g/generated_smc.txt" "$WORK/main.dol" "$g/module_tables.inc"
}
TARGETS=" ${TARGETS:-windows linux android} "
[[ $TARGETS == *" windows "* ]] &&
  gen windows "$SECRETS/pgo/windows.profdata" DOLRECOMP_LLVM_TARGET=x86_64-pc-windows-msvc --
[[ $TARGETS == *" linux "* ]] &&
  gen linux "$SECRETS/pgo/windows.profdata" DOLRECOMP_LLVM_TARGET=x86_64-pc-linux-gnu --
[[ $TARGETS == *" android "* ]] &&
  gen android "$SECRETS/pgo/android.profdata" DOLRECOMP_LLVM_CPU=cortex-a77 \
    DOLRECOMP_LLVM_TARGET=aarch64-unknown-linux-android24 -- --targets=aarch64

# No copy of the DOL (original or patched) may leave this step.
# (Listings go to files first: `find | grep -q` under pipefail can fail on find's SIGPIPE.)
find "$OUT" -type f -size +1M -exec sha256sum {} + > "$WORK/sums"
for h in $(sha256sum "$SECRETS/main.dol" "$WORK/main.dol" | cut -d' ' -f1); do
  if grep -q "^$h " "$WORK/sums"; then
    echo "a DOL ended up in $OUT"; exit 1
  fi
done
find "$OUT" -iname '*.dol' > "$WORK/dols"
[ -s "$WORK/dols" ] && { echo "a .dol file ended up in $OUT"; exit 1; }
echo "codegen done: $OUT"
