#!/usr/bin/env bash
set -euo pipefail

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
  tests/test_0_9_9_p1a_pattern_lease_owner.cpp \
  -o build/host-tests/test_0_9_9_p1a_pattern_lease_owner

build/host-tests/test_0_9_9_p1a_pattern_lease_owner
python3 tests/test_0_9_9_p1a_pattern_lease_owner_source.py

echo "0.9.9-P1a focused suite: PASS"
