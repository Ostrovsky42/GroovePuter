#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build/host-tests/phrase-c1"
mkdir -p "$BUILD"
mapfile -t SOURCES < <(sed -n '/COMMON_SOURCES=(/,/)/p' "$ROOT/tests/run_stage15_tonal_integration_tests.sh" | grep -F '"${ROOT}/src/' | sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/')
build(){ local c="$1" o="$2"; shift 2; "$c" "$@" -std=c++17 -Wall -Wextra -Werror -Wvla -Wno-c++20-extensions -Wno-unused-but-set-variable -I"$ROOT" "${SOURCES[@]/#/$ROOT}" "$ROOT/tests/test_0_9_9_phrase_c1_axes.cpp" -o "$o"; }
build "${CXX:-g++}" "$BUILD/gcc"
"$BUILD/gcc" > "$BUILD/gcc1"
"$BUILD/gcc" > "$BUILD/gcc2"
diff -u "$BUILD/gcc1" "$BUILD/gcc2"
if command -v clang++ >/dev/null; then
  build clang++ "$BUILD/clang"
  "$BUILD/clang" > "$BUILD/clang.out"
  diff -u "$BUILD/gcc1" "$BUILD/clang.out"
fi
build "${CXX:-g++}" "$BUILD/san" -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined
ASAN_OPTIONS=detect_leaks=0 "$BUILD/san" >/dev/null
cat "$BUILD/gcc1"
