#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
bash tests/run_midi_input_0_9_10_r3b2_tests.sh
python3 tests/test_midi_input_0_9_10_r3c_source_regressions.py
echo "MIDI Input 0.9.10 R3c SUCCESS"
