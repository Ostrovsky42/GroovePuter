#!/usr/bin/env python3
from __future__ import annotations

import copy
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools" / "research"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

import harmony_atlas_h1 as h1
import harmony_atlas_h2 as h2
import harmony_atlas_h3 as h3


def build_normalization(specs: list[tuple[str, str, list[str]]]) -> dict:
    source_tokens: dict[str, dict] = {}
    normalized_specs: list[tuple[str, str, list[str], list[str]]] = []

    for source_id, progression, descriptors in specs:
        family = source_id.split(":", 1)[0]
        refs: list[str] = []
        for token in progression.split():
            if token == "REST":
                refs.append("REST")
                continue
            event = h1.normalize_chord_token(token)
            source_tokens[token] = event
            refs.append(token)
        normalized_specs.append((source_id, family, refs, descriptors))

    token_ids = {
        token: f"T{index:03d}"
        for index, token in enumerate(sorted(source_tokens), start=1)
    }
    vocabulary = []
    for token in sorted(source_tokens):
        event = source_tokens[token]
        vocabulary.append(
            {
                "token_id": token_ids[token],
                "source_token": token,
                "source_event_incidence": 1,
                "source_families": [],
                "canonical_token_key": h1.canonical_token_key(event),
                "root": event["root"],
                "quality": event["quality"],
            }
        )

    definitions = []
    for source_id, family, refs, descriptors in normalized_specs:
        typed, unknown = h1.type_descriptors(descriptors)
        if unknown:
            raise AssertionError(f"unknown synthetic descriptors: {unknown}")
        event_refs = [
            "REST" if ref == "REST" else token_ids[ref]
            for ref in refs
        ]
        definitions.append(
            {
                "source_id": source_id,
                "source_family": family,
                "notation_class": h1.NOTATION_CLASS_BY_FAMILY[family],
                "source_definition_sha256": h1.source_definition_sha256(
                    " ".join(refs) + (" =" + " ".join(descriptors) if descriptors else "")
                ),
                "event_count": len(event_refs),
                "chord_event_count": sum(ref != "REST" for ref in event_refs),
                "rest_event_count": sum(ref == "REST" for ref in event_refs),
                "tags": typed,
                "event_refs": event_refs,
            }
        )

    return {
        "schema_version": "1.0.0",
        "stage": "H1_CANONICAL_PARSER_NORMALIZATION",
        "source": {
            "repository": "synthetic/harmony",
            "commit": "synthetic",
            "evidence_class": "EDITORIAL_CATALOG_EVIDENCE",
        },
        "summary": {
            "logical_definition_count": len(definitions),
            "admitted_definition_count": len(definitions),
            "quarantined_definition_count": 0,
            "raw_token_vocabulary_count": len(vocabulary),
        },
        "token_vocabulary": vocabulary,
        "definitions": definitions,
        "quarantine": [],
    }


def analyze(specs: list[tuple[str, str, list[str]]]) -> dict:
    normalization = build_normalization(specs)
    fingerprints = h2.build_fingerprints(
        normalization, h1_input_sha256="synthetic-h1"
    )
    return h3.build_functional_analysis(
        normalization,
        fingerprints,
        h1_digest="synthetic-h1",
        h2_digest="synthetic-h2",
    )


