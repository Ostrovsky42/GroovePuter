#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-c2-v0r"
mkdir -p "${BUILD_DIR}"

mapfile -t COMMON_SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)

build() {
  local compiler="$1"
  local output="$2"
  shift 2
  "${compiler}" "$@" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -I"${ROOT}" \
    "${COMMON_SOURCES[@]/#/${ROOT}}" \
    "${ROOT}/tests/test_gf2_c2_v0r_structured_observation.cpp" \
    -o "${output}"
}

build "${CXX:-g++}" "${BUILD_DIR}/structured_observation_gcc"
"${BUILD_DIR}/structured_observation_gcc" > "${BUILD_DIR}/run1.txt"
"${BUILD_DIR}/structured_observation_gcc" > "${BUILD_DIR}/run2.txt"
diff -u "${BUILD_DIR}/run1.txt" "${BUILD_DIR}/run2.txt"
cat "${BUILD_DIR}/run1.txt"
echo "GF2-C2-V0R deterministic process replay: PASS"

if command -v clang++ >/dev/null 2>&1; then
  build clang++ "${BUILD_DIR}/structured_observation_clang"
  "${BUILD_DIR}/structured_observation_clang" >/dev/null
  echo "GF2-C2-V0R clang host run: PASS"
fi

build "${CXX:-g++}" "${BUILD_DIR}/structured_observation_sanitize" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  "${BUILD_DIR}/structured_observation_sanitize" >/dev/null
echo "GF2-C2-V0R ASAN/UBSAN host run: PASS"
