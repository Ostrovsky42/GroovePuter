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
  tests/test_0_9_9_d1_pattern_phrase_liveness.cpp \
  -o build/host-tests/test_0_9_9_d1_pattern_phrase_liveness
build/host-tests/test_0_9_9_d1_pattern_phrase_liveness

g++ "${CXXFLAGS[@]}" \
  tests/test_song_pattern_phrase_liveness.cpp \
  -o build/host-tests/test_song_pattern_phrase_liveness
build/host-tests/test_song_pattern_phrase_liveness

python3 tests/test_0_9_9_d1_source_regressions.py

# Re-run the persistence/legacy compatibility evidence D1 depends on.
g++ -std=c++17 -Wall -Wextra -Werror -Wno-unused-variable \
  -Wno-unused-but-set-variable -Wno-c++20-extensions \
  -I. -Iplatform_sdl -include platform_sdl/arduino_compat.h \
  tests/test_generation_scene_roundtrip.cpp \
  scenes.cpp json_evented.cpp src/audio/pattern_paging.cpp \
  -o build/host-tests/test_generation_scene_roundtrip
build/host-tests/test_generation_scene_roundtrip

g++ "${CXXFLAGS[@]}" \
  tests/test_pattern_paging_ownership.cpp \
  src/audio/pattern_paging.cpp \
  src/audio/pattern_project_cleanup.cpp \
  -o build/host-tests/test_pattern_paging_ownership
build/host-tests/test_pattern_paging_ownership

python3 tests/test_generation_0_9_9_source_regressions.py

echo "0.9.9-D1 focused suite: PASS"
