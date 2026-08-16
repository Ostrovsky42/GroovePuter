#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build/undo-0.9.8-r3"

mkdir -p "$BUILD_DIR"

# R3 is additive on top of the accepted R2 owner/receipt contract.
bash "$ROOT/tests/run_undo_0_9_8_r2_tests.sh"

python3 "$ROOT/tests/test_pattern_mutations_0_9_8_r3_source_regressions.py"

g++ -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions \
  -I"$ROOT" \
  "$ROOT/tests/test_pattern_mutations_0_9_8_r3.cpp" \
  -o "$BUILD_DIR/test_pattern_mutations_0_9_8_r3"

"$BUILD_DIR/test_pattern_mutations_0_9_8_r3"

echo "0.9.8 R3 Pattern persistent mutation migration: PASS"
