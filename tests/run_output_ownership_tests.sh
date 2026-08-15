#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/output-ownership"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

COMMON_FLAGS=(
  -std=c++17
  -Wall
  -Wextra
  -Werror
  -I"${ROOT_DIR}"
)

"${CXX}" "${COMMON_FLAGS[@]}" \
  "${ROOT_DIR}/tests/test_output_ownership.cpp" \
  -o "${BUILD_DIR}/test_output_ownership"

"${CXX}" "${COMMON_FLAGS[@]}" \
  "${ROOT_DIR}/tests/test_output_ownership_queues.cpp" \
  -o "${BUILD_DIR}/test_output_ownership_queues"

"${BUILD_DIR}/test_output_ownership"
"${BUILD_DIR}/test_output_ownership_queues"
python3 "${ROOT_DIR}/tests/test_output_ownership_source_contract.py"

echo "0.9.6 output ownership contract gate: PASS"
