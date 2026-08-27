#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/phrase-c2-c0"
BASE="016bcd6ba514b3a57f8803c63c869f1b2a8953a7"
mkdir -p "$BUILD"

git -C "$ROOT" diff --exit-code "$BASE" -- src/
echo "C2-C0 frozen src firewall: OK"

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

# Keep the already-proven bounded existence witness as an independent oracle.
build "${CXX:-g++}" "$BUILD/witness-gcc" "$WITNESS_TEST"
"$BUILD/witness-gcc" > "$BUILD/witness-gcc-1.out"
"$BUILD/witness-gcc" > "$BUILD/witness-gcc-2.out"
diff -u "$BUILD/witness-gcc-1.out" "$BUILD/witness-gcc-2.out"
cat "$BUILD/witness-gcc-1.out"
echo "C2-C0 known witness deterministic repeat: OK"

if command -v clang++ >/dev/null 2>&1; then
  build clang++ "$BUILD/witness-clang" "$WITNESS_TEST"
  "$BUILD/witness-clang" > "$BUILD/witness-clang.out"
  diff -u "$BUILD/witness-gcc-1.out" "$BUILD/witness-clang.out"
  echo "C2-C0 known witness Clang gate: OK"
fi

# Authoritative attempt-0 corpus. Full enumeration is intentionally optimized,
# but every admitted phrase still goes through the exact P1R length selector.
build "${CXX:-g++}" "$BUILD/corpus-gcc" "$CORPUS_TEST" -O2
"$BUILD/corpus-gcc" > "$BUILD/corpus-gcc-1.out"
"$BUILD/corpus-gcc" > "$BUILD/corpus-gcc-2.out"
diff -u "$BUILD/corpus-gcc-1.out" "$BUILD/corpus-gcc-2.out"
cat "$BUILD/corpus-gcc-1.out"
echo "C2-C0 full corpus/count/signature determinism: OK"

if command -v clang++ >/dev/null 2>&1; then
  build clang++ "$BUILD/corpus-clang" "$CORPUS_TEST" -O2
  "$BUILD/corpus-clang" > "$BUILD/corpus-clang.out"
  diff -u "$BUILD/corpus-gcc-1.out" "$BUILD/corpus-clang.out"
  echo "C2-C0 full corpus Clang equivalence: OK"
fi

# Sanitizers exercise the same production replay, signature, classifier,
# continuation proof, M1L controls and frozen known witness. Re-running the
# 65,535-identity census under instrumentation would add cost without changing
# the authoritative denominator above.
"$BUILD/corpus-gcc" --smoke > "$BUILD/corpus-smoke-gcc.out"

build "${CXX:-g++}" "$BUILD/corpus-asan" "$CORPUS_TEST" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  "$BUILD/corpus-asan" --smoke > "$BUILD/corpus-smoke-asan.out"
diff -u "$BUILD/corpus-smoke-gcc.out" "$BUILD/corpus-smoke-asan.out"
echo "C2-C0 ASan smoke gate: OK"

build "${CXX:-g++}" "$BUILD/corpus-ubsan" "$CORPUS_TEST" \
  -O1 -g -fno-omit-frame-pointer \
  -fsanitize=undefined -fno-sanitize-recover=undefined
"$BUILD/corpus-ubsan" --smoke > "$BUILD/corpus-smoke-ubsan.out"
diff -u "$BUILD/corpus-smoke-gcc.out" "$BUILD/corpus-smoke-ubsan.out"
echo "C2-C0 UBSan smoke gate: OK"

# Freeze physical-gate evidence without linking or changing the runtime owner.
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
print("C2-C0 PHYSICAL GATE SOURCE: Acid 0.8 * SynthB 1.05 = 0.84 step; countdown release occurs before bar boundary")
PY
echo "C2-C0 physical gate source contract: OK"

# P1R remains the compatibility oracle for exact-length execution and the
# unchanged legacy M1 path.
bash "${ROOT}/tests/run_0_9_9_phrase_p1r_tests.sh"
echo "C2-C0 P1R compatibility: OK"

git -C "$ROOT" diff --exit-code "$BASE" -- src/
echo "C2-C0 final frozen src firewall: OK"
echo "C2-C0 boundary topology characterization: OK"
