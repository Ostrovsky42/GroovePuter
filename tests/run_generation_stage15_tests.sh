#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_generation_stage15_chord_progression_source_regressions.py"

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

build_one() {
  local compiler="$1"
  local suffix="$2"
  local test_name="$3"
  shift 3
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -I"${ROOT_DIR}" "$@" \
    "${SOURCES[@]}" \
    "${ROOT_DIR}/tests/${test_name}.cpp" \
    -o "${BUILD_DIR}/${test_name}_${suffix}"
  "${BUILD_DIR}/${test_name}_${suffix}"
}

run_suite() {
  local compiler="$1"
  local suffix="$2"
  shift 2
  build_one "${compiler}" "${suffix}" \
    test_generation_stage15_chord_progression "$@"
  build_one "${compiler}" "${suffix}" \
    test_generation_stage15_grammar_catalog "$@"
  build_one "${compiler}" "${suffix}" \
    test_generation_stage15_reachability "$@"
}

run_suite "${CXX:-g++}" gcc
if command -v clang++ >/dev/null 2>&1; then
  run_suite clang++ clang
fi
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  run_suite "${CXX:-g++}" sanitize \
    -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

STACK_OBJECT="${BUILD_DIR}/stage15_chord_progression_stack.o"
STACK_USAGE="${BUILD_DIR}/stage15_chord_progression_stack.su"
rm -f "${STACK_OBJECT}" "${STACK_USAGE}"
"${CXX:-g++}" -std=c++17 -O2 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -I"${ROOT_DIR}" -fstack-usage \
  -c "${ROOT_DIR}/src/generation/roles/chord_progression.cpp" \
  -o "${STACK_OBJECT}"

if [[ ! -f "${STACK_USAGE}" ]]; then
  echo "Stage 15 stack-usage file was not emitted: ${STACK_USAGE}" >&2
  exit 1
fi
STACK_BYTES="$(awk -F '\t' '/realizeChordProgression/ {print $2; exit}' "${STACK_USAGE}")"
if [[ -z "${STACK_BYTES}" ]]; then
  echo "Stage 15 stack usage for realizeChordProgression was not found" >&2
  cat "${STACK_USAGE}" >&2
  exit 1
fi
if (( STACK_BYTES > 192 )); then
  echo "Stage 15 realizeChordProgression stack ${STACK_BYTES} B exceeds 192 B" >&2
  exit 1
fi
printf 'Generation Stage 15 stack usage: %s B <= 192 B\n' "${STACK_BYTES}"
printf 'Generation Stage 15 chord-progression host matrix: OK\n'
