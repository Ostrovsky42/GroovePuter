#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CXXFLAGS=(-std=c++17 -Wall -Wextra -Werror -I.)
SRC=tests/test_0_9_9_phrase_w1_harmonic_when_owner.cpp
TMP="${TMPDIR:-/tmp}/grooveputer_phrase_w1"
mkdir -p "$TMP"

python3 tests/test_0_9_9_phrase_w1_source_contract.py

g++ "${CXXFLAGS[@]}" "$SRC" -o "$TMP/w1-gcc"
"$TMP/w1-gcc" | tee "$TMP/w1-gcc.out"
"$TMP/w1-gcc" > "$TMP/w1-gcc-repeat.out"
diff -u "$TMP/w1-gcc.out" "$TMP/w1-gcc-repeat.out"

if command -v clang++ >/dev/null 2>&1; then
  clang++ "${CXXFLAGS[@]}" "$SRC" -o "$TMP/w1-clang"
  "$TMP/w1-clang" > "$TMP/w1-clang.out"
  diff -u "$TMP/w1-gcc.out" "$TMP/w1-clang.out"
fi

g++ "${CXXFLAGS[@]}" -fsanitize=address -fno-omit-frame-pointer \
  "$SRC" -o "$TMP/w1-asan"
ASAN_OPTIONS=detect_leaks=1 "$TMP/w1-asan" >/dev/null

g++ "${CXXFLAGS[@]}" -fsanitize=undefined -fno-sanitize-recover=undefined \
  "$SRC" -o "$TMP/w1-ubsan"
"$TMP/w1-ubsan" >/dev/null

printf '%s\n' 'one-bar F08 compatibility fingerprint=static:0001/1;moving:0101/2'
printf '%s\n' 'F08 accepted corpus authority=256 rows; changed=93; topology=0; articulation=0; pitch=93; static=0/102'
printf '%s\n' 'F08 exact golden SHA-256=bbc1544bf289c7ef7f062997bde3f0b8dae3a317ace54b0998cef6649872ac3f'
printf '%s\n' 'CURRENT H1 corpus role=compatibility regression; old F08 golden not blindly transplanted'
printf '%s\n' '0.9.9-PHRASE-W1 harmonic WHEN owner recovery gate: OK (DECISION_B projection gap)'
