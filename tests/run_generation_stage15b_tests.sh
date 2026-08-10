#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_generation_stage15b_source_regressions.py"

SOURCES=(
  "${ROOT_DIR}/src/generation/generation_context.cpp"
  "${ROOT_DIR}/src/generation/roles/melodic_pitch_intent.cpp"
)

build_and_run() {
  local compiler="$1"
  local suffix="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -I"${ROOT_DIR}" "$@" \
    "${SOURCES[@]}" \
    "${ROOT_DIR}/tests/test_generation_stage15b_melodic_intent.cpp" \
    -o "${BUILD_DIR}/test_generation_stage15b_${suffix}"
  "${BUILD_DIR}/test_generation_stage15b_${suffix}"
}

build_and_run "${CXX:-g++}" gcc
if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ clang
fi
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  build_and_run "${CXX:-g++}" sanitize \
    -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

printf 'Generation Stage 15B host matrix: OK\n'
