#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_midi_output_destination_source_regressions.py"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_midi_output_mode_ui.cpp" \
  -o "${BUILD_DIR}/test_midi_output_mode_ui"

"${BUILD_DIR}/test_midi_output_mode_ui"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_midi_next_boot_settings.cpp" \
  "${ROOT_DIR}/src/midi/midi_companion_settings.cpp" \
  "${ROOT_DIR}/src/midi/midi_companion_settings_codec.cpp" \
  -o "${BUILD_DIR}/test_midi_next_boot_settings"

"${BUILD_DIR}/test_midi_next_boot_settings"
echo "MIDI output destination tests: PASS"
