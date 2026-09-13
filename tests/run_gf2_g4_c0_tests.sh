#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/gf2-g4-c0"
mkdir -p "$BUILD"

mapfile -t SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' \
    "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)
SOURCES+=("/src/generation/migration/phrase_execution.cpp")

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

build() {
  local compiler="$1"
  local output="$2"
  local test="$3"
  shift 3
  local resolved=()
  local source
  for source in "${SOURCES[@]}"; do
    resolved+=("${ROOT}${source}")
  done
  "$compiler" "${CXXFLAGS[@]}" "$@" "${resolved[@]}" "$test" -o "$output"
}

MAIN_TEST="${ROOT}/tests/test_gf2_g4_c0_materialized_corpus.cpp"
CROSS_TEST="${ROOT}/tests/test_gf2_g4_c0_cross_genre.cpp"

build "${CXX:-g++}" "$BUILD/g4-c0" "$MAIN_TEST"
"$BUILD/g4-c0" | tee "$BUILD/g4-c0-main.txt"

build "${CXX:-g++}" "$BUILD/g4-c0-cross" "$CROSS_TEST"
"$BUILD/g4-c0-cross" | tee "$BUILD/g4-c0-cross.txt"

cat "$BUILD/g4-c0-main.txt" "$BUILD/g4-c0-cross.txt" > "$BUILD/g4-c0-summary.txt"

echo "G4-C0 materialized corpus host gate: PASS"
