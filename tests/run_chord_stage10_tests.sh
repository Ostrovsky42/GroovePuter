#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_chord_stage10_source_regressions.py"

SOURCES=(
  "${ROOT_DIR}/src/generation/generation_context.cpp"
  "${ROOT_DIR}/src/generation/feel/feel_interpreter.cpp"
  "${ROOT_DIR}/src/generation/roles/semantic_pattern_projector.cpp"
  "${ROOT_DIR}/src/generation/roles/chord_rhythm.cpp"
)

build_and_run() {
  local compiler="$1"
  local output="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror \
    -Wno-c++20-extensions -I"${ROOT_DIR}" "$@" \
    "${SOURCES[@]}" "${ROOT_DIR}/tests/test_chord_stage10.cpp" \
    -o "${output}"
  "${output}"
}

build_and_run "${CXX:-g++}" "${BUILD_DIR}/test_chord_stage10_gcc"
if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ "${BUILD_DIR}/test_chord_stage10_clang"
fi
build_and_run "${CXX:-g++}" "${BUILD_DIR}/test_chord_stage10_sanitize" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

printf 'Generation Stage 10 host matrix: OK\n'
