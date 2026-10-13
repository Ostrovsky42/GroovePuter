#!/usr/bin/env python3
"""Compiler manifest contracts: provenance, stable content and corpus isolation."""

import copy
import hashlib
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/harmony"))

from import_fannon_vocab import compile_vocabulary  # noqa: E402
from import_ldrolez_progressions import GROUPS, compile_progressions  # noqa: E402
from validate_harmony_model import build_model, validate_model  # noqa: E402

LOCK = json.loads((ROOT / "tools/harmony/source_lock.json").read_text())
QUALITY = json.loads((ROOT / "tools/harmony/quality_registry.json").read_text())
SCALE = json.loads((ROOT / "tools/harmony/scale_registry.json").read_text())


def _vocabulary():
    return {
        "schema_version": 1,
        "source_commit": LOCK["sources"]["fannon"]["commit"],
        "qualities": [{
            "id": 0, "name": "maj", "intervals": [0, 4, 7],
            "interval_mask": (1 << 0) | (1 << 4) | (1 << 7),
            "cardinality": 3, "aliases": ["", "M"], "family": "MajorMinor",
            "sources": [{"path": "src/generateChords.ts", "line": 1}],
        }],
        "scales": [{
            "id": 1, "name": "major", "intervals": [0, 2, 4, 5, 7, 9, 11],
            "interval_mask": sum(1 << p for p in [0, 2, 4, 5, 7, 9, 11]),
            "cardinality": 7, "aliases": ["major"], "family": "Major",
            "sources": [{"path": "src/generateScales.ts", "line": 1}],
        }],
    }


def _route(root=0, quality=0, spelling="I"):
    return {
        "degree": 1, "accidental": 0, "root_offset": root,
        "quality_id": quality, "suffix": "", "slash_bass_offset": None,
        "slash_bass_interval": None, "source_spelling": spelling,
    }


def _progressions():
    repeated = [_route(0), _route(0), _route(7, spelling="V")]
    normal = [_route(0), _route(7, spelling="V")]
    return {
        "schema_version": 1,
        "sources": {
            "ldrolez_commit": LOCK["sources"]["ldrolez"]["commit"],
            "mingus_resolver_commit": LOCK["sources"]["mingus_resolver"]["commit"],
        },
        "accepted": [
            {"status": "accepted", "source": {"path": "chords.py", "line": 10},
             "raw": "I V", "route_text": "I V", "mood_tags": ["Hopeful"],
             "source_dialect": "major", "context": "major",
             "source_tokens": ["I", "V"], "route": normal},
            {"status": "accepted", "source": {"path": "chords.py", "line": 20},
             "raw": "I V =Dark", "route_text": "I V", "mood_tags": ["Dark"],
             "source_dialect": "major", "context": "major",
             "source_tokens": ["I", "V"], "route": normal},
            {"status": "accepted", "source": {"path": "chords.py", "line": 30},
             "raw": "I I V", "route_text": "I I V", "mood_tags": [],
             "source_dialect": "major", "context": "major",
             "source_tokens": ["I", "I", "V"], "route": repeated},
        ],
        "quarantine": [{
            "status": "quarantined", "source": {"path": "chords.py", "line": 40},
            "token": "Iadd9", "reason_code": "unsupported_suffix",
        }],
    }


def _digest(value):
    content = json.dumps(value, ensure_ascii=False, sort_keys=True,
                         separators=(",", ":")) + "\n"
    return hashlib.sha256(content.encode("utf-8")).hexdigest()


def _input_manifests(vocab, progressions):
    fannon = LOCK["sources"]["fannon"]
    ldrolez = LOCK["sources"]["ldrolez"]
    mingus = LOCK["sources"]["mingus_resolver"]
    return (
        {"schema_version": 1, "source_commit": fannon["commit"],
         "source_file_sha256": fannon["files"], "quality_count": 1, "scale_count": 1,
         "normalized_content_sha256": _digest(vocab)},
        {"schema_version": 1, "ldrolez_commit": ldrolez["commit"],
         "ldrolez_files": ldrolez["files"],
         "ldrolez_chords_sha256": ldrolez["files"]["chords.py"],
         "mingus_resolver_commit": mingus["commit"],
         "mingus_resolver_files": mingus["files"], "row_count": 4,
         "accepted_count": 3, "quarantined_count": 1,
         "content_sha256": _digest(progressions)},
    )


