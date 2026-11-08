#!/usr/bin/env bash
# STEP SETTINGS windows (Alt+E): synth STEPS and DRUMS grid.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/step-settings-tests"
CXX="${CXX:-g++}"
mkdir -p "${BUILD_DIR}"

"${CXX}" -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions \
  -I"${ROOT_DIR}" "${ROOT_DIR}/tests/test_drum_step_settings.cpp" \
  -o "${BUILD_DIR}/test_drum_step_settings"
"${BUILD_DIR}/test_drum_step_settings"

python3 "${ROOT_DIR}/tests/test_drum_step_settings_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_step_settings_popup_source_regressions.py"
