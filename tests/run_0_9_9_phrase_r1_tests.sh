#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build/host-tests/phrase-r1"
BASE="2f9b6c7659bb4e0560c129ee33951f7adfcba8a4"
mkdir -p "$BUILD"

merge_base="$(git -C "$ROOT" merge-base HEAD "$BASE")"
if [[ "$merge_base" != "$BASE" ]]; then
  echo "R1 predecessor mismatch: merge-base=$merge_base expected=$BASE" >&2
  exit 1
fi

git -C "$ROOT" diff --exit-code "$BASE" HEAD -- src/generation/
python3 "${ROOT}/tests/test_0_9_9_phrase_r1_source_regressions.py"

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

resolve_sources() {
  local resolved=()
  local source
  for source in "${SOURCES[@]}"; do
    resolved+=("${ROOT}${source}")
  done
  printf '%s\n' "${resolved[@]}"
}
mapfile -t RESOLVED_SOURCES < <(resolve_sources)

build_test() {
  local compiler="$1"
  local test="$2"
  local output="$3"
  shift 3
  "$compiler" "${CXXFLAGS[@]}" "$@" \
    "${RESOLVED_SOURCES[@]}" "$test" -o "$output"
}

R1_TEST="${ROOT}/tests/test_0_9_9_phrase_r1_crossbar_lifetime.cpp"
build_test "${CXX:-g++}" "$R1_TEST" "$BUILD/r1-gcc"
"$BUILD/r1-gcc" > "$BUILD/r1-gcc-1.out"
"$BUILD/r1-gcc" > "$BUILD/r1-gcc-2.out"
diff -u "$BUILD/r1-gcc-1.out" "$BUILD/r1-gcc-2.out"
cat "$BUILD/r1-gcc-1.out"
echo "R1 deterministic repeat: OK"

if command -v clang++ >/dev/null 2>&1; then
  build_test clang++ "$R1_TEST" "$BUILD/r1-clang"
  "$BUILD/r1-clang" > "$BUILD/r1-clang.out"
  diff -u "$BUILD/r1-gcc-1.out" "$BUILD/r1-clang.out"
  echo "R1 Clang gate: OK"
fi

build_test "${CXX:-g++}" "$R1_TEST" "$BUILD/r1-asan" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  "$BUILD/r1-asan" > "$BUILD/r1-asan.out"
diff -u "$BUILD/r1-gcc-1.out" "$BUILD/r1-asan.out"
echo "R1 ASan gate: OK"

build_test "${CXX:-g++}" "$R1_TEST" "$BUILD/r1-ubsan" \
  -O1 -g -fno-omit-frame-pointer \
  -fsanitize=undefined -fno-sanitize-recover=undefined
"$BUILD/r1-ubsan" > "$BUILD/r1-ubsan.out"
diff -u "$BUILD/r1-gcc-1.out" "$BUILD/r1-ubsan.out"
echo "R1 UBSan gate: OK"

# Frozen C2 functional suite on the R1 exact SHA. We intentionally do not call
# the predecessor C2 shell wrapper because its branch-local source firewall is
# designed to reject any post-C2 production delta, including this permitted R1
# DSP-only executor. The same frozen C2 test binary and expected output are run
# here under GCC/repeat/Clang/ASan/UBSan without changing C2 expectations.
python3 "${ROOT}/tests/test_0_9_9_phrase_p1r_source_regressions.py"
C2_TEST="${ROOT}/tests/test_0_9_9_phrase_c2_a_onset_lifetime.cpp"
build_test "${CXX:-g++}" "$C2_TEST" "$BUILD/c2-gcc"
"$BUILD/c2-gcc" > "$BUILD/c2-gcc-1.out"
"$BUILD/c2-gcc" > "$BUILD/c2-gcc-2.out"
diff -u "$BUILD/c2-gcc-1.out" "$BUILD/c2-gcc-2.out"
cat "$BUILD/c2-gcc-1.out"
if command -v clang++ >/dev/null 2>&1; then
  build_test clang++ "$C2_TEST" "$BUILD/c2-clang"
  "$BUILD/c2-clang" > "$BUILD/c2-clang.out"
  diff -u "$BUILD/c2-gcc-1.out" "$BUILD/c2-clang.out"
fi
build_test "${CXX:-g++}" "$C2_TEST" "$BUILD/c2-asan" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" \
  "$BUILD/c2-asan" > "$BUILD/c2-asan.out"
diff -u "$BUILD/c2-gcc-1.out" "$BUILD/c2-asan.out"
build_test "${CXX:-g++}" "$C2_TEST" "$BUILD/c2-ubsan" \
  -O1 -g -fno-omit-frame-pointer \
  -fsanitize=undefined -fno-sanitize-recover=undefined
