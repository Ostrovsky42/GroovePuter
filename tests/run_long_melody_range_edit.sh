#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/long-melody-range-edit"
mkdir -p "${BUILD_DIR}"

g++ -std=c++17 -Wno-c++20-extensions -Wall -Wextra -Werror \
  -I"${ROOT_DIR}" "${ROOT_DIR}/tests/test_long_melody_range_edit.cpp" \
  -o "${BUILD_DIR}/test"
"${BUILD_DIR}/test"

printf '%s\n' 'Long Melody bounded range edit and Undo: PASS'
