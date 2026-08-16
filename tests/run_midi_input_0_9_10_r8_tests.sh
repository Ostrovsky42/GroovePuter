#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests"
CXX="${CXX:-g++}"
mkdir -p "${BUILD_DIR}"
bash "${ROOT}/tests/run_midi_input_0_9_10_r7_tests.sh"
"${CXX}" -std=c++17 -Wall -Wextra -Werror -pedantic -I"${ROOT}" \
  "${ROOT}/tests/test_midi_input_0_9_10_r8.cpp" \
  -o "${BUILD_DIR}/test_midi_input_0_9_10_r8"
"${BUILD_DIR}/test_midi_input_0_9_10_r8"
python3 "${ROOT}/tests/test_midi_input_0_9_10_r8_source_regressions.py"
echo "MIDI Input 0.9.10 R8 SUCCESS"
