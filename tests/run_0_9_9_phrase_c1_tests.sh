#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/phrase-c1"
mkdir -p "$BUILD"

mapfile -t COMMON_SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' "$ROOT/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)
COMMON_SOURCES+=("/src/generation/composition/phrase_length_request.cpp")

TESTS=(
  test_0_9_9_phrase_c1_axes
  test_0_9_9_phrase_c1_m2_lifetime
  test_0_9_9_phrase_c1_m4_length
  test_0_9_9_phrase_c1_m3_timeline
  test_0_9_9_phrase_c1_integrated
)

compile_test() {
  local compiler="$1"
  local test_name="$2"
  local output="$3"
  shift 3
  local sources=()
  local source
  for source in "${COMMON_SOURCES[@]}"; do
    sources+=("$ROOT$source")
  done
  "$compiler" "$@" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -I"$ROOT" "${sources[@]}" "$ROOT/tests/${test_name}.cpp" -o "$output"
}

run_suite() {
  local compiler="$1"
  local flavor="$2"
  local output_log="$3"
  shift 3
  : > "$output_log"
  local test_name
  for test_name in "${TESTS[@]}"; do
    local binary="$BUILD/${flavor}_${test_name}"
    compile_test "$compiler" "$test_name" "$binary" "$@"
    "$binary" >> "$output_log"
  done
}

GXX="${CXX:-g++}"
run_suite "$GXX" gcc "$BUILD/gcc1.out"
run_suite "$GXX" gcc_repeat "$BUILD/gcc2.out"
diff -u "$BUILD/gcc1.out" "$BUILD/gcc2.out"

echo "PHRASE-C1 focused GCC: PASS"
echo "PHRASE-C1 focused GCC deterministic repeat: PASS"

if command -v clang++ >/dev/null 2>&1; then
  run_suite clang++ clang "$BUILD/clang.out"
  diff -u "$BUILD/gcc1.out" "$BUILD/clang.out"
  echo "PHRASE-C1 focused Clang parity: PASS"
fi

ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  run_suite "$GXX" asan "$BUILD/asan.out" \
    -O1 -g -fno-omit-frame-pointer -fsanitize=address

echo "PHRASE-C1 focused ASan: PASS"

UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1}" \
  run_suite "$GXX" ubsan "$BUILD/ubsan.out" \
    -O1 -g -fno-omit-frame-pointer -fsanitize=undefined

echo "PHRASE-C1 focused UBSan: PASS"
cat "$BUILD/gcc1.out"
