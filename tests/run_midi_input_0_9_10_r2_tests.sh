#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

python3 "${ROOT}/tests/test_midi_input_0_9_10_r2_source_regressions.py"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -pedantic \
  -I"${ROOT}" \
  "${ROOT}/tests/test_midi_input_0_9_10_r2.cpp" \
  -o "${BUILD_DIR}/test_midi_input_0_9_10_r2"

"${BUILD_DIR}/test_midi_input_0_9_10_r2"
