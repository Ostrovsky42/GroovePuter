#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/instrument-interaction"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"
cd "${ROOT_DIR}"

python3 tests/test_instrument_interaction_closure_source_regressions.py
python3 tests/test_cardputer_input_source_regressions.py
python3 tests/test_step_note_entry_source_regressions.py
python3 tests/test_pattern_mutations_0_9_8_r3_source_regressions.py
python3 tests/test_drum_grid_labels_source_regressions.py
python3 tests/test_hub_song_drum_ui_source_regressions.py

"${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
  tests/test_cardputer_input_edges.cpp \
  -o "${BUILD_DIR}/test_cardputer_input_edges"
"${BUILD_DIR}/test_cardputer_input_edges"

"${CXX}" -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions -I"${ROOT_DIR}" \
  tests/test_pattern_mutations_0_9_8_r3.cpp \
  -o "${BUILD_DIR}/test_pattern_mutations"
"${BUILD_DIR}/test_pattern_mutations"

echo "Instrument interaction closure focused tests: PASS"
