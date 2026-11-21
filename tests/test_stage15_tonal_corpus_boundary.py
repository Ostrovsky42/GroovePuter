#!/usr/bin/env python3
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LEGACY = ROOT / "tests/data/stage15_tonal_legacy_baseline.tsv"
CURRENT = ROOT / "tests/data/stage15_tonal_enabled_baseline.tsv"


def load(path: Path):
    with path.open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    assert len(rows) == 256, f"{path.name}: expected 256 corpus rows, got {len(rows)}"
    keyed = {(row["mode"], row["ordinal"], row["voice"]): row for row in rows}
    assert len(keyed) == 256, f"{path.name}: duplicate corpus key"
    return keyed


legacy = load(LEGACY)
current = load(CURRENT)
assert legacy.keys() == current.keys(), "current legacy/tonal corpus keys diverged"

pitch_changes = 0
articulation_changes = 0
for key in sorted(legacy):
    before = legacy[key]
    after = current[key]
    for field in ("status", "secondary_role", "topology"):
        assert before[field] == after[field], f"{key}: tonal path changed {field}"
    if before["pitch"] != after["pitch"]:
        pitch_changes += 1
    if before["articulation"] != after["articulation"]:
        articulation_changes += 1

assert pitch_changes > 0, "current tonal-enabled corpus never changes pitch"
assert articulation_changes > 0, "current tonal-enabled corpus never changes articulation"
print(
    "Stage 15 current legacy/tonal corpus boundary: OK "
    f"(256 rows, pitch changed in {pitch_changes}, "
    f"articulation changed in {articulation_changes}, topology unchanged)"
)
