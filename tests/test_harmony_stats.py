#!/usr/bin/env python3
"""Hand-counted contracts for transposition-invariant corpus statistics."""

import copy
import hashlib
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/harmony"))

from build_transition_stats import (  # noqa: E402
    compile_transition_stats,
    quantize_successors,
    top_n_report,
    validate_stats,
)
from build_harmony_model import (  # noqa: E402
    compile_runtime_model,
    render_header,
    render_manifest,
)
from import_fannon_vocab import compile_vocabulary  # noqa: E402
from import_ldrolez_progressions import compile_progressions  # noqa: E402
from validate_harmony_model import build_model  # noqa: E402

QUALITY = json.loads((ROOT / "tools/harmony/quality_registry.json").read_text())


def token(degree, root, quality=0, accidental=0, slash=None):
    return {
        "degree": degree,
        "accidental": accidental,
        "root_offset": root,
        "quality_id": quality,
        "suffix": "",
        "slash_bass_offset": None,
        "slash_bass_interval": slash,
    }


def route(route_id, tokens, dialect="major", weight=1):
    return {
        "id": route_id,
        "tokens": tokens,
        "weight": weight,
        "mood_tags": ["Never a genre prior"],
        "source_locators": [{"list": "prog_maj" if dialect == "major" else "prog_modal",
                             "source_dialect": dialect}],
    }


def model(routes):
    return {
        "schema_version": 1,
        "quality_registry": QUALITY["qualities"],
        "quality_vocabulary": [{"id": 200, "name": "Fannon-only"}],
        "corpus": {"routes": routes},
        "provenance": {"sources": {"fannon": {"role": "vocabulary_only"}}},
    }


