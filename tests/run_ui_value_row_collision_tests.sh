#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"
BUILD_DIR="${ROOT_DIR}/build/ui-value-row"
mkdir -p "${BUILD_DIR}"
g++ -std=c++17 -I. -Iplatform_sdl -include platform_sdl/arduino_compat.h \
  tests/test_ui_value_row_collision.cpp -o "${BUILD_DIR}/test_ui_value_row_collision"
"${BUILD_DIR}/test_ui_value_row_collision"
printf '%s\n' 'UI value row collision gate: PASS'
