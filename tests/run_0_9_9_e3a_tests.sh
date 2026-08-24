#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/e3a-drop-displace-execution"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_0_9_9_e3a_source_contract.py"

SOURCES=(
  "${ROOT_DIR}/src/generation/generation_context.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_catalog.cpp"
  "${ROOT_DIR}/src/generation/rhythm/relationship_resolver.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer_evolution.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_canonical_diff.cpp"
)

build_and_run() {
  local compiler="$1"
  local suffix="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -Wno-unused-but-set-variable -Wno-unused-function \
    -I"${ROOT_DIR}" \
    "$@" \
    "${SOURCES[@]}" \
    "${ROOT_DIR}/tests/test_0_9_9_e3a_execution_contract.cpp" \
    -o "${BUILD_DIR}/test_0_9_9_e3a_${suffix}"
  "${BUILD_DIR}/test_0_9_9_e3a_${suffix}"
}

build_and_run "${CXX:-g++}" gcc

if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ clang
fi

ASAN_OPTIONS="detect_leaks=0" build_and_run "${CXX:-g++}" sanitize \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

# Explicit cumulative contract gates. E2c reruns the E1a/Stage 6.1 owner
# matrix; E2b also reruns E2c. Repetition is intentional for a freeze checkpoint.
bash "${ROOT_DIR}/tests/run_0_9_9_e2c_tests.sh"
bash "${ROOT_DIR}/tests/run_0_9_9_e2a_tests.sh"
bash "${ROOT_DIR}/tests/run_0_9_9_e2b_tests.sh"

printf '0.9.9-E3a DROP / DISPLACE exact execution contract: OK\n'
