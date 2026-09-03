#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BASE_SHA="c02d9ae04fcd43b5e3ced11e5aa50e850e26b4e6"
TMP="${TMPDIR:-/tmp}/grooveputer_pattern_phrase_p1"
mkdir -p "$TMP"

python3 tests/test_pattern_phrase_p1_source_contract.py

CXXFLAGS=(-std=c++20 -Wall -Wextra -Werror -I.)
TEST_SRC=tests/test_pattern_phrase_p1_runtime_events.cpp
OWNER_SRC=()
if [[ -f src/phrase/runtime_synth_events.cpp ]]; then
  OWNER_SRC+=(src/phrase/runtime_synth_events.cpp)
fi

build_and_run() {
  local cxx="$1"
  local output="$2"
  shift 2
  "$cxx" "${CXXFLAGS[@]}" "$@" "${OWNER_SRC[@]}" "$TEST_SRC" -o "$output"
  "$output"
}

build_and_run g++ "$TMP/p1-gcc" | tee "$TMP/p1-gcc.out"
"$TMP/p1-gcc" > "$TMP/p1-gcc-repeat.out"
diff -u "$TMP/p1-gcc.out" "$TMP/p1-gcc-repeat.out"

if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ "$TMP/p1-clang" > "$TMP/p1-clang.out"
  diff -u "$TMP/p1-gcc.out" "$TMP/p1-clang.out"
fi

build_and_run g++ "$TMP/p1-asan" \
  -fsanitize=address -fno-omit-frame-pointer > /dev/null

build_and_run g++ "$TMP/p1-ubsan" \
  -fsanitize=undefined -fno-sanitize-recover=undefined > /dev/null

python3 tests/test_pattern_phrase_p1_source_contract.py

git diff --name-only "$BASE_SHA"...HEAD | while IFS= read -r path; do
  case "$path" in
    src/phrase/runtime_synth_events.h|src/phrase/runtime_synth_events.cpp|tests/*|docs/*|.github/workflows/0_9_10_pattern_phrase_p1_runtime_events.yml)
      ;;
    src/*)
      echo "P1 production firewall: unexpected path $path" >&2
      exit 1
      ;;
  esac
done

printf '%s\n' 'PATTERN/PHRASE P1 focused gate: PASS'
