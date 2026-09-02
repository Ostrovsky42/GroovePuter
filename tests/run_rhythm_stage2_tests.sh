#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_rhythm_stage2_source_regressions.py"

COMMON_SOURCES=(
  "${ROOT_DIR}/src/generation/generation_context.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_catalog.cpp"
  "${ROOT_DIR}/src/generation/rhythm/relationship_resolver.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer.cpp"
)

build_and_run() {
  local compiler="$1"
  local test_source="$2"
  local output="$3"
  shift 3
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    "$@" "${test_source}" "${COMMON_SOURCES[@]}" -o "${output}"
  "${output}"
}

run_suite() {
  local suffix="$1"
  local compiler="$2"
  shift 2

  build_and_run "${compiler}" \
    "${ROOT_DIR}/tests/test_rhythm_stage2.cpp" \
    "${BUILD_DIR}/test_rhythm_stage2_${suffix}" "$@"

  build_and_run "${compiler}" \
    "${ROOT_DIR}/tests/test_rhythm_stage2_adversarial.cpp" \
    "${BUILD_DIR}/test_rhythm_stage2_adversarial_${suffix}" "$@"

  build_and_run "${compiler}" \
    "${ROOT_DIR}/tests/test_rhythm_stage2_adversarial_relationships.cpp" \
    "${BUILD_DIR}/test_rhythm_stage2_adversarial_relationships_${suffix}" "$@"

  build_and_run "${compiler}" \
    "${ROOT_DIR}/tests/test_rhythm_stage2_adversarial_respond.cpp" \
    "${BUILD_DIR}/test_rhythm_stage2_adversarial_respond_${suffix}" "$@"

  build_and_run "${compiler}" \
    "${ROOT_DIR}/tests/test_rhythm_stage2_atlas_realization.cpp" \
    "${BUILD_DIR}/test_rhythm_stage2_atlas_realization_${suffix}" "$@"
}

run_suite gcc "${CXX:-g++}"

if command -v clang++ >/dev/null 2>&1; then
  run_suite clang clang++
fi

run_suite sanitize "${CXX:-g++}" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

printf 'Groove Vocabulary Stage 2 full host matrix: OK\n'
