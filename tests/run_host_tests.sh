#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_source_regressions.py"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_sampler_voice.cpp" \
  "${ROOT_DIR}/src/sampler/sampler_voice.cpp" \
  -o "${BUILD_DIR}/test_sampler_voice"

"${BUILD_DIR}/test_sampler_voice"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -pthread \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_audio_mutation_gate.cpp" \
  -o "${BUILD_DIR}/test_audio_mutation_gate"

"${BUILD_DIR}/test_audio_mutation_gate"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -I"${ROOT_DIR}" \
  -I"${ROOT_DIR}/platform_sdl" \
  -include "${ROOT_DIR}/platform_sdl/arduino_compat.h" \
  "${ROOT_DIR}/tests/test_scene_roundtrip.cpp" \
  "${ROOT_DIR}/scenes.cpp" \
  "${ROOT_DIR}/json_evented.cpp" \
  "${ROOT_DIR}/src/audio/pattern_paging.cpp" \
  -o "${BUILD_DIR}/test_scene_roundtrip"

"${BUILD_DIR}/test_scene_roundtrip"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -I"${ROOT_DIR}" \
  -I"${ROOT_DIR}/platform_sdl" \
  -include "${ROOT_DIR}/platform_sdl/arduino_compat.h" \
  "${ROOT_DIR}/tests/test_pattern_paging.cpp" \
  "${ROOT_DIR}/src/audio/pattern_paging.cpp" \
  -o "${BUILD_DIR}/test_pattern_paging"

"${BUILD_DIR}/test_pattern_paging"
echo "host regressions: OK"
