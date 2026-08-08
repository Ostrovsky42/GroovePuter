#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
mkdir -p "${BUILD_DIR}"

COMMON_SOURCES=(
  "${ROOT_DIR}/src/generation/generation_context.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_catalog.cpp"
  "${ROOT_DIR}/src/generation/rhythm/relationship_resolver.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer.cpp"
  "${ROOT_DIR}/src/generation/audition/rhythm_audition_catalog.cpp"
)

build_source_and_run() {
  local compiler="$1"
  local test_source="$2"
  local output="$3"
  shift 3
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    "$@" \
    "${test_source}" \
    "${COMMON_SOURCES[@]}" \
    -o "${output}"
  "${output}"
}

build_source_and_run "${CXX:-g++}" \
  "${ROOT_DIR}/tests/test_rhythm_stage3a_audition_diagnostics.cpp" \
  "${BUILD_DIR}/test_rhythm_stage3a_diagnostics"

build_source_and_run "${CXX:-g++}" \
  "${ROOT_DIR}/tests/test_rhythm_stage3a_audition_catalog.cpp" \
  "${BUILD_DIR}/test_rhythm_stage3a_gcc"

if command -v clang++ >/dev/null 2>&1; then
  build_source_and_run clang++ \
    "${ROOT_DIR}/tests/test_rhythm_stage3a_audition_catalog.cpp" \
    "${BUILD_DIR}/test_rhythm_stage3a_clang"
fi

build_source_and_run "${CXX:-g++}" \
  "${ROOT_DIR}/tests/test_rhythm_stage3a_audition_catalog.cpp" \
  "${BUILD_DIR}/test_rhythm_stage3a_sanitize" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

printf 'Groove Vocabulary Stage 3A audition host matrix: OK\n'
