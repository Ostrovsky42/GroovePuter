#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-gate-b"
BIN="${BUILD_DIR}/gf2_gate_b_dump_gcc"
TOOL="${ROOT}/tools/gf2/g4_c0r_corpus.py"
OUT1="${BUILD_DIR}/g4-c0r-corpus-small.tsv"
OUT2="${BUILD_DIR}/g4-c0r-corpus-small-replay.tsv"

if [[ ! -x "${BIN}" ]]; then
  echo "G4-C0R corpus test requires the Gate B dump binary from the C0R preflight" >&2
  exit 2
fi

python3 "${TOOL}" \
  --dump-binary "${BIN}" \
  --profiles 0,21 \
  --identity-start 7 \
  --identity-count 3 \
  --attempt-count 2 \
  --pattern-address 23 \
  --output "${OUT1}"

python3 "${TOOL}" \
  --dump-binary "${BIN}" \
  --profiles 0,21 \
  --identity-start 7 \
  --identity-count 3 \
  --attempt-count 2 \
  --pattern-address 23 \
  --output "${OUT2}"

cmp "${OUT1}" "${OUT2}"

python3 - "${OUT1}" <<'PY'
import csv
import itertools
import sys
from collections import defaultdict
from pathlib import Path

path = Path(sys.argv[1])
with path.open(newline="", encoding="utf-8") as handle:
    rows = list(csv.DictReader(handle, delimiter="\t"))

expected_count = 2 * 3 * 2 * 3
assert len(rows) == expected_count, (len(rows), expected_count)

required = {
    "profile_ordinal",
    "profile_id",
    "depth",
    "identity_ordinal",
    "generation_attempt_ordinal",
    "pattern_address",
    "selection_status",
    "migration_status",
    "v0r_archetype",
    "declared_phrase_law",
    "density_min",
    "density_max",
    "resolved_density",
    "resolved_feel",
    "realization_seed",
    "effective_material_fingerprint",
}
assert rows, "empty corpus"
assert required <= set(rows[0]), required - set(rows[0])

expected_coordinates = {
    (str(profile), str(identity), str(attempt), depth)
    for profile, identity, attempt, depth in itertools.product(
        (0, 21), range(7, 10), range(2), ("P1", "P2", "P3")
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
assert actual_coordinates == expected_coordinates, (
    expected_coordinates - actual_coordinates,
    actual_coordinates - expected_coordinates,
)

identity_fields = (
    "profile_id",
    "depth",
    "selection_status",
    "v0r_archetype",
    "declared_phrase_law",
    "density_min",
    "density_max",
    "resolved_density",
    "resolved_feel",
)

groups = defaultdict(list)
for row in rows:
    assert row["pattern_address"] == "23"
    assert row["selection_status"] == "APPLIED"
    assert row["migration_status"] == "APPLIED"
    assert row["realization_seed"] not in ("", "NOT_OBSERVED")
    assert row["effective_material_fingerprint"] not in ("", "NOT_OBSERVED")
    key = (row["profile_ordinal"], row["identity_ordinal"], row["depth"])
    groups[key].append(row)

assert len(groups) == 2 * 3 * 3
for key, group in groups.items():
    assert {row["generation_attempt_ordinal"] for row in group} == {"0", "1"}, key
    first = group[0]
    for field in identity_fields:
        assert len({row[field] for row in group}) == 1, (
            "attempt changed identity-owned field",
            key,
            field,
            [(row["generation_attempt_ordinal"], row[field]) for row in group],
        )
    assert len({row["realization_seed"] for row in group}) == 2, (
        "attempt coordinate was ignored",
        key,
    )

print(f"G4-C0R2 small corpus: {len(rows)} deterministic rows")
print("G4-C0R2 corpus coordinates: complete Cartesian product; attempt preserves identity fields")
PY
