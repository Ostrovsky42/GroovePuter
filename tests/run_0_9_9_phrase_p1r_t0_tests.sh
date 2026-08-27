#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BASE_SHA="c9c0dc852dfb96b191c5d7066c81af99e3df189a"
CXXFLAGS=(-std=c++17 -Wall -Wextra -Werror -I.)
SRC=tests/test_0_9_9_phrase_p1r_t0_h1_source_period_projection.cpp
OWNER_SRC=(src/generation/generation_context.cpp)
TMP="${TMPDIR:-/tmp}/grooveputer_phrase_p1r_t0"
mkdir -p "$TMP"

# P1R-T0 is characterization only: production source must remain byte-for-byte
# identical to the frozen H2 base.
git diff --exit-code "$BASE_SHA" -- src/
printf '%s\n' 'P1R-T0 production source delta guard: OK'

run_characterization() {
  local binary="$1"
  local output="$2"
  set +e
  "$binary" >"$output" 2>&1
  local rc=$?
  set -e
  if [[ $rc -ne 0 && $rc -ne 42 ]]; then
    cat "$output"
    printf 'P1R-T0 technical/test failure: unexpected exit code %d\n' "$rc" >&2
    exit "$rc"
  fi
  printf '%d' "$rc"
}

g++ "${CXXFLAGS[@]}" "${OWNER_SRC[@]}" "$SRC" -o "$TMP/t0-gcc"
gcc_rc="$(run_characterization "$TMP/t0-gcc" "$TMP/t0-gcc.out")"
cat "$TMP/t0-gcc.out"

gcc_repeat_rc="$(run_characterization "$TMP/t0-gcc" "$TMP/t0-gcc-repeat.out")"
[[ "$gcc_repeat_rc" == "$gcc_rc" ]]
diff -u "$TMP/t0-gcc.out" "$TMP/t0-gcc-repeat.out"

if command -v clang++ >/dev/null 2>&1; then
  clang++ "${CXXFLAGS[@]}" "${OWNER_SRC[@]}" "$SRC" -o "$TMP/t0-clang"
  clang_rc="$(run_characterization "$TMP/t0-clang" "$TMP/t0-clang.out")"
  [[ "$clang_rc" == "$gcc_rc" ]]
  diff -u "$TMP/t0-gcc.out" "$TMP/t0-clang.out"
fi

g++ "${CXXFLAGS[@]}" -fsanitize=address -fno-omit-frame-pointer \
  "${OWNER_SRC[@]}" "$SRC" -o "$TMP/t0-asan"
asan_rc="$(ASAN_OPTIONS=detect_leaks=1 run_characterization "$TMP/t0-asan" "$TMP/t0-asan.out")"
[[ "$asan_rc" == "$gcc_rc" ]]
diff -u "$TMP/t0-gcc.out" "$TMP/t0-asan.out"

g++ "${CXXFLAGS[@]}" -fsanitize=undefined -fno-sanitize-recover=undefined \
  "${OWNER_SRC[@]}" "$SRC" -o "$TMP/t0-ubsan"
ubsan_rc="$(run_characterization "$TMP/t0-ubsan" "$TMP/t0-ubsan.out")"
[[ "$ubsan_rc" == "$gcc_rc" ]]
diff -u "$TMP/t0-gcc.out" "$TMP/t0-ubsan.out"

if [[ "$gcc_rc" == "42" ]]; then
  grep -Fq 'P1R-T0 RESULT: H1_SOURCE_PERIOD_NOT_REPRESENTABLE' "$TMP/t0-gcc.out"
  printf '%s\n' 'P1R-T0 semantic characterization: BLOCKED — H1 source period is not representable by public ChordProgressionPlan.' >&2
  exit 42
fi

grep -Fq 'P1R-T0 RESULT: PUBLIC_PLAN_PRESERVES_GLOBAL_SOURCE_PERIOD' "$TMP/t0-gcc.out"
printf '%s\n' 'P1R-T0 focused GCC/repeat/Clang/ASan/UBSan gate: OK'
