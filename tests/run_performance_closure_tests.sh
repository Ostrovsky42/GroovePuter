#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/performance-closure-tests"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_performance_closure.cpp" \
  "${ROOT_DIR}/src/input/performance_keyboard.cpp" \
  -o "${BUILD_DIR}/test_performance_closure"

"${BUILD_DIR}/test_performance_closure"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}/tests/stubs" \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_internal_synth_output_cleanup.cpp" \
  "${ROOT_DIR}/src/input/internal_synth_output.cpp" \
  -o "${BUILD_DIR}/test_internal_synth_output_cleanup"

"${BUILD_DIR}/test_internal_synth_output_cleanup"
