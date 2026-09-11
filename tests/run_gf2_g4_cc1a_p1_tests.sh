#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/gf2-g4-cc1a-p1"
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
  "${ROOT}/tests/test_gf2_g4_cc1a_p1_acid_articulation_authority.cpp" \
  -o "$BUILD/g4-cc1a-p1-acid-articulation-authority"

set +e
"$BUILD/g4-cc1a-p1-acid-articulation-authority" \
  "$BUILD/g4-cc1a-p1-census.tsv" \
  > "$BUILD/g4-cc1a-p1-summary.txt" 2>&1
status=$?
set -e

cat "$BUILD/g4-cc1a-p1-summary.txt"
exit "$status"
