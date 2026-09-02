#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_melodic_stage11_source_regressions.py"

SOURCES=(
  "${ROOT_DIR}/src/generation/generation_context.cpp"
  "${ROOT_DIR}/src/generation/feel/feel_interpreter.cpp"
  "${ROOT_DIR}/src/generation/roles/semantic_pattern_projector.cpp"
  "${ROOT_DIR}/src/generation/roles/melodic_motif.cpp"
)

build_and_run() {
  local compiler="$1"
  local output="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror \
    -Wno-c++20-extensions -I"${ROOT_DIR}" "$@" \
    "${SOURCES[@]}" "${ROOT_DIR}/tests/test_melodic_stage11.cpp" \
    -o "${output}"
  "${output}"
}

build_and_run "${CXX:-g++}" "${BUILD_DIR}/test_melodic_stage11_gcc"
if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ "${BUILD_DIR}/test_melodic_stage11_clang"
fi
build_and_run "${CXX:-g++}" "${BUILD_DIR}/test_melodic_stage11_sanitize" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

printf 'Generation Stage 11 host matrix: OK\n'
