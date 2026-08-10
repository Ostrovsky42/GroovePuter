#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_cardputer_input_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_drum_grid_labels_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_hub_song_drum_ui_source_regressions.py"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_smf_track_level.cpp" \
  -o "${BUILD_DIR}/test_smf_track_level"

"${BUILD_DIR}/test_smf_track_level"

echo "Hub/Song/drum focused tests: OK"
