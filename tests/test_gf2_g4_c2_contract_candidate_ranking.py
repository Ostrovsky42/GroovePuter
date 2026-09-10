#!/usr/bin/env python3
from __future__ import annotations

import csv
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

EXPECTED_TOP5 = [
    ("7", "2"),   # Broken / Drum&Bass
    ("7", "8"),   # Broken / Classic 2-Step
    ("4", "4"),   # Rave / Psytrance
    ("5", "11"),  # Reggae / Minimal Space
    ("5", "0"),   # Reggae / BASE
]
OUTPUTS = (
    "GF2_G4_C2_CONTRACT_CANDIDATES.tsv",
    "GF2_G4_C2_CONTRACT_CANDIDATES.md",
    "g4-c2-summary.txt",
)


def fail(message: str) -> None:
    print(f"G4_C2_FAIL {message}", file=sys.stderr)
    raise SystemExit(1)


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def run_ranker(root: Path, census: Path, out_dir: Path) -> None:
    tool = root / "tools/gf2/rank_gf2_g4_c2_contract_candidates.py"
    if not tool.is_file():
        fail("ranker_missing")
    proc = subprocess.run(
        [sys.executable, str(tool), "--census", str(census), "--output-dir", str(out_dir)],
        cwd=root,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if proc.returncode != 0:
        fail(f"ranker_exit={proc.returncode} output={proc.stdout.strip()}")
    if "G4_C2_RANKING_PASS" not in proc.stdout:
        fail("ranker_pass_marker_missing")


def assert_deterministic(left: Path, right: Path) -> None:
    for name in OUTPUTS:
        lp = left / name
        rp = right / name
        if not lp.is_file() or not rp.is_file():
            fail(f"missing_output={name}")
        if lp.read_bytes() != rp.read_bytes():
            fail(f"nondeterministic_output={name}")


def mutate_non_structural_fields(src: Path, dst: Path) -> None:
    with src.open(encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        fields = list(reader.fieldnames or [])
        rows = list(reader)
    if not fields:
        fail("census_header_missing")
    for row in rows:
        row["owner_genre"] = f"LABEL_{row['owner_mode']}"
        row["recipe_name"] = f"RECIPE_{row['recipe_id']}"
        row["weight_provenance"] = "999999"
    with dst.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def ranking_core(rows: list[dict[str, str]]) -> list[tuple[str, str, str, str]]:
    return [
        (row["owner_mode"], row["recipe_id"], row["rank"], row["score"])
        for row in rows
    ]


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    census = root / "docs/research/GF2_G4_I6_OWNERSHIP_CENSUS.tsv"
    if not census.is_file():
        fail("committed_census_missing")

    source_rows = read_tsv(census)
    review_owners = {
        (row["owner_mode"], row["recipe_id"])
        for row in source_rows
        if row["contract_status"] == "REVIEW_REQUIRED"
    }
    if len(review_owners) != 30:
        fail(f"review_owner_count={len(review_owners)} expected=30")

    with tempfile.TemporaryDirectory(prefix="g4-c2-") as tmp:
        base = Path(tmp)
        run_a = base / "run-a"
        run_b = base / "run-b"
        run_a.mkdir()
        run_b.mkdir()
        run_ranker(root, census, run_a)
        run_ranker(root, census, run_b)
        assert_deterministic(run_a, run_b)

        ranked = read_tsv(run_a / OUTPUTS[0])
        if len(ranked) != 30:
            fail(f"ranked_owner_count={len(ranked)} expected=30")
        if [int(row["rank"]) for row in ranked] != list(range(1, 31)):
            fail("rank_sequence_not_1_to_30")
        ranked_keys = {(row["owner_mode"], row["recipe_id"]) for row in ranked}
        if ranked_keys != review_owners:
            fail("ranking_owner_set_differs_from_review_required")

        actual_top5 = [(row["owner_mode"], row["recipe_id"]) for row in ranked[:5]]
        if actual_top5 != EXPECTED_TOP5:
            fail(f"top5={actual_top5} expected={EXPECTED_TOP5}")

        for row in ranked[:5]:
            if row["fully_observed"] != "1":
                fail(f"top_candidate_not_fully_observed rank={row['rank']}")
            if int(row["effective_candidate_count"]) < 2:
                fail(f"top_candidate_not_plural rank={row['rank']}")
            if row["rhythm_family_count"] != "1":
                fail(f"top_candidate_not_single_family rank={row['rank']}")
            if row["promotion_decision"] != "BLOCKED":
                fail(f"c2_must_not_promote rank={row['rank']} decision={row['promotion_decision']}")

        first = ranked[0]
        if first["exact_proven_admission_alias"] != "1":
            fail("rank1_missing_proven_admission_alias")
        if (first["alias_owner_mode"], first["alias_recipe_id"]) != ("14", "0"):
            fail("rank1_alias_is_not_proven_dnb")
        if first["observed_archetypes"] != "413,414,415,416":
            fail(f"rank1_archetypes={first['observed_archetypes']}")
        if first["observed_bass_identities"] != "2,5,7,9":
            fail(f"rank1_bass={first['observed_bass_identities']}")
        if first["blocking_reason"] != "OWNER_EQUIVALENCE_NOT_PROVEN;BASS_VOCABULARY_DIFFERS_FROM_PROVEN_ALIAS":
            fail(f"rank1_blocker={first['blocking_reason']}")

        if any(row["promotion_decision"] != "BLOCKED" for row in ranked):
            fail("c2_contains_automatic_promotion")

        summary = (run_a / OUTPUTS[2]).read_text(encoding="utf-8")
        expected_summary = (
            "G4_C2_RANKING owners=30 top_candidates=5 "
            "single_family_plural=5 exact_proven_admission_aliases=1 auto_promoted=0"
        )
        if expected_summary not in summary:
            fail("summary_metrics_missing")

        report = (run_a / OUTPUTS[1]).read_text(encoding="utf-8")
        if "C2 does not promote contracts" not in report:
            fail("report_non_promotion_boundary_missing")
        for mode, recipe in EXPECTED_TOP5:
            if f"mode={mode}, recipe={recipe}" not in report:
                fail(f"report_missing_top_candidate={mode}/{recipe}")

        mutated = base / "mutated.tsv"
        mutated_out = base / "mutated-out"
        mutated_out.mkdir()
        mutate_non_structural_fields(census, mutated)
        run_ranker(root, mutated, mutated_out)
        mutated_ranked = read_tsv(mutated_out / OUTPUTS[0])
        if ranking_core(mutated_ranked) != ranking_core(ranked):
            fail("ranking_depends_on_genre_labels_recipe_names_or_weights")

    print("G4-C2 contract candidate ranking: PASS")


if __name__ == "__main__":
    main()
