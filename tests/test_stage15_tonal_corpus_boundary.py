#!/usr/bin/env python3
import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LEGACY = ROOT / "tests/data/stage15_tonal_legacy_baseline.tsv"
HISTORIC_TONAL = ROOT / "tests/data/stage15_tonal_enabled_baseline.tsv"
CURRENT_TONAL = Path(sys.argv[1]) if len(sys.argv) > 1 else HISTORIC_TONAL


def load(path: Path):
    with path.open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    assert len(rows) == 256, f"{path.name}: expected 256 corpus rows, got {len(rows)}"
    keyed = {(row["mode"], row["ordinal"], row["voice"]): row for row in rows}
    assert len(keyed) == 256, f"{path.name}: duplicate corpus key"
    return keyed


legacy = load(LEGACY)
historic_tonal = load(HISTORIC_TONAL)
current_tonal = load(CURRENT_TONAL)
assert legacy.keys() == current_tonal.keys(), "legacy/current tonal corpus keys diverged"
assert historic_tonal.keys() == current_tonal.keys(), "historic/current tonal corpus keys diverged"

pitch_changes_from_legacy = 0
pitch_changes_from_stage15 = 0
for key in sorted(legacy):
    before = legacy[key]
    historic = historic_tonal[key]
    current = current_tonal[key]
    for field in ("status", "secondary_role", "topology", "articulation"):
        assert before[field] == current[field], f"{key}: current tonal path changed {field}"
    if before["pitch"] != current["pitch"]:
        pitch_changes_from_legacy += 1
    if historic["pitch"] != current["pitch"]:
        pitch_changes_from_stage15 += 1

assert pitch_changes_from_legacy > 0, "current tonal corpus never changes pitch"
if CURRENT_TONAL != HISTORIC_TONAL:
    assert pitch_changes_from_stage15 > 0, "harmonic-rhythm integration never changes Stage15 pitch fingerprints"

print(
    "Stage 15 tonal corpus boundary: OK "
    f"(256 rows, pitch changed vs legacy in {pitch_changes_from_legacy}, "
    f"vs frozen Stage15 in {pitch_changes_from_stage15}; "
    "status/topology/articulation unchanged)"
)
