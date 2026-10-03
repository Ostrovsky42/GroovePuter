#!/usr/bin/env bash
set -euo pipefail

python3 tests/test_phrase_scene_source_regressions.py
python3 tests/test_phrase_ui_source_regressions.py
python3 tests/test_global_help_source_regressions.py

mkdir -p build/host-tests
COMMON_FLAGS='-std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions -I.'

g++ ${COMMON_FLAGS} tests/test_phrase_core.cpp -o build/host-tests/test_phrase_core
g++ ${COMMON_FLAGS} tests/test_phrase_persistence_preview.cpp -o build/host-tests/test_phrase_persistence_preview
g++ ${COMMON_FLAGS} tests/test_phrase_workspace.cpp -o build/host-tests/test_phrase_workspace
g++ ${COMMON_FLAGS} tests/test_global_help_content.cpp -o build/host-tests/test_global_help_content
g++ ${COMMON_FLAGS} tests/test_ui_session_state.cpp -o build/host-tests/test_ui_session_state

g++ ${COMMON_FLAGS} \
  -Iplatform_sdl \
  -include platform_sdl/arduino_compat.h \
  -c src/ui/pages/phrase_page.cpp \
  -o build/host-tests/phrase_page.o

g++ ${COMMON_FLAGS} \
  -Wno-unused-variable \
  -Wno-unused-but-set-variable \
  -Iplatform_sdl \
  -include platform_sdl/arduino_compat.h \
  tests/test_scene_roundtrip.cpp \
  scenes.cpp \
  json_evented.cpp \
  src/audio/pattern_paging.cpp \
  -o build/host-tests/test_scene_roundtrip

build/host-tests/test_phrase_core
build/host-tests/test_phrase_persistence_preview
build/host-tests/test_phrase_workspace
build/host-tests/test_global_help_content
build/host-tests/test_ui_session_state
build/host-tests/test_scene_roundtrip
