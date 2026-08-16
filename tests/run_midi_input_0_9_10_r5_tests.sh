#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests"
CXX="${CXX:-g++}"
mkdir -p "${BUILD_DIR}"
bash "${ROOT}/tests/run_midi_input_0_9_10_r4_tests.sh"
"${CXX}" -std=c++17 -Wall -Wextra -Werror -pedantic -I"${ROOT}" \
  "${ROOT}/tests/test_midi_input_0_9_10_r5.cpp" \
  -o "${BUILD_DIR}/test_midi_input_0_9_10_r5"
"${BUILD_DIR}/test_midi_input_0_9_10_r5"
python3 "${ROOT}/tests/test_midi_input_0_9_10_r5_source_regressions.py"
echo "MIDI Input 0.9.10 R5 SUCCESS"