class HarmonySourceManifestTests(unittest.TestCase):
    def build(self, vocab=None, progressions=None, vocab_manifest=None,
              progression_manifest=None):
        vocab = vocab if vocab is not None else _vocabulary()
        progressions = progressions if progressions is not None else _progressions()
        if vocab_manifest is None:
            vocab_manifest, progression_manifest = _input_manifests(vocab, progressions)
        return build_model(
            vocab, progressions,
            vocab_manifest, progression_manifest, LOCK, QUALITY, SCALE,
        )

    def test_model_counts_unique_routes_once_and_preserves_ordered_repeats(self):
        model = self.build()
        self.assertEqual(model["corpus"]["raw_accepted_count"], 3)
        self.assertEqual(model["corpus"]["unique_route_count"], 2)
        self.assertEqual(model["corpus"]["duplicate_route_count"], 1)
        self.assertEqual(model["corpus"]["quarantined_count"], 1)
        self.assertEqual(model["corpus"]["routes"][0]["weight"], 1)
        duplicate_route = next(route for route in model["corpus"]["routes"]
                               if len(route["tokens"]) == 2)
        repeated_route = next(route for route in model["corpus"]["routes"]
                              if len(route["tokens"]) == 3)
        self.assertEqual(duplicate_route["mood_tags"], ["Dark", "Hopeful"])
        self.assertEqual(len(repeated_route["tokens"]), 3)
        self.assertNotIn("transition_counts", model["corpus"])
        validate_model(model)

    def test_compiled_model_is_identical_after_input_permutation(self):
        vocab = _vocabulary()
        progressions = _progressions()
        first = self.build(vocab, progressions)
        vocab2 = copy.deepcopy(vocab)
        progression2 = copy.deepcopy(progressions)
        vocab2["qualities"].reverse()
        vocab2["scales"].reverse()
        progression2["accepted"].reverse()
        progression2["quarantine"].reverse()
        second = self.build(vocab2, progression2)
        self.assertEqual(first, second)

    def test_fannon_vocabulary_never_increases_progression_route_counts(self):
        model = self.build()
        self.assertEqual(model["provenance"]["sources"]["fannon"]["role"],
                         "vocabulary_only")
        self.assertIn("license", model["provenance"]["sources"]["fannon"])
        self.assertIn("license", model["provenance"]["sources"]["ldrolez"])
        self.assertIn("license", model["provenance"]["sources"]["mingus_resolver"])
        self.assertEqual(model["corpus"]["raw_accepted_count"], 3)
        self.assertEqual(model["corpus"]["unique_route_count"], 2)

    def test_pinned_full_sources_compile_to_reviewed_manifest_fixture(self):
        raw_vocab = json.loads(
            (ROOT / "tests/fixtures/harmony/fannon_vocab.json").read_text()
        )
        vocab = compile_vocabulary(
            raw_vocab, QUALITY, SCALE,
        )
        source_fixture = json.loads(
            (ROOT / "tests/fixtures/harmony/ldrolez_progressions.json").read_text()
        )
        groups = {group["name"]: group["rows"] for group in source_fixture["groups"]}
        accepted, quarantine = compile_progressions(groups)
        progression = {
            "schema_version": 1,
            "sources": {
                "ldrolez_commit": LOCK["sources"]["ldrolez"]["commit"],
                "mingus_resolver_commit": LOCK["sources"]["mingus_resolver"]["commit"],
            },
            "accepted": accepted,
            "quarantine": quarantine,
        }
        fannon_manifest = {
            "schema_version": 1,
            "source_commit": LOCK["sources"]["fannon"]["commit"],
            "source_file_sha256": LOCK["sources"]["fannon"]["files"],
            "quality_count": len(vocab["qualities"]),
            "scale_count": len(vocab["scales"]),
            "normalized_content_sha256": _digest(vocab),
        }
        progression_manifest = {
            "schema_version": 1,
            "ldrolez_commit": LOCK["sources"]["ldrolez"]["commit"],
            "ldrolez_files": LOCK["sources"]["ldrolez"]["files"],
            "ldrolez_chords_sha256":
                LOCK["sources"]["ldrolez"]["files"]["chords.py"],
            "mingus_resolver_commit": LOCK["sources"]["mingus_resolver"]["commit"],
            "mingus_resolver_files": LOCK["sources"]["mingus_resolver"]["files"],
            "row_count": len(accepted) + len(quarantine),
            "accepted_count": len(accepted),
            "quarantined_count": len(quarantine),
            "content_sha256": _digest(progression),
        }
        model = build_model(vocab, progression, fannon_manifest,
                            progression_manifest, LOCK, QUALITY, SCALE)
        expected = json.loads(
            (ROOT / "tests/fixtures/harmony/source_manifest_expected.json").read_text()
        )
        self.assertEqual(len(accepted) + len(quarantine), expected["ldrolez_rows"])
        self.assertEqual(len(accepted), expected["accepted_routes"])
        self.assertEqual(len(quarantine), expected["quarantined_routes"])
        self.assertEqual(model["corpus"]["unique_route_count"], expected["unique_routes"])
        self.assertEqual(model["corpus"]["duplicate_route_count"],
                         expected["duplicate_routes"])
        self.assertEqual(model["content_sha256"], expected["content_sha256"])

    def test_validator_rejects_schema_registry_route_and_checksum_errors(self):
        baseline = self.build()
        cases = []
        bad = copy.deepcopy(baseline)
        bad["schema_version"] = 9
        cases.append(bad)
        bad = copy.deepcopy(baseline)
        bad["quality_registry"].append(copy.deepcopy(bad["quality_registry"][0]))
        cases.append(bad)
        bad = copy.deepcopy(baseline)
        bad["corpus"]["routes"][0]["tokens"][0]["quality_id"] = 255
        cases.append(bad)
        bad = copy.deepcopy(baseline)
        bad["content_sha256"] = "0" * 64
        cases.append(bad)
        bad = copy.deepcopy(baseline)
        bad["provenance"]["sources"]["mingus_resolver"]["commit"] = "0" * 40
        cases.append(bad)
        for model in cases:
            with self.subTest(model=model):
                with self.assertRaises(ValueError):
                    validate_model(model)


if __name__ == "__main__":
    unittest.main()
