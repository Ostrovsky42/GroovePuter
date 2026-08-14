#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/tape-resource-recovery"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_tape_resource_recovery_source_regressions.py"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -Wno-c++20-extensions \
  -DARDUINO_M5STACK_CARDPUTER=1 \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_tape_unavailable_contract.cpp" \
  "${ROOT_DIR}/src/dsp/tape_looper.cpp" \
  -o "${BUILD_DIR}/test_tape_unavailable_contract"

"${BUILD_DIR}/test_tape_unavailable_contract"

echo "Tape resource-recovery host tests: PASS"
