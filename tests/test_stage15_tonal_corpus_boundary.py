#!/usr/bin/env python3
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LEGACY = ROOT / "tests/data/stage15_tonal_legacy_baseline.tsv"
PRE_F13 = ROOT / "tests/data/stage15_tonal_enabled_pre_f13_baseline.tsv"


def load(path: Path):
    with path.open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    assert len(rows) == 256, f"{path.name}: expected 256 corpus rows, got {len(rows)}"
    keyed = {(row["mode"], row["ordinal"], row["voice"]): row for row in rows}
    assert len(keyed) == 256, f"{path.name}: duplicate corpus key"
    return keyed


legacy = load(LEGACY)
pre_f13 = load(PRE_F13)
assert legacy.keys() == pre_f13.keys(), "legacy/PRE-F13 tonal corpus keys diverged"

pitch_changes = 0
for key in sorted(legacy):
    before = legacy[key]
    after = pre_f13[key]
    for field in ("status", "secondary_role", "topology", "articulation"):
        assert before[field] == after[field], f"{key}: PRE-F13 tonal path changed {field}"
    if before["pitch"] != after["pitch"]:
        pitch_changes += 1

assert pitch_changes > 0, "PRE-F13 tonal-enabled corpus never changes pitch"
print(
    "Stage 15 PRE-F13 tonal corpus boundary: OK "
    f"(256 rows, pitch changed in {pitch_changes}, topology/articulation unchanged)"
)
