#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT}/tests/test_stage15_tonal_integration_source_regressions.py"

SOURCES=(
  "${ROOT}/src/generation/generation_context.cpp"
  "${ROOT}/src/generation/composition/rhythm_selection.cpp"
  "${ROOT}/src/generation/composition/generation_profile.cpp"
  "${ROOT}/src/generation/composition/tonal_profile.cpp"
  "${ROOT}/src/generation/feel/feel_interpreter.cpp"
  "${ROOT}/src/generation/feel/feel_pattern_adapter.cpp"
  "${ROOT}/src/generation/rhythm/rhythm_catalog.cpp"
  "${ROOT}/src/generation/rhythm/reference_vocabulary.cpp"
  "${ROOT}/src/generation/rhythm/relationship_resolver.cpp"
  "${ROOT}/src/generation/rhythm/rhythm_realizer.cpp"
  "${ROOT}/src/generation/materialization/pattern_materializer.cpp"
  "${ROOT}/src/generation/roles/semantic_pattern_projector.cpp"
  "${ROOT}/src/generation/roles/bass_rhythm.cpp"
  "${ROOT}/src/generation/roles/bass_pitch_behavior.cpp"
  "${ROOT}/src/generation/roles/chord_rhythm.cpp"
  "${ROOT}/src/generation/roles/chord_progression.cpp"
  "${ROOT}/src/generation/roles/melodic_motif.cpp"
  "${ROOT}/src/generation/roles/melodic_pitch_intent.cpp"
  "${ROOT}/src/generation/tonal/tonal_projector.cpp"
  "${ROOT}/src/generation/tonal/tonal_materializer.cpp"
  "${ROOT}/src/generation/migration/tonal_pattern_adapter.cpp"
  "${ROOT}/src/generation/migration/strong_rhythm_migration.cpp"
  "${ROOT}/tests/test_stage15_tonal_integration.cpp"
)

build_and_run() {
  local compiler="$1"
  local output="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -I"${ROOT}" "$@" "${SOURCES[@]}" -o "${output}"
  "${output}"
}

build_and_run "${CXX:-g++}" "${BUILD_DIR}/stage15_tonal_integration_gcc"
if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ "${BUILD_DIR}/stage15_tonal_integration_clang"
fi
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  build_and_run "${CXX:-g++}" "${BUILD_DIR}/stage15_tonal_integration_sanitize" \
    -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

echo "Stage 15 tonal integration gate: OK"
