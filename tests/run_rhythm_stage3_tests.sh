#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_rhythm_stage3_source_regressions.py"

COMMON_SOURCES=(
  "${ROOT_DIR}/src/generation/generation_context.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_catalog.cpp"
  "${ROOT_DIR}/src/generation/rhythm/relationship_resolver.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer.cpp"
  "${ROOT_DIR}/src/generation/rhythm/reference_vocabulary.cpp"
)

build_and_run() {
  local compiler="$1"
  local output="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    "$@" \
    "${ROOT_DIR}/tests/test_rhythm_stage3_reference_vocabulary.cpp" \
    "${COMMON_SOURCES[@]}" \
    -o "${output}"
  "${output}"
}

build_and_run "${CXX:-g++}" \
  "${BUILD_DIR}/test_rhythm_stage3_gcc"

if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ \
    "${BUILD_DIR}/test_rhythm_stage3_clang"
fi

build_and_run "${CXX:-g++}" \
  "${BUILD_DIR}/test_rhythm_stage3_sanitize" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

printf 'Groove Vocabulary Stage 3 reference vocabulary matrix: OK\n'
