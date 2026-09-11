#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/m-working-a2b-integration"
mkdir -p "${BUILD}"

"${CXX:-g++}" \
  -std=c++17 -Wall -Wextra -Werror -I"${ROOT}" \
  "${ROOT}/tests/test_0_9_11_m_working_a2b_integration.cpp" \
  -o "${BUILD}/material_identity"

"${BUILD}/material_identity"
