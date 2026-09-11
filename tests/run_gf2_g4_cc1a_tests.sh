#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/gf2-g4-cc1a"
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
  "${ROOT}/tests/test_gf2_g4_cc1a_acid_structural_minimum.cpp" \
  -o "$BUILD/g4-cc1a-acid-structural-minimum"

run_once() {
  local suffix="$1"
  set +e
  "$BUILD/g4-cc1a-acid-structural-minimum" \
    "$BUILD/g4-cc1a-census.${suffix}.tsv" \
    > "$BUILD/g4-cc1a-summary.${suffix}.txt" 2>&1
  local status=$?
  set -e
  cat "$BUILD/g4-cc1a-summary.${suffix}.txt"
  return "$status"
}

run_once a
run_once b

cmp "$BUILD/g4-cc1a-summary.a.txt" "$BUILD/g4-cc1a-summary.b.txt"
cmp "$BUILD/g4-cc1a-census.a.tsv" "$BUILD/g4-cc1a-census.b.tsv"

cp "$BUILD/g4-cc1a-summary.a.txt" "$BUILD/g4-cc1a-summary.txt"
cp "$BUILD/g4-cc1a-census.a.tsv" "$BUILD/g4-cc1a-acid-articulation-census.tsv"

echo "G4_CC1A_PASS deterministic_repeat"