class HarmonyAtlasH3Tests(unittest.TestCase):
    def test_major_authentic_cadence_and_roles(self) -> None:
        result = analyze([("Major:001", "I IV V I", [])])
        row = result["definitions"][0]
        roles = [
            event["functional_role"]
            for event in row["functional_events"]
            if event["kind"] == "CHORD"
        ]
        self.assertEqual(roles, ["TONIC", "PREDOMINANT", "DOMINANT", "TONIC"])
        self.assertEqual(
            row["features"]["cadence_class"],
            "AUTHENTIC_CADENCE_CANDIDATE",
        )
        self.assertEqual(row["features"]["closure_class"], "CADENTIAL")
        self.assertEqual(row["features"]["dominant_resolution_count"], 1)

    def test_major_borrowed_candidates_are_candidates_not_facts(self) -> None:
        result = analyze([("Major:001", "I iv bVII I", [])])
        row = result["definitions"][0]
        chords = [
            event for event in row["functional_events"] if event["kind"] == "CHORD"
        ]
        self.assertTrue(chords[1]["borrowed_candidate"])
        self.assertEqual(chords[1]["color_class"], "BORROWED_CANDIDATE")
        self.assertTrue(chords[2]["borrowed_candidate"])
        self.assertTrue(chords[2]["chromatic_root"])
        self.assertEqual(row["features"]["borrowed_candidate_count"], 2)
        self.assertEqual(row["features"]["chromatic_root_count"], 1)
        self.assertEqual(
            result["analysis_contract"]["borrowed_label_strength"],
            "CANDIDATE_ONLY",
        )

    def test_minor_dominant_can_resolve_without_being_called_borrowed(self) -> None:
        result = analyze([("Minor:001", "i iv V i", [])])
        row = result["definitions"][0]
        chords = [
            event for event in row["functional_events"] if event["kind"] == "CHORD"
        ]
        self.assertEqual(chords[2]["functional_role"], "DOMINANT")
        self.assertFalse(chords[2]["borrowed_candidate"])
        self.assertEqual(
            row["features"]["cadence_class"],
            "AUTHENTIC_CADENCE_CANDIDATE",
        )

    def test_modal_context_does_not_force_common_practice_function(self) -> None:
        result = analyze([("Modal:001", "i bVII IV i", [])])
        row = result["definitions"][0]
        roles = [
            event["functional_role"]
            for event in row["functional_events"]
            if event["kind"] == "CHORD"
        ]
        self.assertEqual(roles, ["TONIC", "MODAL_COLOR", "MODAL_COLOR", "TONIC"])
        self.assertEqual(row["features"]["cadence_class"], "MODAL_AMBIGUOUS")
        self.assertEqual(
            row["features"]["closure_class"], "MODAL_AMBIGUOUS_LOOP"
        )
        self.assertGreater(row["features"]["low_confidence_event_count"], 0)

    def test_source_cadence_tag_does_not_force_derived_cadence(self) -> None:
        result = analyze([("Major:001", "I ii iii", ["Cadence"])])
        row = result["definitions"][0]
        self.assertEqual(row["features"]["cadence_class"], "NO_CADENCE")
        self.assertEqual(
            row["features"]["cadence_tag_comparison"], "TAGGED_ONLY"
        )

    def test_f4_can_generalize_cross_family_same_functional_shape(self) -> None:
        result = analyze(
            [
                ("Major:001", "I V I", []),
                ("Minor:001", "i V i", []),
            ]
        )
        rows = {row["source_id"]: row for row in result["definitions"]}
        self.assertEqual(
            rows["Major:001"]["fingerprints"]["F4"],
            rows["Minor:001"]["fingerprints"]["F4"],
        )
        self.assertEqual(result["f4"]["duplicate_group_count"], 1)
        self.assertEqual(result["f4"]["cross_family_duplicate_group_count"], 1)

    def test_f4_ignores_confidence_and_reason_diagnostics(self) -> None:
        event = {
            "kind": "CHORD",
            "functional_role": "DOMINANT",
            "color_class": "UNALTERED_CONTEXT",
            "confidence": "HIGH",
            "reason_code": "A",
        }
        changed = copy.deepcopy(event)
        changed["confidence"] = "LOW"
        changed["reason_code"] = "B"
        self.assertEqual(h3.f4_event(event), h3.f4_event(changed))

    def test_f0_f3_drift_is_rejected(self) -> None:
        normalization = build_normalization([("Major:001", "I V I", [])])
        fingerprints = h2.build_fingerprints(
            normalization, h1_input_sha256="synthetic-h1"
        )
        fingerprints["definitions"][0]["fingerprints"]["F3"] = "0" * 64
        with self.assertRaises(h3.FunctionalAnalysisError):
            h3.build_functional_analysis(
                normalization,
                fingerprints,
                h1_digest="synthetic-h1",
                h2_digest="synthetic-h2",
            )

    def test_wrong_dependency_digest_is_rejected(self) -> None:
        with self.assertRaises(h3.FunctionalAnalysisError):
            h3.verify_json_bytes(b"{}", "0" * 64, "synthetic")

    def test_quality_entropy_distinguishes_quality_diversity(self) -> None:
        result = analyze(
            [
                ("Major:001", "I I I I", []),
                ("Major:002", "I im I im", []),
            ]
        )
        rows = {row["source_id"]: row for row in result["definitions"]}
        self.assertEqual(rows["Major:001"]["features"]["quality_entropy_bits"], 0.0)
        self.assertGreater(rows["Major:002"]["features"]["quality_entropy_bits"], 0.0)

    def test_repetition_period_preserves_explicit_form(self) -> None:
        result = analyze([("Major:001", "I V I V", [])])
        row = result["definitions"][0]
        self.assertEqual(row["features"]["repetition_period_f3_events"], 2)
        self.assertEqual(
            result["analysis_contract"]["derived_labels_rewrite_source_identity"],
            False,
        )

    def test_modal_cadence_tag_is_unresolved_not_contradicted(self) -> None:
        result = analyze([("Modal:001", "iv bIII I", ["Cadence"])])
        row = result["definitions"][0]
        self.assertEqual(row["features"]["cadence_class"], "MODAL_AMBIGUOUS")
        self.assertEqual(
            row["features"]["cadence_tag_comparison"],
            "TAGGED_MODAL_UNRESOLVED",
        )

    def test_f4_is_not_dedup_authority(self) -> None:
        result = analyze(
            [
                ("Major:001", "I V I", []),
                ("Minor:001", "i V i", []),
            ]
        )
        self.assertEqual(
            result["analysis_contract"]["f4_destructive_dedup_authority"],
            "FORBIDDEN",
        )
        self.assertGreaterEqual(
            result["f4_abstraction_report"][
                "f4_groups_spanning_multiple_f3_classes"
            ],
            1,
        )

    def test_h4_and_runtime_are_still_out_of_scope(self) -> None:
        result = analyze([("Major:001", "I V I", [])])
        contract = result["analysis_contract"]
        self.assertEqual(contract["harmonic_event_density_timing_claim"], "DEFERRED_TO_H4")
        self.assertEqual(contract["runtime_admission"], "NOT_PERFORMED")
        self.assertEqual(contract["absolute_midi_projection"], "FORBIDDEN")


if __name__ == "__main__":
    unittest.main()