"$BUILD/c2-ubsan" > "$BUILD/c2-ubsan.out"
diff -u "$BUILD/c2-gcc-1.out" "$BUILD/c2-ubsan.out"
echo "T40 PHRASE-C2 focused compatibility: OK"

# Frozen C2-C0 minimal witness and complete attempt-0 corpus. Exact counts are
# predecessor contract values; no corpus/golden is regenerated or rewritten.
C0_TEST="${ROOT}/tests/test_0_9_9_phrase_c2_c0_boundary_topology.cpp"
build_test "${CXX:-g++}" "$C0_TEST" "$BUILD/c2-c0" -O2
"$BUILD/c2-c0" > "$BUILD/c2-c0.out"
grep -F "C2-C0 TARGET PURE A: mode=0 recipe=0 bars=2 identity=2 boundary=0->1" \
  "$BUILD/c2-c0.out"
grep -F "C2-C0 TARGET SUBCLASS: A_ONSET" "$BUILD/c2-c0.out"
grep -F "C2-C0 TARGET RESULT A: NATURAL PURE-MELODIC CROSSBAR TOPOLOGY REACHABLE" \
  "$BUILD/c2-c0.out"

C0_CORPUS="${ROOT}/tests/test_0_9_9_phrase_c2_c0_corpus.cpp"
build_test "${CXX:-g++}" "$C0_CORPUS" "$BUILD/c2-c0-corpus" -O2
"$BUILD/c2-c0-corpus" > "$BUILD/c2-c0-corpus.out"
grep -F "C2-C0 CORPUS DOMAIN: modes=16 recipes=18 identities=65535 lengths=4 attempt=0 rhythm_selection=AUTO request_tuples=75496320" \
  "$BUILD/c2-c0-corpus.out"
grep -F "C2-C0 CORPUS TOTALS: phrases=41418120 adjacent_boundaries=96729660 unique_boundary_signatures=294725 pure_melodic=52296930 pure_melodic_nonempty_incoming=41306411" \
  "$BUILD/c2-c0-corpus.out"
grep -F "C2-C0 CLASS A_ONSET: raw=17530610 unique=30408" "$BUILD/c2-c0-corpus.out"
grep -F "C2-C0 CLASS A_CONTINUATION: raw=0 unique=0" "$BUILD/c2-c0-corpus.out"
grep -F "C2-C0 CLASS A_OVERLAP: raw=0 unique=0" "$BUILD/c2-c0-corpus.out"
grep -F "C2-C0 CLASS B: raw=22115006 unique=33632" "$BUILD/c2-c0-corpus.out"
grep -F "C2-C0 CLASS H: raw=9276932 unique=80113" "$BUILD/c2-c0-corpus.out"
grep -F "C2-C0 CLASS N0: raw=1644348 unique=1408" "$BUILD/c2-c0-corpus.out"
grep -F "C2-C0 CLASS N1: raw=21660687 unique=87948" "$BUILD/c2-c0-corpus.out"
grep -F "C2-C0 CLASS N3: raw=909477 unique=6091" "$BUILD/c2-c0-corpus.out"
grep -F "C2-C0 CLASS OTHER: raw=23592600 unique=55125" "$BUILD/c2-c0-corpus.out"
grep -F "C2-C0 CLASS N2_TERMINAL: raw=41418120 unique=104104" "$BUILD/c2-c0-corpus.out"
grep -F "C2-C0 DEFAULT PATH: GenreSettings{} => mode=0(Acid) recipe=0 rhythm_selection=AUTO; A_ONSET raw=59684 reachable=YES" \
  "$BUILD/c2-c0-corpus.out"
grep -F "C2-C0 A_ONSET LOCATION: intra=17495781 seam_3_to_4=34829" \
  "$BUILD/c2-c0-corpus.out"
grep -F "C2-C0 DECISION A: NATURAL PURE-MELODIC CROSSBAR TOPOLOGY REACHABLE" \
  "$BUILD/c2-c0-corpus.out"
grep -F "C2-C0 NEXT PRODUCER SCOPE: A-ONSET ONLY" "$BUILD/c2-c0-corpus.out"
echo "C2-C0 exhaustive compatibility: OK"

# P1R runner has no post-P1R branch source-scope restriction, so execute it
# unchanged on the exact R1 candidate SHA.
bash "${ROOT}/tests/run_0_9_9_phrase_p1r_tests.sh"
echo "T41 PHRASE-P1R focused compatibility: OK"

git -C "$ROOT" diff --exit-code "$BASE" HEAD -- src/generation/
python3 "${ROOT}/tests/test_0_9_9_phrase_r1_source_regressions.py" >/dev/null

echo "PHRASE-R1 focused production gate: OK"
