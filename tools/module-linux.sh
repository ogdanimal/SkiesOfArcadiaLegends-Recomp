#!/usr/bin/env bash
# Link the Linux module (gGEAE8P_recomp.so) from an existing DolRecomp LLVM output with the module
# template, configured as moderngekko-port configures it (system C compiler, -O2, no LTO). For outputs
# the port doesn't build itself: CI's codegen (tools/ci-codegen.sh), where the module tables come
# prebuilt so no DOL is needed here.
#   tools/module-linux.sh GENERATED_DIR BUILD_DIR    (MODULE_TABLES_INC=... to use prebuilt tables)
set -euo pipefail
cd "$(dirname "$0")/.."
GEN=$(realpath "${1:?usage: tools/module-linux.sh GENERATED_DIR BUILD_DIR}")
OUT=$(realpath -m "${2:?usage: tools/module-linux.sh GENERATED_DIR BUILD_DIR}")
D=$PWD/lib/ModernGekko/vendor/dolphin
TABLES=()
[ -n "${MODULE_TABLES_INC:-}" ] && TABLES=("-DMODULE_TABLES_INC=$(realpath "$MODULE_TABLES_INC")")
# thousands of chunk objects: the link line doesn't fit in ARG_MAX without a response file
export CMAKE_NINJA_FORCE_RESPONSE_FILE=1
cmake -S "$D/module-template" -B "$OUT" -G Ninja -DCMAKE_BUILD_TYPE=Release -DGAME_ID=GEAE8P \
  "-DGENERATED_DIR=$GEN" "-DGXRUNTIME_DIR=$D/GXRuntime" \
  "-DCHASSIS_ABI_DIR=$D/Source/Core/Core/PowerPC/StaticRecomp" "${TABLES[@]}"
cmake --build "$OUT"
ls -la "$OUT/gGEAE8P_recomp.so"
