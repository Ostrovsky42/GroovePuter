#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_rhythm_stage4_source_regressions.py"

COMMON_SOURCES=(
  "${ROOT_DIR}/src/generation/materialization/pattern_materializer.cpp"
  "${ROOT_DIR}/src/generation/shadow/pattern_shadow_metrics.cpp"
)

build_and_run() {
  local compiler="$1"
  local suffix="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -I"${ROOT_DIR}" \
    "$@" \
    "${ROOT_DIR}/tests/test_rhythm_stage4_materializer.cpp" \
    "${COMMON_SOURCES[@]}" \
    -o "${BUILD_DIR}/test_rhythm_stage4_${suffix}"
  "${BUILD_DIR}/test_rhythm_stage4_${suffix}"
}

build_and_run "${CXX:-g++}" gcc

if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ clang
fi

build_and_run "${CXX:-g++}" sanitize \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

printf 'Groove Vocabulary Stage 4 materializer/shadow host matrix: OK\n'
