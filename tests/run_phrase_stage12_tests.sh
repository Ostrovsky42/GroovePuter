#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_phrase_stage12_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_phrase_stage12_reference_catalog_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_phrase_stage12_audition_source_regressions.py"

SOURCES=(
  "${ROOT_DIR}/src/generation/generation_context.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_catalog.cpp"
  "${ROOT_DIR}/src/generation/rhythm/reference_vocabulary.cpp"
  "${ROOT_DIR}/src/generation/rhythm/reference_phrase_vocabulary.cpp"
  "${ROOT_DIR}/src/generation/rhythm/relationship_resolver.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer.cpp"
  "${ROOT_DIR}/src/generation/rhythm/bar_evolution.cpp"
  "${ROOT_DIR}/src/generation/roles/bass_rhythm.cpp"
  "${ROOT_DIR}/src/generation/roles/chord_rhythm.cpp"
  "${ROOT_DIR}/src/generation/roles/melodic_motif.cpp"
  "${ROOT_DIR}/src/generation/phrase/phrase_evolution.cpp"
)

build_and_run() {
  local compiler="$1"
  local output="$2"
  local test_source="$3"
  shift 3
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -I"${ROOT_DIR}" "$@" \
    "${SOURCES[@]}" "${ROOT_DIR}/${test_source}" \
    -o "${output}"
  "${output}"
}

run_matrix() {
  local name="$1"
  local test_source="$2"
  build_and_run "${CXX:-g++}" "${BUILD_DIR}/${name}_gcc" "${test_source}"
  if command -v clang++ >/dev/null 2>&1; then
    build_and_run clang++ "${BUILD_DIR}/${name}_clang" "${test_source}"
  fi
  build_and_run "${CXX:-g++}" "${BUILD_DIR}/${name}_sanitize" "${test_source}" \
    -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined
}

run_matrix test_phrase_stage12 tests/test_phrase_stage12.cpp
run_matrix test_phrase_stage12_reference_catalog \
  tests/test_phrase_stage12_reference_catalog.cpp
run_matrix test_e1_runtime_characterization tests/test_e1_runtime_characterization.cpp
"${BUILD_DIR}/test_e1_runtime_characterization_gcc" | grep -E '^(DIFF|SECONDARY-SEARCH|AUDITION-DIRECT|LEGACY)' > "${BUILD_DIR}/e1_runtime_characterization.golden"
cmp "${BUILD_DIR}/e1_runtime_characterization.golden" \
  "${ROOT_DIR}/tests/goldens/e1_runtime_characterization.golden"

printf 'Generation Stage 12 host matrix: OK\n'
