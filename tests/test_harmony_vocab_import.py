#!/usr/bin/env python3
"""Pinned Fannon vocabulary normalization and stable registry contracts."""

import json
import hashlib
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/harmony"))

from import_fannon_vocab import (  # noqa: E402
    compile_vocabulary,
    normalize_quality,
    normalize_scale,
    registry_index,
)


FIXTURE = ROOT / "tests/fixtures/harmony/fannon_vocab.json"
QUALITY_REGISTRY = json.loads(
    (ROOT / "tools/harmony/quality_registry.json").read_text(encoding="utf-8")
)
SCALE_REGISTRY = json.loads(
    (ROOT / "tools/harmony/scale_registry.json").read_text(encoding="utf-8")
)


class HarmonyVocabImportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixture = json.loads(FIXTURE.read_text(encoding="utf-8"))

    def test_pinned_chord_examples_normalize_to_expected_pitch_classes(self):
        expected = {
            "": [0, 4, 7],
            "m": [0, 3, 7],
            "sus2": [0, 2, 7],
            "sus4": [0, 5, 7],
            "5": [0, 7],
            "7": [0, 4, 7, 10],
            "maj7": [0, 4, 7, 11],
            "m7": [0, 3, 7, 10],
            "2": [0, 2, 4, 7],
            "6": [0, 4, 7, 9],
            "m6": [0, 3, 7, 9],
            "69": [0, 2, 4, 7, 9],
            "9": [0, 2, 4, 7, 10],
            "maj9": [0, 2, 4, 7, 11],
            "m9": [0, 2, 3, 7, 10],
            "add9": [0, 2, 4, 7],
            "13": [0, 2, 4, 7, 9, 10],
            "m13": [0, 2, 3, 7, 9, 10],
            "maj13": [0, 2, 4, 7, 9, 11],
            "dim": [0, 3, 6],
            "dim7": [0, 3, 6, 9],
            "7b5": [0, 4, 6, 10],
            "aug": [0, 4, 8],
            "aug7": [0, 4, 8, 10],
        }
        observed = {
            record["symbol"]: normalize_quality(record)["intervals"]
            for record in self.fixture["qualities"]
        }
        for symbol, intervals in expected.items():
            self.assertEqual(observed[symbol], intervals, symbol)

    def test_seven_modes_and_related_scales_match_pinned_formulas(self):
        expected = {
            "major": [0, 2, 4, 5, 7, 9, 11],
            "minor": [0, 2, 3, 5, 7, 8, 10],
            "dorian": [0, 2, 3, 5, 7, 9, 10],
            "phrygian": [0, 1, 3, 5, 7, 8, 10],
            "lydian": [0, 2, 4, 6, 7, 9, 11],
            "mixolydian": [0, 2, 4, 5, 7, 9, 10],
            "locrian": [0, 1, 3, 5, 6, 8, 10],
        }
        observed = {
            record["symbol"]: normalize_scale(record)["intervals"]
            for record in self.fixture["scales"]
        }
        for symbol, intervals in expected.items():
            self.assertEqual(observed[symbol], intervals, symbol)

    def test_roots_aliases_and_export_variants_deduplicate_by_formula(self):
        records = list(self.fixture["qualities"])
        minor = next(record for record in records if record["symbol"] == "m")
        records.append({
            **minor,
            "root_pc": 2,
            "pitch_classes": [2, 5, 9],
            "export_variant": "transposed-root",
            "source": {"path": "fixture/transposed", "line": 1},
        })
        major = next(record for record in records if record["symbol"] == "")
        records.append({
            **major,
            "export_variant": "without-title",
            "source": {"path": "fixture/export-variant", "line": 1},
        })
        compiled = compile_vocabulary(
            {"qualities": records, "scales": self.fixture["scales"]},
            QUALITY_REGISTRY,
            SCALE_REGISTRY,
        )
        minor_formula = [row for row in compiled["qualities"]
                         if row["intervals"] == [0, 3, 7]]
        self.assertEqual(len(minor_formula), 1)
        self.assertGreaterEqual(len(minor_formula[0]["sources"]), 2)
        self.assertEqual(minor_formula[0]["id"], 1)
        add9_formula = [row for row in compiled["qualities"]
                        if row["intervals"] == [0, 2, 4, 7]]
        self.assertEqual(len(add9_formula), 1)
        self.assertEqual(add9_formula[0]["id"], 12)
        self.assertEqual(len(compiled["qualities"]), 23)
        self.assertEqual(len(compiled["scales"]), 17)
        self.assertEqual(compiled["quarantine"], [])

    def test_invalid_pitch_data_is_rejected_and_unknown_formula_gets_no_id(self):
        with self.assertRaises(ValueError):
            normalize_quality({"root_pc": 0, "pitch_classes": [0, 12]})
        with self.assertRaises(ValueError):
            normalize_scale({"root_pc": 12, "pitch_classes": [0, 2, 4]})
        index = registry_index(QUALITY_REGISTRY["qualities"])
        self.assertIsNone(index.get((0, 1, 4, 7)))

    def test_output_is_deterministic_and_keeps_unassigned_formulas_explicit(self):
        first = compile_vocabulary(self.fixture, QUALITY_REGISTRY, SCALE_REGISTRY)
        reversed_fixture = {
            **self.fixture,
            "qualities": list(reversed(self.fixture["qualities"])),
            "scales": list(reversed(self.fixture["scales"])),
        }
        second = compile_vocabulary(reversed_fixture, QUALITY_REGISTRY, SCALE_REGISTRY)
        canonical = lambda value: json.dumps(value, sort_keys=True, separators=(",", ":"))
        self.assertEqual(canonical(first), canonical(second))
        self.assertNotIn("timestamp", first)
        unknown = {
            "schema_version": 1,
            "qualities": [{"symbol": "unknown", "root_pc": 0,
                           "pitch_classes": [0, 1, 4, 7]}],
            "scales": [],
        }
        result = compile_vocabulary(unknown, QUALITY_REGISTRY, SCALE_REGISTRY)
        self.assertEqual(result["qualities"], [])
        self.assertEqual(result["quarantine"][0]["kind"], "quality")

    def test_source_lock_notices_and_runtime_registry_prefix_are_pinned(self):
        lock = json.loads((ROOT / "tools/harmony/source_lock.json").read_text())
        for source in lock["sources"].values():
            notice = source["license"]
            path = ROOT / notice["included_path"]
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            self.assertEqual(digest, notice["repository_sha256"])
        fannon = lock["sources"]["fannon"]
        notice = fannon["tonal_license"]
        self.assertEqual(
            hashlib.sha256((ROOT / notice["included_path"]).read_bytes()).hexdigest(),
            notice["included_sha256"],
        )
        qualities = QUALITY_REGISTRY["qualities"]
        self.assertEqual(QUALITY_REGISTRY["runtime_bootstrap_count"], 10)
        self.assertEqual([row["id"] for row in qualities[:10]], list(range(10)))
        self.assertEqual(qualities[0]["intervals"], [0, 4, 7])
        self.assertEqual(qualities[9]["intervals"], [0, 2, 4, 7, 11])
        self.assertEqual([row["id"] for row in SCALE_REGISTRY["scales"]],
                         list(range(17)))


if __name__ == "__main__":
    unittest.main()
