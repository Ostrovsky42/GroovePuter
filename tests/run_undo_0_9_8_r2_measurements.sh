#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build/undo-0.9.8-r2"

mkdir -p "$BUILD_DIR"

g++ -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions \
  -I"$ROOT" \
  "$ROOT/tests/measure_undo_payloads_0_9_8.cpp" \
  -o "$BUILD_DIR/measure_undo_payloads_0_9_8"

"$BUILD_DIR/measure_undo_payloads_0_9_8"
