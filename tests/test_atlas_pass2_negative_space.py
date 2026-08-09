#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/atlas"))

import extract_atlas_pass2_negative_space as negative

assert negative.MIN_ACTIVE_STRUCTURAL_GROUPS == 5
assert negative.MIN_ABSENCE_FRACTION == 0.90

# A completely missing role is not evidence of protected space.
empty = {role: set() for role in range(8)}
rows = negative.compute_negative_space_rows({"test": [empty.copy() for _ in range(10)]})
assert rows == []

# Only active-role observations participate in the denominator. Five active
# groups are enough; five additional groups with the role absent must not
# increase support or manufacture evidence.
active_observations = []
for _ in range(5):
    observation = {role: set() for role in range(8)}
    observation[0] = {0, 4}
    active_observations.append(observation)
missing_role = [{role: set() for role in range(8)} for _ in range(5)]
rows = negative.compute_negative_space_rows({"test": active_observations + missing_role})
step_one = [row for row in rows if row["role"] == "Kick" and row["step"] == 1]
assert len(step_one) == 1
assert step_one[0]["active_structural_group_count"] == 5
assert step_one[0]["absence_fraction"] == 1.0

# A step used in one of five active groups has only 80% absence and must not
# cross the 90% candidate threshold.
active_observations[0][0].add(1)
rows = negative.compute_negative_space_rows({"test": active_observations})
assert not any(row["role"] == "Kick" and row["step"] == 1 for row in rows)

print("Atlas Pass 2 negative-space contracts: OK")
