#!/usr/bin/env python3
"""Genre policy matrix, hard-budget and reproducible codegen contracts."""

import copy
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/harmony"))

from build_genre_harmonic_policy import (  # noqa: E402
    compile_policy,
    render_header,
    render_report,
)

SOURCE_PATH = ROOT / "tools/harmony/genre_policy_source.json"
QUALITY_PATH = ROOT / "tools/harmony/quality_registry.json"
SCALE_PATH = ROOT / "tools/harmony/scale_registry.json"

EXPECTED = [
    ("Acid", [2, 1, 0, 2, 2, 0, 0, 1, 4], 0, 1, [1, 1, 2], [0, 0], 0, 0, 1),
    ("Detroit Techno", [2, 3, 1, 2, 2, 1, 1, 3, 2], 2, 2, [1, 2, 3], [1, 1], 1, 1, 3),
    ("Electro", [2, 2, 1, 2, 3, 1, 1, 3, 2], 1, 1, [1, 2, 3], [1, 1], 1, 1, 3),
    ("EBM", [1, 1, 0, 3, 4, 1, 0, 2, 3], 1, 1, [1, 2, 3], [0, 1], 1, 0, 2),
    ("UK Garage", [3, 3, 3, 2, 1, 1, 3, 4, 2], 2, 2, [0, 1, 2], [1, 1], 1, 2, 4),
    ("Dub Techno", [2, 2, 1, 2, 1, 0, 2, 1, 4], 2, 1, [0, 1, 1], [0, 0], 0, 2, 1),
    ("Ambient", [2, 4, 1, 3, 2, 3, 4, 4, 1], 3, 3, [1, 2, 4], [1, 2], 1, 3, 4),
    ("Lo-Fi", [3, 3, 3, 3, 1, 1, 3, 3, 2], 2, 2, [0, 1, 2], [1, 1], 1, 2, 3),
    ("Chillout", [3, 3, 2, 2, 1, 1, 3, 3, 2], 2, 2, [0, 1, 2], [1, 1], 1, 2, 3),
    ("Deep House", [3, 3, 2, 2, 1, 1, 3, 3, 3], 2, 2, [0, 1, 2], [1, 1], 1, 2, 3),
    ("Jungle", [2, 1, 1, 2, 2, 0, 0, 1, 4], 0, 1, [0, 1, 2], [0, 0], 0, 0, 1),
    ("Trap", [2, 2, 1, 3, 3, 1, 1, 2, 3], 1, 1, [1, 2, 3], [0, 1], 1, 1, 2),
    ("Hip-Hop", [3, 3, 2, 3, 2, 1, 3, 3, 2], 2, 2, [0, 1, 2], [1, 1], 1, 2, 3),
    ("Footwork", [1, 1, 0, 2, 3, 0, 0, 1, 4], 0, 1, [1, 2, 3], [0, 0], 0, 0, 1),
    ("DnB", [2, 1, 1, 2, 2, 0, 1, 1, 4], 1, 1, [0, 1, 2], [0, 0], 0, 1, 1),
    ("Salsa", [4, 0, 4, 1, 1, 0, 1, 4, 3], 2, 1, [0, 1, 1], [1, 1], 0, 1, 4),
    ("Synthwave", [4, 3, 2, 2, 1, 2, 2, 3, 2], 2, 2, [0, 1, 2], [1, 1], 1, 2, 3),
]


