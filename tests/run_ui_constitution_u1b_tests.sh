#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/ui-constitution-u1b"
CXX="${CXX:-g++}"
FAILED=0

mkdir -p "${BUILD_DIR}"

echo "== test_ui_constitution_u1b_status_routing =="
if "${CXX}" \
    -std=c++17 \
    -Wall \
    -Wextra \
    -Werror \
    -Wno-error=c++20-extensions \
    -I"${ROOT_DIR}" \
    "${ROOT_DIR}/tests/test_ui_constitution_u1b_status_routing.cpp" \
    -o "${BUILD_DIR}/test_ui_constitution_u1b_status_routing"; then
  if ! "${BUILD_DIR}/test_ui_constitution_u1b_status_routing"; then
    FAILED=1
  fi
else
  FAILED=1
fi

echo "== test_ui_constitution_u1b_source_regressions.py =="
if ! python3 "${ROOT_DIR}/tests/test_ui_constitution_u1b_source_regressions.py"; then
  FAILED=1
fi

if [[ "${FAILED}" -ne 0 ]]; then
  echo "UI Constitution U1B source/transport gate: FAIL"
  exit 1
fi

echo "UI Constitution U1B source/transport gate: PASS"
