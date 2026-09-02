#!/usr/bin/env python3
"""Validate the checked-in GF2-C1RF final-release research artifact.

This validates schema, coverage, and frozen characterization findings. It does
not infer reachability from source text and is not a substitute for the manual
code-path audit documented in the report.
"""

from __future__ import annotations

import csv
import subprocess
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TABLE = ROOT / "docs/research/GF2_C1RF_FINAL_SEMANTIC_REACHABILITY.tsv"
REPORT = ROOT / "docs/research/GF2_C1RF_FINAL_SEMANTIC_REACHABILITY.md"
C1_TABLE = ROOT / "docs/research/GF2_C1F_FINAL_STATIC_SEMANTIC_CENSUS.tsv"
RELEASE_BASE = "0a2a6211ef00dcf2214dfd4704b6c34b424b1c9d"
C1F_BASE = "d24ebf42ba48c50d2057af055807dd2c1ec6f096"
C1RF_HEAD = "574b830526c784ffe761286096dd62e22d6361d4"

EXPECTED_COLUMNS = [
    "domain",
    "role",
    "semantic_field",
    "authoritative_owner",
    "resolver",
    "carrier",
    "consumer",
    "terminal_effect",
    "status",
    "blocker",
    "failure_mode",
    "fallback",
    "release_base_evidence",
    "final_revalidation_evidence",
    "notes",
]

ALLOWED_STATUSES = {
    "CONNECTED",
    "PARTIALLY_CONNECTED",
    "BLOCKED",
    "DROPPED",
    "FAILURE_MASKED",
    "LEGACY_FALLBACK",
    "DUPLICATE_OWNER",
    "AMBIGUOUS_OWNER",
    "DECLARED_ONLY",
    "UNKNOWN",
    "NOT_APPLICABLE",
}

REQUIRED_DOMAINS = {
    "RHYTHM_COMPATIBILITY",
    "FEEL",
    "COMPOSITION",
    "PHRASE_EVOLUTION",
    "PHRASE_LENGTH",
    "SECONDARY_ROLE",
    "GENERATION_CORRIDOR",
    "TEMPO",
    "TONAL_POLICY",
    "DRUM_POLICY",
    "HARMONIC_RHYTHM",
    "DEPTH_MUTATION",
    "FAILURE_PROPAGATION",
}

EXPECTED_STATUS_COUNTS = {
    "CONNECTED": 26,
    "PARTIALLY_CONNECTED": 2,
    "BLOCKED": 0,
    "DROPPED": 5,
    "FAILURE_MASKED": 1,
    "LEGACY_FALLBACK": 1,
    "DUPLICATE_OWNER": 1,
    "AMBIGUOUS_OWNER": 0,
    "DECLARED_ONLY": 3,
    "UNKNOWN": 0,
    "NOT_APPLICABLE": 0,
}


def load_tsv(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        return list(reader.fieldnames or []), list(reader)


def row_for(rows: list[dict[str, str]], semantic_field: str, role: str | None = None) -> dict[str, str]:
    matches = [
        row
        for row in rows
        if row["semantic_field"] == semantic_field
        and (role is None or row["role"] == role)
    ]
    assert len(matches) == 1, (semantic_field, role, len(matches))
    return matches[0]


columns, rows = load_tsv(TABLE)
assert columns == EXPECTED_COLUMNS
assert len(rows) == 39
assert {row["domain"] for row in rows} == REQUIRED_DOMAINS
assert all(row["status"] in ALLOWED_STATUSES for row in rows)
assert len({(row["domain"], row["role"], row["semantic_field"]) for row in rows}) == len(rows)

for row in rows:
    assert all(row[column].strip() for column in EXPECTED_COLUMNS)
    for evidence in row["release_base_evidence"].split("; "):
        source_path = evidence.split("::", 1)[0]
        assert (ROOT / source_path).is_file(), (row["semantic_field"], source_path)

counts = Counter(row["status"] for row in rows)
assert {status: counts[status] for status in EXPECTED_STATUS_COUNTS} == EXPECTED_STATUS_COUNTS

assert row_for(rows, "profile_suggested_feel")["status"] == "DROPPED"
assert row_for(rows, "profile_phrase_law")["status"] == "DROPPED"
assert row_for(rows, "profile_phrase_bars")["status"] == "PARTIALLY_CONNECTED"
assert row_for(rows, "explicit_phrase_length_request")["status"] == "CONNECTED"
assert row_for(rows, "suggested_bpm")["status"] == "PARTIALLY_CONNECTED"

minimal_space = row_for(rows, "minimal_space_atlas_bpm_vs_corridor")
assert minimal_space["status"] == "DUPLICATE_OWNER"
assert "DECLARATIVE_POLICY_INCONSISTENCY" in minimal_space["notes"]
assert "116" in minimal_space["notes"] and "72-102" in minimal_space["notes"]

masked = row_for(rows, "normal_genre_strong_migration_result")
assert masked["status"] == "FAILURE_MASKED"
assert "Atlas/legacy/current material" in masked["fallback"]
assert row_for(rows, "phrase_p1r_migration_result")["status"] == "CONNECTED"
assert row_for(rows, "upstream_material_fallback")["status"] == "LEGACY_FALLBACK"
assert row_for(rows, "harmonic_event_timing")["status"] == "CONNECTED"
assert row_for(rows, "harmonic_event_timing")["authoritative_owner"] == "HarmonicRhythm"

assert row_for(rows, "realization_level_and_mutation_budget", "DRUMS")["status"] == "CONNECTED"
for role in ("BASS", "CHORD", "MELODIC_SECONDARY"):
    assert row_for(rows, "realization_level", role)["status"] == "DROPPED"

c1_columns, c1_rows = load_tsv(C1_TABLE)
assert "exact_base" in c1_columns
assert len(c1_rows) == 33
assert {row["exact_base"] for row in c1_rows} == {RELEASE_BASE}

subprocess.run(
    ["git", "merge-base", "--is-ancestor", C1RF_HEAD, "HEAD"],
    cwd=ROOT,
    check=True,
)
changed_paths = subprocess.run(
    ["git", "diff", "--name-only", C1F_BASE, C1RF_HEAD, "--"],
    cwd=ROOT,
    check=True,
    text=True,
    capture_output=True,
).stdout.splitlines()
assert changed_paths
assert all(
    path.startswith(("docs/research/", "tests/"))
    for path in changed_paths
), changed_paths

report = REPORT.read_text(encoding="utf-8")
assert RELEASE_BASE in report
assert C1F_BASE in report
assert "GF2-C2-V0" in report
assert "WITHIN-POLICY STOCHASTIC DISTANCE" in report
assert "BETWEEN-POLICY CAUSAL DISTANCE" in report
assert "Semantic delta: NONE" in report
assert (ROOT / "tests/data/stage15_tonal_enabled_f13_baseline.tsv.gz.b64").is_file()

print(
    f"GF2-C1RF reachability artifact OK: "
    f"{len(rows)} rows, {len(REQUIRED_DOMAINS)} domains"
)
