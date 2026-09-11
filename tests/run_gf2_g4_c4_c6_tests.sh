#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/gf2-g4-c4-c6"
mkdir -p "$BUILD"

mapfile -t SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' \
    "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)

RESOLVED=()
for source in "${SOURCES[@]}"; do
  RESOLVED+=("${ROOT}${source}")
done

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -Wno-unused-but-set-variable \
  -I"${ROOT}" \
  "${RESOLVED[@]}" \
  "${ROOT}/tests/test_gf2_g4_c4_c6_structural_witness.cpp" \
  -o "$BUILD/g4-c4-c6-structural-witness"

"$BUILD/g4-c4-c6-structural-witness"
