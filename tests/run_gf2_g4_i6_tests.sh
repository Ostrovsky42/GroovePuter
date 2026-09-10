#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/gf2-g4-i6"
RUN_A="$BUILD/run-a"
RUN_B="$BUILD/run-b"
mkdir -p "$BUILD"
rm -rf "$RUN_A" "$RUN_B"
mkdir -p "$RUN_A" "$RUN_B"

mapfile -t SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' \
    "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)

RESOLVED=()
for source in "${SOURCES[@]}"; do
  RESOLVED+=("${ROOT}${source}")
done

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -Wno-unused-but-set-variable \
  -I"${ROOT}" \
  "${RESOLVED[@]}" \
  "${ROOT}/tests/test_gf2_g4_i6_ownership_census.cpp" \
  -o "$BUILD/g4-i6-ownership-census"

# Corrected-methodology audit is deliberately independent from the main census
# emitter. It guards detector invariants that must not be inferred from totals.
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -Wno-unused-but-set-variable \
  -I"${ROOT}" \
  "${RESOLVED[@]}" \
  "${ROOT}/tests/test_gf2_g4_i6_methodology_controls.cpp" \
  -o "$BUILD/g4-i6-methodology-controls"

for run in "$RUN_A" "$RUN_B"; do
  "$BUILD/g4-i6-ownership-census" --emit "$run" | tee "$run.log"
  "$BUILD/g4-i6-methodology-controls" --emit "$run" | tee "$run.methodology.log"
  python3 "${ROOT}/tools/gf2/finalize_gf2_g4_i6_census.py" --run-dir "$run"
done

for artifact in \
  GF2_G4_I6_OWNERSHIP_CENSUS.tsv \
  GF2_G4_I6_OWNERSHIP_ANOMALIES.tsv \
  GF2_G4_I6_OWNERSHIP_CENSUS.md \
  GF2_G4_I6_COLLISION_PROJECTION.tsv \
  g4-i6-summary.txt; do
  cmp "$RUN_A/$artifact" "$RUN_B/$artifact"
done

echo "G4-I6 full derived corpus deterministic repeat: PASS"
echo "G4-I6 corrected methodology controls: PASS"
cp "$RUN_A/g4-i6-summary.txt" "$BUILD/g4-i6-summary.txt"
cat "$BUILD/g4-i6-summary.txt"

# An unknown nonzero raw archetype may be explained, but must never disappear
# silently and still allow closure.
if grep -q 'UNKNOWN_ID_REJECTED' "$RUN_A/GF2_G4_I6_OWNERSHIP_CENSUS.tsv"; then
  echo "G4_I6_UNKNOWN_RAW_REJECTION_PRESENT"
  exit 1
fi

stale=0
for artifact in \
  GF2_G4_I6_OWNERSHIP_CENSUS.tsv \
  GF2_G4_I6_OWNERSHIP_ANOMALIES.tsv \
  GF2_G4_I6_OWNERSHIP_CENSUS.md; do
  committed="$ROOT/docs/research/$artifact"
  if [[ ! -f "$committed" ]]; then
    echo "G4_I6_ARTIFACT_STALE missing=$artifact"
    stale=1
    continue
  fi
  if ! cmp "$RUN_A/$artifact" "$committed"; then
    echo "G4_I6_ARTIFACT_STALE drift=$artifact"
    stale=1
  fi
done
if [[ "$stale" -ne 0 ]]; then
  echo "G4-I6 committed evidence freshness: RED"
  exit 1
fi

echo "G4-I6 committed evidence freshness: UP TO DATE"
echo "G4-I6 global ownership census host gate: PASS"
