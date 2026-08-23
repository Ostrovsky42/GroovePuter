#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
CXX="${CXX:-g++}"
COMMON_FLAGS=(
  -std=c++17
  -O2
  -Wall
  -Wextra
  -Werror
  -Wno-c++20-extensions
  -I"${ROOT_DIR}"
)

mkdir -p "${BUILD_DIR}"
cd "${ROOT_DIR}"

echo "[P1b 1/6] focused KEEP / Undo ownership"
"${CXX}" "${COMMON_FLAGS[@]}" \
  tests/test_0_9_9_p1b_phrase_keep_undo.cpp \
  -o "${BUILD_DIR}/test_0_9_9_p1b_phrase_keep_undo"
"${BUILD_DIR}/test_0_9_9_p1b_phrase_keep_undo"

echo "[P1b 2/6] source contracts"
python3 tests/test_0_9_9_p1b_phrase_keep_undo_source.py

echo "[P1b 3/6] cumulative P1a"
bash tests/run_0_9_9_p1a_tests.sh

echo "[P1b 4/6] cumulative P1a2"
bash tests/run_0_9_9_p1a2_tests.sh

echo "[P1b 5/6] Phrase Core + canonical Undo regressions"
python3 tests/test_phrase_scene_source_regressions.py
python3 tests/test_phrase_ui_source_regressions.py

"${CXX}" "${COMMON_FLAGS[@]}" tests/test_phrase_core.cpp \
  -o "${BUILD_DIR}/test_phrase_core"
"${BUILD_DIR}/test_phrase_core"

"${CXX}" "${COMMON_FLAGS[@]}" tests/test_phrase_persistence_preview.cpp \
  -o "${BUILD_DIR}/test_phrase_persistence_preview"
"${BUILD_DIR}/test_phrase_persistence_preview"

"${CXX}" "${COMMON_FLAGS[@]}" tests/test_phrase_workspace.cpp \
  -o "${BUILD_DIR}/test_phrase_workspace"
"${BUILD_DIR}/test_phrase_workspace"

"${CXX}" "${COMMON_FLAGS[@]}" \
  -I"${ROOT_DIR}/platform_sdl" \
  -include "${ROOT_DIR}/platform_sdl/arduino_compat.h" \
  -c src/ui/pages/phrase_page.cpp \
  -o "${BUILD_DIR}/phrase_page.o"

bash tests/run_0_9_9_undo_regression_tests.sh

echo "[P1b 6/6] persistence + memory"
"${CXX}" "${COMMON_FLAGS[@]}" \
  -Wno-unused-variable \
  -Wno-unused-but-set-variable \
  -I"${ROOT_DIR}/platform_sdl" \
  -include "${ROOT_DIR}/platform_sdl/arduino_compat.h" \
  tests/test_scene_roundtrip.cpp \
  scenes.cpp \
  json_evented.cpp \
  src/audio/pattern_paging.cpp \
  -o "${BUILD_DIR}/test_scene_roundtrip"
"${BUILD_DIR}/test_scene_roundtrip"

"${CXX}" "${COMMON_FLAGS[@]}" \
  -Wno-unused-variable \
  -Wno-unused-but-set-variable \
  -I"${ROOT_DIR}/platform_sdl" \
  -include "${ROOT_DIR}/platform_sdl/arduino_compat.h" \
  tests/test_0_9_9_p1b_scene_persistence.cpp \
  scenes.cpp \
  json_evented.cpp \
  src/audio/pattern_paging.cpp \
  -o "${BUILD_DIR}/test_0_9_9_p1b_scene_persistence"
"${BUILD_DIR}/test_0_9_9_p1b_scene_persistence"

"${CXX}" "${COMMON_FLAGS[@]}" \
  tests/measure_0_9_9_p1b_memory.cpp \
  -o "${BUILD_DIR}/measure_0_9_9_p1b_memory"
"${BUILD_DIR}/measure_0_9_9_p1b_memory"

echo "0.9.9-P1b focused + cumulative ownership suite: PASS"
