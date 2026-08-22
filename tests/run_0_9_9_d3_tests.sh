#!/usr/bin/env bash
set -euo pipefail

bash tests/run_0_9_9_d2_tests.sh

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
  tests/test_song_live_arrangement_0_9_9.cpp \
  -o build/host-tests/test_song_live_arrangement_0_9_9
build/host-tests/test_song_live_arrangement_0_9_9

python3 tests/test_song_live_arrangement_0_9_9_source.py

# Re-run the direct owners D3 extends.
python3 tests/test_phrase_live_arrangement_0_9_9_source.py
python3 tests/test_generation_activation_0_9_9_source.py

echo "0.9.9-D3 focused suite: PASS"
