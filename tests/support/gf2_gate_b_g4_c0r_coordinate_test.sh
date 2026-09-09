#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-gate-b"
BIN="${BUILD_DIR}/gf2_gate_b_dump_gcc"

if [[ ! -x "${BIN}" ]]; then
  echo "G4-C0R coordinate test requires the Gate B dump binary from the C0R preflight" >&2
  exit 2
fi

OUT0="${BUILD_DIR}/g4-c0r-identity7-attempt0-address23.tsv"
OUT3="${BUILD_DIR}/g4-c0r-identity7-attempt3-address23.tsv"
OUT_ADDR24="${BUILD_DIR}/g4-c0r-identity7-attempt0-address24.tsv"

"${BIN}" --g4-c0r-dump 0 7 0 23 P1 > "${OUT0}"
"${BIN}" --g4-c0r-dump 0 7 3 23 P1 > "${OUT3}"
"${BIN}" --g4-c0r-dump 0 7 0 24 P1 > "${OUT_ADDR24}"

python3 - "${OUT0}" "${OUT3}" "${OUT_ADDR24}" <<'PY'
import csv
import sys
from pathlib import Path


def load(path: str) -> dict[str, str]:
    with Path(path).open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    assert len(rows) == 1, (path, len(rows))
    return rows[0]


attempt0 = load(sys.argv[1])
attempt3 = load(sys.argv[2])
address24 = load(sys.argv[3])

required = {
    "identity_ordinal",
    "generation_attempt_ordinal",
    "pattern_address",
    "profile_ordinal",
    "profile_id",
    "depth",
    "selection_status",
    "migration_status",
    "v0r_attempt",
    "v0r_requested_mode",
    "v0r_requested_recipe",
    "v0r_level",
    "v0r_migration_route",
    "v0r_archetype",
    "declared_phrase_law",
    "requested_bars",
    "density_min",
    "density_max",
    "resolved_density",
    "resolved_feel",
    "realization_seed",
}
for row in (attempt0, attempt3, address24):
    assert required <= set(row), required - set(row)

assert attempt0["profile_ordinal"] == attempt3["profile_ordinal"] == address24["profile_ordinal"] == "0"
assert attempt0["identity_ordinal"] == attempt3["identity_ordinal"] == address24["identity_ordinal"] == "7"
assert attempt0["generation_attempt_ordinal"] == "0"
assert attempt3["generation_attempt_ordinal"] == "3"
assert address24["generation_attempt_ordinal"] == "0"
assert attempt0["v0r_attempt"] == "0"
assert attempt3["v0r_attempt"] == "3"
assert address24["v0r_attempt"] == "0"

# Attempt is a real realization coordinate, not an ignored label.
assert attempt0["realization_seed"] != attempt3["realization_seed"], (
    "generation attempt did not change realization seed",
    attempt0["realization_seed"],
    attempt3["realization_seed"],
)

# Attempt variation must not silently move the storage/destination coordinate.
assert attempt0["pattern_address"] == attempt3["pattern_address"] == "23"

# Storage address is independently supplied: changing it does not redefine the
# musical identity or realization attempt.
assert address24["pattern_address"] == "24"
assert address24["realization_seed"] == attempt0["realization_seed"]

selection_fields = (
    "profile_id",
    "depth",
    "selection_status",
    "v0r_requested_mode",
    "v0r_requested_recipe",
    "v0r_level",
    "v0r_migration_route",
    "v0r_archetype",
    "declared_phrase_law",
    "requested_bars",
    "density_min",
    "density_max",
    "resolved_density",
    "resolved_feel",
)
for field in selection_fields:
    assert attempt0[field] == attempt3[field], (
        "attempt changed identity-level selection",
        field,
        attempt0[field],
        attempt3[field],
    )
    assert attempt0[field] == address24[field], (
        "storage address changed identity-level selection",
        field,
        attempt0[field],
        address24[field],
    )

print("G4-C0R coordinates: identity=7 attempt=0/3 selection-invariant; attempt seed varies")
print("G4-C0R coordinates: address=23/24 selection-invariant; storage remains independent")
PY
