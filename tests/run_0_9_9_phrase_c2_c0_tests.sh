#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/phrase-c2-c0-replay"
BASE_SHA="a413561136b274a1b16b079f95f8d3ce3353fac5"
REFERENCE_SHA="cd0e77a8acdf62e449792964f14899cfa118120b"
REFERENCE_BOUNDARY_BLOB="840cbbfd6edb598ca64f136d2af2ec3665cb8450"
REFERENCE_CORPUS_BLOB="39de5dc185c87237dd5083a6993fa611a673fedc"
mkdir -p "$BUILD"

# Corrected replay must remain research-only: no production byte may differ
# from the authoritative finalized P1R base.
git -C "$ROOT" diff --exit-code "$BASE_SHA" -- src/
echo "C2-C0-R frozen P1R src firewall: OK"

boundary_blob="$(git -C "$ROOT" rev-parse HEAD:tests/test_0_9_9_phrase_c2_c0_boundary_topology.cpp)"
corpus_blob="$(git -C "$ROOT" rev-parse HEAD:tests/test_0_9_9_phrase_c2_c0_corpus.cpp)"
test "$boundary_blob" = "$REFERENCE_BOUNDARY_BLOB"
test "$corpus_blob" = "$REFERENCE_CORPUS_BLOB"
echo "C2-C0-R old boundary oracle byte identity: EXACT"
echo "C2-C0-R old exhaustive corpus oracle byte identity: EXACT"

# Finalized P1R adapts the old bool/out-param source accessor to H1-F1's
# ChordProgressionEventResult shape. The characterization below must exercise
# that corrected path without reintroducing a finite-plan modulo fallback.
grep -Fq 'const ChordProgressionEventResult result =' \
  "$ROOT/src/generation/migration/strong_rhythm_migration.h"
grep -Fq 'chordProgressionEventAt(source, globalHarmonicOrdinal)' \
  "$ROOT/src/generation/migration/strong_rhythm_migration.h"
echo "C2-C0-R finalized H1-F1 accessor adapter: PRESENT"

mapfile -t SOURCES < <(
  sed -n '/COMMON_SOURCES=(/,/)/p' \
    "${ROOT}/tests/run_stage15_tonal_integration_tests.sh" |
    grep -F '"${ROOT}/src/' |
    sed -E 's/.*"\$\{ROOT\}(.*)".*/\1/'
)
SOURCES+=("/src/generation/migration/phrase_execution.cpp")

CXXFLAGS=(
  -std=c++17
  -Wall
  -Wextra
  -Werror
  -Wvla
  -Wno-c++20-extensions
  -Wno-unused-but-set-variable
  -I"${ROOT}"
)
WITNESS_TEST="${ROOT}/tests/test_0_9_9_phrase_c2_c0_boundary_topology.cpp"
CORPUS_TEST="${ROOT}/tests/test_0_9_9_phrase_c2_c0_corpus.cpp"

build() {
  local compiler="$1"
  local output="$2"
  local test="$3"
  shift 3
  local resolved=()
  local source
  for source in "${SOURCES[@]}"; do
    resolved+=("${ROOT}${source}")
  done
  "$compiler" "${CXXFLAGS[@]}" "$@" "${resolved[@]}" "$test" -o "$output"
}

# Preserve the frozen bounded witness oracle unchanged.
build "${CXX:-g++}" "$BUILD/witness-gcc" "$WITNESS_TEST"
"$BUILD/witness-gcc" > "$BUILD/witness-gcc-1.out"
"$BUILD/witness-gcc" > "$BUILD/witness-gcc-2.out"
diff -u "$BUILD/witness-gcc-1.out" "$BUILD/witness-gcc-2.out"
cat "$BUILD/witness-gcc-1.out"
echo "C2-C0-R known witness deterministic repeat: OK"

if command -v clang++ >/dev/null 2>&1; then
  build clang++ "$BUILD/witness-clang" "$WITNESS_TEST"
  "$BUILD/witness-clang" > "$BUILD/witness-clang.out"
  diff -u "$BUILD/witness-gcc-1.out" "$BUILD/witness-clang.out"
  echo "C2-C0-R known witness Clang gate: OK"
fi

# Run the byte-identical old exhaustive oracle against the corrected final P1R
# ancestry. Two full runs prove deterministic result parity on this exact tree.
build "${CXX:-g++}" "$BUILD/corpus-gcc" "$CORPUS_TEST" -O2
"$BUILD/corpus-gcc" > "$BUILD/corpus-gcc-1.out"
"$BUILD/corpus-gcc" > "$BUILD/corpus-gcc-2.out"
diff -u "$BUILD/corpus-gcc-1.out" "$BUILD/corpus-gcc-2.out"
cat "$BUILD/corpus-gcc-1.out"
echo "C2-C0-R full corpus/count/signature determinism: OK"

