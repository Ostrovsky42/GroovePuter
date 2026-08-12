#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests"
mkdir -p "${BUILD_DIR}"

COMMON_SOURCES=(
  "${ROOT}/src/generation/generation_context.cpp"
  "${ROOT}/src/generation/roles/chord_rhythm.cpp"
  "${ROOT}/src/generation/roles/chord_progression.cpp"
  "${ROOT}/src/generation/roles/chord_rhythm_timeline.cpp"
  "${ROOT}/src/generation/roles/chord_rhythm_retrigger.cpp"
)

build_and_run() {
  local compiler="$1"
  local output="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -I"${ROOT}" "$@" \
    "${COMMON_SOURCES[@]}" \
    "${ROOT}/tests/test_genre_harmonic_rhythm.cpp" \
    -o "${output}"
  "${output}"
}

build_and_run "${CXX:-g++}" "${BUILD_DIR}/genre_harmonic_rhythm_gcc"
if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ "${BUILD_DIR}/genre_harmonic_rhythm_clang"
fi
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  build_and_run "${CXX:-g++}" "${BUILD_DIR}/genre_harmonic_rhythm_sanitize" \
    -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

echo "Genre harmonic rhythm host gate: OK"
