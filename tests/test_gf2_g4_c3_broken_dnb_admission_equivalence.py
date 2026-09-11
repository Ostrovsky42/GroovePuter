#!/usr/bin/env python3
from __future__ import annotations

import csv
import sys
from pathlib import Path

BROKEN = ("7", "2")
DNB = ("14", "0")
EXPECTED_ARCHETYPES = {"413", "414", "415", "416"}
BROKEN_BASS = {"2", "5", "7", "9"}
DNB_BASS = {"3", "4", "8", "9"}
C3_ID = "G4-C3-BROKEN-DNB-ADMISSION-BREAKBEAT-ALIAS"
C1_ID = "G4-C1-DNB-MATERIALIZED-BASS-SPACE"
EXPECTED_COVERAGE = "G4_I6_CONTRACT_COVERAGE proven=7 provisional=0 review_required=29 unknown=11136"
EXPECTED_MARKER = (
    "G4_C3_BROKEN_DNB_ADMISSION equivalence=PROVEN rows=384 archetypes=4 "
    "broken_bass_ids=4 dnb_bass_ids=4 bass_semantics=INDEPENDENT"
)


def fail(message: str) -> None:
    print(f"G4_C3_FAIL {message}", file=sys.stderr)
    raise SystemExit(1)


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        fields = set(reader.fieldnames or [])
        required = {
            "owner_mode",
            "recipe_id",
            "effective_candidate_count",
            "selected_archetype_id",
            "RhythmFamily",
            "bass_identity",
            "contract_id",
            "contract_status",
            "evaluation",
        }
        if not required.issubset(fields):
            fail("census_schema")
        return list(reader)


def select_owner(rows: list[dict[str, str]], owner: tuple[str, str]) -> list[dict[str, str]]:
    mode, recipe = owner
    return [
        row for row in rows
        if row["owner_mode"] == mode and row["recipe_id"] == recipe
    ]


def assert_admission(owner_name: str, rows: list[dict[str, str]]) -> None:
    if len(rows) != 384:
        fail(f"{owner_name}_row_count={len(rows)} expected=384")

    candidate_counts = {row["effective_candidate_count"] for row in rows}
    if candidate_counts != {"4"}:
        fail(f"{owner_name}_candidate_counts={sorted(candidate_counts)} expected=4")

    archetypes = {row["selected_archetype_id"] for row in rows}
    if archetypes != EXPECTED_ARCHETYPES:
        fail(f"{owner_name}_archetypes={sorted(archetypes)} expected={sorted(EXPECTED_ARCHETYPES)}")

    families = {row["RhythmFamily"] for row in rows}
    if families != {"Breakbeat"}:
        fail(f"{owner_name}_families={sorted(families)} expected=Breakbeat")


def assert_contract(
    owner_name: str,
    rows: list[dict[str, str]],
    expected_id: str,
) -> None:
    for row in rows:
        if row["contract_id"] != expected_id:
            fail(
                f"{owner_name}_contract_id={row['contract_id']} expected={expected_id} "
                f"identity={row.get('identity_ordinal', '?')}"
            )
        if row["contract_status"] != "PROVEN" or row["evaluation"] != "SATISFIED":
            fail(
                f"{owner_name}_contract_not_proven "
                f"status={row['contract_status']} evaluation={row['evaluation']}"
            )


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: test_gf2_g4_c3_broken_dnb_admission_equivalence.py RUN_DIR")

    run_dir = Path(sys.argv[1])
    census = run_dir / "GF2_G4_I6_OWNERSHIP_CENSUS.tsv"
    summary = run_dir / "g4-i6-summary.txt"
    report = run_dir / "GF2_G4_I6_OWNERSHIP_CENSUS.md"
    for path in (census, summary, report):
        if not path.is_file():
            fail(f"missing_artifact={path.name}")

    rows = read_rows(census)
    broken = select_owner(rows, BROKEN)
    dnb = select_owner(rows, DNB)

    assert_admission("broken_dnb", broken)
    assert_admission("canonical_dnb", dnb)

    broken_bass = {row["bass_identity"] for row in broken}
    dnb_bass = {row["bass_identity"] for row in dnb}
    if broken_bass != BROKEN_BASS:
        fail(f"broken_bass={sorted(broken_bass)} expected={sorted(BROKEN_BASS)}")
    if dnb_bass != DNB_BASS:
        fail(f"dnb_bass={sorted(dnb_bass)} expected={sorted(DNB_BASS)}")
    if broken_bass == dnb_bass:
        fail("bass_spaces_collapsed_owner_equivalence_would_be_overclaimed")

    assert_contract("canonical_dnb", dnb, C1_ID)
    assert_contract("broken_dnb", broken, C3_ID)

    summary_text = summary.read_text(encoding="utf-8")
    if EXPECTED_COVERAGE not in summary_text:
        fail("summary_contract_coverage_not_7_29_11136")
    if "review_required=29 unknown=11136" not in summary_text:
        fail("summary_findings_not_29_11136")
    if EXPECTED_MARKER not in summary_text:
        fail("summary_c3_marker_missing")

    report_text = report.read_text(encoding="utf-8")
    if C3_ID not in report_text:
        fail("report_c3_admission_contract_missing")
    if "G4-C3-BROKEN-DNB-MATERIALIZED" in report_text:
        fail("report_overclaims_c3_materialized_contract")
    if "G4-C3-BROKEN-DNB-BASS" in report_text:
        fail("report_overclaims_c3_bass_contract")
    if "bass semantics remain independent" not in report_text.lower():
        fail("report_bass_independence_boundary_missing")

    print("G4-C3 Broken/DnB admission equivalence: PASS")


if __name__ == "__main__":
    main()
