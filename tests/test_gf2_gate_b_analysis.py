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
