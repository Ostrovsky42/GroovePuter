#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/gf2-g4-i8"
mkdir -p "${BUILD}"

echo "G4-I8 head: $(git -C "${ROOT}" rev-parse HEAD)"

SOURCES=(
  "${ROOT}/src/generation/generation_context.cpp"
  "${ROOT}/src/generation/composition/rhythm_selection.cpp"
  "${ROOT}/src/generation/composition/generation_profile.cpp"
  "${ROOT}/src/generation/feel/feel_interpreter.cpp"
  "${ROOT}/src/generation/rhythm/rhythm_catalog.cpp"
  "${ROOT}/src/generation/rhythm/reference_vocabulary.cpp"
  "${ROOT}/src/generation/roles/bass_rhythm.cpp"
  "${ROOT}/src/generation/roles/chord_rhythm.cpp"
  "${ROOT}/src/generation/roles/chord_progression.cpp"
  "${ROOT}/src/generation/roles/melodic_motif.cpp"
)

build_and_run() {
  local compiler="$1"
  local suffix="$2"
  local output="${BUILD}/g4-i8-${suffix}"
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -I"${ROOT}" \
    "${SOURCES[@]}" \
    "${ROOT}/tests/test_gf2_g4_i8_bass_family_binding.cpp" \
    -o "${output}"
  set +e
  "${output}" > "${BUILD}/run-${suffix}.txt"
  local status=$?
  set -e
  cat "${BUILD}/run-${suffix}.txt"
  return "${status}"
}

build_and_run "${CXX:-g++}" gcc
if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ clang
  cmp "${BUILD}/run-gcc.txt" "${BUILD}/run-clang.txt"
  echo "G4-I8 GCC/Clang replay: BYTE-IDENTICAL"
fi

"${BUILD}/g4-i8-gcc" > "${BUILD}/run-gcc-replay.txt"
cmp "${BUILD}/run-gcc.txt" "${BUILD}/run-gcc-replay.txt"
echo "G4-I8 deterministic replay: BYTE-IDENTICAL"
