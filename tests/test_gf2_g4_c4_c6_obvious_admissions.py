#!/usr/bin/env python3
from __future__ import annotations

import csv
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Contract:
    mode: str
    recipe: str
    contract_id: str
    candidate_count: str
    archetypes: frozenset[str]
    family: str
    bass_ids: frozenset[str]


CONTRACTS = (
    Contract(
        "7", "8", "G4-C4-CLASSIC-2STEP-ADMISSION", "2",
        frozenset({"417", "419"}), "UkTwoStep", frozenset({"3", "4", "5", "9"}),
    ),
    Contract(
        "4", "4", "G4-C5-PSYTRANCE-FOURFLOOR-ADMISSION", "3",
        frozenset({"401", "402", "406"}), "FourFloor", frozenset({"2", "5", "7", "9"}),
    ),
    Contract(
        "5", "0", "G4-C6-REGGAE-BASE-DUBPULSE-ADMISSION", "4",
        frozenset({"409", "410", "411", "412"}), "DubPulse", frozenset({"3", "4", "6", "10"}),
    ),
    Contract(
        "5", "11", "G4-C6-REGGAE-MINIMAL-SPACE-DUBPULSE-ADMISSION", "3",
        frozenset({"409", "411", "412"}), "DubPulse", frozenset({"3", "4", "6", "10"}),
    ),
)

EXPECTED_COVERAGE = "G4_I6_CONTRACT_COVERAGE proven=11 provisional=0 review_required=25 unknown=9600"
EXPECTED_MARKER = "G4_C4_C6_OBVIOUS_ADMISSIONS contracts=4 owners=4 rows=1536 review_required=25 unknown=9600 proven_contracts=11 bass_semantics=INDEPENDENT"


def fail(message: str) -> None:
    print(f"G4_C4_C6_FAIL {message}", file=sys.stderr)
    raise SystemExit(1)


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        required = {
            "owner_mode", "recipe_id", "effective_candidate_count",
            "selected_archetype_id", "RhythmFamily", "bass_identity",
            "contract_id", "contract_status", "evaluation",
        }
        if not required.issubset(set(reader.fieldnames or [])):
            fail("census_schema")
        return list(reader)


def owner_rows(rows: list[dict[str, str]], contract: Contract) -> list[dict[str, str]]:
    return [
        row for row in rows
        if row["owner_mode"] == contract.mode and row["recipe_id"] == contract.recipe
    ]


def verify_owner(rows: list[dict[str, str]], contract: Contract) -> None:
    selected = owner_rows(rows, contract)
    key = f"{contract.mode}/{contract.recipe}"
    if len(selected) != 384:
        fail(f"owner={key} rows={len(selected)} expected=384")
    if {row["effective_candidate_count"] for row in selected} != {contract.candidate_count}:
        fail(f"owner={key} candidate_count_drift")
    if {row["selected_archetype_id"] for row in selected} != set(contract.archetypes):
        fail(f"owner={key} archetype_set_drift")
    if {row["RhythmFamily"] for row in selected} != {contract.family}:
        fail(f"owner={key} family_drift")
    if {row["bass_identity"] for row in selected} != set(contract.bass_ids):
        fail(f"owner={key} bass_space_changed")
    if {row["contract_id"] for row in selected} != {contract.contract_id}:
        actual = sorted({row["contract_id"] for row in selected})
        fail(f"owner={key} contract={actual} expected={contract.contract_id}")
    if {row["contract_status"] for row in selected} != {"PROVEN"}:
        fail(f"owner={key} contract_not_proven")
    if {row["evaluation"] for row in selected} != {"SATISFIED"}:
        fail(f"owner={key} evaluation_not_satisfied")


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: test_gf2_g4_c4_c6_obvious_admissions.py RUN_DIR")
    run_dir = Path(sys.argv[1])
    census = run_dir / "GF2_G4_I6_OWNERSHIP_CENSUS.tsv"
    summary = run_dir / "g4-i6-summary.txt"
    report = run_dir / "GF2_G4_I6_OWNERSHIP_CENSUS.md"
    for path in (census, summary, report):
        if not path.is_file():
            fail(f"missing_artifact={path.name}")

    rows = read_rows(census)
    for contract in CONTRACTS:
        verify_owner(rows, contract)

    # Minimal Space is intentionally BASE minus Steppers. This prohibition is
    # part of the contract, not an incidental family label.
    base = next(c for c in CONTRACTS if c.mode == "5" and c.recipe == "0")
    minimal = next(c for c in CONTRACTS if c.mode == "5" and c.recipe == "11")
    if set(base.archetypes) - set(minimal.archetypes) != {"410"}:
        fail("reggae_minimal_space_not_base_minus_steppers")
    if "410" in minimal.archetypes:
        fail("reggae_minimal_space_admits_steppers")

    summary_text = summary.read_text(encoding="utf-8")
    if EXPECTED_COVERAGE not in summary_text:
        fail("summary_contract_coverage_not_11_25_9600")
    if "review_required=25 unknown=9600" not in summary_text:
        fail("summary_findings_not_25_9600")
    if EXPECTED_MARKER not in summary_text:
        fail("summary_wave_marker_missing")

    report_text = report.read_text(encoding="utf-8")
    for contract in CONTRACTS:
        if contract.contract_id not in report_text:
            fail(f"report_missing_contract={contract.contract_id}")
    if "bass semantics remain independent" not in report_text.lower():
        fail("report_bass_independence_boundary_missing")

    print("G4-C4-C6 obvious admission wave: PASS")


if __name__ == "__main__":
    main()
