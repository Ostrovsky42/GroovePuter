#!/usr/bin/env python3
"""Focused contracts for the GF2 semantic analysis orchestration tools."""

from __future__ import annotations

import csv
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import genre_diff  # noqa: E402
import orchestrate_semantic_analysis  # noqa: E402
import pattern_statistics  # noqa: E402
import reachability_report  # noqa: E402
import recipe_matrix  # noqa: E402
import semantic_census  # noqa: E402

sys.path.insert(0, str(ROOT / "tools/gf2"))
import generate_gf2_c1f_final_static_census as c1f_generator  # noqa: E402


def profile(
    key: str,
    kind: str,
    recipe_id: str,
    rhythm: str,
    tonal_payload: str,
) -> dict[str, object]:
    genre_key = key.split(":", 1)[0]
    axes = {
        "rhythm": {
            "rhythms": rhythm,
            "rhythm_payload_fingerprint": f"rhythm-{rhythm}",
            "canonical_drum_fingerprint": f"drum-{rhythm}",
        },
        "feel": {"feels": "0:STRAIGHT@100"},
        "bass": {"bass": "1:ROOT@100"},
        "chord": {"chord": "1:HOLD@100"},
        "progression": {"progressions": "1:STATIC@100"},
        "melodic": {"melodic": "1:SPARSE@100"},
        "motif": {"motifs": "1:SOURCE@100"},
        "phrase": {"phrases": "1:LOOP/1bar@100"},
        "corridor": {
            "bpm_min": "100",
            "bpm_suggested": "110",
            "bpm_max": "120",
            "grid_steps": "16",
            "density_min": "4",
            "density_max": "8",
        },
        "secondary_role": {"secondary_role": "Melodic"},
        "tonal": {"tonal_payload": tonal_payload},
    }
    return {
        "key": key,
        "genre_id": "1",
        "genre_key": genre_key,
        "genre": "Test Genre",
        "kind": kind,
        "recipe_id": recipe_id,
        "recipe": "BASE" if kind == "BASE" else "Test Recipe",
        "axes": axes,
        "fingerprints": {
            "primary_static_fingerprint": f"primary-{rhythm}-{tonal_payload}",
            "full_trace_fingerprint": f"full-{rhythm}-{tonal_payload}",
        },
        "classification_vs_base": "BASE" if kind == "BASE" else "MEMBERSHIP-CHANGE",
        "changed_domains_vs_base": "NONE" if kind == "BASE" else "RHYTHM",
        "single_option_axes": "tonal.bass_contour=ROOT",
    }


def census(source_sha: str = "a" * 40) -> dict[str, object]:
    return {
        "schema_version": 1,
        "source_sha": source_sha,
        "semantic_base_sha": "b" * 40,
        "profiles": [
            profile("test:BASE", "BASE", "BASE", "1:FOUR@100", "ROOT"),
            profile("test:7", "RECIPE", "7", "2:BROKEN@120", "ROOT"),
        ],
        "archetypes": [
            {
                "archetype_id": "1",
                "archetype_key": "four",
                "name": "FOUR",
                "family": "FourOnFloor",
                "bpm_min": "100",
                "bpm_max": "140",
                "semantic_payload_fingerprint": "semantic-1",
                "drum_payload_fingerprint": "drum-1",
            }
        ],
        "base_pairs": [],
    }


class GenreDiffTests(unittest.TestCase):
    def test_source_sha_alone_is_not_a_semantic_change(self) -> None:
        baseline = census("a" * 40)
        candidate = census("c" * 40)
        result = genre_diff.build_diff(baseline, candidate)
        self.assertFalse(result["has_semantic_changes"])

    def test_changed_axis_is_reported(self) -> None:
        baseline = census()
        candidate = census()
        candidate["profiles"][1]["axes"]["tonal"]["tonal_payload"] = "FIFTH"
        result = genre_diff.build_diff(baseline, candidate)
        self.assertTrue(result["has_semantic_changes"])
        self.assertEqual(result["profiles"]["changed"][0]["changed_axes"], ["tonal"])

    def test_fingerprint_only_change_is_reported(self) -> None:
        baseline = census()
        candidate = census()
        candidate["profiles"][1]["fingerprints"]["full_trace_fingerprint"] = "changed"
        result = genre_diff.build_diff(baseline, candidate)
        self.assertEqual(
            result["profiles"]["changed"][0]["changed_fingerprints"],
            ["full_trace_fingerprint"],
        )

    def test_archetype_change_reports_fields(self) -> None:
        baseline = census()
        candidate = census()
        candidate["archetypes"][0]["family"] = "Broken"
        result = genre_diff.build_diff(baseline, candidate)
        self.assertEqual(
            result["archetypes"]["changed"],
            [{"archetype_id": "1", "changed_fields": ["family"]}],
        )


