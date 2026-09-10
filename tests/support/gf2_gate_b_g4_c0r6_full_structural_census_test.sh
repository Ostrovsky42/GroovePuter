#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-gate-b/g4-c0r6-structural-census"
BIN="${ROOT}/build/host-tests/gf2-gate-b/gf2_gate_b_dump_c0r3_gcc"
CORPUS="${BUILD_DIR}/G4_C0R6_STRUCTURAL_CORPUS.tsv"
SUMMARY="${BUILD_DIR}/G4_C0R6_STRUCTURAL_SUMMARY.tsv"
ANOMALIES="${BUILD_DIR}/G4_C0R6_STRUCTURAL_ANOMALIES.tsv"
mkdir -p "${BUILD_DIR}"

bash "${ROOT}/tests/support/build_gf2_gate_b_g4_c0r3_probe.sh"

# One production-backed corpus is enough here. C0R2/C0R3 already prove the
# deterministic coordinate/dump seam; replaying all 6144 rows again would add
# runtime cost without adding a new structural invariant. This checkpoint needs
# the richer C0R3 observer, so call its proven CLI directly instead of routing it
# through the older C0R2 corpus helper/flag.
python3 - "${BIN}" "${CORPUS}" <<'PY'
from __future__ import annotations

import csv
import subprocess
import sys
from pathlib import Path

binary = sys.argv[1]
output_path = Path(sys.argv[2])
profiles = (0, 9, 20, 27)
depths = ("P1", "P2", "P3")

fieldnames: list[str] | None = None
rows: list[dict[str, str]] = []
for profile in profiles:
    for identity in range(128):
        for attempt in range(4):
            for depth in depths:
                completed = subprocess.run(
                    [
                        binary,
                        "--g4-c0r3-dump",
                        str(profile),
                        str(identity),
                        str(attempt),
                        "23",
                        depth,
                    ],
                    check=True,
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                )
                reader = csv.DictReader(completed.stdout.splitlines(), delimiter="\t")
                current_fields = list(reader.fieldnames or [])
                observed = list(reader)
                if len(observed) != 1:
                    raise RuntimeError(
                        "C0R3 observer must emit exactly one row for "
                        f"profile={profile} identity={identity} attempt={attempt} depth={depth}; "
                        f"got {len(observed)}; stderr={completed.stderr!r}"
                    )
                if fieldnames is None:
                    fieldnames = current_fields
                elif current_fields != fieldnames:
                    raise RuntimeError(
                        "C0R3 observer header drift at "
                        f"profile={profile} identity={identity} attempt={attempt} depth={depth}"
                    )
                rows.append(observed[0])

if fieldnames is None:
    raise RuntimeError("C0R3 observer produced no rows")

with output_path.open("w", newline="", encoding="utf-8") as handle:
    writer = csv.DictWriter(
        handle,
        fieldnames=fieldnames,
        delimiter="\t",
        lineterminator="\n",
    )
    writer.writeheader()
    writer.writerows(rows)

print(f"G4-C0R6 C0R3 corpus materialized: rows={len(rows)}")
PY

python3 - "${CORPUS}" "${SUMMARY}" "${ANOMALIES}" "${ROOT}" <<'PY'
from __future__ import annotations

import csv
import itertools
import sys
from collections import defaultdict
from dataclasses import asdict
from pathlib import Path

corpus_path = Path(sys.argv[1])
summary_path = Path(sys.argv[2])
anomalies_path = Path(sys.argv[3])
root = Path(sys.argv[4])
sys.path.insert(0, str(root))

from tools.gf2 import g4_c0r6_structural_separation as c0r6

EXPECTED_PROFILES = {
    "0": "Acid/BASE",
    "9": "Dub/Reggae/Dub Techno",
    "20": "House/BASE",
    "27": "Drum&Bass/BASE",
}
DEPTHS = ("P1", "P2", "P3")
REQUIRED_FIELDS = {
    "profile_ordinal",
    "profile_id",
    "depth",
    "identity_ordinal",
    "generation_attempt_ordinal",
    "pattern_address",
    "migration_status",
    "selected_archetype",
    "selected_bass_rhythm",
    "selected_chord_rhythm",
    "selected_melodic_rhythm",
    "selected_motif_shape",
    "selected_progression",
    "synth_b_role",
    "archetype_axis_status",
    "bass_axis_status",
    "chord_axis_status",
    "melodic_axis_status",
    "motif_axis_status",
    "progression_axis_status",
    "bass_attack_mask",
    "bass_continuation_mask",
    "secondary_attack_mask",
    "secondary_continuation_mask",
    "secondary_topology_role",
    "rhythm_family",
}

with corpus_path.open(newline="", encoding="utf-8") as handle:
    reader = csv.DictReader(handle, delimiter="\t")
    if reader.fieldnames is None:
        raise AssertionError("C0R6 corpus has no header")
    missing = REQUIRED_FIELDS - set(reader.fieldnames)
    assert not missing, sorted(missing)
    rows = list(reader)

