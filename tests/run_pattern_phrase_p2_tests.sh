#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BASE_SHA="fa552763d34e0172ceed1d07913743165d9a5867"
TMP="${TMPDIR:-/tmp}/grooveputer_pattern_phrase_p2"
mkdir -p "$TMP"

git cat-file -e "${BASE_SHA}^{commit}"
if [[ "$(git merge-base HEAD "$BASE_SHA")" != "$BASE_SHA" ]]; then
  echo "P2 authoritative base ancestry verification failed" >&2
  exit 1
fi
printf '%s\n' 'P2 authoritative Gate-B base ancestry: PASS'

TEST_SRC="tests/test_pattern_phrase_p2_runtime_playback.cpp"
OWNER_SRC=()
if [[ -f src/phrase/runtime_synth_playback.cpp ]]; then
  OWNER_SRC+=(src/phrase/runtime_synth_playback.cpp)
fi
OWNER_SRC+=(src/phrase/runtime_synth_events.cpp)
CXXFLAGS=(-std=c++20 -Wall -Wextra -Werror -I.)

build_and_run() {
  local cxx="$1"
  local output="$2"
  shift 2
  "$cxx" "${CXXFLAGS[@]}" "$@" "${OWNER_SRC[@]}" "$TEST_SRC" -o "$output"
  "$output"
}

build_and_run g++ "$TMP/p2-gcc" | tee "$TMP/p2-gcc.out"
"$TMP/p2-gcc" > "$TMP/p2-gcc-repeat.out"
diff -u "$TMP/p2-gcc.out" "$TMP/p2-gcc-repeat.out"
printf '%s\n' 'P2 deterministic GCC repeat: PASS'

if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ "$TMP/p2-clang" > "$TMP/p2-clang.out"
  diff -u "$TMP/p2-gcc.out" "$TMP/p2-clang.out"
  printf '%s\n' 'P2 Clang parity: PASS'
fi

ASAN_OPTIONS=detect_leaks=0 build_and_run g++ "$TMP/p2-asan" \
  -fsanitize=address -fno-omit-frame-pointer > /dev/null
printf '%s\n' 'P2 ASan: PASS'

build_and_run g++ "$TMP/p2-ubsan" \
  -fsanitize=undefined -fno-sanitize-recover=undefined > /dev/null
printf '%s\n' 'P2 UBSan: PASS'

git diff --check "$BASE_SHA"...HEAD
printf '%s\n' 'PATTERN/PHRASE P2 focused gate: PASS'
