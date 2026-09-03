#!/usr/bin/env python3
"""Validate the deterministic GF2-C1DF dependency/loss maps."""

from __future__ import annotations

import csv
import subprocess
from collections import Counter
from pathlib import Path

from gf2_frozen_git_boundary import resolve_frozen_commit


ROOT = Path(__file__).resolve().parents[1]
CENSUS = ROOT / "docs/research/GF2_C1F_FINAL_STATIC_SEMANTIC_CENSUS.tsv"
C1_PAIRS = ROOT / "docs/research/GF2_C1F_BASE_GENRE_PAIRS.tsv"
C1RF = ROOT / "docs/research/GF2_C1RF_FINAL_SEMANTIC_REACHABILITY.tsv"
BASE_MAP = ROOT / "docs/research/GF2_C1DF_BASE_PAIR_DEPENDENCY.tsv"
RECIPE_MAP = ROOT / "docs/research/GF2_C1DF_RECIPE_BASE_DEPENDENCY.tsv"
REPORT = ROOT / "docs/research/GF2_C1DF_FINAL_DECLARATIVE_DISTINCTNESS_DEPENDENCY.md"
GENERATOR = ROOT / "tools/gf2/generate_gf2_c1df_dependency_maps.py"

GF2_BASE = "0a2a6211ef00dcf2214dfd4704b6c34b424b1c9d"
C1RF_HEAD = "574b830526c784ffe761286096dd62e22d6361d4"
C1DF_HEAD = "82cfbd9f2c05074cd58b3841b29fb871219e54c8"
C1RF_TREE = "1ff2312cc9888cb33546eff0d47c00a4149f85c0"
C1DF_TREE = "382708e6d1fd5ca07031a90ed2a57f9879d060ec"
C1RF_SUBJECT = "research(0.9.10-gf2): revalidate reachability on v0.9.9"
C1DF_SUBJECT = "research(0.9.10-gf2): map final distinctness dependencies"

MAPPED_DOMAIN_ROOTS = {
    "RHYTHM_COMPATIBILITY",
    "PROFILE_FEEL",
    "BASS_RHYTHM",
    "CHORD_RHYTHM",
    "CHORD_PROGRESSION",
    "MELODIC_RHYTHM",
    "MOTIF_SHAPE",
    "PHRASE_EVOLUTION",
    "PHRASE_LENGTH",
    "CORRIDOR_SUGGESTED_BPM",
    "CORRIDOR_BPM_BOUNDS",
    "CORRIDOR_DENSITY",
    "CORRIDOR_GRID",
    "SECONDARY_ROLE",
    "TONAL_BASS_CONTOUR",
    "TONAL_BASS_ARTICULATION",
    "TONAL_MELODIC_RHYTHM_OPERATION",
    "TONAL_MELODIC_CONTOUR",
    "TONAL_MELODIC_MOTIF_OPERATION",
    "TONAL_BASS_REGISTER",
    "TONAL_SECONDARY_REGISTER",
    "DRUM_POLICY",
}


