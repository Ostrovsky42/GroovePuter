#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/ui-final"
mkdir -p "$BUILD"
cd "$ROOT"

python3 tests/test_0_9_9_ui_final_source_regressions.py

${CXX:-g++} -std=c++17 -Wall -Wextra -Werror -I. \
  tests/test_0_9_9_ui_phrase_product_state.cpp \
  -o "$BUILD/product-state"
"$BUILD/product-state"
echo "UI phrase request/read-model state: OK"

# D2 behavior primitives reused by I1. Run behavior binaries directly: the
# historical D2/I1 source firewalls intentionally reject any later consumer
# branch and therefore are not valid post-I1 compatibility tests.
D2_FLAGS=(-std=c++17 -O2 -Wall -Wextra -Werror -Wno-c++20-extensions -I.)
${CXX:-g++} "${D2_FLAGS[@]}" \
  tests/test_phrase_live_arrangement_0_9_9.cpp \
  -o "$BUILD/d2-arrangement"
"$BUILD/d2-arrangement"
${CXX:-g++} "${D2_FLAGS[@]}" \
  tests/test_generation_activation_0_9_9.cpp \
  -o "$BUILD/d2-activation"
"$BUILD/d2-activation"
echo "UI final D2 behavior compatibility: OK"

# Rebuild the frozen I1 end-to-end behavior oracle against this consumer HEAD.
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
I1_TEST="${ROOT}/tests/test_0_9_9_phrase_i1_end_to_end.cpp"

build_i1() {
  local compiler="$1"
  local output="$2"
  shift 2
  local resolved=()
  local source
  for source in "${SOURCES[@]}"; do
    resolved+=("${ROOT}${source}")
  done
  "$compiler" "${CXXFLAGS[@]}" "$@" "${resolved[@]}" "$I1_TEST" -o "$output"
}

build_i1 "${CXX:-g++}" "$BUILD/i1-gcc"
"$BUILD/i1-gcc" > "$BUILD/i1-gcc-1.out"
"$BUILD/i1-gcc" > "$BUILD/i1-gcc-2.out"
diff -u "$BUILD/i1-gcc-1.out" "$BUILD/i1-gcc-2.out"
cat "$BUILD/i1-gcc-1.out"
echo "UI final I1 deterministic behavior: OK"

if command -v clang++ >/dev/null 2>&1; then
  build_i1 clang++ "$BUILD/i1-clang"
  "$BUILD/i1-clang" > "$BUILD/i1-clang.out"
  diff -u "$BUILD/i1-gcc-1.out" "$BUILD/i1-clang.out"
  echo "UI final I1 Clang behavior: OK"
fi

build_i1 "${CXX:-g++}" "$BUILD/i1-asan" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  "$BUILD/i1-asan" > "$BUILD/i1-asan.out"
diff -u "$BUILD/i1-gcc-1.out" "$BUILD/i1-asan.out"
echo "UI final I1 ASan behavior: OK"

build_i1 "${CXX:-g++}" "$BUILD/i1-ubsan" \
  -O1 -g -fno-omit-frame-pointer \
  -fsanitize=undefined -fno-sanitize-recover=undefined
"$BUILD/i1-ubsan" > "$BUILD/i1-ubsan.out"
diff -u "$BUILD/i1-gcc-1.out" "$BUILD/i1-ubsan.out"
echo "UI final I1 UBSan behavior: OK"

echo "UI final focused product gate: OK"
