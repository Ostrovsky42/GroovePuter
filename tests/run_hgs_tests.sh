#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/hgs-tests"
CXX="${CXX:-g++}"
EXTRA_CXXFLAGS="${HGS_CXXFLAGS:-}"
mkdir -p "${BUILD_DIR}"

usage() {
  echo "usage: $0 [legacy] [vocab] [roman] [manifest] [algebra] [pcvl] [relation] [corpus] [genre] [genome] [candidates] [fingerprint] [route] [projection] [role] [song-state] [persistence] [song-storage]" >&2
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

run_corpus() {
  (cd "${ROOT_DIR}" && python3 tests/test_harmony_stats.py)
  # shellcheck disable=SC2086
  "${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    ${EXTRA_CXXFLAGS} \
    "${ROOT_DIR}/tests/test_hgs_corpus_prior.cpp" \
    "${ROOT_DIR}/src/generation/harmony/corpus_prior.cpp" \
    -o "${BUILD_DIR}/test_corpus_prior"
  "${BUILD_DIR}/test_corpus_prior"
}

run_genre() {
  (cd "${ROOT_DIR}" && python3 tests/test_hgs_genre_matrix.py)
  # scenes.h has defaulted bit-fields; this host-only mapping test uses C++20.
  # shellcheck disable=SC2086
  "${CXX}" -std=c++20 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    ${EXTRA_CXXFLAGS} \
    "${ROOT_DIR}/tests/test_hgs_genre_policy.cpp" \
    "${ROOT_DIR}/src/generation/harmony/genre_harmonic_policy.cpp" \
    -o "${BUILD_DIR}/test_genre_policy"
  "${BUILD_DIR}/test_genre_policy"
}

run_genome() {
  # scenes.h has defaulted bit-fields; this host-only policy dependency uses C++20.
  # shellcheck disable=SC2086
  "${CXX}" -std=c++20 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    ${EXTRA_CXXFLAGS} \
    "${ROOT_DIR}/tests/test_hgs_genome.cpp" \
    "${ROOT_DIR}/src/generation/generation_context.cpp" \
    "${ROOT_DIR}/src/generation/harmony/genre_harmonic_policy.cpp" \
    "${ROOT_DIR}/src/generation/harmony/song_variant_genome.cpp" \
    -o "${BUILD_DIR}/test_genome"
  "${BUILD_DIR}/test_genome"
}

run_candidates() {
  # scenes.h has defaulted bit-fields; the genre policy dependency uses C++20.
  # shellcheck disable=SC2086
  "${CXX}" -std=c++20 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    ${EXTRA_CXXFLAGS} \
    "${ROOT_DIR}/tests/test_hgs_candidates.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_candidate.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_relation.cpp" \
    "${ROOT_DIR}/src/generation/harmony/pitch_set.cpp" \
    "${ROOT_DIR}/src/generation/harmony/corpus_prior.cpp" \
    "${ROOT_DIR}/src/generation/harmony/genre_harmonic_policy.cpp" \
    -o "${BUILD_DIR}/test_candidates"
  "${BUILD_DIR}/test_candidates"
}

run_fingerprint() {
  # shellcheck disable=SC2086
  "${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    ${EXTRA_CXXFLAGS} \
    "${ROOT_DIR}/tests/test_hgs_fingerprint.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_fingerprint.cpp" \
    -o "${BUILD_DIR}/test_fingerprint"
  "${BUILD_DIR}/test_fingerprint"
}

run_route() {
  # scenes.h has defaulted bit-fields; genre policy dependency uses C++20.
  # shellcheck disable=SC2086
  "${CXX}" -std=c++20 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    ${EXTRA_CXXFLAGS} \
    "${ROOT_DIR}/tests/test_hgs_route.cpp" \
    "${ROOT_DIR}/src/generation/generation_context.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_route.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_candidate.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_fingerprint.cpp" \
    "${ROOT_DIR}/src/generation/harmony/song_variant_genome.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_relation.cpp" \
    "${ROOT_DIR}/src/generation/harmony/pitch_set.cpp" \
    "${ROOT_DIR}/src/generation/harmony/corpus_prior.cpp" \
    "${ROOT_DIR}/src/generation/harmony/genre_harmonic_policy.cpp" \
    -o "${BUILD_DIR}/test_route"
  "${BUILD_DIR}/test_route"
}

run_projection() {
  # shellcheck disable=SC2086
  "${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    ${EXTRA_CXXFLAGS} \
    "${ROOT_DIR}/tests/test_hgs_projection.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_source.cpp" \
    "${ROOT_DIR}/src/generation/harmony/pitch_set.cpp" \
    "${ROOT_DIR}/src/generation/harmony/corpus_prior.cpp" \
    "${ROOT_DIR}/src/generation/roles/chord_progression.cpp" \
    "${ROOT_DIR}/src/generation/generation_context.cpp" \
    "${ROOT_DIR}/src/generation/tonal/tonal_projector.cpp" \
    -o "${BUILD_DIR}/test_projection"
  "${BUILD_DIR}/test_projection"
}

run_role() {
  # shellcheck disable=SC2086
  "${CXX}" -std=c++17 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    ${EXTRA_CXXFLAGS} \
    "${ROOT_DIR}/tests/test_hgs_role_policy.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_role_policy.cpp" \
    -o "${BUILD_DIR}/test_role_policy"
  "${BUILD_DIR}/test_role_policy"
}

run_song_state() {
  # scenes.h uses defaulted bit-fields; keep this host target on C++20.
  # shellcheck disable=SC2086
  "${CXX}" -std=c++20 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    ${EXTRA_CXXFLAGS} \
    "${ROOT_DIR}/tests/test_hgs_song_state.cpp" \
    "${ROOT_DIR}/src/generation/harmony/song_harmonic_state.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_fingerprint.cpp" \
    "${ROOT_DIR}/src/generation/harmony/song_variant_genome.cpp" \
    "${ROOT_DIR}/src/generation/harmony/genre_harmonic_policy.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_relation.cpp" \
    "${ROOT_DIR}/src/generation/harmony/pitch_set.cpp" \
    "${ROOT_DIR}/src/generation/harmony/corpus_prior.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_candidate.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_route.cpp" \
    "${ROOT_DIR}/src/generation/generation_context.cpp" \
    -o "${BUILD_DIR}/test_song_state"
  "${BUILD_DIR}/test_song_state"
}

run_persistence() {
  # shellcheck disable=SC2086
  "${CXX}" -std=c++20 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    ${EXTRA_CXXFLAGS} \
    "${ROOT_DIR}/tests/test_hgs_persistence.cpp" \
    "${ROOT_DIR}/src/generation/harmony/song_harmonic_state.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_source_codec.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_fingerprint.cpp" \
    "${ROOT_DIR}/src/generation/harmony/song_variant_genome.cpp" \
    "${ROOT_DIR}/src/generation/harmony/genre_harmonic_policy.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_relation.cpp" \
    "${ROOT_DIR}/src/generation/harmony/pitch_set.cpp" \
    "${ROOT_DIR}/src/generation/harmony/corpus_prior.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_candidate.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_route.cpp" \
    "${ROOT_DIR}/src/generation/generation_context.cpp" \
    -o "${BUILD_DIR}/test_persistence"
  "${BUILD_DIR}/test_persistence"
}

run_song_storage() {
  cat > "${BUILD_DIR}/pattern_paging_scratch.cpp" <<'CPP'
#include "scenes.h"
Scene& sceneTransactionScratch() {
  static Scene scratch{};
  return scratch;
}
CPP
  # The paging adapter uses C++20 scene declarations and the HGS codec/state.
  # shellcheck disable=SC2086
  "${CXX}" -std=c++20 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
    ${EXTRA_CXXFLAGS} \
    "${ROOT_DIR}/tests/test_hgs_song_storage.cpp" \
    "${ROOT_DIR}/src/audio/pattern_paging.cpp" \
    "${ROOT_DIR}/src/audio/pattern_project_cleanup.cpp" \
    "${ROOT_DIR}/src/generation/harmony/song_harmonic_state.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_source_codec.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_fingerprint.cpp" \
    "${ROOT_DIR}/src/generation/harmony/song_variant_genome.cpp" \
    "${ROOT_DIR}/src/generation/harmony/genre_harmonic_policy.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_relation.cpp" \
    "${ROOT_DIR}/src/generation/harmony/pitch_set.cpp" \
    "${ROOT_DIR}/src/generation/harmony/corpus_prior.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_candidate.cpp" \
    "${ROOT_DIR}/src/generation/harmony/harmonic_route.cpp" \
    "${ROOT_DIR}/src/generation/generation_context.cpp" \
    "${BUILD_DIR}/pattern_paging_scratch.cpp" \
    -o "${BUILD_DIR}/test_song_storage"
  "${BUILD_DIR}/test_song_storage"
}

targets=("$@")
if ((${#targets[@]} == 0)); then
  targets=(legacy vocab roman manifest algebra pcvl relation corpus genre genome candidates fingerprint route projection role song-state persistence song-storage)
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
    corpus) run_corpus ;;
    genre) run_genre ;;
    genome) run_genome ;;
    candidates) run_candidates ;;
    fingerprint) run_fingerprint ;;
    route) run_route ;;
    projection) run_projection ;;
    role) run_role ;;
    song-state) run_song_state ;;
    persistence) run_persistence ;;
    song-storage) run_song_storage ;;
    *) usage; echo "unknown target: ${target}" >&2; exit 2 ;;
  esac
done

echo "HGS tests passed: ${targets[*]}"
