#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BASELINE_SHA="2f4058ee27fc6185868f33ade470b4098a677fcf"
I6_BASE="c7be954f9739cf6f11b723b2dc094d303dc4f15b"
BUILD_DIR="${ROOT}/build/host-tests/gf2-g4-c0r6-causal-delta"
BASELINE_WT="${BUILD_DIR}/baseline-worktree"
BASELINE_ROWS="${BUILD_DIR}/BASELINE_C0R5_ROLE_COHERENCE_ROWS.tsv"
I6_ROWS="${BUILD_DIR}/I6_C0R5_ROLE_COHERENCE_ROWS.tsv"
ANALYZER="${ROOT}/tools/gf2/g4_c0r6_causal_delta.py"
mkdir -p "${BUILD_DIR}"

echo "G4-C0R6 baseline: ${BASELINE_SHA}"
echo "G4-C0R6 I6 production base: ${I6_BASE}"
echo "G4-C0R6 head: $(git -C "${ROOT}" rev-parse HEAD)"

if ! git -C "${ROOT}" diff --quiet "${I6_BASE}...HEAD" -- src/; then
  echo "G4_C0R6_FAIL reason=research checkpoint changed src/" >&2
  git -C "${ROOT}" diff --stat "${I6_BASE}...HEAD" -- src/ >&2
  exit 1
fi
echo "G4-C0R6 source firewall: src/ DELTA = NONE"

required=(
  tests/support/build_gf2_gate_b_g4_c0r3_probe.sh
  tests/support/gf2_gate_b_g4_c0r5_full_role_census_test.sh
  tools/gf2/g4_c0r3_dump.cpp
  tools/gf2/g4_c0r3_tonal_probe.cpp
  tools/gf2/g4_c0r3_tonal_probe.h
  tools/gf2/g4_c0r4_bass_candidates_probe.cpp
  tools/gf2/g4_c0r4_bass_candidates_probe.h
  tools/gf2/g4_c0r4_role_plan_probe.cpp
  tools/gf2/g4_c0r4_role_plan_probe.h
  tools/gf2/g4_c0r5_role_coherence.py
)
for path in "${required[@]}"; do
  if [[ ! -f "${ROOT}/${path}" ]]; then
    echo "G4_C0R6_FAIL reason=missing proven observer path=${path}" >&2
    exit 1
  fi
done

if [[ ! -f "${ANALYZER}" ]]; then
  echo "G4_C0R6_RED missing_causal_analyzer=tools/gf2/g4_c0r6_causal_delta.py" >&2
  exit 1
fi

cleanup() {
  git -C "${ROOT}" worktree remove --force "${BASELINE_WT}" >/dev/null 2>&1 || true
}
trap cleanup EXIT
cleanup
rm -rf "${BASELINE_WT}"

git -C "${ROOT}" worktree add --detach "${BASELINE_WT}" "${BASELINE_SHA}"
bash "${BASELINE_WT}/tests/support/gf2_gate_b_g4_c0r5_full_role_census_test.sh"
BASELINE_SRC="${BASELINE_WT}/build/host-tests/gf2-gate-b/g4-c0r2-full/g4-c0r5-full-role-census/G4_C0R5_ROLE_COHERENCE_ROWS.tsv"
cp "${BASELINE_SRC}" "${BASELINE_ROWS}"

bash "${ROOT}/tests/support/gf2_gate_b_g4_c0r5_full_role_census_test.sh"
I6_SRC="${ROOT}/build/host-tests/gf2-gate-b/g4-c0r2-full/g4-c0r5-full-role-census/G4_C0R5_ROLE_COHERENCE_ROWS.tsv"
cp "${I6_SRC}" "${I6_ROWS}"

OUT_FILES=(
  G4_C0R6_CAUSAL_DELTA_ROWS.tsv
  G4_C0R6_CAUSAL_DELTA_SUMMARY.tsv
  G4_C0R6_NATIVE_TRANSITIONS.tsv
  G4_C0R6_TOPOLOGY_TRANSITIONS.tsv
  G4_C0R6_PROFILE_REPORT.md
)

