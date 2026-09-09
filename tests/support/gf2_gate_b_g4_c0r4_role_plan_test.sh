#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-gate-b"
BIN="${BUILD_DIR}/gf2_gate_b_dump_c0r3_gcc"
OUT="${BUILD_DIR}/g4-c0r4-role-plan.tsv"

# C0R3 owns the pre-adapter probe build. C0R4 deliberately reuses the same
# production-backed binary instead of introducing another materialization path.
bash "${ROOT}/tests/support/build_gf2_gate_b_g4_c0r3_probe.sh"

: > "${OUT}"
first=1
for profile in 0 9 20 27; do
  for identity in 7 19; do
    tmp="${BUILD_DIR}/g4-c0r4-${profile}-${identity}.tsv"
    "${BIN}" --g4-c0r3-dump "${profile}" "${identity}" 0 23 P1 > "${tmp}"
    if [[ "${first}" -eq 1 ]]; then
      cat "${tmp}" >> "${OUT}"
      first=0
    else
      tail -n +2 "${tmp}" >> "${OUT}"
    fi
  done
done

python3 - "${OUT}" <<'PY'
import csv
import sys
from pathlib import Path

path = Path(sys.argv[1])
with path.open(newline="", encoding="utf-8") as handle:
    rows = list(csv.DictReader(handle, delimiter="\t"))

assert len(rows) == 8, len(rows)
required = {
    "rhythm_family",
    "bass_native_candidate_mask",
    "bass_native_membership",
    "planning_bass_onset_mask",
    "planning_bass_structural_mask",
    "planning_bass_secondary_mask",
    "planning_bass_ghost_mask",
}
assert required <= set(rows[0]), (
    "G4-C0R4 missing role-plan/native-vocabulary evidence",
    sorted(required - set(rows[0])),
)

allowed_membership = {"NATIVE", "OUTSIDE_NATIVE_SET"}
for row in rows:
    assert row["migration_status"] == "APPLIED", row
    assert row["rhythm_family"] not in ("", "NOT_OBSERVED", "INVALID"), row
    assert row["bass_native_membership"] in allowed_membership, row
    native = int(row["bass_native_candidate_mask"], 16)
    planning = int(row["planning_bass_onset_mask"], 16)
    structural = int(row["planning_bass_structural_mask"], 16)
    secondary = int(row["planning_bass_secondary_mask"], 16)
    ghosts = int(row["planning_bass_ghost_mask"], 16)
    assert native != 0, row
    assert planning == (structural | secondary | ghosts), row
    assert (structural & secondary) == 0, row
    assert (structural & ghosts) == 0, row
    assert (secondary & ghosts) == 0, row

print("G4-C0R4 native bass vocabulary: production candidate set observed")
print("G4-C0R4 planning bass: realized role-plan masks observed separately from audible bass")
PY
