#!/usr/bin/env python3
from __future__ import annotations

import csv
import hashlib
import json
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "atlas"))

import extract_atlas_pass2_hardening as hardening  # noqa: E402

DATA = ROOT / "docs" / "architecture" / "atlas_pass2"


def track(track_id: str, role: str = "DRUM_OR_PERCUSSION") -> dict[str, str]:
    return {
        "track_id": track_id,
        "track_role": role,
        "source_instrument_name": track_id,
    }


def pattern(kind: str = "SOURCE_OBSERVATION", source: str = "SRC_POCKET_OPS_31") -> dict[str, str]:
    return {"pattern_kind": kind, "source_id": source}


def test_role_mapping() -> None:
    source = pattern()
    assert hardening.semantic_role(track("KICK"), source)[0] == 0
    assert hardening.semantic_role(track("SNARE"), source)[0] == 1
    assert hardening.semantic_role(track("CLOSED_HAT"), source)[0] == 2
    assert hardening.semantic_role(track("OPEN_HAT"), source)[0] == 3
    assert hardening.semantic_role(track("RIMSHOT"), source)[0] == 4
    assert hardening.semantic_role(track("KICK", "DRUM_OR_PERCUSSION"), source)[0] == 0
    assert hardening.semantic_role(track("SNARE", "DRUM_OR_PERCUSSION"), source)[0] == 1
    assert hardening.semantic_role(track("CLAP"), source)[0] == 4
    assert hardening.semantic_role(track("CYMBAL"), source)[0] is None
    assert hardening.semantic_role(track("RIDE"), source)[0] is None
    assert hardening.semantic_role(track("LASER"), source)[0] is None


def test_grammar_coverage() -> None:
    archetype = {
        "lanes": {
            0: {"required": {0}, "support": {0, 4}, "forbidden": set(), "min": 1, "max": 2},
            1: {"required": {4}, "support": {4, 12}, "forbidden": set(), "min": 1, "max": 2},
            2: {"required": set(), "support": {2, 6, 10, 14}, "forbidden": set(), "min": 0, "max": 4},
            3: {"required": set(), "support": {6, 14}, "forbidden": set(), "min": 0, "max": 2},
            4: {"required": set(), "support": {1, 3, 5, 7}, "forbidden": set(), "min": 0, "max": 4},
        },
        "spaces": [{"steps": {4}, "roles": 1 << 4}],
        "relationships": [{
            "source": 0,
            "target": 4,
            "op": 0,
            "strength": 1,
            "scope": 0,
            "zone": set(range(16)),
            "min_offset": 0,
            "max_offset": 0,
            "min_matches": 0,
            "max_matches": 0,
            "min_responses": 0,
            "max_responses": 0,
            "weight": 0,
        }],
    }
    legal = {role: set() for role in range(8)}
    legal[0] = {0}
    legal[1] = {4}
    legal[4] = {1}
    assert hardening.grammar_coverage(legal, archetype)["hard_covered"]

    protected = {role: set(values) for role, values in legal.items()}
    protected[4].add(4)
    protected_result = hardening.grammar_coverage(protected, archetype)
    assert not protected_result["hard_covered"]
    assert protected_result["protected_space_hits"] == 1

    exclude = {role: set(values) for role, values in legal.items()}
    exclude[4] = {0}
    exclude_result = hardening.grammar_coverage(exclude, archetype)
    assert not exclude_result["hard_covered"]
    assert exclude_result["hard_relationship_violations"] == 1


