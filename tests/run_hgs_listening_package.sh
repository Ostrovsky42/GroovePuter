#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SEED_COUNT=256
OUT_DIR="${ROOT_DIR}/build/hgs-listening-package/evidence"
CXX="${CXX:-g++}"
EXTRA_CXXFLAGS="${HGS_TRACE_CXXFLAGS:-}"

while (($#)); do
  case "$1" in
    --seed-count)
      (($# >= 2)) || { echo "missing --seed-count value" >&2; exit 2; }
      SEED_COUNT="$2"; shift 2 ;;
    --out-dir)
      (($# >= 2)) || { echo "missing --out-dir value" >&2; exit 2; }
      OUT_DIR="$2"; shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

mkdir -p "${OUT_DIR}" "${ROOT_DIR}/build/hgs-listening-package"
BIN="${ROOT_DIR}/build/hgs-listening-package/hgs_trace"
# shellcheck disable=SC2086
"${CXX}" -std=c++20 -O2 -Wall -Wextra -Werror -I"${ROOT_DIR}" \
  ${EXTRA_CXXFLAGS} \
  "${ROOT_DIR}/tests/tools/hgs_trace.cpp" \
  "${ROOT_DIR}/src/generation/generation_context.cpp" \
  "${ROOT_DIR}/src/generation/roles/chord_progression.cpp" \
  "${ROOT_DIR}/src/generation/harmony/harmonic_route.cpp" \
  "${ROOT_DIR}/src/generation/harmony/harmonic_candidate.cpp" \
  "${ROOT_DIR}/src/generation/harmony/harmonic_fingerprint.cpp" \
  "${ROOT_DIR}/src/generation/harmony/song_variant_genome.cpp" \
  "${ROOT_DIR}/src/generation/harmony/harmonic_relation.cpp" \
  "${ROOT_DIR}/src/generation/harmony/pitch_set.cpp" \
  "${ROOT_DIR}/src/generation/harmony/corpus_prior.cpp" \
  "${ROOT_DIR}/src/generation/harmony/genre_harmonic_policy.cpp" \
  "${ROOT_DIR}/src/generation/harmony/harmonic_source.cpp" \
  -o "${BIN}"

(cd "${ROOT_DIR}" && "${BIN}" --seed-count "${SEED_COUNT}" --out-dir "${OUT_DIR}")
echo "HGS evidence package: ${OUT_DIR}"
