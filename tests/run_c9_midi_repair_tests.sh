#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/c9-midi-repair"
CXX="${CXX:-g++}"
mkdir -p "${BUILD_DIR}"

"${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_c9_tee_recovery.cpp" \
  -o "${BUILD_DIR}/test_c9_tee_recovery"
"${BUILD_DIR}/test_c9_tee_recovery"

"${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_c9_midi_input_dispatcher.cpp" \
  -o "${BUILD_DIR}/test_c9_midi_input_dispatcher"
"${BUILD_DIR}/test_c9_midi_input_dispatcher"

echo "C9 MIDI repair contracts: PASS"
