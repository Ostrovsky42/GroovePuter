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

echo "Atlas Pass 2 runtime catalog dump: OK"
if [[ "${ATLAS_PASS2_PRINT_RUNTIME_DUMP:-0}" == "1" ]]; then
  cat "$DUMP"
fi
