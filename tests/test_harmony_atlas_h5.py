#!/usr/bin/env python3
from __future__ import annotations

import sys
import unittest
from fractions import Fraction
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools" / "research"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

import harmony_atlas_h5 as h5


def quality(triad: str, extension: str = "NONE", seventh: str = "NONE", fifth: int = 0):
    return triad, extension, seventh, fifth


def frac(value: Fraction):
    return {"numerator": value.numerator, "denominator": value.denominator}


def segment(kind: str, duration: Fraction):
    return {"kind": kind, "duration_beats": frac(duration)}


class HarmonyAtlasH5Tests(unittest.TestCase):
    def test_generic_seventh_is_not_silently_dominant(self) -> None:
        self.assertEqual(h5.qclass(quality("MAJOR", "SEVENTH", "DOMINANT")), ("EXACT_ENUM_LABEL", "Dominant7"))
        self.assertEqual(h5.qclass(quality("MAJOR", "SEVENTH", "MAJOR")), ("EXACT_ENUM_LABEL", "Major7"))
        self.assertEqual(h5.qclass(quality("MAJOR", "SEVENTH", "UNSPECIFIED")), ("UNREPRESENTABLE_QUALITY", None))

    def test_plain_major_minor_triads_are_context_dependent(self) -> None:
        self.assertEqual(h5.qclass(quality("MAJOR")), ("CONTEXT_DEPENDENT_TRIAD", "Triad"))
        self.assertEqual(h5.qclass(quality("MINOR")), ("CONTEXT_DEPENDENT_TRIAD", "Triad"))

    def test_quality_vocabulary_is_loss_aware(self) -> None:
        self.assertEqual(h5.qclass(quality("MINOR", "NINTH", "UNSPECIFIED")), ("EXACT_ENUM_LABEL", "Minor9"))
        self.assertEqual(h5.qclass(quality("SUSPENDED_4")), ("EXACT_ENUM_LABEL", "Sus4"))
        self.assertEqual(h5.qclass(quality("SUSPENDED_2")), ("UNREPRESENTABLE_QUALITY", None))
        self.assertEqual(h5.qclass(quality("POWER_5")), ("UNREPRESENTABLE_QUALITY", None))

    def test_root_only_match_is_not_quality_match(self) -> None:
        grammar = h5.TARGET_GRAMMARS["PopCycle"][0]
        roots = [(degree, offset) for degree, offset, _ in grammar]
        self.assertTrue(h5.cyclic_match(roots, roots))
        wrong = list(grammar)
        wrong[0] = (0, 0, "Major7")
        self.assertFalse(h5.cyclic_match(wrong, grammar))

    def test_whole_bar_hold_is_fixed_catalog_match(self) -> None:
        shape = {"canonical_f5_segments": [segment("CHORD_ONSET", Fraction(4))]}
        self.assertEqual(h5.fixed_rhythm_matches(shape), ["WholeBarHold"])

    def test_half_bar_change_is_fixed_catalog_match(self) -> None:
        shape = {"canonical_f5_segments": [segment("CHORD_ONSET", Fraction(2)), segment("CHORD_ONSET", Fraction(2))]}
        self.assertEqual(h5.fixed_rhythm_matches(shape), ["HalfBarChange"])

    def test_retrigger_is_not_equivalent_to_harmonic_advance(self) -> None:
        shape = {"canonical_f5_segments": [segment("CHORD_ONSET", Fraction(2)), segment("CHORD_RETRIGGER", Fraction(2))]}
        self.assertEqual(h5.fixed_rhythm_matches(shape), [])

    def test_catalog_match_requires_one_bar(self) -> None:
        shape = {"canonical_f5_segments": [segment("CHORD_ONSET", Fraction(8))]}
        self.assertEqual(h5.fixed_rhythm_matches(shape), [])

    def test_finer_than_quarter_beat_segment_is_not_fixed_catalog(self) -> None:
        shape = {"canonical_f5_segments": [segment("CHORD_ONSET", Fraction(3, 8)), segment("REST", Fraction(29, 8))]}
        self.assertEqual(h5.fixed_rhythm_matches(shape), [])

    def test_capability_support_uses_logical_definitions(self) -> None:
        harmonic = {
            "definition_count": 190,
            "quality_definition_support": {"CONTEXT_DEPENDENT_TRIAD": 185, "UNREPRESENTABLE_QUALITY": 29},
            "definitions_with_altered_degree": 71,
            "definitions_over_max_harmonic_events": 1,
        }
        rhythm = {
            "gap_logical_definition_support": {
                "LIVE_ONE_BAR_DURATION": 190,
                "SAME_CHORD_RETRIGGER_SEMANTICS": 190,
                "GENERIC_PHRASE_GT4_BARS": 32,
                "HARMONIC_EVENTS_GT8": 32,
            },
            "gap_observation_counts": {
                "LIVE_ONE_BAR_DURATION": 950,
                "SAME_CHORD_RETRIGGER_SEMANTICS": 380,
                "GENERIC_PHRASE_GT4_BARS": 67,
                "HARMONIC_EVENTS_GT8": 55,
            },
        }
        rows = h5.capability_ranking(harmonic, rhythm)
        self.assertTrue(all(row["logical_definition_support"] <= 190 for row in rows))
        self.assertTrue(all(row["support_is_non_additive"] for row in rows))
        multi = next(row for row in rows if row["capability"] == "MULTI_BAR_CHORD_RHYTHM_IDENTITY")
        self.assertEqual(multi["logical_definition_support"], 190)
        self.assertEqual(multi["rhythm_observation_count"], 950)

    def test_enum_parser_is_bounded(self) -> None:
        text = "enum class X : uint8_t { A = 0, B, Count, };"
        self.assertEqual(h5.enum_members(text, "X"), ["A", "B", "Count"])


if __name__ == "__main__":
    unittest.main()
