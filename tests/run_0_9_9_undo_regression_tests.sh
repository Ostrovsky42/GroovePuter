#!/usr/bin/env bash
set -euo pipefail

mkdir -p build/host-tests

python3 tests/test_undo_0_9_8_r8_global_ctrl_z_source.py
python3 tests/test_undo_0_9_9_r9_regression_source.py
python3 tests/test_undo_0_9_9_phase_contract_source.py
python3 tests/test_drum_g_undo_0_9_9_source.py

g++ -std=c++17 -O2 -Wall -Wextra -Werror -I. \
  tests/test_undo_0_9_8_r9_toggle.cpp \
  -o build/host-tests/test_undo_0_9_8_r9_toggle
build/host-tests/test_undo_0_9_8_r9_toggle

# The regression fix must coexist with the complete D3 live-arrangement owner.
bash tests/run_0_9_9_d3_tests.sh

echo "0.9.9 R8/R9 Undo regression suite: PASS"