# Frozen #395 cardinalities. These assertions distinguish adjacent-boundary
# signatures (294,725) from terminal N2 signatures (104,104).
grep -Fq 'request_tuples=75496320' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 CORPUS SETTINGS: active=288 legacy=0' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 CORPUS TOTALS: phrases=41418120 adjacent_boundaries=96729660 unique_boundary_signatures=294725 pure_melodic=52296930 pure_melodic_nonempty_incoming=41306411' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 CLASS A_ONSET: raw=17530610 unique=30408' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 CLASS A_CONTINUATION: raw=0 unique=0' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 CLASS A_OVERLAP: raw=0 unique=0' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 CLASS B: raw=22115006 unique=33632' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 CLASS H: raw=9276932 unique=80113' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 CLASS N0: raw=1644348 unique=1408' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 CLASS N1: raw=21660687 unique=87948' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 CLASS N3: raw=909477 unique=6091' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 CLASS OTHER: raw=23592600 unique=55125' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 CLASS N2_TERMINAL: raw=41418120 unique=104104 denominator=terminal_phrase_controls_not_adjacent' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 SIGNATURE COLLISION VALIDATION: groups=290961 replays=729398 profile_diverse_groups=147476 PASS' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 A_CONTINUATION: UNREACHABLE UNDER FROZEN ATTEMPT-0 PRODUCTION SEMANTICS' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 A_OVERLAP: 0; no semantic ownership ambiguity observed' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 DECISION A: NATURAL PURE-MELODIC CROSSBAR TOPOLOGY REACHABLE' "$BUILD/corpus-gcc-1.out"
grep -Fq 'C2-C0 NEXT PRODUCER SCOPE: A-ONSET ONLY' "$BUILD/corpus-gcc-1.out"
echo "C2-C0-R frozen old corpus cardinality parity: EXACT"
echo "C2-C0-R corrected H1-F1 adapter result parity: EXACT"

if command -v clang++ >/dev/null 2>&1; then
  build clang++ "$BUILD/corpus-clang" "$CORPUS_TEST" -O2
  "$BUILD/corpus-clang" > "$BUILD/corpus-clang.out"
  diff -u "$BUILD/corpus-gcc-1.out" "$BUILD/corpus-clang.out"
  echo "C2-C0-R full corpus Clang equivalence: OK"
fi

# Sanitizers cover the same replay/classifier on the bounded smoke domain.
"$BUILD/corpus-gcc" --smoke > "$BUILD/corpus-smoke-gcc.out"
build "${CXX:-g++}" "$BUILD/corpus-asan" "$CORPUS_TEST" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  "$BUILD/corpus-asan" --smoke > "$BUILD/corpus-smoke-asan.out"
diff -u "$BUILD/corpus-smoke-gcc.out" "$BUILD/corpus-smoke-asan.out"
echo "C2-C0-R ASan smoke gate: OK"

build "${CXX:-g++}" "$BUILD/corpus-ubsan" "$CORPUS_TEST" \
  -O1 -g -fno-omit-frame-pointer \
  -fsanitize=undefined -fno-sanitize-recover=undefined
"$BUILD/corpus-ubsan" --smoke > "$BUILD/corpus-smoke-ubsan.out"
diff -u "$BUILD/corpus-smoke-gcc.out" "$BUILD/corpus-smoke-ubsan.out"
echo "C2-C0-R UBSan smoke gate: OK"

# Keep the old physical-gate source evidence unchanged; this is source-only
# characterization and does not claim hardware listening.
python3 - "$ROOT" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1])
genre = (root / "src/dsp/genre_manager.cpp").read_text()
engine = (root / "src/dsp/miniacid_engine.cpp").read_text()
required_genre = (
    "constexpr GenerativeParams kPresetAcid =\n"
    "    {8, 14, 36, 72, 0.40f, 0.50f, 0.8f"
)
assert required_genre in genre, "Acid gateLengthMultiplier source changed"
assert "float vMult = (synthIdx == 0) ? 0.85f : 1.05f;" in engine
assert "if (synthIdx == 1 && effectiveGateMult > 0.98f) effectiveGateMult = 0.98f;" in engine
assert "long dur = (long)(samplesPerStep_ * effectiveGateMult);" in engine
assert "gateCountdownB_ = dur;" in engine
assert "if (gateCountdownB_ > 0 && --gateCountdownB_ <= 0)" in engine
assert "if (synthVoices_[1]) synthVoices_[1]->release();" in engine
assert "publishPatternNoteOff_(1);" in engine
effective = 0.8 * 1.05
assert abs(effective - 0.84) < 1e-9 and effective < 1.0
print("C2-C0-R PHYSICAL GATE SOURCE: Acid 0.8 * SynthB 1.05 = 0.84 step; countdown release occurs before bar boundary")
PY
echo "C2-C0-R physical gate source contract: OK"

# P1R remains the exact execution and legacy compatibility oracle on corrected
# H1-F1 -> W1R -> H2R -> P1R ancestry.
bash "${ROOT}/tests/run_0_9_9_phrase_p1r_tests.sh"
echo "C2-C0-R finalized P1R compatibility: OK"

git -C "$ROOT" diff --exit-code "$BASE_SHA" -- src/
echo "C2-C0-R final frozen P1R src firewall: OK"
echo "C2-C0-R reference=$REFERENCE_SHA"
echo "0.9.9-PHRASE-C2-C0-R audit: PASS (characterization replay only; no producer)"
