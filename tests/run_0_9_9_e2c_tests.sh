#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/e2c-mutation-delta"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_0_9_9_e2c_source_contract.py"

build_and_run() {
  local compiler="$1"
  local suffix="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -I"${ROOT_DIR}" \
    "$@" \
    "${ROOT_DIR}/tests/test_0_9_9_e2c_mutation_delta_contract.cpp" \
    -o "${BUILD_DIR}/test_0_9_9_e2c_mutation_delta_${suffix}"
  "${BUILD_DIR}/test_0_9_9_e2c_mutation_delta_${suffix}"
}

build_and_run "${CXX:-g++}" gcc

if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ clang
fi

build_and_run "${CXX:-g++}" sanitize \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

# E2c is contract-only. Re-run the existing E1a/Stage 6.1 matrix to prove the
# authoritative executor, validator, deterministic output and owner split are
# unchanged by the additive header contract.
bash "${ROOT_DIR}/tests/run_rhythm_stage6_1_tests.sh"

printf '0.9.9-E2c canonical rhythm mutation delta contract: OK\n'
