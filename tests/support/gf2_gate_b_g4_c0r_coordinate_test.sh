#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-gate-b"
BIN="${BUILD_DIR}/gf2_gate_b_dump_gcc"

if [[ ! -x "${BIN}" ]]; then
  echo "G4-C0R coordinate test requires the Gate B dump binary from run_gf2_gate_b_tests.sh" >&2
  exit 2
fi

OUT0="${BUILD_DIR}/g4-c0r-identity7-attempt0.tsv"
OUT3="${BUILD_DIR}/g4-c0r-identity7-attempt3.tsv"

"${BIN}" --g4-c0r-dump 0 7 0 P1 > "${OUT0}"
"${BIN}" --g4-c0r-dump 0 7 3 P1 > "${OUT3}"

python3 - "${OUT0}" "${OUT3}" <<'PY'
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
}
assert required <= set(attempt0), required - set(attempt0)
assert required <= set(attempt3), required - set(attempt3)

assert attempt0["profile_ordinal"] == attempt3["profile_ordinal"] == "0"
assert attempt0["identity_ordinal"] == attempt3["identity_ordinal"] == "7"
assert attempt0["generation_attempt_ordinal"] == "0"
assert attempt3["generation_attempt_ordinal"] == "3"
assert attempt0["v0r_attempt"] == "0"
assert attempt3["v0r_attempt"] == "3"

# A TAKE/generation attempt may vary realization, but it must not silently move
# the observation to another physical identity/destination coordinate.
assert attempt0["pattern_address"] == attempt3["pattern_address"]

# These fields are selected/frozen from the idea identity and therefore must be
# invariant across repeated generation attempts of that same identity.
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

print("G4-C0R coordinate separation: identity=7 attempt=0/3 selection-invariant")
PY
