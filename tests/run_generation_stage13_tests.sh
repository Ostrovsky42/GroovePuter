#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_generation_stage13_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_generation_orthogonality_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_generation_stage14_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_generation_stage14_drum_generate_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_generation_stage14_p_level_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_release_generation_routing_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_p_level_production_selector_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_generation_attempt_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_genre_reroll_consistency_source_regressions.py"
python3 "${ROOT_DIR}/tests/test_0_9_9_m4_a1_source_contract.py"

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_0_9_9_m4_a1_phrase_coordinates.cpp" \
  -o "${BUILD_DIR}/test_0_9_9_m4_a1_phrase_coordinates"
"${BUILD_DIR}/test_0_9_9_m4_a1_phrase_coordinates"

COMPOSITION_SOURCES=(
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

MIGRATION_SOURCES=(
  "${COMPOSITION_SOURCES[@]}"
  "${ROOT_DIR}/src/generation/composition/tonal_profile.cpp"
  "${ROOT_DIR}/src/generation/feel/feel_pattern_adapter.cpp"
  "${ROOT_DIR}/src/generation/rhythm/relationship_resolver.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer.cpp"
  "${ROOT_DIR}/src/generation/materialization/pattern_materializer.cpp"
  "${ROOT_DIR}/src/generation/roles/semantic_pattern_projector.cpp"
  "${ROOT_DIR}/src/generation/roles/bass_pitch_behavior.cpp"
  "${ROOT_DIR}/src/generation/roles/melodic_pitch_intent.cpp"
  "${ROOT_DIR}/src/generation/tonal/tonal_projector.cpp"
  "${ROOT_DIR}/src/generation/tonal/tonal_materializer.cpp"
  "${ROOT_DIR}/src/generation/migration/tonal_pattern_adapter.cpp"
  "${ROOT_DIR}/src/generation/migration/strong_rhythm_migration.cpp"
)

build_and_run() {
  local compiler="$1"
  local test_source="$2"
  local output="$3"
  local source_set="$4"
  shift 4

  local -a sources
  if [[ "${source_set}" == "migration" ]]; then
    sources=("${MIGRATION_SOURCES[@]}")
  else
    sources=("${COMPOSITION_SOURCES[@]}")
  fi

  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -I"${ROOT_DIR}" "$@" \
    "${sources[@]}" "${test_source}" -o "${output}"
  "${output}"
}

build_request_state_test() {
  local compiler="$1"
  local output="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -I"${ROOT_DIR}" "$@" \
    "${ROOT_DIR}/tests/test_generation_request_state.cpp" \
    -o "${output}"
  "${output}"
}

run_suite() {
  local suffix="$1"
  local compiler="$2"
  shift 2

  build_request_state_test "${compiler}" \
    "${BUILD_DIR}/test_generation_request_state_${suffix}" "$@"
  build_and_run "${compiler}" \
    "${ROOT_DIR}/tests/test_generation_stage13.cpp" \
    "${BUILD_DIR}/test_generation_stage13_${suffix}" composition "$@"
  build_and_run "${compiler}" \
    "${ROOT_DIR}/tests/test_generation_stage14_genres.cpp" \
    "${BUILD_DIR}/test_generation_stage14_${suffix}" composition "$@"
  build_and_run "${compiler}" \
    "${ROOT_DIR}/tests/test_generation_stage14_hybrid_materialization.cpp" \
    "${BUILD_DIR}/test_generation_stage14_hybrid_${suffix}" migration "$@"
  build_and_run "${compiler}" \
    "${ROOT_DIR}/tests/test_generation_stage14_p_level_semantics.cpp" \
    "${BUILD_DIR}/test_generation_stage14_p_levels_${suffix}" migration "$@"
  build_and_run "${compiler}" \
    "${ROOT_DIR}/tests/test_generation_attempt_semantics.cpp" \
    "${BUILD_DIR}/test_generation_attempt_semantics_${suffix}" migration "$@"
  build_and_run "${compiler}" \
    "${ROOT_DIR}/tests/test_stage12_audition_genre_coverage.cpp" \
    "${BUILD_DIR}/test_stage12_audition_genres_${suffix}" migration "$@"
}

run_suite gcc "${CXX:-g++}"
if command -v clang++ >/dev/null 2>&1; then
  run_suite clang clang++
fi
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  run_suite sanitize "${CXX:-g++}" \
    -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined

bash "${ROOT_DIR}/tests/run_generation_stage15_tests.sh"
printf 'Generation Stage 13/14/15 host matrix: OK\n'
