#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-gate-b"
BIN="${BUILD_DIR}/gf2_gate_b_dump_c0r3_gcc"
# Keep C0R5 below the already-uploaded full-pilot directory so the existing
# strict-proof artifact includes the role-coherence evidence without widening
# workflow artifact policy.
OUT_DIR="${BUILD_DIR}/g4-c0r2-full/g4-c0r5-full-role-census"
RAW="${OUT_DIR}/G4_C0R5_ROLE_COHERENCE_RAW.tsv"
DETAIL="${OUT_DIR}/G4_C0R5_ROLE_COHERENCE_ROWS.tsv"
SUMMARY="${OUT_DIR}/G4_C0R5_ROLE_COHERENCE_SUMMARY.tsv"
DETAIL_REPLAY="${OUT_DIR}/G4_C0R5_ROLE_COHERENCE_ROWS_REPLAY.tsv"
SUMMARY_REPLAY="${OUT_DIR}/G4_C0R5_ROLE_COHERENCE_SUMMARY_REPLAY.tsv"

bash "${ROOT}/tests/support/build_gf2_gate_b_g4_c0r3_probe.sh"
mkdir -p "${OUT_DIR}"

: > "${RAW}"
first=1
for profile in 0 9 20 27; do
  for identity in $(seq 0 127); do
    tmp="${OUT_DIR}/row-${profile}-${identity}.tsv"
    "${BIN}" --g4-c0r3-dump "${profile}" "${identity}" 0 23 P1 > "${tmp}"
    if [[ "${first}" -eq 1 ]]; then
      cat "${tmp}" >> "${RAW}"
      first=0
    else
      tail -n +2 "${tmp}" >> "${RAW}"
    fi
  done
done

python3 - "${RAW}" <<'PY'
import csv
import itertools
import sys
from pathlib import Path

path = Path(sys.argv[1])
with path.open(newline="", encoding="utf-8") as handle:
    rows = list(csv.DictReader(handle, delimiter="\t"))

expected_profiles = {
    "0": "Acid/BASE",
    "9": "Dub/Reggae/Dub Techno",
    "20": "House/BASE",
    "27": "Drum&Bass/BASE",
}
expected_coordinates = {
    (profile, str(identity))
    for profile, identity in itertools.product(expected_profiles, range(128))
}
actual_coordinates = {
    (row["profile_ordinal"], row["identity_ordinal"])
    for row in rows
}

assert len(rows) == 512, len(rows)
assert actual_coordinates == expected_coordinates
for row in rows:
    assert row["profile_id"] == expected_profiles[row["profile_ordinal"]], row
    assert row["depth"] == "P1", row
    assert row["generation_attempt_ordinal"] == "0", row
    assert row["pattern_address"] == "23", row
    assert row["migration_status"] == "APPLIED", row
    assert row["bass_native_membership"] in {"NATIVE", "OUTSIDE_NATIVE_SET"}, row
    for field in (
        "bass_attack_mask",
        "planning_bass_onset_mask",
        "bass_native_candidate_mask",
        "rhythm_family",
        "selected_archetype",
        "selected_bass_rhythm",
    ):
        assert row[field] not in ("", "NOT_OBSERVED", "INVALID"), (field, row)

print("G4-C0R5 raw role-coherence corpus: 512 deterministic P1/attempt0 rows")
PY

python3 "${ROOT}/tools/gf2/g4_c0r5_role_coherence.py" \
  --input "${RAW}" \
  --rows-output "${DETAIL}" \
  --summary-output "${SUMMARY}"

python3 "${ROOT}/tools/gf2/g4_c0r5_role_coherence.py" \
  --input "${RAW}" \
  --rows-output "${DETAIL_REPLAY}" \
  --summary-output "${SUMMARY_REPLAY}" \
  > "${OUT_DIR}/analyzer-replay.log"

cmp "${DETAIL}" "${DETAIL_REPLAY}"
cmp "${SUMMARY}" "${SUMMARY_REPLAY}"
echo "G4-C0R5 analyzer replay: BYTE-IDENTICAL"

python3 - "${DETAIL}" "${SUMMARY}" <<'PY'
import csv
import sys
from pathlib import Path

detail_path = Path(sys.argv[1])
summary_path = Path(sys.argv[2])
with detail_path.open(newline="", encoding="utf-8") as handle:
    detail = list(csv.DictReader(handle, delimiter="\t"))
with summary_path.open(newline="", encoding="utf-8") as handle:
    summary = list(csv.DictReader(handle, delimiter="\t"))

assert len(detail) == 512, len(detail)
assert len(summary) == 4, len(summary)

allowed_topology = {
    "EXACT_MATCH",
    "BOTH_EMPTY",
    "PLANNING_EMPTY_AUDIBLE_NONEMPTY",
    "AUDIBLE_EMPTY_PLANNING_NONEMPTY",
    "DISJOINT_NONEMPTY",
    "PARTIAL_OVERLAP",
}
for row in detail:
    assert row["planning_vs_audible"] in allowed_topology, row
    assert row["bass_native_membership"] in {"NATIVE", "OUTSIDE_NATIVE_SET"}, row

required_summary = {
    "profile_ordinal",
    "profile_id",
    "rows",
    "native_count",
    "outside_native_count",
    "exact_match_count",
    "partial_overlap_count",
    "disjoint_nonempty_count",
    "planning_empty_audible_nonempty_count",
    "audible_empty_planning_nonempty_count",
    "both_empty_count",
    "unique_planning_masks",
    "unique_audible_attack_masks",
    "unique_archetypes",
    "unique_rhythm_families",
}
assert required_summary <= set(summary[0]), sorted(required_summary - set(summary[0]))
for row in summary:
    assert int(row["rows"]) == 128, row
    assert int(row["native_count"]) + int(row["outside_native_count"]) == 128, row
    topology_total = sum(
        int(row[field])
        for field in (
            "exact_match_count",
            "partial_overlap_count",
            "disjoint_nonempty_count",
            "planning_empty_audible_nonempty_count",
            "audible_empty_planning_nonempty_count",
            "both_empty_count",
        )
    )
    assert topology_total == 128, row

print("G4-C0R5 full role-coherence census: analyzer contract PASS")
PY
