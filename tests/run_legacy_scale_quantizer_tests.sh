#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
mkdir -p "${BUILD_DIR}"

build_and_run() {
  local compiler="$1"
  local suffix="$2"
  shift 2

  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -I"${ROOT_DIR}" "$@" \
    "${ROOT_DIR}/src/dsp/advanced_pattern_generator.cpp" \
    "${ROOT_DIR}/tests/test_legacy_scale_quantizer.cpp" \
    -o "${BUILD_DIR}/test_legacy_scale_quantizer_${suffix}"

  "${BUILD_DIR}/test_legacy_scale_quantizer_${suffix}"
}

build_and_run "${CXX:-g++}" gcc
if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ clang
fi
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  build_and_run "${CXX:-g++}" sanitize \
    -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

printf 'Legacy scale quantizer host matrix: OK\n'
