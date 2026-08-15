#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/output-ownership"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_output_ownership.cpp" \
  -o "${BUILD_DIR}/test_output_ownership"

"${BUILD_DIR}/test_output_ownership"

echo "0.9.6 output ownership contract gate: PASS"
