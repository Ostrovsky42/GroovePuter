#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"; B="$ROOT/build/host-tests"; mkdir -p "$B"
bash "$ROOT/tests/run_midi_input_0_9_10_r6_tests.sh"
${CXX:-g++} -std=c++17 -Wall -Wextra -Werror -pedantic -I"$ROOT" "$ROOT/tests/test_midi_input_0_9_10_r7.cpp" -o "$B/test_midi_input_0_9_10_r7"
"$B/test_midi_input_0_9_10_r7"; python3 "$ROOT/tests/test_midi_input_0_9_10_r7_source_regressions.py"; echo "MIDI Input 0.9.10 R7 SUCCESS"
