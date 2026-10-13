#!/usr/bin/env python3
"""Source-specific Roman parser contracts for the pinned progression corpus."""

import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/harmony"))

from normalize_roman import (  # noqa: E402
    RomanParseError,
    normalize_absolute_chords,
    normalize_roman,
)

FIXTURE = json.loads(
    (ROOT / "tests/fixtures/harmony/roman_cases.json").read_text(encoding="utf-8")
)


class HarmonyRomanImportTests(unittest.TestCase):
    def test_source_notation_resolves_to_pinned_semantics(self):
        for case in FIXTURE["cases"]:
            with self.subTest(case=case["name"]):
                result = normalize_roman(
                    case["text"], {"tonality": case["key_context"]}, case["dialect"]
                )
                route = result["route"]
                self.assertEqual([item["root_offset"] for item in route],
                                 case["expected_roots"])
                self.assertEqual([item["quality_id"] for item in route],
                                 case["expected_quality_ids"])
                if "expected_spellings" in case:
                    self.assertEqual([item["source_spelling"] for item in route],
                                     case["expected_spellings"])
                if "expected_bass_roots" in case:
                    self.assertEqual([item["slash_bass_offset"] for item in route],
                                     case["expected_bass_roots"])
                    self.assertEqual([item["slash_bass_interval"] for item in route],
                                     case["expected_bass_intervals"])
                else:
                    self.assertTrue(all(item["slash_bass_offset"] is None
                                        for item in route))
                    self.assertTrue(all(item["slash_bass_interval"] is None
                                        for item in route))
                self.assertEqual(len(result["source_tokens"]), len(route))

    def test_parse_errors_retain_the_original_token_and_reason(self):
        for case in FIXTURE["errors"]:
            with self.subTest(case=case["name"]):
                with self.assertRaises(RomanParseError) as raised:
                    normalize_roman(
                        case["text"], {"tonality": case["key_context"]}, case["dialect"]
                    )
                self.assertEqual(raised.exception.token, case["token"])
                self.assertTrue(raised.exception.reason)
                if "reason" in case:
                    self.assertEqual(raised.exception.code, case["reason"])

    def test_transposition_equivalent_routes_have_same_canonical_identity(self):
        pair = FIXTURE["transposition_pair"]
        left = normalize_roman(pair["a"]["text"], {"tonality": pair["a"]["key_context"]},
                               pair["a"]["dialect"])
        right = normalize_roman(pair["b"]["text"], {"tonality": pair["b"]["key_context"]},
                                pair["b"]["dialect"])
        identity = lambda result: [
            (row["root_offset"], row["quality_id"], row["slash_bass_offset"])
            for row in result["route"]
        ]
        self.assertEqual(identity(left), identity(right))
        self.assertNotEqual(pair["key_a"], pair["key_b"])
        absolute_a = normalize_absolute_chords(pair["absolute_a"], pair["key_a"])
        absolute_b = normalize_absolute_chords(pair["absolute_b"], pair["key_b"])
        self.assertEqual(identity(left), identity(absolute_a))
        self.assertEqual(identity(absolute_a), identity(absolute_b))

    def test_parser_covers_every_row_of_pinned_progression_fixture(self):
        source_fixture_path = ROOT / "tests/fixtures/harmony/ldrolez_progressions.json"
        source_fixture = json.loads(source_fixture_path.read_text(encoding="utf-8"))
        accepted = 0
        quarantined = []
        for group in source_fixture["groups"]:
            for row in group["rows"]:
                with self.subTest(group=group["name"], line=row["line"]):
                    try:
                        result = normalize_roman(
                            row["route_text"], {"tonality": group["key_context"]},
                            group["dialect"]
                        )
                    except RomanParseError as error:
                        self.assertEqual(error.code, "unsupported_suffix")
                        self.assertIn(error.token, source_fixture["documented_unsupported_tokens"])
                        quarantined.append(row)
                    else:
                        self.assertEqual(len(result["route"]), row["token_count"])
                        accepted += 1
        self.assertEqual(accepted, source_fixture["expected_accepted_count"])
        self.assertEqual(len(quarantined), source_fixture["expected_quarantined_count"])
        self.assertEqual(accepted + len(quarantined), source_fixture["row_count"])
        self.assertEqual(source_fixture["row_count"], 195)


if __name__ == "__main__":
    unittest.main()
