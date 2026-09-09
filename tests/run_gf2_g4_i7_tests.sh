#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/gf2-g4-i7"
PROBE="${ROOT}/tests/test_gf2_g4_i7_dub_bass_compatibility.cpp"
mkdir -p "$BUILD"

EXPECTED_BASE="c7be954f9739cf6f11b723b2dc094d303dc4f15b"
echo "G4-I7 production base: ${EXPECTED_BASE}"
echo "G4-I7 head: $(git -C "$ROOT" rev-parse HEAD)"

if ! git -C "$ROOT" merge-base --is-ancestor "$EXPECTED_BASE" HEAD; then
  echo "G4_I7_BASE_MISMATCH expected_ancestor=${EXPECTED_BASE}"
  exit 1
fi

if [[ -n "$(git -C "$ROOT" diff "$EXPECTED_BASE"...HEAD -- src/)" ]]; then
  echo "G4_I7_SOURCE_FIREWALL_FAIL src_delta_present"
  exit 1
fi
echo "G4-I7 source firewall: src/ DELTA = NONE"

if [[ ! -f "$PROBE" ]]; then
  echo "G4_I7_RED missing_probe=tests/test_gf2_g4_i7_dub_bass_compatibility.cpp"
  exit 1
fi

# Static causal path checks. These do not decide correctness; they prove which
# production owner supplies and consumes the selected bass identity.
grep -Fq 'selectWeightedIdentityFromView(profile.bassRhythms, GenerationDomain::BassRhythmSelection' \
  "$ROOT/src/generation/composition/generation_profile.cpp"
grep -Fq 'bassRequest.requestedId = result.bassRhythmId;' \
  "$ROOT/src/generation/migration/strong_rhythm_migration.cpp"
grep -Fq 'if (request.requestedId != BassRhythmId::Auto) return request.requestedId;' \
  "$ROOT/src/generation/roles/bass_rhythm.cpp"
echo "G4-I7 causal source path: profile bag -> explicit request -> family bypass PRESENT"

mapfile -t SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' \
    "$ROOT/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/' |
    grep -v '^/src/generation/roles/bass_rhythm.cpp$'
)

RESOLVED=()
for source in "${SOURCES[@]}"; do
  RESOLVED+=("${ROOT}${source}")
done

"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Wvla \
  -Wno-c++20-extensions -Wno-unused-but-set-variable \
  -I"${ROOT}" \
  "${RESOLVED[@]}" \
  "$PROBE" \
  -o "$BUILD/g4-i7-dub-bass-compatibility"

"$BUILD/g4-i7-dub-bass-compatibility" > "$BUILD/run-a.txt"
"$BUILD/g4-i7-dub-bass-compatibility" > "$BUILD/run-b.txt"
cmp "$BUILD/run-a.txt" "$BUILD/run-b.txt"
cat "$BUILD/run-a.txt"
echo "G4-I7 deterministic replay: BYTE-IDENTICAL"
echo "G4-I7 Dub bass compatibility diagnosis: PASS"
