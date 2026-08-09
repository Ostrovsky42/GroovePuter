#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import sys
import tempfile
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "tools/atlas/extract_atlas_pass2.py"
sys.path.insert(0, str(ROOT / "tools/atlas"))

spec = importlib.util.spec_from_file_location("extract_atlas_pass2", MODULE_PATH)
module = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(module)

import extract_atlas_pass2_negative_space as negative

assert module.EXPECTED_ATLAS_SHA256 == "5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd"
assert module.EXPECTED_SCHEMA_VERSION == "2.6.0"

assert module.map_track({"track_id": "KICK", "track_role": "DRUM_OR_PERCUSSION"}) == 0
assert module.map_track({"track_id": "SNARE", "track_role": "DRUM_OR_PERCUSSION"}) == 1
assert module.map_track({"track_id": "HAT1", "track_role": "drums"}) == 2
assert module.map_track({"track_id": "HAT2", "track_role": "drums"}) == 3
assert module.map_track({"track_id": "COWBELL", "track_role": "DRUM_OR_PERCUSSION"}) == 4
assert module.map_track({"track_id": "SYNTH1", "track_role": "bass"}) == 5
assert module.map_track({"track_id": "SYNTH2", "track_role": "harmony"}) == 6
assert module.map_track({"track_id": "DX", "track_role": "melody"}) == 7
assert module.map_track({"track_id": "SAMPLER", "track_role": "sample"}) is None

tracks = defaultdict(list)
tracks["P"].append({"track_id": "KICK", "track_role": "DRUM_OR_PERCUSSION"})
events = defaultdict(list)
events[("P", "KICK")] = [
    {"bar_index": "1", "step_index": "1"},
    {"bar_index": "1", "step_index": "8"},
    {"bar_index": "2", "step_index": "2"},
]
masks = module.pattern_masks("P", 2, 8, tracks, events)
assert masks[0][0] == {0, 14}
assert masks[1][0] == {2}

a = {role: set() for role in range(8)}
b = {role: set() for role in range(8)}
a[0] = {0, 4, 8, 12}
b[0] = {0, 4, 8, 12}
assert module.observation_distance(a, b) == 0.0
b[0] = {1, 5, 9, 13}
assert module.observation_distance(a, b) > 0.0

lane = {"required": {0, 8}, "support": {0, 4, 8, 12}, "min": 2, "max": 4}
assert module.role_compatible({0, 8}, lane)
assert module.role_compatible({0, 4, 8}, lane)
assert not module.role_compatible({0}, lane)
assert not module.role_compatible({0, 8, 15}, lane)

relation = module.relation_features({0, 4, 8, 12}, {1, 5, 9, 13})
assert relation is not None
assert relation["coincide_fraction"] == 0.0
assert relation["target_in_gaps_fraction"] == 1.0
assert relation["respond_1_3_fraction"] == 1.0

bar_a = {role: set() for role in range(8)}
bar_b = {role: set() for role in range(8)}
bar_a[0] = {0, 4}
bar_b[0] = {0, 4, 12}
assert module.transition_features(bar_a, bar_b)["transition_class"] == "ADD_ONLY"
bar_b[0] = {0}
assert module.transition_features(bar_a, bar_b)["transition_class"] == "DROP_ONLY"

assert module.contour_class([60, 60, 60]) == "STATIC"
assert module.contour_class([60, 62, 64]) == "RISE"
assert module.contour_class([64, 62, 60]) == "FALL"
assert module.contour_class([60, 64, 62]) == "ARCH"
assert module.contour_class([64, 60, 62]) == "VALLEY"

with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp) / "bad.csv"
    try:
        module.write_csv(path, [{"pattern_id": "P"}], ["pattern_id"])
    except ValueError:
        pass
    else:
        raise AssertionError("pattern_id output column must be rejected")

# Missing roles are not negative-space evidence. Only observations where a
# role is active somewhere are allowed into that role's denominator.
assert negative.MIN_ACTIVE_STRUCTURAL_GROUPS == 5
assert negative.MIN_ABSENCE_FRACTION == 0.90
empty = {role: set() for role in range(8)}
assert negative.compute_negative_space_rows({"test": [empty.copy() for _ in range(10)]}) == []

active = []
for _ in range(5):
    observation = {role: set() for role in range(8)}
    observation[0] = {0, 4}
    active.append(observation)
missing_role = [{role: set() for role in range(8)} for _ in range(5)]
negative_rows = negative.compute_negative_space_rows({"test": active + missing_role})
step_one = [row for row in negative_rows if row["role"] == "Kick" and row["step"] == 1]
assert len(step_one) == 1
assert step_one[0]["active_structural_group_count"] == 5
assert step_one[0]["absence_fraction"] == 1.0

active[0][0].add(1)
negative_rows = negative.compute_negative_space_rows({"test": active})
assert not any(row["role"] == "Kick" and row["step"] == 1 for row in negative_rows)

print("Atlas Pass 2 extractor unit contracts: OK")
