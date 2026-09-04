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

python3 tests/test_pattern_phrase_p2_source_contract.py
python3 tests/test_pattern_phrase_p2_executor_contract.py

CXXFLAGS=(-std=c++20 -Wall -Wextra -Werror -I.)
COMMON_SRC=(src/phrase/runtime_synth_events.cpp)
PLAYBACK_SRC=(src/phrase/runtime_synth_playback.cpp)
PATTERN_BANK_SRC=()
if [[ -f src/phrase/runtime_pattern_event_bank.cpp ]]; then
  PATTERN_BANK_SRC+=(src/phrase/runtime_pattern_event_bank.cpp)
fi

build_suite() {
  local cxx="$1"
  local suffix="$2"
  shift 2
  local extra=("$@")

  "$cxx" "${CXXFLAGS[@]}" "${extra[@]}" \
    "${COMMON_SRC[@]}" "${PLAYBACK_SRC[@]}" \
    tests/test_pattern_phrase_p2_runtime_playback.cpp \
    -o "$TMP/p2-playback-$suffix"
  "$TMP/p2-playback-$suffix"

  "$cxx" "${CXXFLAGS[@]}" "${extra[@]}" \
    "${COMMON_SRC[@]}" \
    tests/test_pattern_phrase_p2_conditional_expiry.cpp \
    -o "$TMP/p2-expiry-$suffix"
  "$TMP/p2-expiry-$suffix"

  "$cxx" "${CXXFLAGS[@]}" "${extra[@]}" \
    "${COMMON_SRC[@]}" "${PATTERN_BANK_SRC[@]}" \
    tests/test_pattern_phrase_p2_pattern_bank.cpp \
    -o "$TMP/p2-bank-$suffix"
  "$TMP/p2-bank-$suffix"

  "$cxx" "${CXXFLAGS[@]}" "${extra[@]}" \
    "${COMMON_SRC[@]}" "${PATTERN_BANK_SRC[@]}" \
    tests/test_pattern_phrase_p2_page_identity.cpp \
    -o "$TMP/p2-page-$suffix"
  "$TMP/p2-page-$suffix"

  "$cxx" "${CXXFLAGS[@]}" "${extra[@]}" \
    "${COMMON_SRC[@]}" "${PATTERN_BANK_SRC[@]}" \
    tests/test_pattern_phrase_p2_step_order.cpp \
    -o "$TMP/p2-step-order-$suffix"
  "$TMP/p2-step-order-$suffix"
}

build_suite g++ gcc | tee "$TMP/p2-gcc.out"
build_suite g++ gcc-repeat > "$TMP/p2-gcc-repeat.out"
diff -u "$TMP/p2-gcc.out" "$TMP/p2-gcc-repeat.out"
printf '%s\n' 'P2 deterministic GCC repeat: PASS'

if command -v clang++ >/dev/null 2>&1; then
  build_suite clang++ clang > "$TMP/p2-clang.out"
  diff -u "$TMP/p2-gcc.out" "$TMP/p2-clang.out"
  printf '%s\n' 'P2 Clang parity: PASS'
fi

ASAN_OPTIONS=detect_leaks=0 build_suite g++ asan \
  -fsanitize=address -fno-omit-frame-pointer > /dev/null
printf '%s\n' 'P2 ASan: PASS'

build_suite g++ ubsan \
  -fsanitize=undefined -fno-sanitize-recover=undefined > /dev/null
printf '%s\n' 'P2 UBSan: PASS'

git diff --check "$BASE_SHA"...HEAD
printf '%s\n' 'PATTERN/PHRASE P2 focused gate: PASS'
