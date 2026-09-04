#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "tools/gf2_gate_b.py"
SEEDS_PATH = ROOT / "tests/support/gf2_gate_b_seeds.tsv"


def load_gate_b_module():
    spec = importlib.util.spec_from_file_location("gf2_gate_b", MODULE_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {MODULE_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def applied_row(
    *,
    profile_id: str = "Fixture/Profile",
    seed: str = "0x00000001",
    depth: str = "P1",
    kick_onsets: str = "8000",
    secondary_role: str = "0",
    physical_event_count: str = "10",
) -> dict[str, str]:
    return {
        "profile_id": profile_id,
        "genre": "Fixture",
        "recipe": "Profile",
        "seed": seed,
        "depth": depth,
        "migration_status": "APPLIED",
        "kick_onsets": kick_onsets,
        "backbeat_onsets": "0000",
        "hat_onsets": "0000",
        "support_onsets": "0000",
        "kick_accents": "0000",
        "backbeat_accents": "0000",
        "hat_accents": "0000",
        "support_accents": "0000",
        "drum_timing": "",
        "synth_a_onsets": "0000",
        "synth_b_onsets": "0000",
        "synth_a_accents": "0000",
        "synth_b_accents": "0000",
        "synth_a_ghosts": "0000",
        "synth_b_ghosts": "0000",
        "synth_a_timing": "",
        "synth_b_timing": "",
        "synth_a_pitch_class": "",
        "synth_b_pitch_class": "",
        "synth_a_contour": "",
        "synth_b_contour": "",
        "harmonic_event_onsets": "8000",
        "harmonic_event_count": "1",
        "chord_onsets": "8000",
        "melodic_fill_onsets": "0000",
        "chord_applied": "YES",
        "melodic_applied": "NO",
        "synth_b_role": secondary_role,
        "physical_event_count": physical_event_count,
        "phrase_admitted": "NO",
        "phrase_materialization_status": "NOT_OBSERVED",
        "phrase_material": "",
        "phrase_reject_reason": "NO_ADMISSIBLE_LAW",
    }


def depth_triplet(
    *,
    profile_id: str,
    seed: str,
    kick_masks: tuple[str, str, str],
    event_counts: tuple[int, int, int],
    secondary_role: str,
) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for depth, mask, events in zip(("P1", "P2", "P3"), kick_masks, event_counts):
        rows.append(
            applied_row(
                profile_id=profile_id,
                seed=seed,
                depth=depth,
                kick_onsets=mask,
                secondary_role=secondary_role,
                physical_event_count=str(events),
            )
        )
    return rows


class GateBAnalysisContractTests(unittest.TestCase):
    def test_analyzer_exists_before_contract_is_exercised(self) -> None:
        self.assertTrue(
            MODULE_PATH.exists(),
            "Gate B RED: tools/gf2_gate_b.py has not been implemented yet",
        )

    def test_frozen_seed_contract(self) -> None:
        gate_b = load_gate_b_module()
        seeds = gate_b.load_seeds(SEEDS_PATH)
        self.assertEqual(55, len(seeds))
        self.assertEqual(55, len(set(seeds)))
        self.assertEqual(0, seeds[0])
        for seed in seeds:
            self.assertGreaterEqual(seed, 0)
            self.assertLessEqual(seed, 0xFFFFFFFF)
            self.assertNotEqual(0xFFFF, seed & 0xFFFF)

    def test_relation_model_is_dimension_level_and_exact(self) -> None:
        gate_b = load_gate_b_module()
        self.assertEqual("SAME", gate_b.relation_for_sequences(("a", "b"), ("a", "b")))
        self.assertEqual("DISJOINT", gate_b.relation_for_sequences(("a", "b"), ("c", "d")))
        self.assertEqual("OVERLAP", gate_b.relation_for_sequences(("a", "b"), ("a", "c")))
        self.assertEqual("NOT_OBSERVED", gate_b.relation_for_sequences((), ()))

    def test_pair_classification_requires_structural_evidence(self) -> None:
        gate_b = load_gate_b_module()
        same = {
            "rhythm": "SAME",
            "bass": "SAME",
            "harmony": "SAME",
            "phrase": "SAME",
            "role": "SAME",
            "transformation": "SAME",
            "negative": "SAME",
        }
        self.assertEqual(
            "STRUCTURALLY REDUNDANT",
            gate_b.pair_classification(same, timbre_evidence=False),
        )

        one_axis = dict(same)
        one_axis["rhythm"] = "DISJOINT"
        self.assertEqual(
            "PARTIALLY DISTINCT",
            gate_b.pair_classification(one_axis, timbre_evidence=False),
        )

        two_axes = dict(same)
        two_axes["rhythm"] = "DISJOINT"
        two_axes["role"] = "DISJOINT"
        self.assertEqual(
            "STRUCTURALLY DISTINCT",
            gate_b.pair_classification(two_axes, timbre_evidence=False),
        )

        insufficient = {key: "NOT_OBSERVED" for key in same}
        self.assertEqual(
            "INSUFFICIENT EVIDENCE",
            gate_b.pair_classification(insufficient, timbre_evidence=False),
        )

    def test_negative_signature_cannot_double_vote_source_dimension(self) -> None:
        gate_b = load_gate_b_module()
        relations = {
            "rhythm": "DISJOINT",
            "bass": "SAME",
            "harmony": "SAME",
            "phrase": "SAME",
            "role": "SAME",
            "transformation": "SAME",
            "negative": "DISJOINT",
        }
        self.assertEqual(
            "PARTIALLY DISTINCT",
            gate_b.pair_classification(relations, timbre_evidence=False),
            "NegativeSignature must explain prohibition differences without becoming a second structural vote",
        )

    def test_timbre_dependent_is_impossible_without_positive_observation(self) -> None:
        gate_b = load_gate_b_module()
        relations = {
            "rhythm": "SAME",
            "bass": "SAME",
            "harmony": "SAME",
            "phrase": "SAME",
            "role": "SAME",
            "transformation": "SAME",
            "negative": "SAME",
        }
        self.assertNotEqual(
            "TIMBRE-DEPENDENT",
            gate_b.pair_classification(relations, timbre_evidence=False),
        )
        self.assertEqual(
            "TIMBRE-DEPENDENT",
            gate_b.pair_classification(relations, timbre_evidence=True),
        )

    def test_harmony_signature_does_not_own_secondary_role_identity(self) -> None:
        gate_b = load_gate_b_module()
        chord_role = applied_row(secondary_role="0")
        melodic_role = applied_row(secondary_role="1")

        chord_payloads = gate_b._signature_payloads(chord_role)
        melodic_payloads = gate_b._signature_payloads(melodic_role)

        self.assertEqual(
            chord_payloads["harmony"],
            melodic_payloads["harmony"],
            "pure secondaryRole identity changes belong to RoleSignature, not HarmonySignature",
        )
        self.assertNotEqual(chord_payloads["role"], melodic_payloads["role"])

    def test_phrase_signature_is_relative_temporal_form(self) -> None:
        gate_b = load_gate_b_module()
        first = {
            "phrase_admitted": "YES",
            "phrase_materialization_status": "ALL_APPLIED",
            "phrase_material": "0@STATEMENT@BASE_A|1@RESPONSE@CHANGE_A|2@RETURN@BASE_A",
        }
        transposed_identity = {
            "phrase_admitted": "YES",
            "phrase_materialization_status": "ALL_APPLIED",
            "phrase_material": "0@STATEMENT@BASE_B|1@RESPONSE@CHANGE_B|2@RETURN@BASE_B",
        }
        different_form = {
            "phrase_admitted": "YES",
            "phrase_materialization_status": "ALL_APPLIED",
            "phrase_material": "0@STATEMENT@BASE_C|1@RESPONSE@CHANGE_C|2@RETURN@OTHER_C",
        }

        first_signature = gate_b._phrase_metrics(first)["signature_payload"]
        equivalent_signature = gate_b._phrase_metrics(transposed_identity)["signature_payload"]
        different_signature = gate_b._phrase_metrics(different_form)["signature_payload"]

        self.assertEqual(
            first_signature,
            equivalent_signature,
            "PhraseSignature must encode relative temporal form, not absolute bar material identity",
        )
        self.assertNotEqual(first_signature, different_signature)

    def test_transformation_signature_is_relative_depth_intervention(self) -> None:
        gate_b = load_gate_b_module()
        rows = depth_triplet(
            profile_id="Fixture/A",
            seed="0x00000001",
            kick_masks=("8000", "c000", "e000"),
            event_counts=(10, 12, 14),
            secondary_role="0",
        )
        rows += depth_triplet(
            profile_id="Fixture/B",
            seed="0x00000002",
            kick_masks=("0800", "0c00", "0e00"),
            event_counts=(20, 22, 24),
            secondary_role="1",
        )
        different = depth_triplet(
            profile_id="Fixture/C",
            seed="0x00000003",
            kick_masks=("0080", "00c0", "00c0"),
            event_counts=(30, 32, 32),
            secondary_role="2",
        )
        rows += different

        gate_b._prepare_rows(rows)
        ids = {
            row["profile_id"]: row["transformation_signature_id"]
            for row in rows
            if row["depth"] == "P1"
        }
        self.assertEqual(
            ids["Fixture/A"],
            ids["Fixture/B"],
            "TransformationSignature must describe equivalent P1→P2→P3 deltas independent of absolute P1 identity",
        )
        self.assertNotEqual(ids["Fixture/A"], ids["Fixture/C"])

    def test_pair_count_and_ids_are_deterministic(self) -> None:
        gate_b = load_gate_b_module()
        self.assertEqual(528, gate_b.expected_pair_count(33))
        a = gate_b.canonical_id("rhythm", "kick=1111")
        b = gate_b.canonical_id("rhythm", "kick=1111")
        c = gate_b.canonical_id("bass", "kick=1111")
        self.assertEqual(a, b)
        self.assertNotEqual(a, c)
        self.assertTrue(a.startswith("RHYTHM-"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
