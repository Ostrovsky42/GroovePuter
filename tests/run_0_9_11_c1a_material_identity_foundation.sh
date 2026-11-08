#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/c1a-material-identity-foundation"
CXX="${CXX:-g++}"
mkdir -p "${BUILD_DIR}"
"${CXX}" -std=c++17 -O2 -Wall -Wextra -Werror -Wno-c++20-extensions \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_0_9_11_c1a_material_identity_foundation.cpp" \
  -o "${BUILD_DIR}/test_c1a_material_identity_foundation"
"${BUILD_DIR}/test_c1a_material_identity_foundation"
