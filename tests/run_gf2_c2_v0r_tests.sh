#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-c2-v0r"
mkdir -p "${BUILD_DIR}"

mapfile -t COMMON_SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -Wno-unused-but-set-variable \
  -I"${ROOT}" \
  "${COMMON_SOURCES[@]/#/${ROOT}}" \
  "${ROOT}/tests/test_gf2_c2_v0r_structured_observation.cpp" \
  -o "${BUILD_DIR}/structured_observation"

"${BUILD_DIR}/structured_observation"