def rows(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def values(value: str) -> set[str]:
    return set() if value == "NONE" else set(value.split(";"))


def classes(row: dict[str, str]) -> set[str]:
    return values(row["classification"])


def class_counts(records: list[dict[str, str]]) -> Counter[str]:
    result: Counter[str] = Counter()
    for record in records:
        result.update(classes(record))
    return result


profiles = rows(CENSUS)
c1_pairs = rows(C1_PAIRS)
c1rf = rows(C1RF)
base_map = rows(BASE_MAP)
recipe_map = rows(RECIPE_MAP)

assert len(profiles) == 33
assert len(c1_pairs) == 120
assert len(c1rf) == 39
assert len(base_map) == 120
assert len(recipe_map) == 17

assert all(row["exact_base"] == GF2_BASE for row in base_map + recipe_map)
assert all(row["c1rf_head"] == C1RF_HEAD for row in base_map + recipe_map)

bases = [row for row in profiles if row["is_base"] == "1"]
recipes = [row for row in profiles if row["is_base"] == "0"]
assert len(bases) == 16
assert len(recipes) == 17

display_by_key = {row["genre_key"]: row["genre_display"] for row in bases}
expected_pairs = {
    (display_by_key[row["genre_a"]], display_by_key[row["genre_b"]])
    for row in c1_pairs
}
actual_pairs = {(row["genre_a"], row["genre_b"]) for row in base_map}
assert len(expected_pairs) == 120
assert actual_pairs == expected_pairs

expected_recipes = {
    (row["genre_display"], row["recipe_id"], row["recipe_name"])
    for row in recipes
}
actual_recipes = {
    (row["genre"], row["recipe_id"], row["recipe"])
    for row in recipe_map
}
assert actual_recipes == expected_recipes

for row in base_map + recipe_map:
    declared = values(row["declared_difference_domains"])
    reachable = values(row["reachable_difference_domains"])
    unreachable = values(row["unreachable_difference_domains"])
    unreliable = values(row["unreliable_difference_domains"])
    assert reachable.isdisjoint(unreachable)
    assert reachable.isdisjoint(unreliable)
    assert unreachable.isdisjoint(unreliable)
    assert declared == reachable | unreachable | unreliable
    assert all(token.split(":", 1)[0] in MAPPED_DOMAIN_ROOTS for token in declared)

    tags = classes(row)
    if "REACHABILITY_DEPENDENT" in tags:
        assert declared
        assert not reachable
    if "DECLARATIVE_COLLISION" in tags:
        assert not declared
    else:
        assert declared
    if "REACHABLE_POLICY_DIFFERENCE" in tags:
        assert reachable
    if "PARTIALLY_REACHABLE" in tags:
        assert {
            "CORRIDOR_SUGGESTED_BPM",
            "PHRASE_LENGTH:MEMBERSHIP",
        } & unreliable
    if "FAILURE_SENSITIVE" in tags:
        assert row["failure_sensitive_domains"] == (
            "NORMAL_GENRE_STRONG_MIGRATION_PUBLICATION"
        )
    if values(row["unmapped_trace_domains"]):
        assert values(row["unmapped_trace_domains"]) == {"ATLAS_METADATA"}

base_counts = class_counts(base_map)
assert base_counts["DECLARATIVE_COLLISION"] == 0
assert base_counts["REACHABLE_POLICY_DIFFERENCE"] == 120
assert base_counts["MIXED_REACHABILITY"] == 120
assert base_counts["REACHABILITY_DEPENDENT"] == 0
assert base_counts["FAILURE_SENSITIVE"] == 120
assert base_counts["OWNER_UNRESOLVED"] == 0
assert base_counts["PARTIALLY_REACHABLE"] == 69
for row in base_map:
    if "CORRIDOR_SUGGESTED_BPM" in values(row["declared_difference_domains"]):
        assert "CORRIDOR_SUGGESTED_BPM" in values(row["reachable_difference_domains"])

recipe_counts = class_counts(recipe_map)
assert recipe_counts["DECLARATIVE_COLLISION"] == 0
assert recipe_counts["REACHABLE_POLICY_DIFFERENCE"] == 17
assert recipe_counts["MIXED_REACHABILITY"] == 17
assert recipe_counts["REACHABILITY_DEPENDENT"] == 0
assert recipe_counts["FAILURE_SENSITIVE"] == 17
assert recipe_counts["OWNER_UNRESOLVED"] == 0
assert recipe_counts["PARTIALLY_REACHABLE"] == 9
assert recipe_counts["POLICY_INCONSISTENCY_AFFECTED"] == 1

profile_by_recipe = {
    (row["genre_display"], row["recipe_id"]): row for row in recipes
}
for row in recipe_map:
    if "CORRIDOR_SUGGESTED_BPM" not in values(row["declared_difference_domains"]):
        continue
    profile = profile_by_recipe[(row["genre"], row["recipe_id"])]
    destination = (
        row["unreliable_difference_domains"]
        if profile["atlas_backed"] == "1"
        else row["reachable_difference_domains"]
    )
    assert "CORRIDOR_SUGGESTED_BPM" in values(destination)

# No current recipe is corridor-only under the frozen C1 primary model.
for row in recipe_map:
    roots = {token.split(":", 1)[0] for token in values(row["declared_difference_domains"])}
    assert not roots <= {
        "CORRIDOR_SUGGESTED_BPM",
        "CORRIDOR_BPM_BOUNDS",
        "CORRIDOR_DENSITY",
        "CORRIDOR_GRID",
    }

minimal_space = next(row for row in recipe_map if row["recipe"] == "Minimal Space")
assert minimal_space["policy_inconsistency_domains"] == "MINIMAL_SPACE_TEMPO"
assert "POLICY_INCONSISTENCY_AFFECTED" in classes(minimal_space)
assert not any(row["policy_inconsistency_domains"] != "NONE" for row in base_map)

harmonic = next(
    row
    for row in c1rf
    if row["semantic_field"] == "harmonic_event_timing"
    and row["role"] == "CHORD_BASS_MELODIC"
)
masked = next(
    row
    for row in c1rf
    if row["semantic_field"] == "normal_genre_strong_migration_result"
)
fallback = next(
    row
    for row in c1rf
    if row["semantic_field"] == "upstream_material_fallback"
)
assert harmonic["status"] == "CONNECTED"
assert masked["status"] == "FAILURE_MASKED"
assert fallback["status"] == "LEGACY_FALLBACK"
assert not any("OWNER_UNRESOLVED" in classes(row) for row in base_map + recipe_map)

report = REPORT.read_text(encoding="utf-8")
assert GF2_BASE in report
assert C1RF_HEAD in report
assert "**Semantic delta: NONE.**" in report
assert "GF2-C2 = BLOCKED" in report
assert "`GF2-C2`, `GF2-G1`, and `GF2-R2` are not started." in report
assert "No aggregate reachability or Genre distance is calculated." in report

C1RF_BOUNDARY = resolve_frozen_commit(
    ROOT,
    exact_sha=C1RF_HEAD,
    expected_tree=C1RF_TREE,
    expected_subject=C1RF_SUBJECT,
)
C1DF_BOUNDARY = resolve_frozen_commit(
    ROOT,
    exact_sha=C1DF_HEAD,
    expected_tree=C1DF_TREE,
    expected_subject=C1DF_SUBJECT,
)

subprocess.run(
    ["git", "merge-base", "--is-ancestor", C1DF_BOUNDARY, "HEAD"],
    cwd=ROOT,
    check=True,
)
changed_paths = subprocess.run(
    ["git", "diff", "--name-only", C1RF_BOUNDARY, C1DF_BOUNDARY, "--"],
    cwd=ROOT,
    check=True,
    capture_output=True,
    text=True,
).stdout.splitlines()
assert changed_paths
assert all(
    path.startswith(("docs/research/", "tests/", "tools/gf2/"))
    for path in changed_paths
)

# Recompute both artifacts and compare exact bytes with the checked-in files.
subprocess.run(
    ["python3", str(GENERATOR), "--check"],
    cwd=ROOT,
    check=True,
)

print("GF2-C1DF dependency invariants passed: 120 BASE pairs, 17 recipes")