class DerivedReportTests(unittest.TestCase):
    def test_recipe_matrix_detects_only_changed_axes(self) -> None:
        result = recipe_matrix.build_matrix(census())
        self.assertEqual(result["recipe_count"], 1)
        self.assertEqual(result["recipes"][0]["changed_axes"], ["rhythm"])

    def test_pattern_statistics_count_declared_vocabulary(self) -> None:
        result = pattern_statistics.build_statistics(census())
        self.assertEqual(result["profile_count"], 2)
        self.assertEqual(result["vocabulary"]["rhythm"]["candidate_count"], 2)
        self.assertEqual(result["vocabulary"]["feel"]["candidate_count"], 1)

    def test_reachability_report_groups_statuses(self) -> None:
        rows = [
            {
                "domain": "FEEL",
                "role": "ALL",
                "semantic_field": "profile_feel",
                "authoritative_owner": "ProfileDefinition",
                "terminal_effect": "NONE",
                "status": "DROPPED",
                "blocker": "UNKNOWN",
                "failure_mode": "selection is diagnostic only",
                "fallback": "Scene FEEL",
            },
            {
                "domain": "FEEL",
                "role": "DRUMS",
                "semantic_field": "scene_feel",
                "authoritative_owner": "Scene",
                "terminal_effect": "timing",
                "status": "CONNECTED",
                "blocker": "NONE",
                "failure_mode": "adapter failure",
                "fallback": "upstream drums",
            },
        ]
        result = reachability_report.build_report(rows)
        self.assertEqual(result["status_counts"], {"CONNECTED": 1, "DROPPED": 1})
        self.assertEqual(result["domain_status_counts"]["FEEL"]["CONNECTED"], 1)

    def test_reachability_diff_detects_owner_only_change(self) -> None:
        rows = [
            {
                "domain": "FEEL",
                "role": "ALL",
                "semantic_field": "profile_feel",
                "authoritative_owner": "ProfileDefinition",
                "terminal_effect": "NONE",
                "status": "DROPPED",
                "blocker": "UNKNOWN",
                "failure_mode": "selection is diagnostic only",
                "fallback": "Scene FEEL",
            }
        ]
        baseline = reachability_report.build_report(rows)
        rows[0]["authoritative_owner"] = "Scene"
        candidate = reachability_report.build_report(rows)
        result = reachability_report.build_diff(baseline, candidate)
        self.assertTrue(result["has_reachability_changes"])
        self.assertEqual(
            result["traces"]["changed"][0]["changed_fields"],
            ["authoritative_owner"],
        )

    def test_markdown_report_contains_diff_and_counts(self) -> None:
        candidate = census()
        diff = genre_diff.build_diff(census(), candidate)
        reachability = reachability_report.build_report(
            [
                {
                    "domain": "RHYTHM",
                    "role": "DRUMS",
                    "semantic_field": "rhythm",
                    "authoritative_owner": "Catalog",
                    "terminal_effect": "onsets",
                    "status": "CONNECTED",
                    "blocker": "NONE",
                    "failure_mode": "NONE",
                    "fallback": "NONE",
                }
            ]
        )
        recipes = recipe_matrix.build_matrix(candidate)
        patterns = pattern_statistics.build_statistics(candidate)
        reachability_diff = reachability_report.build_diff(
            reachability, reachability
        )
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "report.md"
            orchestrate_semantic_analysis.render_report(
                output,
                candidate,
                diff,
                reachability,
                reachability_diff,
                recipes,
                patterns,
            )
            report = output.read_text(encoding="utf-8")
        self.assertIn("Census changes: **NONE**", report)
        self.assertIn("Reachability changes: **NONE**", report)
        self.assertIn("| Profiles | 2 |", report)
        self.assertIn("| CONNECTED | 1 |", report)

    def test_markdown_report_shows_fingerprint_only_changes(self) -> None:
        baseline = census()
        candidate = census()
        candidate["profiles"][1]["fingerprints"]["full_trace_fingerprint"] = "changed"
        diff = genre_diff.build_diff(baseline, candidate)
        reachability = reachability_report.build_report(
            [
                {
                    "domain": "RHYTHM",
                    "role": "DRUMS",
                    "semantic_field": "rhythm",
                    "authoritative_owner": "Catalog",
                    "terminal_effect": "onsets",
                    "status": "CONNECTED",
                    "blocker": "NONE",
                    "failure_mode": "NONE",
                    "fallback": "NONE",
                }
            ]
        )
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "report.md"
            orchestrate_semantic_analysis.render_report(
                output,
                candidate,
                diff,
                reachability,
                reachability_report.build_diff(reachability, reachability),
                recipe_matrix.build_matrix(candidate),
                pattern_statistics.build_statistics(candidate),
            )
            report = output.read_text(encoding="utf-8")
        self.assertIn("full_trace_fingerprint", report)

    def test_markdown_report_lists_changed_base_pair_identity(self) -> None:
        baseline = census()
        candidate = census()
        pair = {
            "genre_a": "GenreA",
            "genre_b": "GenreB",
            "classification": "PARTIAL_COLLISION",
        }
        baseline["base_pairs"] = [pair]
        candidate["base_pairs"] = [dict(pair, classification="STRONG_DISTINCTNESS")]
        diff = genre_diff.build_diff(baseline, candidate)
        reachability = reachability_report.build_report(
            [
                {
                    "domain": "RHYTHM",
                    "role": "DRUMS",
                    "semantic_field": "rhythm",
                    "authoritative_owner": "Catalog",
                    "terminal_effect": "onsets",
                    "status": "CONNECTED",
                    "blocker": "NONE",
                    "failure_mode": "NONE",
                    "fallback": "NONE",
                }
            ]
        )
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "report.md"
            orchestrate_semantic_analysis.render_report(
                output,
                candidate,
                diff,
                reachability,
                reachability_report.build_diff(reachability, reachability),
                recipe_matrix.build_matrix(candidate),
                pattern_statistics.build_statistics(candidate),
            )
            report = output.read_text(encoding="utf-8")
        self.assertIn("GenreA::GenreB", report)


