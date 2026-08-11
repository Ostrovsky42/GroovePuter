#!/usr/bin/env python3
from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools" / "research"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

import harmony_atlas_h6 as h6
import harmony_atlas_h6_decision as decision
import harmony_atlas_h6_sweep as sweep


class HarmonyAtlasH6SweepTests(unittest.TestCase):
    def compact_row(self, styles, *, f6=10, f5=8, f3=4, complexity=10, payload=100, budget=8):
        macros = [sweep.RHYTHM_MACRO_FAMILY[style] for style in styles]
        return {
            "sweep_id": "SYNTH",
            "capabilities": [h6.QUALITY_RENDER, h6.MULTI_BAR],
            "harmonic_template_budget": budget,
            "rhythm_style_candidates": list(styles),
            "rhythm_macro_families": macros,
            "rhythm_grammar_count": len(styles),
            "rhythm_macro_family_count": len(set(macros)),
            "proposal": {
                "exact_unique_f3": f3,
                "exact_unique_f5": f5,
                "exact_unique_f6": f6,
                "exact_f5_observations": 10,
                "exact_f6_observations": f6,
            },
            "envelope": {"exact_unique_f3": 20, "exact_unique_f5": 20, "exact_unique_f6": 100},
            "cost": {"candidate_payload_bytes": payload, "capability_complexity_points": complexity},
        }

    def test_phase1_rejects_two_asymmetric_near_neighbor_slots(self) -> None:
        row = self.compact_row(["default", "pop", "pop2", "soul"])
        self.assertFalse(sweep.phase1_ok(row))

    def test_phase1_accepts_four_distinct_macro_families(self) -> None:
        row = self.compact_row(["default", "pop", "hiphop2", "soul"])
        self.assertTrue(sweep.phase1_ok(row))

    def test_phase1_rank_prefers_f6_before_cost(self) -> None:
        more_f6 = self.compact_row(["default", "pop"], f6=11, complexity=16, payload=150)
        cheaper = self.compact_row(["default", "pop"], f6=10, complexity=8, payload=80)
        self.assertLess(sweep.phase1_rank(more_f6), sweep.phase1_rank(cheaper))

    def test_pareto_keeps_cost_coverage_tradeoff(self) -> None:
        a = self.compact_row(["default"], f6=12, complexity=12, payload=120)
        a["sweep_id"] = "A"
        b = self.compact_row(["default"], f6=10, complexity=8, payload=80)
        b["sweep_id"] = "B"
        ids = {row["sweep_id"] for row in sweep.pareto([a, b])}
        self.assertEqual(ids, {"A", "B"})

    def test_selected_decision_stays_inside_current_generic_limits(self) -> None:
        self.assertNotIn(h6.RHYTHM_GT4, decision.SELECTED_SPEC["capabilities"])
        self.assertNotIn(h6.ONSETS_GT8, decision.SELECTED_SPEC["capabilities"])
        self.assertNotIn(h6.HARMONIC_GT8, decision.SELECTED_SPEC["capabilities"])
        self.assertEqual(decision.SELECTED_SPEC["harmonic_template_budget"], 8)
        self.assertEqual(decision.SELECTED_SPEC["rhythm_styles"], ["default", "pop", "hiphop2", "soul"])

    def test_asymmetric_editorial_choice_is_not_support_weight(self) -> None:
        self.assertEqual(sweep.RHYTHM_MACRO_FAMILY["pop"], sweep.RHYTHM_MACRO_FAMILY["pop2"])
        self.assertEqual(decision.TIED_ASYMMETRIC_ALTERNATE_SWEEP_ID, "S00618")

    def test_decision_spec_match_is_order_insensitive_for_capabilities_only(self) -> None:
        row = {
            "capabilities": list(reversed(decision.SELECTED_SPEC["capabilities"])),
            "harmonic_template_budget": 8,
            "rhythm_style_candidates": ["default", "pop", "hiphop2", "soul"],
        }
        self.assertTrue(decision.spec_matches(row, decision.SELECTED_SPEC))


if __name__ == "__main__":
    unittest.main()
