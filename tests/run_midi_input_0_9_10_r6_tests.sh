#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"; B="$ROOT/build/host-tests"; mkdir -p "$B"
bash "$ROOT/tests/run_midi_input_0_9_10_r5_tests.sh"
${CXX:-g++} -std=c++17 -Wall -Wextra -Werror -pedantic -I"$ROOT" "$ROOT/tests/test_midi_input_0_9_10_r6.cpp" -o "$B/test_midi_input_0_9_10_r6"
"$B/test_midi_input_0_9_10_r6"
python3 "$ROOT/tests/test_midi_input_0_9_10_r6_source_regressions.py"
echo "MIDI Input 0.9.10 R6 SUCCESS"
