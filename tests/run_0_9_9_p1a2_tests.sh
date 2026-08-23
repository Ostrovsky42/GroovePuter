#!/usr/bin/env bash
set -euo pipefail

bash tests/run_0_9_9_p1a_tests.sh

mkdir -p build/host-tests

CXXFLAGS=(
  -std=c++17
  -O2
  -Wall
  -Wextra
  -Werror
  -Wno-c++20-extensions
  -I.
)

g++ "${CXXFLAGS[@]}" \
  tests/test_0_9_9_p1a2_pattern_lease_generalization.cpp \
  -o build/host-tests/test_0_9_9_p1a2_pattern_lease_generalization

build/host-tests/test_0_9_9_p1a2_pattern_lease_generalization
python3 tests/test_0_9_9_p1a2_pattern_lease_source.py

echo "0.9.9-P1a2 focused + cumulative lease suite: PASS"
