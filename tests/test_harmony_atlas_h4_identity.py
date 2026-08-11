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

import harmony_atlas_h4 as h4


class HarmonyAtlasH4IdentityTests(unittest.TestCase):
    def _segment(self, kind: str, *, rest: bool, retrigger: bool = False) -> dict:
        return {
            "kind": kind,
            "rest": rest,
            "same_chord_retrigger": retrigger,
            "duration_beats": h4.fraction_payload(Fraction(1)),
            "start_beats": h4.fraction_payload(Fraction(0)),
            "note_onset": not rest,
            "source_advance": kind == "CHORD_ONSET",
            "continuation": False,
        }

    def test_rest_origin_is_not_f5_identity(self) -> None:
        source_rest = self._segment("REST_SOURCE", rest=True)
        pattern_rest = self._segment("REST_PATTERN", rest=True)
        self.assertEqual(h4.f5_segment(source_rest), h4.f5_segment(pattern_rest))
        self.assertEqual(h4.f5_segment(source_rest)["kind"], "REST")

    def test_new_chord_and_same_chord_retrigger_are_distinct_f5_actions(self) -> None:
        onset = self._segment("CHORD_ONSET", rest=False)
        retrigger = self._segment("CHORD_RETRIGGER", rest=False, retrigger=True)
        self.assertNotEqual(h4.f5_segment(onset), h4.f5_segment(retrigger))

    def test_rhythm_shape_exposes_required_masks(self) -> None:
        expanded = h4.expand_pattern(
            ["T001", "T002"],
            "hiphop2",
            ["1N", "0.75X", "2.25S", "0.75X", "1.25N", "2S"],
            base_duration=Fraction(1),
        )
        observation = {
            "fingerprints": {"F5": h4.namespaced_sha256("F5", h4.f5_payload(expanded["segments"]))},
            "phrase_length_beats": expanded["phrase_length_beats"],
            "segments": expanded["segments"],
        }
        shape = h4.rhythm_shape(observation)
        self.assertEqual(len(shape["rest_mask"]), 6)
        self.assertEqual(sum(shape["rest_mask"]), 2)
        self.assertEqual(sum(shape["same_chord_retrigger_mask"]), 2)
        self.assertEqual(sum(shape["continuation_mask"]), 0)
        self.assertEqual(len(shape["note_onset_coordinates_beats"]), 4)


if __name__ == "__main__":
    unittest.main()
