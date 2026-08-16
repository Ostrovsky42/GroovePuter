#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/midi-device-profiles-0-9-7"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_midi_device_profiles_0_9_7_source_regressions.py"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_midi_device_profiles_0_9_7.cpp" \
  "${ROOT_DIR}/src/midi/midi_companion_settings.cpp" \
  -o "${BUILD_DIR}/test_midi_device_profiles_0_9_7"

"${BUILD_DIR}/test_midi_device_profiles_0_9_7"

echo "0.9.7-R1 MIDI device-profile contract: PASS"
