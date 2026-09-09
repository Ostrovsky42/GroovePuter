#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${ROOT}/build/host-tests/gf2-gate-b"
BIN="${BUILD_DIR}/gf2_gate_b_dump_c0r3_gcc"
OUT="${BUILD_DIR}/g4-c0r3-semantic-topology.tsv"
REPLAY="${BUILD_DIR}/g4-c0r3-semantic-topology-replay.tsv"

bash "${ROOT}/tests/support/build_gf2_gate_b_g4_c0r3_probe.sh"

if [[ ! -x "${BIN}" ]]; then
  echo "G4-C0R3 semantic pre-adapter probe binary was not built" >&2
  exit 1
fi

: > "${OUT}"
: > "${REPLAY}"

run_probe() {
  local output="$1"
  local first=1
  # Production enumerateProfiles() ordinals proven by C0R2:
  # Acid BASE=0, Dub Techno=9, House BASE=20, DnB BASE=27.
  for profile in 0 9 20 27; do
    for identity in 7 19; do
      tmp="${BUILD_DIR}/g4-c0r3-${profile}-${identity}.tsv"
      "${BIN}" --g4-c0r3-dump "${profile}" "${identity}" 0 23 P1 > "${tmp}"
      if [[ "${first}" -eq 1 ]]; then
        cat "${tmp}" >> "${output}"
        first=0
      else
        tail -n +2 "${tmp}" >> "${output}"
      fi
    done
  done
}

run_probe "${OUT}"
run_probe "${REPLAY}"
cmp "${OUT}" "${REPLAY}"

python3 - "${OUT}" <<'PY'
import csv
import sys
from pathlib import Path

path = Path(sys.argv[1])
with path.open(newline="", encoding="utf-8") as handle:
    rows = list(csv.DictReader(handle, delimiter="\t"))

assert len(rows) == 8, len(rows)

required = {
    "profile_ordinal",
    "profile_id",
    "depth",
    "identity_ordinal",
    "generation_attempt_ordinal",
    "pattern_address",
    "migration_status",
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
    "phrase_law_axis_status",
    "bass_attack_mask",
    "bass_continuation_mask",
    "secondary_attack_mask",
    "secondary_continuation_mask",
    "secondary_topology_role",
}
assert rows, "empty C0R3 observation"
assert required <= set(rows[0]), required - set(rows[0])

allowed_axis = {
    "ACTIVE",
    "INACTIVE_BY_PHYSICAL_ROLE",
    "PLANNING_ONLY",
    "AUDIBILITY_UNPROVEN",
}

expected_profiles = {
    "0": ("Acid/BASE", "MELODIC"),
    "9": ("Dub/Reggae/Dub Techno", "CHORD"),
    "20": ("House/BASE", "MELODIC"),
    "27": ("Drum&Bass/BASE", "MELODIC"),
}

def mask(value: str) -> int:
    assert value.startswith("0x"), value
    return int(value, 16)

for row in rows:
    profile_id, secondary_role = expected_profiles[row["profile_ordinal"]]
    assert row["profile_id"] == profile_id
    assert row["depth"] == "P1"
    assert row["generation_attempt_ordinal"] == "0"
    assert row["pattern_address"] == "23"
    assert row["migration_status"] == "APPLIED"

    for field in (
        "archetype_axis_status",
        "bass_axis_status",
        "chord_axis_status",
        "melodic_axis_status",
        "motif_axis_status",
        "progression_axis_status",
        "phrase_law_axis_status",
    ):
        assert row[field] in allowed_axis, (field, row[field])

    # C0R3 observes one physical bar, so phrase-law selection is planning
    # metadata here rather than evidence of multi-bar execution.
    assert row["phrase_law_axis_status"] == "PLANNING_ONLY", row
    assert row["archetype_axis_status"] == "ACTIVE", row
    assert row["bass_axis_status"] == "ACTIVE", row
    assert row["progression_axis_status"] == "ACTIVE", row

    bass_attacks = mask(row["bass_attack_mask"])
    bass_continuations = mask(row["bass_continuation_mask"])
    secondary_attacks = mask(row["secondary_attack_mask"])
    secondary_continuations = mask(row["secondary_continuation_mask"])
    assert (bass_attacks & bass_continuations) == 0, row
    assert (secondary_attacks & secondary_continuations) == 0, row

    assert row["synth_b_role"] == secondary_role, row
    assert row["secondary_topology_role"] == secondary_role, row
    if secondary_role == "MELODIC":
        assert row["chord_axis_status"] == "INACTIVE_BY_PHYSICAL_ROLE", row
        assert row["melodic_axis_status"] == "ACTIVE", row
        # The current tonal adapter explicitly ignores motif/source ordering.
        # Do not call motif ACTIVE until a separate causal proof exists.
        assert row["motif_axis_status"] == "AUDIBILITY_UNPROVEN", row
    else:
        assert row["chord_axis_status"] == "ACTIVE", row
        assert row["melodic_axis_status"] == "INACTIVE_BY_PHYSICAL_ROLE", row
        assert row["motif_axis_status"] == "INACTIVE_BY_PHYSICAL_ROLE", row

print("G4-C0R3 active-axis projection: profile physical roles are explicit")
print("G4-C0R3 semantic topology: ATTACK and CONTINUATION are separate pre-adapter masks")
print("G4-C0R3 replay: BYTE-IDENTICAL")
PY
