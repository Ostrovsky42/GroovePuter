#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "tools" / "research" / "harmony_atlas_h2.py"
spec = importlib.util.spec_from_file_location("harmony_atlas_h2", MODULE_PATH)
assert spec is not None and spec.loader is not None
h2 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h2)


def quality(
    triad: str,
    *,
    suffix: str = "",
    extension: str = "NONE",
    seventh: str = "NONE",
    fifth_alt: int = 0,
    triad_source: str = "ROMAN_CASE",
) -> dict:
    return {
        "triad_class": triad,
        "triad_source": triad_source,
        "extension_class": extension,
        "seventh_flavor": seventh,
        "fifth_alteration_semitones": fifth_alt,
        "raw_suffix": suffix,
    }


def token(token_id: str, source: str, degree: int, alteration: int, triad: str, **kwargs) -> dict:
    roman = source
    while roman and roman[0] in "b#":
        roman = roman[1:]
    suffix = kwargs.pop("suffix", "")
    if suffix and roman.endswith(suffix):
        roman = roman[: -len(suffix)]
    return {
        "token_id": token_id,
        "source_token": source,
        "source_event_incidence": 1,
        "source_families": ["Major"],
        "canonical_token_key": token_id,
        "root": {
            "diatonic_degree": degree,
            "alteration_semitones": alteration,
            "notation_case": "UPPER" if roman.isupper() else "LOWER",
            "source_accidental": "b" if source.startswith("b") else ("#" if source.startswith("#") else ""),
            "source_roman": roman,
        },
        "quality": quality(triad, suffix=suffix, **kwargs),
    }


def definition(source_id: str, family: str, refs: list[str], tags: dict | None = None) -> dict:
    return {
        "source_id": source_id,
        "source_family": family,
        "notation_class": {
            "Major": "TRADITIONAL_MAJOR",
            "Minor": "TRADITIONAL_MINOR",
            "Modal": "IONIAN_RELATIVE_MODAL",
        }[family],
        "source_definition_sha256": (source_id.replace(":", "") + "0" * 64)[:64],
        "event_count": len(refs),
        "chord_event_count": sum(ref != "REST" for ref in refs),
        "rest_event_count": sum(ref == "REST" for ref in refs),
        "tags": tags or {"mood": [], "structural": [], "catalog": []},
        "event_refs": refs,
    }


def synthetic_h1() -> dict:
    vocabulary = [
        token("T001", "I", 0, 0, "MAJOR"),
        token("T002", "IM", 0, 0, "MAJOR", suffix="M", triad_source="EXPLICIT_MAJOR_SUFFIX"),
        token("T003", "V", 4, 0, "MAJOR"),
        token("T004", "vi", 5, 0, "MINOR"),
        token("T005", "IV", 3, 0, "MAJOR"),
        token("T006", "I7", 0, 0, "MAJOR", suffix="7", extension="SEVENTH", seventh="UNSPECIFIED"),
        token(
            "T007", "Idom7", 0, 0, "MAJOR", suffix="dom7", extension="SEVENTH",
            seventh="DOMINANT", triad_source="EXPLICIT_DOMINANT_SUFFIX",
        ),
        token("T008", "bVII", 6, -1, "MAJOR"),
    ]
    definitions = [
        definition("Major:001", "Major", ["T001", "T003", "T004", "T005"], {"mood": ["Hopeful"], "structural": [], "catalog": []}),
        definition("Major:002", "Major", ["T001", "T003", "T004", "T005"], {"mood": ["Triumphant"], "structural": [], "catalog": []}),
        definition("Major:003", "Major", ["T003", "T004", "T005", "T001"]),
        definition("Major:004", "Major", ["T001", "T003", "T004", "T005", "T001", "T003", "T004", "T005"]),
        definition("Major:005", "Major", ["T002", "T003", "T004", "T005"]),
        definition("Major:006", "Major", ["T006", "T003"]),
        definition("Major:007", "Major", ["T007", "T003"]),
        definition("Modal:001", "Modal", ["T001", "T003", "T004", "T005"]),
        definition("Modal:002", "Modal", ["T001", "REST", "T008"]),
    ]
    return {
        "schema_version": "1.0.0",
        "stage": "H1_CANONICAL_PARSER_NORMALIZATION",
        "source": {
            "repository": "synthetic",
            "commit": "fixture",
            "evidence_class": "EDITORIAL_CATALOG_EVIDENCE",
        },
        "summary": {
            "logical_definition_count": len(definitions),
            "admitted_definition_count": len(definitions),
            "quarantined_definition_count": 0,
            "normalized_chord_event_count": sum(row["chord_event_count"] for row in definitions),
            "normalized_rest_event_count": sum(row["rest_event_count"] for row in definitions),
            "raw_token_vocabulary_count": len(vocabulary),
        },
        "token_vocabulary": vocabulary,
        "definitions": definitions,
        "quarantine": [],
    }


def row_by_id(result: dict, source_id: str) -> dict:
    return next(row for row in result["definitions"] if row["source_id"] == source_id)


