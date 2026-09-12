#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/smf-note-lifetime-panic"
mkdir -p "${BUILD_DIR}"

g++ -std=c++17 -Wall -Wextra -Werror -I"${ROOT}" \
  "${ROOT}/tests/test_usb_midi_smf_output.cpp" \
  "${ROOT}/src/midi/usb_midi_output.cpp" \
  -o "${BUILD_DIR}/test_smf_note_lifetime_panic"

"${BUILD_DIR}/test_smf_note_lifetime_panic"
echo "SMF terminal note lifetime panic: PASS"
