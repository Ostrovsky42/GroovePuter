#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/phrase-c2-c0-corrected-i1-replay"
BASE_SHA="fb30bbb93f739d8b3e4514ae931cbc73fc8eeb09"
REFERENCE_SHA="cd0e77a8acdf62e449792964f14899cfa118120b"
REFERENCE_REPLAY_SHA="74cc88a8fd7cb5995d949a25eaf8baaa2c4d39ed"
REFERENCE_BOUNDARY_BLOB="840cbbfd6edb598ca64f136d2af2ec3665cb8450"
REFERENCE_CORPUS_BLOB="39de5dc185c87237dd5083a6993fa611a673fedc"
mkdir -p "$BUILD"

# C2-C0-R remains research-only. I1 is the exact production base and no src/
# byte may change in this characterization replay.
git -C "$ROOT" diff --exit-code "$BASE_SHA" -- src/
echo "C2-C0-R frozen I1 src firewall: OK"

boundary_blob="$(git -C "$ROOT" rev-parse HEAD:tests/test_0_9_9_phrase_c2_c0_boundary_topology.cpp)"
corpus_blob="$(git -C "$ROOT" rev-parse HEAD:tests/test_0_9_9_phrase_c2_c0_corpus.cpp)"
test "$boundary_blob" = "$REFERENCE_BOUNDARY_BLOB"
test "$corpus_blob" = "$REFERENCE_CORPUS_BLOB"
echo "C2-C0-R old boundary oracle byte identity: EXACT"
echo "C2-C0-R old exhaustive corpus oracle byte identity: EXACT"

# Finalized P1R on the frozen I1 ancestry must retain the H1-F1 result adapter.
grep -Fq 'const ChordProgressionEventResult result =' \
  "$ROOT/src/generation/migration/strong_rhythm_migration.h"
grep -Fq 'chordProgressionEventAt(source, globalHarmonicOrdinal)' \
  "$ROOT/src/generation/migration/strong_rhythm_migration.h"
echo "C2-C0-R finalized H1-F1 accessor adapter: PRESENT"

# I1 must still be lifetime-inert before C2 production starts.
grep -Fq 'P1R deliberately has no non-trivial cross-bar lifetime' \
  "$ROOT/src/generation/migration/phrase_execution.cpp"
if grep -R -n -E 'A_ONSET|A_CONTINUATION|phrase_c2|phrase_r1' \
    "$ROOT/src/dsp/generated_phrase_p1r_materializer.h" \
    "$ROOT/src/dsp/generated_phrase_song.h" \
    "$ROOT/src/ui/pages/pattern_edit_page.h" \
    "$ROOT/src/ui/pages/synth_sequencer_page.cpp"; then
  echo "C2-C0-R unexpected C2/R1 semantics in frozen I1 owners" >&2
  exit 1
fi
echo "C2-C0-R frozen I1 C2/R1 semantic firewall: OK"

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

build "${CXX:-g++}" "$BUILD/corpus-gcc" "$CORPUS_TEST" -O2
"$BUILD/corpus-gcc" > "$BUILD/corpus-gcc-1.out"
"$BUILD/corpus-gcc" > "$BUILD/corpus-gcc-2.out"
diff -u "$BUILD/corpus-gcc-1.out" "$BUILD/corpus-gcc-2.out"
cat "$BUILD/corpus-gcc-1.out"
echo "C2-C0-R full corpus/count/signature determinism: OK"

# Frozen #395/#403 cardinalities. Do not update these values to make a changed
# topology pass.
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
echo "C2-C0-R corrected I1 ancestry result parity: EXACT"

if command -v clang++ >/dev/null 2>&1; then
  build clang++ "$BUILD/corpus-clang" "$CORPUS_TEST" -O2
  "$BUILD/corpus-clang" > "$BUILD/corpus-clang.out"
  diff -u "$BUILD/corpus-gcc-1.out" "$BUILD/corpus-clang.out"
  echo "C2-C0-R full corpus Clang equivalence: OK"
fi

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

# Existing physical gate remains source evidence only; C2-C0-R makes no new
# hardware-listening claim.
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

# The exact frozen I1 suite includes P1R and D2 compatibility and must remain
# green before C2 production can begin.
bash "${ROOT}/tests/run_0_9_9_phrase_i1_tests.sh"
echo "C2-C0-R frozen I1/P1R/D2 compatibility: OK"

git -C "$ROOT" diff --exit-code "$BASE_SHA" -- src/
echo "C2-C0-R final frozen I1 src firewall: OK"
echo "C2-C0-R original_reference=$REFERENCE_SHA p1r_replay_reference=$REFERENCE_REPLAY_SHA"
echo "0.9.9-PHRASE-C2-C0-R corrected I1 audit: PASS (characterization only; no producer)"
