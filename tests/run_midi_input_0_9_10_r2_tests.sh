#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

python3 tests/test_midi_input_0_9_10_r2_source_regressions.py
mkdir -p build/host-tests

g++ -std=c++17 -O2 -Wall -Wextra -Werror -I. \
  tests/test_midi_input_0_9_10_r2.cpp \
  -o build/host-tests/test_midi_input_0_9_10_r2

build/host-tests/test_midi_input_0_9_10_r2
