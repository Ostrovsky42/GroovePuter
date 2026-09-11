#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/material-id-independence-red"
mkdir -p "${BUILD}"

"${CXX:-g++}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT}" \
  "${ROOT}/tests/test_0_9_11_material_id_independence.cpp" \
  -o "${BUILD}/test"

"${BUILD}/test"