class GenreMatrixTests(unittest.TestCase):
    def load(self):
        return (
            json.loads(SOURCE_PATH.read_text()),
            json.loads(QUALITY_PATH.read_text()),
            json.loads(SCALE_PATH.read_text()),
        )

    def test_all_seventeen_rows_match_the_versioned_contract(self):
        source, quality, scale = self.load()
        compiled = compile_policy(source, quality, scale)
        self.assertEqual(len(compiled["profiles"]), 17)
        for row, expected in zip(compiled["profiles"], EXPECTED):
            (name, weights, q_tier, s_tier, outside, modulations,
             portals, color, changes) = expected
            self.assertEqual(row["name"], name)
            self.assertEqual(row["weights"], weights)
            self.assertEqual(row["quality_tier"], q_tier)
            self.assertEqual(row["scale_tier"], s_tier)
            self.assertEqual(
                [row["outside_take_keep"], row["outside_develop"], row["outside_break"]],
                outside,
            )
            self.assertEqual([row["modulations_develop"], row["modulations_break"]],
                             modulations)
            self.assertEqual(row["allow_portals"], bool(portals))
            self.assertEqual(row["color_budget"], color)
            self.assertEqual(row["changes_per_section"], changes)

    def test_quality_and_scale_masks_are_intersected_with_runtime_capability(self):
        source, quality, scale = self.load()
        compiled = compile_policy(source, quality, scale)
        q_runtime = quality["runtime_bootstrap_count"]
        s_runtime = scale["runtime_bootstrap_count"]
        for row in compiled["profiles"]:
            self.assertEqual(row["quality_ids"], source["quality_tier_runtime_ids"][str(row["quality_tier"])])
            self.assertEqual(row["scale_ids"], source["scale_tier_runtime_ids"][str(row["scale_tier"])])
            self.assertTrue(all(0 <= item < q_runtime for item in row["quality_ids"]))
            self.assertTrue(all(0 <= item < s_runtime for item in row["scale_ids"]))
        ambient = compiled["profiles"][6]
        self.assertIn(4, ambient["quality_ids"])
        self.assertLess(q_runtime, len(quality["qualities"]))

    def test_feature_coefficients_follow_the_documented_integer_formula(self):
        source, quality, scale = self.load()
        compiled = compile_policy(source, quality, scale)
        for row in compiled["profiles"]:
            dia, prl, functional, modal, chromatic, augmented, _poly, mobility, home = row["weights"]
            weights = row["feature_weights"]
            self.assertEqual(weights, {
                "common_tone": 2 + prl,
                "pcvl_penalty": 1 + home,
                "fifths_distance_penalty": functional,
                "out_of_scale_penalty": 5 - chromatic,
                "large_root_motion_penalty": 5 - mobility,
                "diatonic_tag": dia * 4,
                "prl_tag": prl * 3,
                "functional_tag": functional * 4,
                "modal_tag": modal * 4,
                "chromatic_mediant_tag": chromatic * 2,
                "portal_tag": augmented * 4,
                "interval_vector": 1,
                "complexity": 2,
                "corpus": 2,
            })

    def test_acid_functional_preparation_is_hard_disabled_and_salsa_enabled(self):
        source, quality, scale = self.load()
        profiles = compile_policy(source, quality, scale)["profiles"]
        self.assertFalse(profiles[0]["functional_preparation"])
        self.assertTrue(profiles[15]["functional_preparation"])

    def test_source_profile_order_does_not_change_generated_artifacts(self):
        source, quality, scale = self.load()
        first = compile_policy(source, quality, scale)
        permuted = copy.deepcopy(source)
        permuted["profiles"].reverse()
        second = compile_policy(permuted, quality, scale)
        self.assertEqual(render_header(first), render_header(second))
        self.assertEqual(render_report(first), render_report(second))

    def test_checked_in_policy_and_report_are_reproducible(self):
        source, quality, scale = self.load()
        compiled = compile_policy(source, quality, scale)
        header = ROOT / "src/generation/harmony/generated/genre_harmonic_policy_data.h"
        report = ROOT / "tools/harmony/generated/genre_harmonic_policy_report.json"
        self.assertEqual(render_header(compiled), header.read_text())
        self.assertEqual(render_report(compiled), json.loads(report.read_text()))

    def test_out_of_range_weights_and_budget_are_rejected(self):
        source, quality, scale = self.load()
        malformed = copy.deepcopy(source)
        malformed["profiles"][0]["weights"][0] = 5
        with self.assertRaises(ValueError):
            compile_policy(malformed, quality, scale)
        malformed = copy.deepcopy(source)
        malformed["profiles"][0]["outside_break"] = 5
        with self.assertRaises(ValueError):
            compile_policy(malformed, quality, scale)


if __name__ == "__main__":
    unittest.main()
