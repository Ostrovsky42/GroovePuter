#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_0_9_9_f08_source_regressions.py"

build_and_run() {
  local compiler="$1"
  local output="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror \
    -Wno-c++20-extensions -I"${ROOT_DIR}" "$@" \
    "${ROOT_DIR}/src/generation/generation_context.cpp" \
    "${ROOT_DIR}/src/generation/roles/chord_progression.cpp" \
    "${ROOT_DIR}/tests/test_0_9_9_f08_harmonic_rhythm.cpp" \
    -o "${output}"
  "${output}"
}

build_corpus_and_run() {
  local compiler="$1"
  local output="$2"
  "${compiler}" -std=c++17 -Wall -Wextra -Werror \
    -Wno-c++20-extensions -I"${ROOT_DIR}" \
    "${ROOT_DIR}/src/generation/generation_context.cpp" \
    "${ROOT_DIR}/src/generation/roles/chord_progression.cpp" \
    "${ROOT_DIR}/tests/test_0_9_9_f08_1_vocabulary_corpus.cpp" \
    -o "${output}"
  "${output}"
}

build_and_run "${CXX:-g++}" "${BUILD_DIR}/test_0_9_9_f08_gcc"
if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ "${BUILD_DIR}/test_0_9_9_f08_clang"
fi
build_and_run "${CXX:-g++}" "${BUILD_DIR}/test_0_9_9_f08_sanitize" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

build_corpus_and_run "${CXX:-g++}" \
  "${BUILD_DIR}/test_0_9_9_f08_1_vocabulary_corpus"

printf '0.9.9-F08.1 harmonic-rhythm host matrix: OK\n'