def main() -> None:
    fixture = synthetic_h1()
    result = h2.build_fingerprints(fixture)

    assert result["stage"] == "H2_STRUCTURAL_FINGERPRINTS_DEDUP"
    assert result["dedup_contract"]["definitions_removed"] == 0
    assert result["dedup_contract"]["representative_selection"] == "NOT_PERFORMED"
    assert result["dedup_contract"]["cyclic_rotation_equivalence"] == "FORBIDDEN"
    assert result["dedup_contract"]["repetition_extension_equivalence"] == "FORBIDDEN"

    for level in ("F0", "F1", "F2", "F3"):
        assert result["levels"][level]["status"] == "COMPUTED"
    for level in ("F4", "F5", "F6"):
        assert result["levels"][level]["status"] == "DEFERRED"
        assert result["levels"][level]["classes"] == []

    a = row_by_id(result, "Major:001")
    b = row_by_id(result, "Major:002")
    rotated = row_by_id(result, "Major:003")
    repeated = row_by_id(result, "Major:004")
    explicit_major = row_by_id(result, "Major:005")
    generic7 = row_by_id(result, "Major:006")
    dominant7 = row_by_id(result, "Major:007")
    modal_same = row_by_id(result, "Modal:001")
    with_rest = row_by_id(result, "Modal:002")

    # F0 is source identity: duplicated musical content remains distinct.
    assert a["fingerprints"]["F0"] != b["fingerprints"]["F0"]
    assert result["levels"]["F0"]["summary"]["unique_class_count"] == len(fixture["definitions"])

    # F1 ignores source id/tags, but preserves source family and exact notation.
    assert a["fingerprints"]["F1"] == b["fingerprints"]["F1"]
    assert a["fingerprints"]["F1"] != explicit_major["fingerprints"]["F1"]
    assert a["fingerprints"]["F1"] != modal_same["fingerprints"]["F1"]

    # F3 intentionally collapses exact spelling/family only when H1 semantics match.
    assert a["fingerprints"]["F3"] == explicit_major["fingerprints"]["F3"]
    assert a["fingerprints"]["F3"] == modal_same["fingerprints"]["F3"]

    # Root-only F2 must not hide seventh-quality differences in F3.
    assert generic7["fingerprints"]["F2"] == dominant7["fingerprints"]["F2"]
    assert generic7["fingerprints"]["F3"] != dominant7["fingerprints"]["F3"]
    assert result["cross_level_diagnostics"]["f2_quality_sensitive_group_count"] >= 1

    # Event order and phrase length remain identity-bearing at F2/F3.
    assert a["fingerprints"]["F3"] != rotated["fingerprints"]["F3"]
    assert a["fingerprints"]["F3"] != repeated["fingerprints"]["F3"]

    # Rest is part of structural sequence, not silently dropped.
    assert with_rest["fingerprints"]["F2"] != modal_same["fingerprints"]["F2"]

    near = result["near_relations"]
    relations = near["relations"]
    assert any(
        row["kind"] == "CYCLIC_ROTATION"
        and {row["source_a"], row["source_b"]} == {"Major:001", "Major:003"}
        and row["dedup_equivalent"] is False
        for row in relations
    )
    assert any(
        row["kind"] == "REPETITION_EXTENSION"
        and row["shorter_source"] == "Major:001"
        and row["longer_source"] == "Major:004"
        and row["repetition_factor"] == 2
        and row["dedup_equivalent"] is False
        for row in relations
    )
    assert near["pair_relation_count"] == len(relations)
    assert near["component_count"] >= 2
    assert near["component_count_by_kind"]["CYCLIC_ROTATION"] >= 1
    assert near["component_count_by_kind"]["REPETITION_EXTENSION"] >= 1
    assert any(
        component["kind"] == "CYCLIC_ROTATION"
        and {"Major:001", "Major:003"}.issubset(set(component["source_ids"]))
        for component in near["components"]
    )
    assert any(
        component["kind"] == "REPETITION_EXTENSION"
        and {"Major:001", "Major:004"}.issubset(set(component["source_ids"]))
        for component in near["components"]
    )

    # Same F1 progression with differing tags must remain an explicit metadata conflict.
    assert result["cross_level_diagnostics"]["f1_metadata_variant_group_count"] >= 1
    assert result["cross_level_diagnostics"]["f1_metadata_variant_groups"][0]["tag_sets"]

    # Fingerprints are deterministic.
    second = h2.build_fingerprints(fixture)
    assert h2.canonical_json(result) == h2.canonical_json(second)

    # Production CLI provenance cannot be relabeled by passing arbitrary H1 JSON bytes.
    try:
        h2.verify_h1_json_bytes(b"{}")
    except h2.FingerprintError:
        pass
    else:
        raise AssertionError("expected H2 to reject non-frozen H1 JSON digest")

    # Reject H1 inputs that are not fully admitted; H2 must not dedup around quarantine.
    invalid = synthetic_h1()
    invalid["summary"]["quarantined_definition_count"] = 1
    try:
        h2.build_fingerprints(invalid)
    except h2.FingerprintError:
        pass
    else:
        raise AssertionError("expected H2 to reject quarantined H1 input")

    print("Harmony Atlas H2 tests: OK")


if __name__ == "__main__":
    main()
