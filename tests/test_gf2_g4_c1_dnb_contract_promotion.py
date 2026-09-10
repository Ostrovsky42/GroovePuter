#!/usr/bin/env python3
from __future__ import annotations

import csv
import sys
from pathlib import Path

EXPECTED_CONTRACT = "G4-C1-DNB-MATERIALIZED-BASS-SPACE"
EXPECTED_ADMISSION = "G4-C1-DNB-ADMISSION-BREAKBEAT"
ALLOWED_BASS = {"3", "4", "8", "9"}  # KickAnswer, GapFill, HalfTimePocket, SyncopatedHook


def fail(message: str) -> None:
    print(f"G4_C1_FAIL {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: test_gf2_g4_c1_dnb_contract_promotion.py RUN_DIR")

    run_dir = Path(sys.argv[1])
    census_path = run_dir / "GF2_G4_I6_OWNERSHIP_CENSUS.tsv"
    summary_path = run_dir / "g4-i6-summary.txt"
    report_path = run_dir / "GF2_G4_I6_OWNERSHIP_CENSUS.md"

    with census_path.open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))

    dnb = [row for row in rows if row["owner_mode"] == "14" and row["recipe_id"] == "0"]
    if len(dnb) != 384:
        fail(f"dnb_row_count={len(dnb)} expected=384")

    for row in dnb:
        if row["RhythmFamily"] != "Breakbeat":
            fail(f"non_breakbeat archetype={row['selected_archetype_id']} family={row['RhythmFamily']}")
        if row["bass_identity"] not in ALLOWED_BASS:
            fail(f"bass_outside_space identity={row['identity_ordinal']} bass={row['bass_identity']}")
        if row["contract_id"] != EXPECTED_CONTRACT:
            fail(f"contract_id={row['contract_id']} expected={EXPECTED_CONTRACT}")
        if row["contract_status"] != "PROVEN" or row["evaluation"] != "SATISFIED":
            fail(
                f"contract_not_proven identity={row['identity_ordinal']} "
                f"status={row['contract_status']} evaluation={row['evaluation']}"
            )

    summary = summary_path.read_text(encoding="utf-8")
    if "review_required=30" not in summary:
        fail("review_required_not_reduced_to_30")
    if "unknown=11520" not in summary:
        fail("unknown_not_reduced_to_11520")

    report = report_path.read_text(encoding="utf-8")
    if EXPECTED_ADMISSION not in report or EXPECTED_CONTRACT not in report:
        fail("dnb_contract_registry_missing")

    print("G4-C1 DnB structural bass contract promotion: PASS")


if __name__ == "__main__":
    main()
