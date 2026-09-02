#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build/undo-0.9.8-r2"

mkdir -p "$BUILD_DIR"

python3 "$ROOT/tests/test_undo_owner_0_9_8_source_regressions.py"

g++ -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions \
  -I"$ROOT" \
  "$ROOT/tests/test_undo_owner_0_9_8.cpp" \
  -o "$BUILD_DIR/test_undo_owner_0_9_8"

"$BUILD_DIR/test_undo_owner_0_9_8"

bash "$ROOT/tests/run_undo_0_9_8_r2_measurements.sh"

echo "0.9.8 R2 authoritative Undo owner: PASS"
