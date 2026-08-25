#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/host-tests/e3r-b-frozen-baseline"
SNAPSHOT_DIR="${BUILD_DIR}/snapshot"
E3A="2549bbfdda84581a14783a775d3c8577f8855c54"
V0R="872fef9331a34e8d48f5703015b1126f11e581c3"
EXPECTED_RAW_SHA="8f4b70f44a2e6c32dce105b5526d532ba52e99021039774c13738bb772bb85d5"
mkdir -p "${BUILD_DIR}" "${SNAPSHOT_DIR}"

cd "${ROOT_DIR}"
git merge-base --is-ancestor "${E3A}" HEAD
git merge-base --is-ancestor "${V0R}" HEAD

# E3R-B is allowed to add research/tests/docs/workflow only. Production source
# must remain exactly E3a, while the frozen V0R harness/snapshots remain exact.
if ! git diff --quiet "${E3A}..HEAD" -- src/; then
  echo "E3R-B production src delta is non-zero" >&2
  git diff --name-only "${E3A}..HEAD" -- src/ >&2
  exit 51
fi

git diff --exit-code "${V0R}..HEAD" -- \
  tools/research/v0r_e2_variant_graph.cpp \
  tools/research/v0r_report.py \
  tools/research/v0r_compact_summary.py \
  tests/data/v0r_e2_variant_graph_summary.csv \
  tests/data/v0r_e2_variant_graph_summary.json

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

if ! command -v clang++ >/dev/null 2>&1; then
  echo "E3R-B frozen baseline requires clang++" >&2
  exit 52
fi
build_tool clang++ "${CLANG_BIN}" -O2
"${CLANG_BIN}" > "${BUILD_DIR}/clang.raw"
cmp "${BUILD_DIR}/gcc-run1.raw" "${BUILD_DIR}/clang.raw"

build_tool "${CXX:-g++}" "${SAN_BIN}" \
  -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined
ASAN_OPTIONS=detect_leaks=0 "${SAN_BIN}" > "${BUILD_DIR}/sanitize.raw"
cmp "${BUILD_DIR}/gcc-run1.raw" "${BUILD_DIR}/sanitize.raw"

ACTUAL_RAW_SHA="$(sha256sum "${BUILD_DIR}/gcc-run1.raw" | awk '{print $1}')"
if [[ "${ACTUAL_RAW_SHA}" != "${EXPECTED_RAW_SHA}" ]]; then
  echo "Frozen V0R authority digest drift" >&2
  echo "expected=${EXPECTED_RAW_SHA}" >&2
  echo "actual=${ACTUAL_RAW_SHA}" >&2
  exit 53
fi

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
  cmp "${BUILD_DIR}/gcc-run1-report/${file}" "${BUILD_DIR}/gcc-run2-report/${file}"
  cmp "${BUILD_DIR}/gcc-run1-report/${file}" "${BUILD_DIR}/clang-report/${file}"
  cmp "${BUILD_DIR}/gcc-run1-report/${file}" "${BUILD_DIR}/sanitize-report/${file}"
done

python3 "${ROOT_DIR}/tools/research/v0r_compact_summary.py" \
  --raw "${BUILD_DIR}/gcc-run1.raw" \
  --authority-csv "${BUILD_DIR}/gcc-run1-report/v0r_e2_variant_graph_summary.csv" \
  --full-json "${BUILD_DIR}/gcc-run1-report/v0r_e2_variant_graph_summary.json" \
  --output-csv "${SNAPSHOT_DIR}/v0r_e2_variant_graph_summary.csv" \
  --output-json "${SNAPSHOT_DIR}/v0r_e2_variant_graph_summary.json"

cmp "${ROOT_DIR}/tests/data/v0r_e2_variant_graph_summary.csv" \
    "${SNAPSHOT_DIR}/v0r_e2_variant_graph_summary.csv"
cmp "${ROOT_DIR}/tests/data/v0r_e2_variant_graph_summary.json" \
    "${SNAPSHOT_DIR}/v0r_e2_variant_graph_summary.json"

echo "E3R-B frozen V0R baseline: PASS"
echo "authority_sha256=${ACTUAL_RAW_SHA}"
