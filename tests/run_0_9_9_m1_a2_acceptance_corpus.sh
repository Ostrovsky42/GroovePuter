#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/m1-a2"
mkdir -p "$BUILD"
python3 "$ROOT/tests/generate_0_9_9_m1_t1f2_pairs.py" "$BUILD/pairs.h"
mapfile -t SOURCES < <(sed -n '/COMMON_SOURCES=(/,/)/p' "$ROOT/tests/run_stage15_tonal_integration_tests.sh" | grep -F '"${ROOT}/src/' | sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/')
build() {
  local compiler="$1" output="$2"; shift 2
  "$compiler" "$@" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -DGROOVEPUTER_M1_TEST_PROBE -I"$ROOT" -I"$BUILD" \
    "${SOURCES[@]/#/$ROOT}" \
    "$ROOT/tests/test_0_9_9_m1_a2_acceptance_corpus.cpp" -o "$output"
}
build "${CXX:-g++}" "$BUILD/gcc"
"$BUILD/gcc" > "$BUILD/gcc-1.tsv"
"$BUILD/gcc" > "$BUILD/gcc-2.tsv"
diff -u "$BUILD/gcc-1.tsv" "$BUILD/gcc-2.tsv"
diff -u "$ROOT/tests/data/m1_a2_acceptance_corpus.tsv" "$BUILD/gcc-1.tsv"
if command -v clang++ >/dev/null; then
  build clang++ "$BUILD/clang"
  "$BUILD/clang" > "$BUILD/clang.tsv"
  diff -u "$BUILD/gcc-1.tsv" "$BUILD/clang.tsv"
fi
build "${CXX:-g++}" "$BUILD/san" -O1 -g -fno-omit-frame-pointer \
  -fsanitize=address,undefined
ASAN_OPTIONS=detect_leaks=0 "$BUILD/san" > /dev/null
cat "$BUILD/gcc-1.tsv"
