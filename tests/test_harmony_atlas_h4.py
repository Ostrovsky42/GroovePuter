#!/usr/bin/env python3
from __future__ import annotations

import ast
import copy
import sys
import tempfile
import unittest
from fractions import Fraction
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools" / "research"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

import harmony_atlas_h1 as h1
import harmony_atlas_h2 as h2
import harmony_atlas_h3 as h3
import harmony_atlas_h4 as h4


def build_normalization(specs: list[tuple[str, list[str], list[str]]]) -> dict:
    source_tokens: dict[str, dict] = {}
    prepared: list[tuple[str, str, list[str], list[str]]] = []
    for source_id, tokens, descriptors in specs:
        family = source_id.split(":", 1)[0]
        for token in tokens:
            if token == "REST":
                continue
            source_tokens[token] = h1.normalize_chord_token(token)
        prepared.append((source_id, family, tokens, descriptors))

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
    for source_id, family, tokens, descriptors in prepared:
        typed, unknown = h1.type_descriptors(descriptors)
        if unknown:
            raise AssertionError(unknown)
        refs = ["REST" if token == "REST" else token_ids[token] for token in tokens]
        definitions.append(
            {
                "source_id": source_id,
                "source_family": family,
                "notation_class": h1.NOTATION_CLASS_BY_FAMILY[family],
                "source_definition_sha256": h1.source_definition_sha256(" ".join(tokens)),
                "event_count": len(refs),
                "chord_event_count": sum(ref != "REST" for ref in refs),
                "rest_event_count": sum(ref == "REST" for ref in refs),
                "tags": typed,
                "event_refs": refs,
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


def dependencies(specs: list[tuple[str, list[str], list[str]]]):
    normalization = build_normalization(specs)
    fingerprints = h2.build_fingerprints(
        normalization, h1_input_sha256="synthetic-h1"
    )
    functional = h3.build_functional_analysis(
        normalization,
        fingerprints,
        h1_digest="synthetic-h1",
        h2_digest="synthetic-h2",
    )
    return normalization, fingerprints, functional


class HarmonyAtlasH4Tests(unittest.TestCase):
    def test_pattern_token_duration_is_exact_fraction(self) -> None:
        self.assertEqual(h4.parse_pattern_token("0.75X"), ("X", Fraction(3, 4)))
        self.assertEqual(h4.parse_pattern_token("2.25S"), ("S", Fraction(9, 4)))
        self.assertEqual(h4.parse_pattern_token("N"), ("N", Fraction(1, 1)))

    def test_invalid_pattern_token_rejected(self) -> None:
        with self.assertRaises(h4.RhythmExtractionError):
            h4.parse_pattern_token("N+evil")

    def test_safe_source_assignments_does_not_execute_calls(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "patterns.py"
            path.write_text(
                "N='N'\npatterns={'ok':[N]}\nevil=__import__('os').system('false')\n",
                encoding="utf-8",
            )
            assignments = h4.safe_source_assignments(path)
            self.assertEqual(assignments["patterns"], {"ok": ["N"]})
            self.assertNotIn("evil", assignments)

    def test_long_pattern_doubles_final_step_for_odd_event_count(self) -> None:
        expanded = h4.expand_pattern(
            ["T001", "T002", "T003"],
            "long",
            ["4N"],
            base_duration=Fraction(1, 1),
        )
        durations = [
            Fraction(
                row["duration_beats"]["numerator"],
                row["duration_beats"]["denominator"],
            )
            for row in expanded["segments"]
        ]
        self.assertEqual(durations, [Fraction(4), Fraction(4), Fraction(8)])
        self.assertEqual(expanded["phrase_length_fraction"], Fraction(16))

    def test_hiphop2_retrigger_is_not_continuation(self) -> None:
        expanded = h4.expand_pattern(
            ["T001", "T002"],
            "hiphop2",
            ["1N", "0.75X", "2.25S", "0.75X", "1.25N", "2S"],
            base_duration=Fraction(1),
        )
        kinds = [row["kind"] for row in expanded["segments"]]
        self.assertEqual(
            kinds,
            [
                "CHORD_ONSET",
                "REST_PATTERN",
                "CHORD_RETRIGGER",
                "REST_PATTERN",
                "CHORD_ONSET",
                "CHORD_RETRIGGER",
            ],
        )
        self.assertEqual(sum(row["continuation"] for row in expanded["segments"]), 0)
        self.assertEqual(sum(row["same_chord_retrigger"] for row in expanded["segments"]), 2)
        self.assertEqual(expanded["phrase_length_fraction"], Fraction(8))

    def test_source_rest_is_preserved_and_default_uses_basic_duration_two(self) -> None:
        pattern, base = h4.resolve_pattern("", ["T001", "REST", "T002"])
        self.assertEqual((pattern, base), ("basic", Fraction(2)))
        expanded = h4.expand_pattern(
            ["T001", "REST", "T002"],
            "basic",
            ["N", "X"],
            base_duration=base,
        )
        kinds = [row["kind"] for row in expanded["segments"]]
        self.assertIn("REST_SOURCE", kinds)
        self.assertIn("REST_PATTERN", kinds)

    def test_f5_is_harmony_independent_but_f6_is_not(self) -> None:
        normalization, fingerprints, functional = dependencies(
            [
                ("Major:001", ["I", "V", "I"], []),
                ("Major:002", ["I", "IV", "I"], []),
            ]
        )
        h2_rows = {row["source_id"]: row for row in fingerprints["definitions"]}
        h3_rows = {row["source_id"]: row for row in functional["definitions"]}
        patterns = {"pop": ["1.5N", "2.5N", "1.5N", "2.5N"]}
        left = h4.make_observation(
            normalization["definitions"][0], h2_rows["Major:001"], h3_rows["Major:001"], "pop", patterns
        )
        right = h4.make_observation(
            normalization["definitions"][1], h2_rows["Major:002"], h3_rows["Major:002"], "pop", patterns
        )
        self.assertEqual(left["fingerprints"]["F5"], right["fingerprints"]["F5"])
        self.assertNotEqual(left["fingerprints"]["F3"], right["fingerprints"]["F3"])
        self.assertNotEqual(left["fingerprints"]["F6"], right["fingerprints"]["F6"])

    def test_f5_does_not_include_style_name(self) -> None:
        segments = h4.expand_pattern(
            ["T001", "T002"], "a", ["N", "N"], base_duration=Fraction(1)
        )["segments"]
        same_segments = copy.deepcopy(segments)
        self.assertEqual(
            h4.namespaced_sha256("F5", h4.f5_payload(segments)),
            h4.namespaced_sha256("F5", h4.f5_payload(same_segments)),
        )

    def test_pop_and_pop2_have_distinct_f5_for_four_chords(self) -> None:
        refs = ["T001", "T002", "T003", "T004"]
        pop = h4.expand_pattern(
            refs, "pop", ["1.5N", "2.5N", "1.5N", "2.5N"], base_duration=Fraction(1)
        )["segments"]
        pop2 = h4.expand_pattern(
            refs, "pop2", ["1.75N", "2.25N", "1.75N", "2.25N"], base_duration=Fraction(1)
        )["segments"]
        self.assertNotEqual(h4.f5_payload(pop), h4.f5_payload(pop2))

    def test_f0_f4_drift_is_rejected(self) -> None:
        normalization, fingerprints, functional = dependencies(
            [("Major:001", ["I", "V", "I"], [])]
        )
        functional["definitions"][0]["fingerprints"]["F4"] = "0" * 64
        with self.assertRaises(h4.RhythmExtractionError):
            h4.verify_f0_f4_stability(
                normalization,
                fingerprints,
                functional,
                h1_digest="synthetic-h1",
                h2_digest="synthetic-h2",
            )

    def test_wrong_dependency_digest_is_rejected(self) -> None:
        with self.assertRaises(h4.RhythmExtractionError):
            h4.verify_json_bytes(b"{}", "0" * 64, "synthetic")

    def test_style_observations_do_not_imply_harmonic_support(self) -> None:
        normalization, fingerprints, functional = dependencies(
            [("Major:001", ["I", "V", "I"], [])]
        )
        h2_row = fingerprints["definitions"][0]
        h3_row = functional["definitions"][0]
        patterns = {
            "long": ["4N"],
            "pop": ["1.5N", "2.5N", "1.5N", "2.5N"],
            "pop2": ["1.75N", "2.25N", "1.75N", "2.25N"],
            "hiphop2": ["1N", "0.75X", "2.25S", "0.75X", "1.25N", "2S"],
            "soul": ["N", "S", "1.5N", "2N", "2.5N"],
        }
        observations = [
            h4.make_observation(
                normalization["definitions"][0], h2_row, h3_row, style, patterns
            )
            for style in ("", "pop", "pop2", "hiphop2", "soul")
        ]
        cross = h4.harmonic_rhythm_cross_table(observations)
        self.assertEqual(cross["logical_source_count"], 1)
        self.assertEqual(cross["rhythm_observation_count"], 5)
        self.assertEqual(cross["f3_class_count"], 1)

    def test_renderer_pattern_literal_shape(self) -> None:
        assignments = {
            "N": "N",
            "S": "S",
            "X": "X",
            "patterns": {"long": ["4N"], "basic": ["N", "X"]},
        }
        self.assertEqual(
            h4.require_patterns(assignments),
            {"long": ["4N"], "basic": ["N", "X"]},
        )


if __name__ == "__main__":
    unittest.main()
