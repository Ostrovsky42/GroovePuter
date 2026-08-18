#!/usr/bin/env bash
set -euo pipefail
mkdir -p build/host-tests
g++ -std=c++17 -Wall -Wextra -Werror -I. \
  tests/test_undo_0_9_8_r9_toggle.cpp \
  -o build/host-tests/test_undo_0_9_8_r9_toggle
build/host-tests/test_undo_0_9_8_r9_toggle
python3 tests/test_undo_0_9_8_r9_source.py
python3 tests/test_undo_0_9_8_r9_hardware_followup_source.py
bash tests/run_undo_0_9_8_r2_tests.sh
bash tests/run_undo_0_9_8_r3_tests.sh
bash tests/run_undo_0_9_8_r4_tests.sh
bash tests/run_undo_0_9_8_r5_tests.sh
bash tests/run_undo_0_9_8_r6_tests.sh
bash tests/run_undo_0_9_8_r7_tests.sh
bash tests/run_undo_0_9_8_r8_tests.sh
