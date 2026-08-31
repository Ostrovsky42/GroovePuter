#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CXXFLAGS=(-std=c++20 -Wall -Wextra -Werror -I.)
SRC=tests/test_0_9_9_phrase_h2r_harmonic_clock_projection.cpp
OWNER_SRC=(
  src/generation/generation_context.cpp
  src/generation/roles/chord_progression.cpp
)
TMP="${TMPDIR:-/tmp}/grooveputer_phrase_h2r"
mkdir -p "$TMP"

python3 tests/test_0_9_9_phrase_h2r_source_guard.py

g++ "${CXXFLAGS[@]}" "${OWNER_SRC[@]}" "$SRC" -o "$TMP/h2r-gcc"
"$TMP/h2r-gcc" | tee "$TMP/h2r-gcc.out"
"$TMP/h2r-gcc" > "$TMP/h2r-gcc-repeat.out"
diff -u "$TMP/h2r-gcc.out" "$TMP/h2r-gcc-repeat.out"

if command -v clang++ >/dev/null 2>&1; then
  clang++ "${CXXFLAGS[@]}" "${OWNER_SRC[@]}" "$SRC" -o "$TMP/h2r-clang"
  "$TMP/h2r-clang" > "$TMP/h2r-clang.out"
  diff -u "$TMP/h2r-gcc.out" "$TMP/h2r-clang.out"
fi

g++ "${CXXFLAGS[@]}" -fsanitize=address -fno-omit-frame-pointer \
  "${OWNER_SRC[@]}" "$SRC" -o "$TMP/h2r-asan"
ASAN_OPTIONS=detect_leaks=1 "$TMP/h2r-asan" >/dev/null

g++ "${CXXFLAGS[@]}" -fsanitize=undefined -fno-sanitize-recover=undefined \
  "${OWNER_SRC[@]}" "$SRC" -o "$TMP/h2r-ubsan"
"$TMP/h2r-ubsan" >/dev/null

printf '%s\n' 'PHRASE-H2R focused source-guard/GCC/repeat/Clang/ASan/UBSan gate: OK'
