#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_rhythm_stage6_source_regressions.py"

SOURCES=(
  "${ROOT_DIR}/src/generation/generation_context.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_catalog.cpp"
  "${ROOT_DIR}/src/generation/rhythm/relationship_resolver.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer.cpp"
  "${ROOT_DIR}/src/generation/rhythm/bar_evolution.cpp"
)

build_and_run() {
  local compiler="$1"
  local output="$2"
  local test_source="$3"
  shift 3
  "${compiler}" -std=c++17 -Wall -Wextra -Werror \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -I"${ROOT_DIR}" \
    "$@" \
    "${SOURCES[@]}" \
    "${ROOT_DIR}/${test_source}" \
    -o "${output}"
  "${output}"
}

run_suite() {
  local compiler="$1"
  local suffix="$2"
  shift 2
  build_and_run "${compiler}" \
    "${BUILD_DIR}/test_rhythm_stage6_${suffix}" \
    "tests/test_rhythm_stage6_bar_evolution.cpp" \
    "$@"
  build_and_run "${compiler}" \
    "${BUILD_DIR}/test_rhythm_stage6_contract_${suffix}" \
    "tests/test_rhythm_stage6_contract_regressions.cpp" \
    "$@"
}

run_suite "${CXX:-g++}" gcc

if command -v clang++ >/dev/null 2>&1; then
  run_suite clang++ clang
fi

run_suite "${CXX:-g++}" sanitize \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

printf 'Groove Vocabulary Stage 6 BarEvolution host matrix: OK\n'
