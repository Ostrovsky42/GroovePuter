#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/phrase-r1"
BASE="2f9b6c7659bb4e0560c129ee33951f7adfcba8a4"
mkdir -p "$BUILD"

merge_base="$(git -C "$ROOT" merge-base HEAD "$BASE")"
if [[ "$merge_base" != "$BASE" ]]; then
  echo "R1 predecessor mismatch: merge-base=$merge_base expected=$BASE" >&2
  exit 1
fi

git -C "$ROOT" diff --exit-code "$BASE" HEAD -- src/generation/
python3 "${ROOT}/tests/test_0_9_9_phrase_r1_source_regressions.py"

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
  local output="$2"
  shift 2
  "$compiler" "${CXXFLAGS[@]}" "$@" \
    "${RESOLVED_SOURCES[@]}" \
    "${ROOT}/tests/test_0_9_9_phrase_r1_crossbar_lifetime.cpp" \
    -o "$output"
}

build "${CXX:-g++}" "$BUILD/gcc"
"$BUILD/gcc" > "$BUILD/gcc-1.out"
"$BUILD/gcc" > "$BUILD/gcc-2.out"
diff -u "$BUILD/gcc-1.out" "$BUILD/gcc-2.out"
cat "$BUILD/gcc-1.out"
echo "R1 deterministic repeat: OK"

if command -v clang++ >/dev/null 2>&1; then
  build clang++ "$BUILD/clang"
  "$BUILD/clang" > "$BUILD/clang.out"
  diff -u "$BUILD/gcc-1.out" "$BUILD/clang.out"
  echo "R1 Clang gate: OK"
fi

build "${CXX:-g++}" "$BUILD/asan" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  "$BUILD/asan" > "$BUILD/asan.out"
diff -u "$BUILD/gcc-1.out" "$BUILD/asan.out"
echo "R1 ASan gate: OK"

build "${CXX:-g++}" "$BUILD/ubsan" \
  -O1 -g -fno-omit-frame-pointer \
  -fsanitize=undefined -fno-sanitize-recover=undefined
"$BUILD/ubsan" > "$BUILD/ubsan.out"
diff -u "$BUILD/gcc-1.out" "$BUILD/ubsan.out"
echo "R1 UBSan gate: OK"

# Frozen predecessor compatibility. The C2 runner itself replays the minimal
# C2-C0 witness, the complete attempt-0 C2-C0 census, and the full P1R suite.
bash "${ROOT}/tests/run_0_9_9_phrase_c2_tests.sh"
echo "T40 PHRASE-C2 focused compatibility: OK"
echo "C2-C0 exhaustive compatibility: OK"
echo "T41 PHRASE-P1R focused compatibility: OK"

git -C "$ROOT" diff --exit-code "$BASE" HEAD -- src/generation/
python3 "${ROOT}/tests/test_0_9_9_phrase_r1_source_regressions.py" >/dev/null

echo "PHRASE-R1 focused production gate: OK"
