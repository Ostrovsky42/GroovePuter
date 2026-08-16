#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests"
CXX="${CXX:-g++}"
mkdir -p "${BUILD_DIR}"
for stage in r2 r3a r3b1; do
  "${CXX}" -std=c++17 -Wall -Wextra -Werror -pedantic -I"${ROOT}" \
    "${ROOT}/tests/test_midi_input_0_9_10_${stage}.cpp" \
    -o "${BUILD_DIR}/test_midi_input_0_9_10_${stage}"
  "${BUILD_DIR}/test_midi_input_0_9_10_${stage}"
done
python3 "${ROOT}/tests/test_midi_input_0_9_10_r3b2_source_regressions.py"
echo "MIDI Input 0.9.10 R3b2 SUCCESS"