expected_coordinates = {
    (profile, str(identity), str(attempt), depth)
    for profile, identity, attempt, depth in itertools.product(
        EXPECTED_PROFILES, range(128), range(4), DEPTHS
    )
}
actual_coordinates = {
    (
        row["profile_ordinal"],
        row["identity_ordinal"],
        row["generation_attempt_ordinal"],
        row["depth"],
    )
    for row in rows
}

assert len(rows) == 6144, len(rows)
assert len(actual_coordinates) == 6144, len(actual_coordinates)
assert actual_coordinates == expected_coordinates

for row in rows:
    profile = row["profile_ordinal"]
    assert row["profile_id"] == EXPECTED_PROFILES[profile], row
    assert row["pattern_address"] == "23", row
    assert row["migration_status"] == "APPLIED", row
    # These checks protect the observer protocol only. They deliberately do not
    # require any specific musical value, mask density, diversity, or genre rule.
    for field in REQUIRED_FIELDS:
        assert row[field] != "", (field, row)

scopes: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
for row in rows:
    scopes[(row["profile_ordinal"], row["depth"])].append(row)

summary_rows: list[dict[str, object]] = []
anomaly_rows: list[dict[str, object]] = []

for profile in EXPECTED_PROFILES:
    for depth in DEPTHS:
        scope = scopes[(profile, depth)]
        assert len(scope) == 512, (profile, depth, len(scope))

        by_identity: dict[int, list[dict[str, str]]] = defaultdict(list)
        for row in scope:
            by_identity[int(row["identity_ordinal"])].append(row)
        assert len(by_identity) == 128, (profile, depth, len(by_identity))
        for identity, group in by_identity.items():
            assert {row["generation_attempt_ordinal"] for row in group} == {
                "0", "1", "2", "3"
            }, (profile, depth, identity)
            if c0r6.analyze_group(group).selection_drift:
                anomaly_rows.append({
                    "profile_ordinal": profile,
                    "profile_id": EXPECTED_PROFILES[profile],
                    "depth": depth,
                    "kind": "SELECTION_DRIFT",
                    "identities": str(identity),
                    "group_size": 1,
                })

        summary = c0r6.summarize_scope(scope)
        summary_rows.append({
            "profile_ordinal": profile,
            "profile_id": EXPECTED_PROFILES[profile],
            "depth": depth,
            **asdict(summary),
        })

        for collision in c0r6.structural_collision_groups(scope):
            anomaly_rows.append({
                "profile_ordinal": profile,
                "profile_id": EXPECTED_PROFILES[profile],
                "depth": depth,
                "kind": "STRUCTURAL_COLLISION",
                "identities": ",".join(map(str, collision)),
                "group_size": len(collision),
            })
        for collision in c0r6.topology_collision_groups(scope):
            anomaly_rows.append({
                "profile_ordinal": profile,
                "profile_id": EXPECTED_PROFILES[profile],
                "depth": depth,
                "kind": "CROSS_SELECTION_TOPOLOGY_CONVERGENCE",
                "identities": ",".join(map(str, collision)),
                "group_size": len(collision),
            })

assert len(summary_rows) == 12, len(summary_rows)

summary_fields = list(summary_rows[0])
with summary_path.open("w", newline="", encoding="utf-8") as handle:
    writer = csv.DictWriter(
        handle, fieldnames=summary_fields, delimiter="\t", lineterminator="\n"
    )
    writer.writeheader()
    writer.writerows(summary_rows)

anomaly_fields = (
    "profile_ordinal",
    "profile_id",
    "depth",
    "kind",
    "identities",
    "group_size",
)
with anomalies_path.open("w", newline="", encoding="utf-8") as handle:
    writer = csv.DictWriter(
        handle, fieldnames=anomaly_fields, delimiter="\t", lineterminator="\n"
    )
    writer.writeheader()
    writer.writerows(anomaly_rows)

print("G4-C0R6 full structural census: observer contract PASS")
print("rows=6144 scopes=12 identities_per_scope=128 attempts_per_identity=4")
for row in summary_rows:
    print(
        "SCOPE",
        row["profile_id"],
        row["depth"],
        f"stable={row['stable_identity_count']}/128",
        f"drift={row['selection_drift_identities']}",
        f"attempt_changes={row['attempt_changes_topology_identities']}",
        f"static={row['topology_static_identities']}",
        f"selection_space={row['unique_active_selection_signatures']}",
        f"take_spaces={row['unique_take_spaces']}",
        "takes="
        f"1:{row['identities_with_1_unique_take']},"
        f"2:{row['identities_with_2_unique_takes']},"
        f"3:{row['identities_with_3_unique_takes']},"
        f"4:{row['identities_with_4_unique_takes']}",
        f"structural_collision={row['structural_collision_groups']}/"
        f"{row['identities_in_structural_collision']}",
        f"cross_selection_convergence={row['topology_collision_groups']}/"
        f"{row['identities_in_topology_collision']}",
    )
print(f"anomaly_rows={len(anomaly_rows)}")
PY
