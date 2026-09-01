#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/gf2-0r"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_gf2_0r_recipe_catalog_source.py"

SOURCES=(
  "${ROOT_DIR}/src/generation/generation_context.cpp"
  "${ROOT_DIR}/src/generation/composition/rhythm_selection.cpp"
  "${ROOT_DIR}/src/generation/composition/generation_profile.cpp"
  "${ROOT_DIR}/src/generation/feel/feel_interpreter.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_catalog.cpp"
  "${ROOT_DIR}/src/generation/rhythm/reference_vocabulary.cpp"
  "${ROOT_DIR}/src/generation/roles/bass_rhythm.cpp"
  "${ROOT_DIR}/src/generation/roles/chord_rhythm.cpp"
  "${ROOT_DIR}/src/generation/roles/chord_progression.cpp"
  "${ROOT_DIR}/src/generation/roles/melodic_motif.cpp"
)

build_and_run() {
  local compiler="$1"
  local suffix="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -I"${ROOT_DIR}" "$@" \
    "${SOURCES[@]}" \
    "${ROOT_DIR}/tests/test_gf2_0r_recipe_catalog.cpp" \
    -o "${BUILD_DIR}/test_gf2_0r_${suffix}"
  "${BUILD_DIR}/test_gf2_0r_${suffix}"
}

build_and_run "${CXX:-g++}" gcc
if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ clang
fi
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  build_and_run "${CXX:-g++}" sanitize \
    -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

printf 'GF2-0R Recipe catalog host matrix: OK\n'
