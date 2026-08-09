#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/architecture/atlas_pass2"

expected_files = {
    "ATLAS_PASS2_SUMMARY.json",
    "ATLAS_PASS2_TOPOLOGY_CANDIDATES.csv",
    "ATLAS_PASS2_DISTANCE_DISTRIBUTIONS.csv",
    "ATLAS_PASS2_RELATIONSHIPS.csv",
    "ATLAS_PASS2_PHRASE_TRANSITIONS.csv",
    "ATLAS_PASS2_PITCH_CONTOURS.csv",
    "ATLAS_PASS2_EVIDENCE_COVERAGE.csv",
    "ATLAS_PASS2_EFFECTIVE_VARIATION_BASELINE.csv",
}
assert expected_files.issubset({path.name for path in OUT.iterdir() if path.is_file()})

summary = json.loads((OUT / "ATLAS_PASS2_SUMMARY.json").read_text(encoding="utf-8"))
assert summary["atlas_sha256"] == "5b155937b8d05f0f0f9f1a02f10d9afe76a917d6035897695cce739eb8d6b1fd"
assert summary["schema_version"] == "2.6.0"
assert summary["validation_failures"] == 0
assert summary["patterns"] == 413
assert summary["events"] == 9377
assert summary["one_bar_eligible_patterns"] == 300
assert summary["one_bar_structural_groups"] == 269
assert summary["recurring_skeleton_candidates"] == 8
assert summary["topology_decisions"] == {"HOLD": 1, "NEAR_EXISTING": 5, "REVIEW": 2}
assert summary["stage7_admission"] == "CLOSED"
assert summary["measured_phrase_patterns"] == 19
assert summary["derived_four_bar_patterns"] == 17
assert summary["bass_rhythm_one_bar_patterns"] == 35
assert summary["bass_pitch_contour_eligible_patterns"] == 35
assert summary["motif_contour_eligible_patterns"] == 23

def read(name):
    with (OUT / name).open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))

topology = read("ATLAS_PASS2_TOPOLOGY_CANDIDATES.csv")
assert [row["candidate_id"] for row in topology] == [f"SKEL_{i:02d}" for i in range(1, 9)]
assert all(int(row["structural_group_count"]) >= 3 for row in topology)
assert {row["decision"] for row in topology} <= {"NEAR_EXISTING", "REVIEW", "HOLD"}
assert not any(row["decision"] == "ACCEPT" for row in topology)
assert sum(row["decision"] == "REVIEW" for row in topology) == 2
assert sum(row["runtime_compatible_archetype_count"] == "0" for row in topology) == 3

distances = read("ATLAS_PASS2_DISTANCE_DISTRIBUTIONS.csv")
metrics = {row["metric"] for row in distances}
assert "atlas_all_structural_group_pair_drum_jaccard" in metrics
assert "atlas_research_structural_group_pair_drum_jaccard" in metrics
assert "atlas_variation_of_drum_jaccard" in metrics
assert "runtime_grammar_envelope_pair_jaccard_diagnostic" in metrics
assert "atlas_to_nearest_runtime_required_miss_rate_diagnostic" in metrics
assert "atlas_to_nearest_runtime_outside_support_rate_diagnostic" in metrics
assert "atlas_to_nearest_runtime_density_deviation_steps_diagnostic" in metrics
assert "atlas_to_nearest_runtime_support_jaccard_diagnostic" in metrics

relationships = read("ATLAS_PASS2_RELATIONSHIPS.csv")
for row in relationships:
    assert int(row["structural_group_count"]) >= 3
    if row["domain"] == "KickToBassRhythm":
        assert row["evidence_class"] == "PROJECT_OWNED_EXACT"
    else:
        assert row["evidence_class"] == "RESEARCH_AGGREGATE"

phrases = read("ATLAS_PASS2_PHRASE_TRANSITIONS.csv")
measured = [row for row in phrases if row["evidence_class"] == "MEASURED"]
derived = [row for row in phrases if row["evidence_class"] == "EDITORIAL_CURATED"]
assert sum(int(row["count"]) for row in measured) == 19
assert sum(int(row["count"]) for row in derived) == 17
assert {row["transition_signature"] for row in measured} == {
    "EXACT_REPEAT", "ADD_ONLY", "DROP_ONLY", "MIXED"
}

pitch = read("ATLAS_PASS2_PITCH_CONTOURS.csv")
assert pitch
assert all(row["evidence_class"] == "PROJECT_OWNED_EXACT" for row in pitch)
assert all(row["decision"] == "HOLD" for row in pitch)

variation = read("ATLAS_PASS2_EFFECTIVE_VARIATION_BASELINE.csv")
by_slot = {row["slot"]: row for row in variation}
assert int(by_slot["P1"]["effective_variation_count"]) == 12
assert int(by_slot["P2"]["effective_variation_count"]) == 10
assert int(by_slot["P3"]["effective_variation_count"]) == 7

for path in OUT.iterdir():
    if not path.is_file():
        continue
    text = path.read_text(encoding="utf-8")
    assert "PAT_" not in text, f"raw pattern id leaked in {path.name}"
    assert "SG_" not in text, f"structural group id leaked in {path.name}"
    assert "source_locator" not in text
    assert "structural_hash" not in text
    assert "expressive_hash" not in text

print("Atlas Pass 2 aggregate output schema/rights: OK")
