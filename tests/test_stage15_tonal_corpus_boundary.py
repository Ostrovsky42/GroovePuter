#!/usr/bin/env python3
import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LEGACY = ROOT / "tests/data/stage15_tonal_legacy_baseline.tsv"
TONAL = ROOT / "tests/data/stage15_tonal_enabled_baseline.tsv"


def load(path: Path):
    with path.open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    assert len(rows) == 256, f"{path.name}: expected 256 corpus rows, got {len(rows)}"
    keyed = {(row["mode"], row["ordinal"], row["voice"]): row for row in rows}
    assert len(keyed) == 256, f"{path.name}: duplicate corpus key"
    return keyed


legacy = load(LEGACY)
tonal = load(TONAL)
assert legacy.keys() == tonal.keys(), "legacy/tonal corpus keys diverged"

pitch_changes = 0
for key in sorted(legacy):
    before = legacy[key]
    after = tonal[key]
    for field in ("status", "secondary_role", "topology", "articulation"):
        assert before[field] == after[field], f"{key}: tonal path changed {field}"
    if before["pitch"] != after["pitch"]:
        pitch_changes += 1

assert pitch_changes > 0, "tonal-enabled corpus never changes pitch"
print(
    "Stage 15 tonal corpus boundary: OK "
    f"(256 rows, pitch changed in {pitch_changes}, topology/articulation unchanged)"
)
