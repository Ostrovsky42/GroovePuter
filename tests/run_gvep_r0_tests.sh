#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests-gvep-r0"
CXX="${CXX:-g++}"

mkdir -p "${BUILD_DIR}"

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_gvep_r0.cpp" \
  -o "${BUILD_DIR}/test_gvep_r0"

"${BUILD_DIR}/test_gvep_r0"

echo "GVEP R0 host tests: PASS"
