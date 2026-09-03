#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

python3 "${ROOT_DIR}/tools/gf2/generate_gf2_c1f_final_static_census.py"

FIRST_HASHES="$(sha256sum \
  "${ROOT_DIR}/docs/research/GF2_C1F_FINAL_STATIC_SEMANTIC_CENSUS.tsv" \
  "${ROOT_DIR}/docs/research/GF2_C1F_RHYTHM_ARCHETYPE_PAYLOADS.tsv" \
  "${ROOT_DIR}/docs/research/GF2_C1F_BASE_GENRE_PAIRS.tsv")"
python3 "${ROOT_DIR}/tools/gf2/generate_gf2_c1f_final_static_census.py"
SECOND_HASHES="$(sha256sum \
  "${ROOT_DIR}/docs/research/GF2_C1F_FINAL_STATIC_SEMANTIC_CENSUS.tsv" \
  "${ROOT_DIR}/docs/research/GF2_C1F_RHYTHM_ARCHETYPE_PAYLOADS.tsv" \
  "${ROOT_DIR}/docs/research/GF2_C1F_BASE_GENRE_PAIRS.tsv")"
test "${FIRST_HASHES}" = "${SECOND_HASHES}"

test "$(wc -l < "${ROOT_DIR}/docs/research/GF2_C1F_FINAL_STATIC_SEMANTIC_CENSUS.tsv")" -eq 34
test "$(wc -l < "${ROOT_DIR}/docs/research/GF2_C1F_RHYTHM_ARCHETYPE_PAYLOADS.tsv")" -eq 25
test "$(wc -l < "${ROOT_DIR}/docs/research/GF2_C1F_BASE_GENRE_PAIRS.tsv")" -eq 121

python3 "${ROOT_DIR}/tests/test_gf2_c1f_final_static_semantic_census.py"

printf 'GF2-C1F final static semantic census: OK\n'
