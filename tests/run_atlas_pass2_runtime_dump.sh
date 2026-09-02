#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build/atlas-pass2"
mkdir -p "$BUILD_DIR"

CXX_BIN="${CXX:-g++}"
"$CXX_BIN" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-c++20-extensions \
  -I"$ROOT" \
  "$ROOT/tools/atlas/dump_runtime_rhythm_catalog.cpp" \
  "$ROOT/src/generation/rhythm/reference_vocabulary.cpp" \
  -o "$BUILD_DIR/dump_runtime_rhythm_catalog"

DUMP="$BUILD_DIR/runtime_rhythm_catalog.tsv"
"$BUILD_DIR/dump_runtime_rhythm_catalog" > "$DUMP"

grep -qx $'FORMAT\tGROOVEPUTER_RUNTIME_RHYTHM_V1' "$DUMP"
grep -qx $'COUNT\t20' "$DUMP"
[[ "$(grep -c $'^A\t' "$DUMP")" -eq 20 ]]
[[ "$(grep -c $'^L\t' "$DUMP")" -gt 0 ]]

FILTERED="$BUILD_DIR/runtime_rhythm_baseline.tsv"
grep -E $'^(FORMAT|A|L|COUNT)\t' "$DUMP" > "$FILTERED"
diff -u "$ROOT/docs/architecture/atlas_pass2/RUNTIME_RHYTHM_BASELINE.tsv" "$FILTERED"

"$CXX_BIN" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-c++20-extensions \
  -I"$ROOT" \
  "$ROOT/tools/atlas/dump_runtime_rhythm_topology_v2.cpp" \
  "$ROOT/src/generation/rhythm/reference_vocabulary.cpp" \
  -o "$BUILD_DIR/dump_runtime_rhythm_topology_v2"

TOPOLOGY="$BUILD_DIR/runtime_rhythm_topology_v2.tsv"
"$BUILD_DIR/dump_runtime_rhythm_topology_v2" > "$TOPOLOGY"
grep -qx $'FORMAT\tGROOVEPUTER_RUNTIME_RHYTHM_TOPOLOGY_V2' "$TOPOLOGY"
grep -qx $'COUNT\t20' "$TOPOLOGY"
[[ "$(grep -c $'^A\t' "$TOPOLOGY")" -eq 20 ]]
[[ "$(grep -c $'^L\t' "$TOPOLOGY")" -gt 0 ]]
[[ "$(grep -c $'^R\t' "$TOPOLOGY")" -gt 0 ]]
[[ "$(grep -c $'^S\t' "$TOPOLOGY")" -gt 0 ]]
diff -u "$ROOT/docs/architecture/atlas_pass2/RUNTIME_RHYTHM_TOPOLOGY_V2.tsv" "$TOPOLOGY"

"$CXX_BIN" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-c++20-extensions \
  -I"$ROOT" \
  "$ROOT/tools/atlas/dump_runtime_rhythm_calibration.cpp" \
  "$ROOT/src/generation/generation_context.cpp" \
  "$ROOT/src/generation/rhythm/rhythm_catalog.cpp" \
  "$ROOT/src/generation/rhythm/relationship_resolver.cpp" \
  "$ROOT/src/generation/rhythm/rhythm_realizer.cpp" \
  "$ROOT/src/generation/rhythm/reference_vocabulary.cpp" \
  -o "$BUILD_DIR/dump_runtime_rhythm_calibration"

CALIBRATION="$BUILD_DIR/runtime_rhythm_calibration.tsv"
"$BUILD_DIR/dump_runtime_rhythm_calibration" > "$CALIBRATION"
grep -qx $'FORMAT\tGROOVEPUTER_RUNTIME_RHYTHM_CALIBRATION_V1' "$CALIBRATION"
grep -qx $'COUNT\t20' "$CALIBRATION"
[[ "$(grep -c $'^SELF\t' "$CALIBRATION")" -eq 20 ]]
[[ "$(grep -c $'^CONF\t' "$CALIBRATION")" -eq 380 ]]
grep -q $'^AGG_SELF\t' "$CALIBRATION"
grep -q $'^AGG_CONF\t' "$CALIBRATION"
# Every archetype must cover all 64 of its own generated P1 samples.
awk -F '\t' '$1 == "SELF" && $5 != 64 { bad = 1 } END { exit bad }' "$CALIBRATION"

CALIBRATION_BASELINE="$BUILD_DIR/runtime_rhythm_calibration_baseline.tsv"
awk -F '\t' '
  $1 == "FORMAT" || $1 == "SELF" || $1 == "AGG_SELF" || $1 == "AGG_CONF" || $1 == "COUNT" { print; next }
  $1 == "CONF" && $4 != 0 { print }
' "$CALIBRATION" > "$CALIBRATION_BASELINE"
diff -u "$ROOT/docs/architecture/atlas_pass2/RUNTIME_RHYTHM_CALIBRATION_V1.tsv" "$CALIBRATION_BASELINE"

python3 "$ROOT/tests/test_atlas_pass2_hardening.py"

echo "Atlas Pass 2 runtime catalog/topology/calibration/hardening gates: OK"
if [[ "${ATLAS_PASS2_PRINT_RUNTIME_DUMP:-0}" == "1" ]]; then
  cat "$DUMP"
  cat "$TOPOLOGY"
  cat "$CALIBRATION"
fi
