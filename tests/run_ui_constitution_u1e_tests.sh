#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="${TMPDIR:-/tmp}/grooveputer-ui-u1e"
mkdir -p "$BUILD_DIR"

g++ -std=c++17 -Wall -Wextra -pedantic \
  tests/test_ui_constitution_u1e_geometry.cpp \
  -o "$BUILD_DIR/test_ui_constitution_u1e_geometry"
"$BUILD_DIR/test_ui_constitution_u1e_geometry"

python3 tests/test_ui_constitution_u1e_geometry_regressions.py

echo "UI Constitution U1E focused gate: PASS"
