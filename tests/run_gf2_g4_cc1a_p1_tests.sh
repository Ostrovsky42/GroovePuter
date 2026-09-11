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

run_once() {
  local tag="$1"
  "$BUILD/g4-cc1a-p1-acid-articulation-authority" \
    "$BUILD/g4-cc1a-p1-census-${tag}.tsv" \
    > "$BUILD/g4-cc1a-p1-summary-${tag}.txt" 2>&1
}

run_once run1
run_once run2

rows1="$(($(wc -l < "$BUILD/g4-cc1a-p1-census-run1.tsv") - 1))"
rows2="$(($(wc -l < "$BUILD/g4-cc1a-p1-census-run2.tsv") - 1))"
test "$rows1" -eq 1152
test "$rows2" -eq 1152
cmp -s "$BUILD/g4-cc1a-p1-census-run1.tsv" \
       "$BUILD/g4-cc1a-p1-census-run2.tsv"
cmp -s "$BUILD/g4-cc1a-p1-summary-run1.txt" \
       "$BUILD/g4-cc1a-p1-summary-run2.txt"

cp "$BUILD/g4-cc1a-p1-census-run1.tsv" "$BUILD/g4-cc1a-p1-census.tsv"
cp "$BUILD/g4-cc1a-p1-summary-run1.txt" "$BUILD/g4-cc1a-p1-summary.txt"
printf 'G4_CC1A_P1_DETERMINISM PASS rows=%s semantic=row-byte-for-byte\n' "$rows1" \
  | tee -a "$BUILD/g4-cc1a-p1-summary.txt"

cat "$BUILD/g4-cc1a-p1-summary.txt"
