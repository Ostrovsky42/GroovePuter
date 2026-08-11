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


class HarmonyAtlasH6Tests(unittest.TestCase):
    def group(self, *, requirements=(), source_id="Major:001", family="Major", org="FOUR_CHORD_LOOP", closure="OPEN_LOOP", cadence="NO_CADENCE", events=4, strategy="NEW_GENERIC_TEMPLATE", bytes_=13, support=1):
        return {
            "F3": "f3-" + source_id,
            "source_ids": [source_id] * support,
            "logical_definition_support": support,
            "support_used_as_popularity_weight": False,
            "representative_source_id": source_id,
            "source_family": family,
            "organization_class": org,
            "closure_class": closure,
            "cadence_class": cadence,
            "harmonic_event_count": events,
            "requirements": sorted(requirements),
            "encoding_strategy": strategy,
            "current_root_path_matches": [],
            "reference_payload_bytes": bytes_,
            "events": [],
        }

    def test_quality_render_is_required_for_exact_f3_candidate(self) -> None:
        row = self.group(requirements=[h6.QUALITY_RENDER])
        self.assertFalse(h6.eligible_harmonic(row, set()))
        self.assertTrue(h6.eligible_harmonic(row, {h6.QUALITY_RENDER}))

    def test_extra_quality_and_triad_polarity_are_independent(self) -> None:
        triad = self.group(requirements=[h6.QUALITY_RENDER, h6.TRIAD_POLARITY])
        generic7 = self.group(source_id="Major:002", requirements=[h6.QUALITY_RENDER, h6.EXTRA_QUALITY])
        caps = {h6.QUALITY_RENDER, h6.TRIAD_POLARITY}
        self.assertTrue(h6.eligible_harmonic(triad, caps))
        self.assertFalse(h6.eligible_harmonic(generic7, caps))

    def test_altered_reachability_is_not_implied_by_numeric_field(self) -> None:
        altered = self.group(requirements=[h6.QUALITY_RENDER, h6.ALTERED_REACH])
        self.assertFalse(h6.eligible_harmonic(altered, {h6.QUALITY_RENDER}))
        self.assertTrue(h6.eligible_harmonic(altered, {h6.QUALITY_RENDER, h6.ALTERED_REACH}))

    def test_selection_does_not_use_source_support_as_popularity(self) -> None:
        a = self.group(source_id="Major:001", support=9, bytes_=13)
        b = self.group(source_id="Major:002", family="Minor", support=1, bytes_=13)
        selected = h6.select_harmonic_candidates([a, b], {h6.QUALITY_RENDER}, 2)
        self.assertEqual(len(selected), 2)
        self.assertTrue(all(row["support_used_as_popularity_weight"] is False for row in selected))

    def test_selection_prefers_structural_diversity_before_payload_tie_break(self) -> None:
        a = self.group(source_id="Major:001", family="Major", org="FOUR_CHORD_LOOP")
        b = self.group(source_id="Minor:001", family="Minor", org="THREE_CHORD_LOOP", bytes_=20)
        c = self.group(source_id="Major:002", family="Major", org="FOUR_CHORD_LOOP", bytes_=4, strategy="ROOT_PATH_OVERLAY")
        selected = h6.select_harmonic_candidates([a, b, c], {h6.QUALITY_RENDER}, 2)
        self.assertEqual({row["source_family"] for row in selected}, {"Major", "Minor"})

    def observation(self, *, phrase_num=16, phrase_den=1, retrigger=0, onsets=4, style="default"):
        return {
            "phrase_length_beats": {"numerator": phrase_num, "denominator": phrase_den},
            "same_chord_retrigger_count": retrigger,
            "note_onset_count": onsets,
            "source_style": style,
        }

    def test_multibar_is_required_for_h4_candidate_identity(self) -> None:
        obs = self.observation()
        self.assertFalse(h6.rhythm_observation_exact(obs, set(), style_allowed=True))
        self.assertTrue(h6.rhythm_observation_exact(obs, {h6.MULTI_BAR}, style_allowed=True))

    def test_retrigger_is_explicit_capability(self) -> None:
        obs = self.observation(retrigger=2, style="hiphop2")
        self.assertFalse(h6.rhythm_observation_exact(obs, {h6.MULTI_BAR}, style_allowed=True))
        self.assertTrue(h6.rhythm_observation_exact(obs, {h6.MULTI_BAR, h6.RETRIGGER}, style_allowed=True))

    def test_gt4_and_gt8_are_separate_rhythm_caps(self) -> None:
        obs = self.observation(phrase_num=20, onsets=9)
        caps = {h6.MULTI_BAR}
        self.assertFalse(h6.rhythm_observation_exact(obs, caps, style_allowed=True))
        self.assertFalse(h6.rhythm_observation_exact(obs, caps | {h6.RHYTHM_GT4}, style_allowed=True))
        self.assertTrue(h6.rhythm_observation_exact(obs, caps | {h6.RHYTHM_GT4, h6.ONSETS_GT8}, style_allowed=True))

    def test_style_candidate_is_not_popularity_weight(self) -> None:
        for row in h6.RHYTHM_GRAMMARS.values():
            self.assertIn("candidate_name", row)
        self.assertNotEqual(h6.RHYTHM_GRAMMARS["pop"]["candidate_name"], h6.RHYTHM_GRAMMARS["pop2"]["candidate_name"])

    def test_reference_payload_is_not_claimed_as_compiled_memory(self) -> None:
        self.assertEqual(h6.BUNDLES["BALANCED"]["harmonic_template_budget"], 8)
        self.assertIsNone(h6.RECOMMENDED_BUNDLE)

    def test_dominance_requires_no_worse_cost_and_coverage(self) -> None:
        a = {
            "bounded_r2_proposal": {"exact_unique_f3": 4, "exact_unique_f5": 8, "exact_unique_f6": 12},
            "reference_cost": {"total_candidate_payload_bytes": 40, "capability_complexity_points": 10},
        }
        b = {
            "bounded_r2_proposal": {"exact_unique_f3": 3, "exact_unique_f5": 8, "exact_unique_f6": 10},
            "reference_cost": {"total_candidate_payload_bytes": 45, "capability_complexity_points": 10},
        }
        self.assertTrue(h6.dominance(a, b))
        self.assertFalse(h6.dominance(b, a))


if __name__ == "__main__":
    unittest.main()
