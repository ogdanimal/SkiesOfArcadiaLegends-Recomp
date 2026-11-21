#!/usr/bin/env bash
# PGO profile for one region, trained in the standalone engine (no Dolphin runner): DolRecomp makes an
# instrumented LLVM module (the same flags as tools/ci-codegen.sh, plus SEMANTICS), the standalone
# runs it headless from each training state for SECS guest seconds (unthrottled, muted; the profile
# counts guest work, so speed doesn't matter), and llvm-profdata merges the runs.
#   tools/pgo-standalone.sh REGION OUT_PROFDATA STATE...
#     REGION: us | eu | jp (the original DOL from SECRETS: main.dol, main-eu.dol, main-jp.dol)
#   env: SECRETS (default ~/skies-secrets), GAME (the extracted game; default build/game/<ID>-ws36),
#        SEMANTICS (default fast), SECS (default 46: 16 s settling + 30 s, as tools/pgo-wsl.sh),
#        DOLRECOMP (default build/dolrecomp/dolrecomp), STANDALONE (default build/standalone/skies-standalone),
#        WORK (default build/pgo-sa-REGION)
# The profile is keyed by the code, so it fits only modules made from the same patched DOL with the
# same DolRecomp semantics; the result goes to the secrets' pgo/ for CI (tools/codegen-targets.txt).
set -euo pipefail
cd "$(dirname "$0")/.."
REGION=$1 OUTP=$(realpath -m "$2"); shift 2
case $REGION in
  us) ID=GEAE8P SUF= ;;
  eu) ID=GEAP8P SUF=-eu ;;
  jp) ID=GEAJ8P SUF=-jp ;;
  *) echo "region: us, eu or jp"; exit 2 ;;
esac
SECRETS=${SECRETS:-$HOME/skies-secrets} GAME=${GAME:-build/game/$ID-ws36} SEMANTICS=${SEMANTICS:-fast}
SECS=${SECS:-46} DOLRECOMP=${DOLRECOMP:-build/dolrecomp/dolrecomp}
STANDALONE=${STANDALONE:-build/standalone/skies-standalone} WORK=$(realpath -m "${WORK:-build/pgo-sa-$REGION}")
LLVM=/usr/lib/llvm-20/bin  # the LLVM DolRecomp links: its clang, profile runtime and llvm-profdata
rm -rf "$WORK"; mkdir -p "$WORK/raw"
python3 -I tools/applydolpatch.py "$SECRETS/main$SUF.dol" "release/skies-widescreen$SUF.dolpatch" "$WORK/main.dol"
echo "== instrumented code ($SEMANTICS) $(date +%T)"
DOLRECOMP_LLVM_TARGET=x86_64-pc-linux-gnu "$DOLRECOMP" -j"$(nproc)" --backend=llvm --state-in-memory \
  --semantics "$SEMANTICS" --profile-generate "$ID-%m.profraw" --cpu gekko --gamecube "$WORK/main.dol" "$WORK/gen" \
  > "$WORK/gen.log" 2>&1
G=$WORK/gen/generated
python3 -I lib/ModernGekko/vendor/dolphin/module-template/gen_module_tables.py "$G/generated.h" \
  "$G/generated_smc.txt" "$WORK/main.dol" "$G/module_tables.inc"
rm -f "$WORK/main.dol"
echo "== instrumented module $(date +%T)"
CC=$LLVM/clang CFLAGS=-fprofile-generate LDFLAGS=-fprofile-generate GAME_ID=$ID MODULE_TABLES_INC=$G/module_tables.inc \
  tools/module-linux.sh "$G" "$WORK/module" > "$WORK/module.log" 2>&1
for st in "$@"; do
  name=$(basename "$st"); name=${name%.*}
  echo "== train $name $(date +%T)"
  rm -rf "$WORK/user"; mkdir -p "$WORK/user"
  LLVM_PROFILE_FILE="$WORK/raw/$ID-$name-%p.profraw" "$STANDALONE" --headless --unthrottled --mute \
    --game "$GAME" --module "$WORK/module/g${ID}_recomp.so" --user "$WORK/user" --load-state "$st" \
    --seconds +"$SECS" > "$WORK/train-$name.log" 2>&1
  grep -q "\[cpu\] stopped" "$WORK/train-$name.log" || { echo "$name: no clean stop (see $WORK/train-$name.log)"; exit 1; }
done
"$LLVM/llvm-profdata" merge -o "$OUTP" "$WORK"/raw/*.profraw
ls -la "$OUTP"
