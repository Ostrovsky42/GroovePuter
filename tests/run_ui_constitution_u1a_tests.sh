#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/ui-constitution-u1a"
CXX="${CXX:-g++}"
FAILED=0

mkdir -p "${BUILD_DIR}"

run_cpp_test() {
  local name="$1"
  shift
  echo "== ${name} =="
  if "${CXX}" \
      -std=c++17 \
      -Wall \
      -Wextra \
      -Werror \
      -Wno-error=c++20-extensions \
      -I"${ROOT_DIR}" \
      "$@" \
      -o "${BUILD_DIR}/${name}"; then
    if ! "${BUILD_DIR}/${name}"; then
      FAILED=1
    fi
  else
    FAILED=1
  fi
}

run_cpp_test \
  test_ui_constitution_u1a_layout_purity \
  "${ROOT_DIR}/tests/test_ui_constitution_u1a_layout_purity.cpp" \
  "${ROOT_DIR}/src/ui/ui_core.cpp"

run_cpp_test \
  test_ui_constitution_u1a_location \
  "${ROOT_DIR}/tests/test_ui_constitution_u1a_location.cpp"

echo "== test_ui_status_chrome_source_regressions.py =="
if ! python3 "${ROOT_DIR}/tests/test_ui_status_chrome_source_regressions.py"; then
  FAILED=1
fi

echo "== test_ui_constitution_u1a_source_regressions.py =="
if ! python3 "${ROOT_DIR}/tests/test_ui_constitution_u1a_source_regressions.py"; then
  FAILED=1
fi

if [[ "${FAILED}" -ne 0 ]]; then
  echo "UI Constitution U1A focused gate: FAIL"
  exit 1
fi

echo "UI Constitution U1A focused gate: PASS"
