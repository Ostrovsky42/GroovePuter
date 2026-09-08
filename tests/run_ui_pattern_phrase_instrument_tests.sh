#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"
BUILD_DIR="${ROOT_DIR}/build/ui-pattern-phrase-instrument"
mkdir -p "${BUILD_DIR}"

g++ -std=c++17 -I. tests/test_ui_pattern_phrase_instrument_controls.cpp \
  -o "${BUILD_DIR}/test_ui_pattern_phrase_instrument_controls"
"${BUILD_DIR}/test_ui_pattern_phrase_instrument_controls"

g++ -std=c++17 -I. tests/test_ui_phrase_selected_event_edits.cpp \
  -o "${BUILD_DIR}/test_ui_phrase_selected_event_edits"
"${BUILD_DIR}/test_ui_phrase_selected_event_edits"

python3 tests/test_ui_phrase_roll_selection_causality.py
python3 tests/test_ui_pattern_phrase_instrument_integration.py

# Existing focused causal contracts that this checkpoint must preserve.
bash tests/run_ui_constitution_u4b7_tests.sh
bash tests/run_ui_constitution_u4b8_tests.sh
bash tests/run_ui_constitution_u4b9_tests.sh
bash tests/run_ui_constitution_u4c1_tests.sh
bash tests/run_ui_constitution_u4c2_tests.sh
bash tests/run_ui_constitution_u4c3_tests.sh
bash tests/run_ui_constitution_u4d1_tests.sh
bash tests/run_ui_pattern_phrase_hardware_controls_tests.sh
bash tests/run_ui_content_bounds_tests.sh

# U4B6 MAKE PHRASE/materialization is now part of the instrument integration
# contract above and the U4B9 centralized PhraseSourceToggle ownership gate.
# The historical run_u4b6_make_phrase_entry_tests.sh no longer exists.

printf '%s\n' 'Pattern/Phrase instrument focused gate: PASS'
