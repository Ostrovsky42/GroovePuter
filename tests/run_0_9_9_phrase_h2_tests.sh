#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CXXFLAGS=(-std=c++17 -Wall -Wextra -Werror -I.)
SRC=tests/test_0_9_9_phrase_h2_harmonic_clock_projection.cpp
OWNER_SRC=(
  src/generation/generation_context.cpp
  src/generation/roles/chord_progression.cpp
)
TMP="${TMPDIR:-/tmp}/grooveputer_phrase_h2"
mkdir -p "$TMP"

python3 tests/test_0_9_9_phrase_h2_source_guard.py

g++ "${CXXFLAGS[@]}" "${OWNER_SRC[@]}" "$SRC" -o "$TMP/h2-gcc"
"$TMP/h2-gcc" | tee "$TMP/h2-gcc.out"
"$TMP/h2-gcc" > "$TMP/h2-gcc-repeat.out"
diff -u "$TMP/h2-gcc.out" "$TMP/h2-gcc-repeat.out"

if command -v clang++ >/dev/null 2>&1; then
  clang++ "${CXXFLAGS[@]}" "${OWNER_SRC[@]}" "$SRC" -o "$TMP/h2-clang"
  "$TMP/h2-clang" > "$TMP/h2-clang.out"
  diff -u "$TMP/h2-gcc.out" "$TMP/h2-clang.out"
fi

g++ "${CXXFLAGS[@]}" -fsanitize=address -fno-omit-frame-pointer \
  "${OWNER_SRC[@]}" "$SRC" -o "$TMP/h2-asan"
ASAN_OPTIONS=detect_leaks=1 "$TMP/h2-asan" >/dev/null

g++ "${CXXFLAGS[@]}" -fsanitize=undefined -fno-sanitize-recover=undefined \
  "${OWNER_SRC[@]}" "$SRC" -o "$TMP/h2-ubsan"
"$TMP/h2-ubsan" >/dev/null

printf '%s\n' 'PHRASE-H2 focused GCC/repeat/Clang/ASan/UBSan gate: OK'
