#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

bash "${ROOT_DIR}/tests/run_midi_device_capabilities_0_9_7_tests.sh"
python3 "${ROOT_DIR}/tests/test_midi_device_profile_runtime_0_9_7_source_regressions.py"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_midi_device_profile_runtime_0_9_7.cpp" \
  "${ROOT_DIR}/src/midi/midi_companion_settings.cpp" \
  -o "${BUILD_DIR}/test_midi_device_profile_runtime_0_9_7"

"${BUILD_DIR}/test_midi_device_profile_runtime_0_9_7"

echo "0.9.7-R3 MIDI device profile runtime: PASS"
