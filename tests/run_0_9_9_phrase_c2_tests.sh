#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/phrase-c2"
BASE="cd0e77a8acdf62e449792964f14899cfa118120b"
mkdir -p "$BUILD"

merge_base="$(git -C "$ROOT" merge-base HEAD "$BASE")"
if [[ "$merge_base" != "$BASE" ]]; then
  echo "C2 predecessor mismatch: merge-base=$merge_base expected=$BASE" >&2
  exit 1
fi

mapfile -t changed_src < <(git -C "$ROOT" diff --name-only "$BASE" HEAD -- src/ | sort)
expected_src=(
  "src/generation/migration/phrase_execution.cpp"
  "src/generation/migration/phrase_execution.h"
)
if [[ "${changed_src[*]}" != "${expected_src[*]}" ]]; then
  printf 'C2 unexpected production delta:\n' >&2
  printf '  %s\n' "${changed_src[@]}" >&2
  exit 1
fi

echo "T1 exact predecessor/source scope: OK"

python3 "${ROOT}/tests/test_0_9_9_phrase_p1r_source_regressions.py"

mapfile -t SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' \
    "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)
SOURCES+=("/src/generation/migration/phrase_execution.cpp")

CXXFLAGS=(
  -std=c++17
  -Wall
  -Wextra
  -Werror
  -Wvla
  -Wno-c++20-extensions
  -Wno-unused-but-set-variable
  -I"${ROOT}"
)

resolve_sources() {
  local resolved=()
  local source
  for source in "${SOURCES[@]}"; do
    resolved+=("${ROOT}${source}")
  done
  printf '%s\n' "${resolved[@]}"
}

mapfile -t RESOLVED_SOURCES < <(resolve_sources)

build() {
  local compiler="$1"
  local test="$2"
  local output="$3"
  shift 3
  "$compiler" "${CXXFLAGS[@]}" "$@" \
    "${RESOLVED_SOURCES[@]}" "$test" -o "$output"
}

TEST="${ROOT}/tests/test_0_9_9_phrase_c2_a_onset_lifetime.cpp"

build "${CXX:-g++}" "$TEST" "$BUILD/gcc"
"$BUILD/gcc" > "$BUILD/gcc-1.out"
"$BUILD/gcc" > "$BUILD/gcc-2.out"
diff -u "$BUILD/gcc-1.out" "$BUILD/gcc-2.out"
cat "$BUILD/gcc-1.out"
echo "C2 deterministic repeat: OK"

if command -v clang++ >/dev/null 2>&1; then
  build clang++ "$TEST" "$BUILD/clang"
  "$BUILD/clang" > "$BUILD/clang.out"
  diff -u "$BUILD/gcc-1.out" "$BUILD/clang.out"
  echo "C2 Clang gate: OK"
fi

build "${CXX:-g++}" "$TEST" "$BUILD/asan" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  "$BUILD/asan" > "$BUILD/asan.out"
diff -u "$BUILD/gcc-1.out" "$BUILD/asan.out"
echo "C2 ASan gate: OK"

build "${CXX:-g++}" "$TEST" "$BUILD/ubsan" \
  -O1 -g -fno-omit-frame-pointer \
  -fsanitize=undefined -fno-sanitize-recover=undefined
"$BUILD/ubsan" > "$BUILD/ubsan.out"
diff -u "$BUILD/gcc-1.out" "$BUILD/ubsan.out"
echo "C2 UBSan gate: OK"

# C2-C0 compatibility: re-run the frozen positive topology observer against the
# current production sources. Its own runner cannot be invoked because its
# research-only src/ firewall correctly expects zero production delta.
C0_TEST="${ROOT}/tests/test_0_9_9_phrase_c2_c0_boundary_topology.cpp"
build "${CXX:-g++}" "$C0_TEST" "$BUILD/c2-c0-compat" -O2
"$BUILD/c2-c0-compat" > "$BUILD/c2-c0-compat.out"
grep -F "C2-C0 TARGET PURE A: mode=0 recipe=0 bars=2 identity=2 boundary=0->1" \
  "$BUILD/c2-c0-compat.out"
grep -F "C2-C0 TARGET SUBCLASS: A_ONSET" "$BUILD/c2-c0-compat.out"
grep -F "C2-C0 TARGET RESULT A: NATURAL PURE-MELODIC CROSSBAR TOPOLOGY REACHABLE" \
  "$BUILD/c2-c0-compat.out"
echo "T19 C2-C0 positive topology compatibility: OK"

# Full frozen P1R focused suite remains relevant. Its lifetime-inert predecessor
# fixture is LoFi/hybrid and therefore must remain all-false under A_ONSET-only C2.
bash "${ROOT}/tests/run_0_9_9_phrase_p1r_tests.sh"
echo "T18 P1R compatibility: OK"

mapfile -t final_src < <(git -C "$ROOT" diff --name-only "$BASE" HEAD -- src/ | sort)
if [[ "${final_src[*]}" != "${expected_src[*]}" ]]; then
  echo "C2 source scope changed during validation" >&2
  exit 1
fi

echo "PHRASE-C2 focused production gate: OK"
