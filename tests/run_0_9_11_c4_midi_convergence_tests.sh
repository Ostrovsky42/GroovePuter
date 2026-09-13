#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/c4-midi"
CXX="${CXX:-g++}"
mkdir -p "${BUILD_DIR}"

"${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_0_9_11_c4_midi_ingress.cpp" \
  -o "${BUILD_DIR}/test_c4_midi_ingress"
"${BUILD_DIR}/test_c4_midi_ingress"

"${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_0_9_11_c4_midi_panic_cc123.cpp" \
  "${ROOT_DIR}/src/midi/usb_midi_output.cpp" \
  -o "${BUILD_DIR}/test_c4_midi_panic_cc123"
"${BUILD_DIR}/test_c4_midi_panic_cc123"

echo "0.9.11 C4 MIDI convergence: PASS"
