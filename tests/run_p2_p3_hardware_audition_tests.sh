#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests"
mkdir -p "${BUILD_DIR}"

SOURCES=(
  "${ROOT}/src/generation/roles/chord_rhythm_timeline.cpp"
  "${ROOT}/src/generation/roles/chord_rhythm_retrigger.cpp"
  "${ROOT}/src/generation/migration/p2_p3_hardware_audition.cpp"
  "${ROOT}/tests/test_p2_p3_hardware_audition.cpp"
)

build_and_run() {
  local compiler="$1"
  local output="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -I"${ROOT}" "$@" "${SOURCES[@]}" -o "${output}"
  "${output}"
}

build_and_run "${CXX:-g++}" "${BUILD_DIR}/p2_p3_hardware_audition_gcc"
if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ "${BUILD_DIR}/p2_p3_hardware_audition_clang"
fi
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  build_and_run "${CXX:-g++}" "${BUILD_DIR}/p2_p3_hardware_audition_sanitize" \
    -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

echo "P2+P3 hardware audition host gate: OK"
