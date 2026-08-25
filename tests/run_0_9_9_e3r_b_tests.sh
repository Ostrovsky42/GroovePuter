#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/e3r-b-drop-displace-value"
FROZEN_SUMMARY="${ROOT_DIR}/tests/data/v0r_e2_variant_graph_summary.csv"
mkdir -p "${BUILD_DIR}"

python3 "${ROOT_DIR}/tests/test_0_9_9_e3r_b_source_guard.py"
bash "${ROOT_DIR}/tests/run_0_9_9_e3r_b_frozen_baseline.sh"

# E3a is the execution authority and already reruns E2c/E2a/E2b. The explicit
# E2a/E2b calls below keep the requested consumer gates visible in this runner.
bash "${ROOT_DIR}/tests/run_0_9_9_e3a_tests.sh"
bash "${ROOT_DIR}/tests/run_0_9_9_e2a_tests.sh"
bash "${ROOT_DIR}/tests/run_0_9_9_e2b_tests.sh"

SOURCES=(
  "${ROOT_DIR}/src/generation/generation_context.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_catalog.cpp"
  "${ROOT_DIR}/src/generation/rhythm/relationship_resolver.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_realizer_evolution.cpp"
  "${ROOT_DIR}/src/generation/rhythm/rhythm_canonical_diff.cpp"
  "${ROOT_DIR}/src/generation/rhythm/reference_vocabulary.cpp"
)

build_tool() {
  local compiler="$1"
  local output="$2"
  shift 2
  "${compiler}" -std=c++17 -Wall -Wextra -Werror -Wvla \
    -Wno-c++20-extensions -Wno-unused-but-set-variable \
    -I"${ROOT_DIR}" "$@" \
    "${SOURCES[@]}" \
    "${ROOT_DIR}/tools/research/e3r_b_drop_displace_graph.cpp" \
    -o "${output}"
}

run_and_report() {
  local binary="$1"
  local name="$2"
  local raw="${BUILD_DIR}/${name}.raw"
  local out="${BUILD_DIR}/${name}-report"
  "${binary}" > "${raw}"
  python3 "${ROOT_DIR}/tools/research/e3r_b_report.py" \
    --input "${raw}" \
    --out-dir "${out}" \
    --frozen-summary "${FROZEN_SUMMARY}"
}

GCC_BIN="${BUILD_DIR}/e3r_b_gcc"
CLANG_BIN="${BUILD_DIR}/e3r_b_clang"
SAN_BIN="${BUILD_DIR}/e3r_b_sanitize"

build_tool "${CXX:-g++}" "${GCC_BIN}" -O2
run_and_report "${GCC_BIN}" gcc-run1
run_and_report "${GCC_BIN}" gcc-run2
cmp "${BUILD_DIR}/gcc-run1.raw" "${BUILD_DIR}/gcc-run2.raw"

if ! command -v clang++ >/dev/null 2>&1; then
  echo "E3R-B requires clang++ for determinism authority" >&2
  exit 61
fi
build_tool clang++ "${CLANG_BIN}" -O2
run_and_report "${CLANG_BIN}" clang
cmp "${BUILD_DIR}/gcc-run1.raw" "${BUILD_DIR}/clang.raw"

build_tool "${CXX:-g++}" "${SAN_BIN}" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined
ASAN_OPTIONS=detect_leaks=0 run_and_report "${SAN_BIN}" sanitize
cmp "${BUILD_DIR}/gcc-run1.raw" "${BUILD_DIR}/sanitize.raw"

ARTIFACTS=(
  e3r_b_graph_summary.csv
  e3r_b_graph_summary.json
  e3r_b_graph_nodes.csv
  e3r_b_graph_edges.csv
  e3r_b_review_corpus.csv
  e3r_b_review_corpus.md
  e3r_b_digests.txt
)
for artifact in "${ARTIFACTS[@]}"; do
  cmp "${BUILD_DIR}/gcc-run1-report/${artifact}" "${BUILD_DIR}/gcc-run2-report/${artifact}"
  cmp "${BUILD_DIR}/gcc-run1-report/${artifact}" "${BUILD_DIR}/clang-report/${artifact}"
  cmp "${BUILD_DIR}/gcc-run1-report/${artifact}" "${BUILD_DIR}/sanitize-report/${artifact}"
done

echo "0.9.9-E3R-B DROP / DISPLACE counterfactual graph cap=1: OK"

# Cap=1 produced useful topology while remaining below authority ceilings.
# Run the requested secondary research-only cap=2 sensitivity with the exact
# same graph code, changing only the copied operation-cap declaration.
bash "${ROOT_DIR}/tests/run_0_9_9_e3r_b_cap2_sensitivity.sh"

echo "0.9.9-E3R-B DROP / DISPLACE graph authority: OK"
