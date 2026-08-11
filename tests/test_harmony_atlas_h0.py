#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import json
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools/research/harmony_atlas_h0.py"
REPORT = ROOT / "docs/research/HARMONY_ATLAS_H0_SOURCE_AUDIT.json"

spec = importlib.util.spec_from_file_location("harmony_atlas_h0", TOOL)
assert spec is not None and spec.loader is not None
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def write_fixture(root: Path) -> None:
    (root / "chords.py").write_text(
        """\
prog_maj = [
    "I  V7 =New Hopeful",
    "bIII IV =Cadence",
]
prog_min = ["i iv =Dark"]
prog_modal = ["#IVdim bVII =Mysterious"]
chord_types_maj = ["7", "sus4"]
chord_types_min = ["m7", "sus4"]
""",
        encoding="utf-8",
    )
    (root / "gen.py").write_text(
        """\
keys = [("C", "A"), ("D", "B")]
styles = ["", "pop"]
""",
        encoding="utf-8",
    )


def test_fixture() -> None:
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        write_fixture(root)
        audit = module.build_audit(root, verify_pin=False)

    p = audit["progressions"]
    g = audit["generation"]
    c = audit["declared_chord_types"]
    require(p["logical_definition_count"] == 4, "logical definition count")
    require(p["by_family"] == {"Major": 2, "Minor": 1, "Modal": 1}, "family counts")
    require(g["materializations_per_logical_progression"] == 4, "multiplicity")
    require(p["projected_materialization_count"] == 16, "projected count")
    require(p["catalog_tag_vocabulary"] == ["New"], "catalog tag typing")
    require(p["structural_tag_vocabulary"] == ["Cadence"], "structural tag typing")
    require(p["mood_tag_vocabulary"] == ["Dark", "Hopeful", "Mysterious"], "mood typing")
    require(p["altered_degree_classes"] == ["#IV", "bIII", "bVII"], "altered degree classes")
    require(p["explicit_double_space_rest_markers"] == 1, "rest marker")
    require(p["lexically_unclassified_tokens"] == [], "unexpected lexical failure")
    require(c["major_third_count"] == 2 and c["minor_third_count"] == 2, "chord type counts")
    require(c["union_count"] == 3, "chord type union")


def test_unclassified_token_is_visible() -> None:
    parsed = module.lexical_chord_token("NOT_A_CHORD")
    require(parsed is None, "invalid token must not parse")


def test_committed_report_contract() -> None:
    require(REPORT.is_file(), f"missing report: {REPORT}")
    report = json.loads(REPORT.read_text(encoding="utf-8"))
    require(report["schema_version"] == "1.0.0", "schema version changed")
    require(report["source"]["commit"] == module.PINNED_SOURCE_COMMIT, "source pin drift")
    require(report["source"]["evidence_class"] == "EDITORIAL_CATALOG_EVIDENCE", "evidence class")
    require(report["progressions"]["logical_definition_count"] == 190, "logical progression count")
    require(report["progressions"]["by_family"] == {"Major": 50, "Minor": 58, "Modal": 82}, "family count drift")
    require(report["generation"]["key_pair_count"] == 12, "key-pair drift")
    require(report["generation"]["style_count"] == 5, "style-count drift")
    require(report["generation"]["materializations_per_logical_progression"] == 60, "multiplicity drift")
    require(report["progressions"]["projected_materialization_count"] == 11400, "materialization drift")
    require(report["methodology"]["runtime_weight_from_file_count"] == "FORBIDDEN", "weight boundary")
    require(report["progressions"]["lexically_unclassified_tokens"] == [], "committed lexical quarantine must be empty")
    require(report["progressions"]["altered_degree_classes"] == ["#IV", "bI", "bII", "bIII", "bVI", "bVII"], "altered degree inventory drift")


def main() -> None:
    test_fixture()
    test_unclassified_token_is_visible()
    test_committed_report_contract()
    print("Harmony Atlas H0 tests: OK")


if __name__ == "__main__":
    main()