class HarmonyStatsTests(unittest.TestCase):
    def _pinned_model(self):
        lock = json.loads((ROOT / "tools/harmony/source_lock.json").read_text())
        quality = json.loads((ROOT / "tools/harmony/quality_registry.json").read_text())
        scale = json.loads((ROOT / "tools/harmony/scale_registry.json").read_text())

        def digest(value):
            encoded = json.dumps(value, ensure_ascii=False, sort_keys=True,
                                 separators=(",", ":")) + "\n"
            return hashlib.sha256(encoded.encode()).hexdigest()

        vocab = compile_vocabulary(
            json.loads((ROOT / "tests/fixtures/harmony/fannon_vocab.json").read_text()),
            quality, scale,
        )
        source = json.loads(
            (ROOT / "tests/fixtures/harmony/ldrolez_progressions.json").read_text()
        )
        groups = {row["name"]: row["rows"] for row in source["groups"]}
        accepted, quarantine = compile_progressions(groups)
        progressions = {
            "schema_version": 1,
            "sources": {
                "ldrolez_commit": lock["sources"]["ldrolez"]["commit"],
                "mingus_resolver_commit": lock["sources"]["mingus_resolver"]["commit"],
            },
            "accepted": accepted,
            "quarantine": quarantine,
        }
        fannon = lock["sources"]["fannon"]
        ldrolez = lock["sources"]["ldrolez"]
        mingus = lock["sources"]["mingus_resolver"]
        vocab_manifest = {
            "schema_version": 1,
            "source_commit": fannon["commit"],
            "source_file_sha256": fannon["files"],
            "quality_count": len(vocab["qualities"]),
            "scale_count": len(vocab["scales"]),
            "normalized_content_sha256": digest(vocab),
        }
        progression_manifest = {
            "schema_version": 1,
            "ldrolez_commit": ldrolez["commit"],
            "ldrolez_files": ldrolez["files"],
            "ldrolez_chords_sha256": ldrolez["files"]["chords.py"],
            "mingus_resolver_commit": mingus["commit"],
            "mingus_resolver_files": mingus["files"],
            "row_count": len(accepted) + len(quarantine),
            "accepted_count": len(accepted),
            "quarantined_count": len(quarantine),
            "content_sha256": digest(progressions),
        }
        return build_model(vocab, progressions, vocab_manifest,
                           progression_manifest, lock, quality, scale)

    def test_hand_counted_unigram_bigram_trigram_and_cadence_totals(self):
        first = route("first", [token(1, 0), token(1, 0), token(5, 7), token(1, 0)])
        second = route("second", [token(1, 0), token(5, 7), token(1, 0)])
        stats = compile_transition_stats(model([first, second]))
        by_token = {tuple(row["token"]): row["count"] for row in stats["unigrams"]}
        ids = {tuple(row["token"]): row["id"] for row in stats["tokens"]}
        tonic = next(key for key in by_token if key[0] == 0 and key[1] == 0)
        dominant = next(key for key in by_token if key[0] == 4 and key[1] == -5)
        self.assertEqual(by_token[tonic], 5)
        self.assertEqual(by_token[dominant], 2)

        bigrams = {(row["from"], row["to"]): row["count"] for row in stats["bigrams"]}
        self.assertEqual(bigrams[(ids[tonic], ids[tonic])], 1)
        self.assertEqual(bigrams[(ids[tonic], ids[dominant])], 2)
        self.assertEqual(bigrams[(ids[dominant], ids[tonic])], 2)

        trigrams = {(row["previous2"], row["previous1"], row["target"]): row["count"]
                    for row in stats["trigrams"]}
        self.assertEqual(trigrams[(ids[tonic], ids[tonic], ids[dominant])], 1)
        self.assertEqual(trigrams[(ids[tonic], ids[dominant], ids[tonic])], 2)
        self.assertEqual(stats["cadence_counts"]["authentic"], 2)
        self.assertEqual(stats["raw_route_weight"], 2)

    def test_one_route_weight_and_fannon_vocabulary_do_not_multiply_counts(self):
        r = route("unique", [token(1, 0), token(5, 7)])
        stats = compile_transition_stats(model([r]))
        self.assertEqual(sum(row["count"] for row in stats["unigrams"]), 2)
        self.assertEqual(stats["raw_route_weight"], 1)
        self.assertEqual(stats["source_vocabulary_count_used"], 0)

    def test_source_order_does_not_change_ids_or_serialized_stats(self):
        routes = [
            route("x", [token(1, 0), token(5, 7)]),
            route("y", [token(1, 0), token(4, 5, 1)], "modal"),
        ]
        one = compile_transition_stats(model(routes))
        two = compile_transition_stats(model(list(reversed(routes))))
        self.assertEqual(one, two)

    def test_top_n_quantization_and_tie_order_are_stable(self):
        successors = [(7, 2), (3, 1), (5, 1), (1, 0)]
        self.assertEqual(quantize_successors(successors), [(1, 0), (3, 8), (5, 8), (7, 15)])
        report = top_n_report({(1, 2): successors}, [2, 4])
        self.assertEqual(report["2"]["retained_unique_successors"], 2)
        self.assertEqual(report["2"]["truncated_unique_successors"], 1)
        self.assertEqual(report["4"]["truncated_unique_successors"], 0)

    def test_modal_borrowed_counts_ignore_mood_words(self):
        routes = [
            route("plain", [token(1, 0), token(5, 7)]),
            route("altered", [token(1, 0), token(7, 11, accidental=-1)]),
            route("modal", [token(1, 0), token(4, 5)], "modal"),
        ]
        stats = compile_transition_stats(model(routes))
        self.assertGreater(stats["modal_borrowed_edge_count"], 0)
        self.assertEqual(stats["mood_tag_count_used"], 0)

    def test_rejects_non_unit_route_weight_and_malformed_tokens(self):
        with self.assertRaises(ValueError):
            compile_transition_stats(model([route("bad-weight", [token(1, 0)], weight=2)]))
        malformed = route("bad-token", [token(1, 0)])
        malformed["tokens"][0]["root_offset"] = 12
        with self.assertRaises(ValueError):
            compile_transition_stats(model([malformed]))

    def test_stats_validate_after_serialization_round_trip(self):
        stats = compile_transition_stats(model([
            route("a", [token(1, 0), token(5, 7)]),
            route("b", [token(1, 0), token(4, 5, 1)]),
        ]))
        validate_stats(copy.deepcopy(stats))

    def test_runtime_model_top_n_is_bounded_and_reproducible(self):
        routes = [
            route("a", [token(1, 0), token(5, 7), token(1, 0)]),
            route("b", [token(1, 0), token(5, 7), token(2, 2)]),
            route("c", [token(1, 0), token(5, 7), token(3, 4)]),
            route("d", [token(1, 0), token(5, 7), token(4, 5, 1)]),
        ]
        stats = compile_transition_stats(model(routes))
        compiled = compile_runtime_model(stats, 2)
        self.assertEqual(compiled["counts"]["trigram"], 2)
        self.assertGreater(compiled["counts"]["trigram_before_top_n"], 2)
        header = render_header(compiled)
        manifest = render_manifest(compiled)
        self.assertIn("kTrigramCount = 2", header)
        self.assertEqual(manifest["flash_bytes"], compiled["flash_bytes"])
        self.assertEqual(render_header(compile_runtime_model(stats, 2)), header)
        self.assertEqual(render_manifest(compile_runtime_model(stats, 2)), manifest)

    def test_top_n_rejects_unmeasured_values(self):
        stats = compile_transition_stats(model([
            route("a", [token(1, 0), token(5, 7)])
        ]))
        with self.assertRaises(ValueError):
            compile_runtime_model(stats, 3)

    def test_checked_in_runtime_tables_reproduce_from_pinned_corpus(self):
        stats = compile_transition_stats(self._pinned_model())
        runtime = compile_runtime_model(stats, 8)
        header = (ROOT / "src/generation/harmony/generated/corpus_prior_data.h").read_text()
        manifest = json.loads(
            (ROOT / "tools/harmony/generated/corpus_prior_manifest.json").read_text()
        )
        self.assertEqual(render_header(runtime), header)
        self.assertEqual(render_manifest(runtime), manifest)


if __name__ == "__main__":
    unittest.main()
