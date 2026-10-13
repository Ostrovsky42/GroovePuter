#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/hgs-tests"
CXX="${CXX:-g++}"
EXTRA_CXXFLAGS="${HGS_CXXFLAGS:-}"
mkdir -p "${BUILD_DIR}"

usage() {
  echo "usage: $0 [legacy] [vocab] [roman] [manifest] [algebra] [pcvl] [relation]" >&2
}

run_legacy() {
  # Intentional word splitting permits caller-provided sanitizer/compiler flags.
  # shellcheck disable=SC2086
  "${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    ${EXTRA_CXXFLAGS} \
    "${ROOT_DIR}/tests/test_hgs_legacy_replay.cpp" \
    "${ROOT_DIR}/src/generation/roles/chord_progression.cpp" \
    "${ROOT_DIR}/src/generation/generation_context.cpp" \
    -o "${BUILD_DIR}/test_legacy"
  (cd "${ROOT_DIR}" && "${BUILD_DIR}/test_legacy")
}

run_vocab() {
  (cd "${ROOT_DIR}" && python3 tests/test_harmony_vocab_import.py)
  (cd "${ROOT_DIR}" && python3 tests/test_harmonic_registry_generation.py)
}

run_roman() {
  (cd "${ROOT_DIR}" && python3 tests/test_harmony_roman_import.py)
}

run_manifest() {
  (cd "${ROOT_DIR}" && python3 tests/test_harmony_source_manifest.py)
}

run_algebra() {
  # shellcheck disable=SC2086
  "${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    ${EXTRA_CXXFLAGS} \
    "${ROOT_DIR}/tests/test_hgs_pitch_set.cpp" \
    "${ROOT_DIR}/src/generation/harmony/pitch_set.cpp" \
    -o "${BUILD_DIR}/test_pitch_set"
  "${BUILD_DIR}/test_pitch_set"
}

run_pcvl() {
  # shellcheck disable=SC2086
  "${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    ${EXTRA_CXXFLAGS} \
    "${ROOT_DIR}/tests/test_hgs_pc_voice_leading.cpp" \
    "${ROOT_DIR}/src/generation/harmony/pitch_set.cpp" \
    -o "${BUILD_DIR}/test_pcvl"
  "${BUILD_DIR}/test_pcvl"
}

run_relation() {
  # shellcheck disable=SC2086
  "${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    ${EXTRA_CXXFLAGS} \
    "${ROOT_DIR}/tests/test_hgs_relation.cpp" \
    "${ROOT_DIR}/src/generation/harmony/pitch_set.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_relation.cpp" \
    -o "${BUILD_DIR}/test_relation"
  "${BUILD_DIR}/test_relation"
}

targets=("$@")
if ((${#targets[@]} == 0)); then
  targets=(legacy vocab roman manifest algebra pcvl relation)
fi

for target in "${targets[@]}"; do
  case "${target}" in
    legacy) run_legacy ;;
    vocab) run_vocab ;;
    roman) run_roman ;;
    manifest) run_manifest ;;
    algebra) run_algebra ;;
    pcvl) run_pcvl ;;
    relation) run_relation ;;
    *) usage; echo "unknown target: ${target}" >&2; exit 2 ;;
  esac
done

echo "HGS tests passed: ${targets[*]}"