class CensusEvolutionTests(unittest.TestCase):
    def test_recipe_classification_allows_new_base_genre(self) -> None:
        profiles = [
            {"genre_key": f"Genre{index}", "is_base": "1"}
            for index in range(17)
        ]
        c1f_generator.classify_recipes(profiles)
        self.assertTrue(
            all(row["classification_vs_base"] == "BASE" for row in profiles)
        )

    def test_dynamic_base_pairs_allow_catalog_growth(self) -> None:
        rows = []
        for index in range(3):
            row = {
                "genre_key": f"Genre{index}",
                "is_base": "1",
                "primary_static_fingerprint": f"primary-{index}",
                "corridor_fingerprint": f"corridor-{index}",
                "role_fingerprint": "shared-role",
                "tonal_payload_fingerprint": f"tonal-{index}",
                "canonical_drum_fingerprint": f"drum-{index}",
                "legacy_drum_fingerprint": f"legacy-{index}",
            }
            for bag in c1f_generator.BAGS:
                row[bag] = f"{index + 1}:candidate@100"
            rows.append(row)
        pairs = c1f_generator.base_pairs(rows, expected_count=None)
        self.assertEqual(len(pairs), 3)

    def test_relative_cli_paths_are_rooted_at_repository(self) -> None:
        self.assertEqual(
            orchestrate_semantic_analysis.repo_path(Path("build/example")),
            ROOT / "build/example",
        )
        absolute = ROOT / "build/absolute"
        self.assertEqual(
            orchestrate_semantic_analysis.repo_path(absolute),
            absolute,
        )


class ReachabilityInputTests(unittest.TestCase):
    def test_required_columns_are_enforced(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "broken.tsv"
            with path.open("w", encoding="utf-8", newline="") as handle:
                writer = csv.DictWriter(handle, fieldnames=["domain"], delimiter="\t")
                writer.writeheader()
                writer.writerow({"domain": "FEEL"})
            with self.assertRaises(RuntimeError):
                reachability_report.read_rows(path)


if __name__ == "__main__":
    unittest.main()
