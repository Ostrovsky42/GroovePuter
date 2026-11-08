#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/performance-instrument-ui-tests"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_performance_instrument_ui_source_regressions.py"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_performance_keyboard.cpp" \
  "${ROOT_DIR}/src/input/performance_keyboard.cpp" \
  -o "${BUILD_DIR}/test_performance_keyboard"

"${BUILD_DIR}/test_performance_keyboard"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_performance_midi_targets.cpp" \
  "${ROOT_DIR}/src/input/performance_keyboard.cpp" \
  -o "${BUILD_DIR}/test_performance_midi_targets"

"${BUILD_DIR}/test_performance_midi_targets"

printf '%s\n' "0.9.9 Performance Instrument UI focused tests: PASS"
