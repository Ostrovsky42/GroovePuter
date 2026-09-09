#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/gf2-g4-r1"
mkdir -p "$BUILD"

mapfile -t SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' \
    "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)

CXXFLAGS=(
  -std=c++17
  -Wall
  -Wextra
  -Werror
  -Wvla
  -Wno-c++20-extensions
  -Wno-unused-but-set-variable
  -I"${ROOT}"
)

resolved=()
for source in "${SOURCES[@]}"; do
  resolved+=("${ROOT}${source}")
done

"${CXX:-g++}" "${CXXFLAGS[@]}" \
  "${resolved[@]}" \
  "${ROOT}/tests/test_gf2_g4_r1_reference_contracts.cpp" \
  -o "$BUILD/g4-r1-reference-contracts"

"$BUILD/g4-r1-reference-contracts" | tee "$BUILD/g4-r1-summary.txt"
