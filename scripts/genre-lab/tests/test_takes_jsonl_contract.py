"""Contract test for gen_probe --emit-takes-jsonl rows (docs/CONTRACT.md).

The fixture has the shape the probe writes: contract keys plus provenance keys
(recipe_id, mode, p_level, pattern_address, build_sha, timing) at the top level.
gplab must parse it and ignore the provenance keys.
"""
import json
from pathlib import Path

import pytest

from gplab.schema import ROLES, read_bars

FIXTURE = Path(__file__).parent / "fixtures" / "takes_emit_sample.jsonl"


def test_emitted_rows_parse_and_roles_are_contract_roles():
    bars = list(read_bars(FIXTURE))
    assert len(bars) == 2
    for i, bar in enumerate(bars):
        assert bar.variant == "house.base"
        assert bar.take == 0
        assert bar.bar_idx == i
        assert set(bar.events) <= set(ROLES)


def test_provenance_keys_are_present_and_not_part_of_events():
    rows = [json.loads(line) for line in FIXTURE.read_text(encoding="utf-8").splitlines()]
    for row in rows:
        assert row["timing"] == "grid"
        assert row["build_sha"]
        assert row["recipe_id"] == 0
        assert set(row["events"]) <= set(ROLES)


def test_unknown_role_is_still_rejected(tmp_path):
    bad = tmp_path / "bad.jsonl"
    row = json.loads(FIXTURE.read_text(encoding="utf-8").splitlines()[0])
    row["events"]["ride"] = [[0, 0.5]]
    bad.write_text(json.dumps(row) + "\n", encoding="utf-8")
    with pytest.raises(ValueError, match="unknown roles"):
        list(read_bars(bad))
