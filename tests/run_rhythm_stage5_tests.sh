#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_rhythm_stage5_source_regressions.py"

SOURCES=(
  "${ROOT_DIR}/src/generation/generation_context.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_catalog.cpp"
  "${ROOT_DIR}/src/generation/rhythm/relationship_resolver.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer.cpp"
  "${ROOT_DIR}/src/generation/rhythm/reference_vocabulary.cpp"
  "${ROOT_DIR}/src/generation/materialization/pattern_materializer.cpp"
  "${ROOT_DIR}/src/generation/migration/strong_rhythm_migration.cpp"
)

build_and_run() {
  local compiler="$1"
  local test_source="$2"
  local output="$3"
  shift 3
  "${compiler}" -std=c++17 -Wall -Wextra -Werror \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -I"${ROOT_DIR}" \
    "$@" \
    "${SOURCES[@]}" \
    "${test_source}" \
    -o "${output}"
  "${output}"
}

run_suite() {
  local suffix="$1"
  local compiler="$2"
  shift 2

  build_and_run "${compiler}" \
    "${ROOT_DIR}/tests/test_rhythm_stage5_strong_migration.cpp" \
    "${BUILD_DIR}/test_rhythm_stage5_${suffix}" \
    "$@"

  build_and_run "${compiler}" \
    "${ROOT_DIR}/tests/test_rhythm_stage5_invalid_context.cpp" \
    "${BUILD_DIR}/test_rhythm_stage5_invalid_${suffix}" \
    "$@"

  build_and_run "${compiler}" \
    "${ROOT_DIR}/tests/test_rhythm_stage5_dub_stab.cpp" \
    "${BUILD_DIR}/test_rhythm_stage5_dub_stab_${suffix}" \
    "$@"
}

run_suite gcc "${CXX:-g++}"

if command -v clang++ >/dev/null 2>&1; then
  run_suite clang clang++
fi

run_suite sanitize "${CXX:-g++}" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

printf 'Groove Vocabulary Stage 5 strong migration host matrix: OK\n'