def test_committed_outputs() -> None:
    summary = json.loads((DATA / "ATLAS_PASS2_HARDENING_SUMMARY.json").read_text(encoding="utf-8"))
    assert summary["atlas_sha256"] == hardening.EXPECTED_ATLAS_SHA256
    assert summary["schema_version"] == "2.6.0"
    assert summary["bar_counts"] == {"1": 302, "2": 93, "4": 17, "5": 1}
    assert summary["role_mapping_schema"] == "ATLAS_ROLE_MAPPING_V2"
    assert summary["runtime_topology_schema"] == "GROOVEPUTER_RUNTIME_RHYTHM_TOPOLOGY_V2"
    assert summary["stage7_production_admission"] == "CLOSED"
    assert summary["candidate_decisions"] == {"AUDITION_REVIEW": 2, "HOLD_SINGLE_ROOT": 7}

    with (DATA / "ATLAS_PASS2_HARDENED_CANDIDATES.csv").open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    assert len(rows) == 9
    audition = [row for row in rows if row["decision"] == "AUDITION_REVIEW"]
    assert len(audition) == 2
    for row in audition:
        assert int(row["independent_provenance_root_count"]) >= 2
        assert int(row["content_deduped_artifact_count"]) >= 2
    for row in rows:
        if row["decision"] == "HOLD_SINGLE_ROOT":
            assert int(row["independent_provenance_root_count"]) == 1

    with (DATA / "ATLAS_PASS2_CALIBRATION_DISTRIBUTIONS.csv").open(encoding="utf-8", newline="") as handle:
        calibration = {row["metric"]: row for row in csv.DictReader(handle)}
    null = calibration["atlas_duplicate_null_same_source_structural_group"]
    assert int(null["count"]) == 29
    assert float(null["max"]) == 0.0

    topology_text = (DATA / "RUNTIME_RHYTHM_TOPOLOGY_V2.tsv").read_text(encoding="utf-8")
    assert topology_text.startswith("FORMAT\tGROOVEPUTER_RUNTIME_RHYTHM_TOPOLOGY_V2\n")
    assert topology_text.count("\nA\t") == 20
    assert "\nR\t" in topology_text
    assert "\nS\t" in topology_text
    assert topology_text.endswith("COUNT\t20\n")

    runtime_calibration = (DATA / "RUNTIME_RHYTHM_CALIBRATION_V1.tsv").read_text(encoding="utf-8").splitlines()
    assert runtime_calibration[0] == "FORMAT\tGROOVEPUTER_RUNTIME_RHYTHM_CALIBRATION_V1"
    self_rows = [line.split("\t") for line in runtime_calibration if line.startswith("SELF\t")]
    confusion_rows = [line.split("\t") for line in runtime_calibration if line.startswith("CONF\t")]
    assert len(self_rows) == 20
    assert all(row[4] == "64" for row in self_rows)
    assert len(confusion_rows) == 9
    assert sum(int(row[3]) for row in confusion_rows) == 335
    assert "AGG_SELF\t40320\t0.052632\t0.219298\t0.289474\t0.429825" in runtime_calibration
    assert "AGG_CONF\t335\t24320\t0.013775" in runtime_calibration
    assert runtime_calibration[-1] == "COUNT\t20"


def test_rights_and_hash_manifest() -> None:
    safe_files = [
        "ATLAS_PASS2_CALIBRATION_DISTRIBUTIONS.csv",
        "ATLAS_PASS2_HARDENED_CANDIDATES.csv",
        "ATLAS_PASS2_HARDENING_SUMMARY.json",
        "ATLAS_PASS2_ROLE_MAPPING_AUDIT.csv",
    ]
    forbidden = tuple(token.lower() for token in hardening.SENSITIVE_TOKENS)
    for filename in safe_files:
        text = (DATA / filename).read_text(encoding="utf-8")
        first_line = text.splitlines()[0].lower()
        assert not any(token in first_line for token in forbidden)
        assert "PAT_PO_" not in text
        assert "PAT_ED_" not in text
        assert "SRC_POCKET_OPS_31" not in text

    manifest = {}
    for line in (DATA / "ATLAS_PASS2_HARDENING_OUTPUT_HASHES.sha256").read_text(encoding="utf-8").splitlines():
        digest, filename = line.split("  ", 1)
        manifest[filename] = digest
    assert set(manifest) == set(safe_files)
    for filename in safe_files:
        actual = hashlib.sha256((DATA / filename).read_bytes()).hexdigest()
        assert actual == manifest[filename]

    with tempfile.TemporaryDirectory() as temp:
        path = Path(temp) / "bad.csv"
        try:
            hardening.write_csv(path, [{"pattern_id": "x"}], ["pattern_id"])
        except ValueError:
            pass
        else:
            raise AssertionError("rights-sensitive output column was accepted")


def main() -> None:
    test_role_mapping()
    test_grammar_coverage()
    test_committed_outputs()
    test_rights_and_hash_manifest()
    print("Atlas Pass 2 adversarial hardening: OK")


if __name__ == "__main__":
    main()