run_analyzer() {
  local out_dir="$1"
  mkdir -p "${out_dir}"
  python3 "${ANALYZER}" \
    --baseline "${BASELINE_ROWS}" \
    --i6 "${I6_ROWS}" \
    --rows-output "${out_dir}/G4_C0R6_CAUSAL_DELTA_ROWS.tsv" \
    --summary-output "${out_dir}/G4_C0R6_CAUSAL_DELTA_SUMMARY.tsv" \
    --native-output "${out_dir}/G4_C0R6_NATIVE_TRANSITIONS.tsv" \
    --topology-output "${out_dir}/G4_C0R6_TOPOLOGY_TRANSITIONS.tsv" \
    --report-output "${out_dir}/G4_C0R6_PROFILE_REPORT.md"
}

PRIMARY="${BUILD_DIR}/primary"
REPLAY="${BUILD_DIR}/replay"
run_analyzer "${PRIMARY}"
run_analyzer "${REPLAY}" > "${BUILD_DIR}/analyzer-replay.log"

for file in "${OUT_FILES[@]}"; do
  cmp "${PRIMARY}/${file}" "${REPLAY}/${file}"
done
echo "G4-C0R6 analyzer replay: ALL OUTPUTS BYTE-IDENTICAL"

python3 - "${PRIMARY}" <<'PY'
import csv
import sys
from pathlib import Path

out = Path(sys.argv[1])
rows_path = out / "G4_C0R6_CAUSAL_DELTA_ROWS.tsv"
summary_path = out / "G4_C0R6_CAUSAL_DELTA_SUMMARY.tsv"
native_path = out / "G4_C0R6_NATIVE_TRANSITIONS.tsv"
topology_path = out / "G4_C0R6_TOPOLOGY_TRANSITIONS.tsv"
report_path = out / "G4_C0R6_PROFILE_REPORT.md"

with rows_path.open(newline="", encoding="utf-8") as handle:
    rows = list(csv.DictReader(handle, delimiter="\t"))
with summary_path.open(newline="", encoding="utf-8") as handle:
    summary = list(csv.DictReader(handle, delimiter="\t"))
with native_path.open(newline="", encoding="utf-8") as handle:
    native = list(csv.DictReader(handle, delimiter="\t"))
with topology_path.open(newline="", encoding="utf-8") as handle:
    topology = list(csv.DictReader(handle, delimiter="\t"))

assert len(rows) == 512, len(rows)
assert len({(r["profile_ordinal"], r["identity_ordinal"]) for r in rows}) == 512
assert len(summary) == 4, len(summary)
assert native, "native transition table empty"
assert topology, "topology transition table empty"
assert report_path.stat().st_size > 0

native_classes = {
    "NATIVE_STABLE",
    "OUTSIDE_STABLE",
    "OUTSIDE_TO_NATIVE",
    "NATIVE_TO_OUTSIDE",
}
topology_classes = {
    "EXACT_MATCH",
    "PARTIAL_OVERLAP",
    "DISJOINT_NONEMPTY",
    "PLANNING_EMPTY_AUDIBLE_NONEMPTY",
    "AUDIBLE_EMPTY_PLANNING_NONEMPTY",
    "BOTH_EMPTY",
}
required = {
    "profile_ordinal", "profile_id", "identity_ordinal",
    "baseline_archetype", "i6_archetype", "archetype_changed",
    "baseline_rhythm_family", "i6_rhythm_family", "rhythm_family_changed",
    "baseline_bass_rhythm", "i6_bass_rhythm", "bass_rhythm_changed",
    "baseline_native_mask", "i6_native_mask", "native_mask_changed",
    "baseline_native_membership", "i6_native_membership", "native_transition",
    "baseline_planning_mask", "i6_planning_mask", "planning_changed",
    "baseline_audible_mask", "i6_audible_mask", "audible_changed",
    "baseline_topology", "i6_topology", "topology_transition",
    "change_source",
}
assert required <= set(rows[0]), sorted(required - set(rows[0]))
for row in rows:
    assert row["native_transition"] in native_classes, row
    assert row["baseline_topology"] in topology_classes, row
    assert row["i6_topology"] in topology_classes, row
    assert row["topology_transition"] == f"{row['baseline_topology']}->{row['i6_topology']}", row
    for field in (
        "archetype_changed", "rhythm_family_changed", "bass_rhythm_changed",
        "native_mask_changed", "planning_changed", "audible_changed",
    ):
        assert row[field] in {"0", "1"}, (field, row)

print("G4-C0R6 causal delta contract: 512 joined coordinates PASS")
PY

echo "G4-C0R6 causal delta attribution: PASS"
