#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/phrase-c2-c0"
BASE="016bcd6ba514b3a57f8803c63c869f1b2a8953a7"
mkdir -p "$BUILD"

git -C "$ROOT" diff --exit-code "$BASE" -- src/
echo "C2-C0 frozen src firewall: OK"

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
TEST="${ROOT}/tests/test_0_9_9_phrase_c2_c0_boundary_topology.cpp"

build() {
  local compiler="$1"
  local output="$2"
  shift 2
  local resolved=()
  local source
  for source in "${SOURCES[@]}"; do
    resolved+=("${ROOT}${source}")
  done
  "$compiler" "${CXXFLAGS[@]}" "$@" "${resolved[@]}" "$TEST" -o "$output"
}

build "${CXX:-g++}" "$BUILD/gcc"
"$BUILD/gcc" > "$BUILD/gcc-1.out"
"$BUILD/gcc" > "$BUILD/gcc-2.out"
diff -u "$BUILD/gcc-1.out" "$BUILD/gcc-2.out"
cat "$BUILD/gcc-1.out"
echo "C2-C0 deterministic repeat: OK"

if command -v clang++ >/dev/null 2>&1; then
  build clang++ "$BUILD/clang"
  "$BUILD/clang" > "$BUILD/clang.out"
  diff -u "$BUILD/gcc-1.out" "$BUILD/clang.out"
  echo "C2-C0 Clang gate: OK"
fi

build "${CXX:-g++}" "$BUILD/asan" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  "$BUILD/asan" > "$BUILD/asan.out"
diff -u "$BUILD/gcc-1.out" "$BUILD/asan.out"
echo "C2-C0 ASan gate: OK"

build "${CXX:-g++}" "$BUILD/ubsan" \
  -O1 -g -fno-omit-frame-pointer \
  -fsanitize=undefined -fno-sanitize-recover=undefined
"$BUILD/ubsan" > "$BUILD/ubsan.out"
diff -u "$BUILD/gcc-1.out" "$BUILD/ubsan.out"
echo "C2-C0 UBSan gate: OK"

echo "C2-C0 boundary topology characterization: OK"
