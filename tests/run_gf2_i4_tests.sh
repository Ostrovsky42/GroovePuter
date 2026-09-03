#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-i4"
mkdir -p "${BUILD_DIR}"

mapfile -t SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -Wno-unused-but-set-variable \
  -I"${ROOT}" \
  "${SOURCES[@]/#/${ROOT}}" \
  "${ROOT}/tests/test_gf2_i4_corridor_consumers.cpp" \
  -o "${BUILD_DIR}/corridor_consumers"
"${BUILD_DIR}/corridor_consumers"

# I4 must preserve every already-accepted GF2 semantic checkpoint.
bash "${ROOT}/tests/run_gf2_i3_tests.sh" > /dev/null
echo "GF2-I4 inherited I1/I2/I2A/I3 chain: intact"
