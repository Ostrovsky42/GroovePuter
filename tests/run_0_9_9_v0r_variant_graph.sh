#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/v0r-e2-variant-graph"
SNAPSHOT_DIR="${BUILD_DIR}/snapshot"
mkdir -p "${BUILD_DIR}" "${SNAPSHOT_DIR}"

python3 "${ROOT_DIR}/tests/test_0_9_9_v0r_source_guard.py"

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
    "${ROOT_DIR}/tools/research/v0r_e2_variant_graph.cpp" \
    -o "${output}"
}

GCC_BIN="${BUILD_DIR}/v0r_gcc"
CLANG_BIN="${BUILD_DIR}/v0r_clang"
SAN_BIN="${BUILD_DIR}/v0r_sanitize"

build_tool "${CXX:-g++}" "${GCC_BIN}" -O2
"${GCC_BIN}" > "${BUILD_DIR}/gcc-run1.raw"
"${GCC_BIN}" > "${BUILD_DIR}/gcc-run2.raw"
cmp "${BUILD_DIR}/gcc-run1.raw" "${BUILD_DIR}/gcc-run2.raw"

if command -v clang++ >/dev/null 2>&1; then
  build_tool clang++ "${CLANG_BIN}" -O2
  "${CLANG_BIN}" > "${BUILD_DIR}/clang.raw"
  cmp "${BUILD_DIR}/gcc-run1.raw" "${BUILD_DIR}/clang.raw"
else
  echo "V0R requires clang++ for cross-compiler authority" >&2
  exit 40
fi

build_tool "${CXX:-g++}" "${SAN_BIN}" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined
ASAN_OPTIONS=detect_leaks=0 \
  "${SAN_BIN}" > "${BUILD_DIR}/sanitize.raw"
cmp "${BUILD_DIR}/gcc-run1.raw" "${BUILD_DIR}/sanitize.raw"

for run in gcc-run1 gcc-run2 clang sanitize; do
  python3 "${ROOT_DIR}/tools/research/v0r_report.py" \
    --input "${BUILD_DIR}/${run}.raw" \
    --out-dir "${BUILD_DIR}/${run}-report"
done

for file in \
  v0r_e2_variant_graph_summary.csv \
  v0r_e2_variant_graph_summary.json \
  v0r_e2_variant_graph_nodes.csv \
  v0r_e2_variant_graph_edges.csv; do
  cmp "${BUILD_DIR}/gcc-run1-report/${file}" \
      "${BUILD_DIR}/gcc-run2-report/${file}"
  cmp "${BUILD_DIR}/gcc-run1-report/${file}" \
      "${BUILD_DIR}/clang-report/${file}"
  cmp "${BUILD_DIR}/gcc-run1-report/${file}" \
      "${BUILD_DIR}/sanitize-report/${file}"
done

python3 "${ROOT_DIR}/tools/research/v0r_compact_summary.py" \
  --raw "${BUILD_DIR}/gcc-run1.raw" \
  --authority-csv \
    "${BUILD_DIR}/gcc-run1-report/v0r_e2_variant_graph_summary.csv" \
  --full-json \
    "${BUILD_DIR}/gcc-run1-report/v0r_e2_variant_graph_summary.json" \
  --output-csv "${SNAPSHOT_DIR}/v0r_e2_variant_graph_summary.csv" \
  --output-json "${SNAPSHOT_DIR}/v0r_e2_variant_graph_summary.json"

compare_snapshot() {
  local label="$1"
  local expected="$2"
  local actual="$3"
  if cmp -s "${expected}" "${actual}"; then
    echo "V0R snapshot ${label}: PASS"
    return 0
  fi

  echo "V0R snapshot ${label}: DRIFT" >&2
  echo "expected committed snapshot: ${expected}" >&2
  echo "regenerated snapshot:        ${actual}" >&2
  echo "Refusing to refresh automatically. Classify as incorrect snapshot, " \
       "nondeterministic compact output, or authority semantic drift." >&2
  diff -u "${expected}" "${actual}" >&2 || true
  return 41
}

compare_snapshot \
  "CSV" \
  "${ROOT_DIR}/tests/data/v0r_e2_variant_graph_summary.csv" \
  "${SNAPSHOT_DIR}/v0r_e2_variant_graph_summary.csv"
compare_snapshot \
  "JSON" \
  "${ROOT_DIR}/tests/data/v0r_e2_variant_graph_summary.json" \
  "${SNAPSHOT_DIR}/v0r_e2_variant_graph_summary.json"

sha256sum \
  "${BUILD_DIR}/gcc-run1.raw" \
  "${BUILD_DIR}/gcc-run2.raw" \
  "${BUILD_DIR}/clang.raw" \
  "${BUILD_DIR}/sanitize.raw" \
  "${SNAPSHOT_DIR}/v0r_e2_variant_graph_summary.csv" \
  "${SNAPSHOT_DIR}/v0r_e2_variant_graph_summary.json"
cat "${BUILD_DIR}/gcc-run1-report/v0r_e2_variant_graph_digests.txt"

echo "V0R_SUMMARY_CSV_BEGIN"
cat "${BUILD_DIR}/gcc-run1-report/v0r_e2_variant_graph_summary.csv"
echo "V0R_SUMMARY_CSV_END"
echo "V0R_SUMMARY_JSON_BEGIN"
cat "${BUILD_DIR}/gcc-run1-report/v0r_e2_variant_graph_summary.json"
echo "V0R_SUMMARY_JSON_END"

# Cumulative frozen contracts. E1a is the Stage 6.1 ownership/executor matrix;
# later runners deliberately re-run earlier contracts as their own regressions.
bash "${ROOT_DIR}/tests/run_rhythm_stage6_1_tests.sh"
bash "${ROOT_DIR}/tests/run_0_9_9_e2c_tests.sh"
bash "${ROOT_DIR}/tests/run_0_9_9_e2a_tests.sh"
bash "${ROOT_DIR}/tests/run_0_9_9_e2b_tests.sh"

echo "0.9.9-V0R E2 authoritative rhythm variant graph: OK"
