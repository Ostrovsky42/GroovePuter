#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path

CENSUS = "GF2_G4_I6_OWNERSHIP_CENSUS.tsv"
REPORT = "GF2_G4_I6_OWNERSHIP_CENSUS.md"
SUMMARY = "g4-i6-summary.txt"

BROKEN_MODE = "7"
BROKEN_RECIPE = "2"
DNB_MODE = "14"
DNB_RECIPE = "0"
EXPECTED_ROWS = 128 * 3
EXPECTED_CANDIDATES = 4
EXPECTED_ARCHETYPES = {"413", "414", "415", "416"}
BROKEN_BASS = {"2", "5", "7", "9"}
DNB_BASS = {"3", "4", "8", "9"}

C3_ID = "G4-C3-BROKEN-DNB-ADMISSION-BREAKBEAT-ALIAS"
C1_ID = "G4-C1-DNB-MATERIALIZED-BASS-SPACE"
SOURCE_REVIEW_ID = "I6-REVIEW-RHYTHM-OWNERSHIP"

REGISTRY_ROW = (
    "| G4-C3-BROKEN-DNB-ADMISSION-BREAKBEAT-ALIAS | 1 | Broken / Drum&Bass (mode=7, recipe=2) "
    "| rhythm admission | effective admitted candidate space | forall admitted candidates "
    "| exact owner (mode=7, recipe=2) | effective admitted archetype set equals {413,414,415,416}; "
    "all candidates are RhythmFamily Breakbeat | G4-C3 | PROVEN |"
)


def die(message: str) -> None:
    print(f"G4_C3_PROMOTION_FAIL reason={message}", file=sys.stderr)
    raise SystemExit(1)


def read_tsv(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open(encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if reader.fieldnames is None:
            die("census_header_missing")
        return list(reader.fieldnames), list(reader)


def write_tsv(path: Path, fields: list[str], rows: list[dict[str, str]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=fields,
            delimiter="\t",
            lineterminator="\n",
            extrasaction="raise",
        )
        writer.writeheader()
        writer.writerows(rows)


def owner_rows(
    rows: list[dict[str, str]], mode: str, recipe: str
) -> list[dict[str, str]]:
    return [
        row for row in rows
        if row["owner_mode"] == mode and row["recipe_id"] == recipe
    ]


def validate_admission(name: str, rows: list[dict[str, str]]) -> None:
    if len(rows) != EXPECTED_ROWS:
        die(f"{name}_rows:{len(rows)}!={EXPECTED_ROWS}")

    counts = {int(row["effective_candidate_count"]) for row in rows}
    if counts != {EXPECTED_CANDIDATES}:
        die(f"{name}_candidate_count:{sorted(counts)}!=[{EXPECTED_CANDIDATES}]")

    archetypes = {row["selected_archetype_id"] for row in rows}
    if archetypes != EXPECTED_ARCHETYPES:
        die(
            f"{name}_admission_set:observed={sorted(archetypes)} "
            f"expected={sorted(EXPECTED_ARCHETYPES)}"
        )

    families = {row["RhythmFamily"] for row in rows}
    if families != {"Breakbeat"}:
        die(f"{name}_rhythm_family:{sorted(families)}!=['Breakbeat']")


def validate_pre_state(
    broken: list[dict[str, str]], dnb: list[dict[str, str]]
) -> tuple[set[str], set[str]]:
    for row in broken:
        if (
            row["contract_id"] != SOURCE_REVIEW_ID
            or row["contract_status"] != "REVIEW_REQUIRED"
            or row["evaluation"] != "UNKNOWN"
        ):
            die(
                "broken_unexpected_pre_promotion_state:"
                f"contract={row['contract_id']} status={row['contract_status']} "
                f"evaluation={row['evaluation']}"
            )

    for row in dnb:
        if (
            row["contract_id"] != C1_ID
            or row["contract_status"] != "PROVEN"
            or row["evaluation"] != "SATISFIED"
        ):
            die(
                "canonical_dnb_c1_state_drift:"
                f"contract={row['contract_id']} status={row['contract_status']} "
                f"evaluation={row['evaluation']}"
            )

    broken_bass = {row["bass_identity"] for row in broken}
    dnb_bass = {row["bass_identity"] for row in dnb}
    if broken_bass != BROKEN_BASS:
        die(f"broken_bass_space:{sorted(broken_bass)}!={sorted(BROKEN_BASS)}")
    if dnb_bass != DNB_BASS:
        die(f"canonical_dnb_bass_space:{sorted(dnb_bass)}!={sorted(DNB_BASS)}")
    if broken_bass == dnb_bass:
        die("bass_spaces_collapsed_owner_equivalence_not_allowed")

    return broken_bass, dnb_bass


def promote_census(path: Path) -> tuple[int, int, int, int]:
    fields, rows = read_tsv(path)
    required = {
        "owner_mode",
        "recipe_id",
        "effective_candidate_count",
        "selected_archetype_id",
        "RhythmFamily",
        "bass_identity",
        "contract_id",
        "contract_status",
        "contract_scope",
        "contract_quantifier",
        "expected_witness",
        "actual_witness",
        "evaluation",
        "reason",
        "offending_stage",
    }
    if not required.issubset(fields):
        die("census_schema")

    broken = owner_rows(rows, BROKEN_MODE, BROKEN_RECIPE)
    dnb = owner_rows(rows, DNB_MODE, DNB_RECIPE)
    validate_admission("broken_dnb", broken)
    validate_admission("canonical_dnb", dnb)
    broken_bass, dnb_bass = validate_pre_state(broken, dnb)

    # The equality claim is deliberately limited to rhythm admission. Bass is
    # observed only as a negative boundary against accidental owner equivalence.
    for row in broken:
        row["contract_id"] = C3_ID
        row["contract_status"] = "PROVEN"
        row["contract_scope"] = "one-bar address-0; identities 1..128; P1/P2/P3"
        row["contract_quantifier"] = (
            "forall materialized rows for exact Broken/DnB owner under admission predicate"
        )
        row["expected_witness"] = "Breakbeat+admission={413,414,415,416}"
        row["actual_witness"] = f"Breakbeat+archetype={row['selected_archetype_id']}"
        row["evaluation"] = "SATISFIED"
        row["reason"] = (
            "selected archetype belongs to proven Broken/DnB Breakbeat admission alias; "
            "bass semantics independent"
        )
        row["offending_stage"] = "NONE"

    write_tsv(path, fields, rows)
    return len(broken), len(EXPECTED_ARCHETYPES), len(broken_bass), len(dnb_bass)


def replace_once(text: str, old: str, new: str, fault: str) -> str:
    if old not in text:
        die(fault)
    return text.replace(old, new, 1)


def update_report(path: Path) -> None:
    text = path.read_text(encoding="utf-8")
    text = replace_once(
        text, "proven_contract_owners=3", "proven_contract_owners=4", "report_owner_count"
    )
    text = replace_once(text, "proven_contracts=6", "proven_contracts=7", "report_contract_count")
    text = replace_once(text, "review_required=30", "review_required=29", "report_review_metric")
    text = replace_once(
        text, "unknown_evaluations=11520", "unknown_evaluations=11136", "report_unknown_metric"
    )
    text = replace_once(
        text,
        "30 shipped owners have no applicable PROVEN rhythm admission contract in I6.",
        "29 shipped owners have no applicable PROVEN rhythm admission contract in I6.",
        "report_review_required_text",
    )
    text = replace_once(
        text,
        "11520 materialized rows are observations under REVIEW_REQUIRED owners.",
        "11136 materialized rows are observations under REVIEW_REQUIRED owners.",
        "report_unknown_text",
    )

    proven_anchor = (
        "- G4-C1: Drum&Bass / BASE admits only Breakbeat archetypes in a plural space; "
        "materialized bass selection stays inside KickAnswer / GapFill / HalfTimePocket / SyncopatedHook.\n"
    )
    text = replace_once(
        text,
        proven_anchor,
        proven_anchor
        + "- G4-C3: Broken / Drum&Bass has the same four-member Breakbeat rhythm-admission set "
        "as canonical DnB; bass semantics remain independent and are not inherited.\n",
        "report_proven_contract_anchor",
    )

    c1_registry = (
        "| G4-C1-DNB-MATERIALIZED-BASS-SPACE | 1 | Drum&Bass / BASE (mode=14, recipe=0) "
        "| bass selection | one-bar address-0; identities 1..128; P1/P2/P3 | forall materialized rows "
        "| exact owner (mode=14, recipe=0) | selected archetype is Breakbeat and bass identity is one of "
        "KickAnswer, GapFill, HalfTimePocket, SyncopatedHook | G4-C1 | PROVEN |\n"
    )
    text = replace_once(
        text,
        c1_registry,
        c1_registry + REGISTRY_ROW + "\n",
        "report_registry_anchor",
    )

    registry_tail = (
        "DnB C1 predicates are structural: RhythmFamily and BassRhythmId only; labels and weights are not evidence."
    )
    text = replace_once(
        text,
        registry_tail,
        registry_tail
        + " C3 aliases only the Broken / Drum&Bass rhythm-admission set; bass semantics remain independent, "
        "so no C3 bass or downstream-materialization contract exists.",
        "report_registry_boundary",
    )
    path.write_text(text, encoding="utf-8")


def update_summary(path: Path, rows: int, archetypes: int, broken_bass: int, dnb_bass: int) -> None:
    text = path.read_text(encoding="utf-8")

    findings = re.compile(
        r"(?m)^(G4_I6_FINDINGS .* review_required=)30( unknown=)11520$"
    )
    text, count = findings.subn(r"\g<1>29\g<2>11136", text, count=1)
    if count != 1:
        die("summary_findings")

    coverage = re.compile(
        r"(?m)^G4_I6_CONTRACT_COVERAGE proven=6 provisional=0 review_required=30 unknown=11520$"
    )
    text, count = coverage.subn(
        "G4_I6_CONTRACT_COVERAGE proven=7 provisional=0 review_required=29 unknown=11136",
        text,
        count=1,
    )
    if count != 1:
        die("summary_contract_coverage")

    marker = "G4-I6 global ownership census: PASS\n"
    if marker not in text:
        die("summary_pass_marker")
    c3 = (
        f"G4_C3_BROKEN_DNB_ADMISSION equivalence=PROVEN rows={rows} archetypes={archetypes} "
        f"broken_bass_ids={broken_bass} dnb_bass_ids={dnb_bass} bass_semantics=INDEPENDENT\n"
    )
    text = text.replace(marker, c3 + marker, 1)
    path.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-dir", required=True, type=Path)
    args = parser.parse_args()
    run_dir: Path = args.run_dir

    for name in (CENSUS, REPORT, SUMMARY):
        if not (run_dir / name).is_file():
            die(f"missing_artifact:{name}")

    rows, archetypes, broken_bass, dnb_bass = promote_census(run_dir / CENSUS)
    update_report(run_dir / REPORT)
    update_summary(run_dir / SUMMARY, rows, archetypes, broken_bass, dnb_bass)

    print(
        "G4_C3_PROMOTION_PASS "
        f"rows={rows} archetypes={archetypes} broken_bass_ids={broken_bass} dnb_bass_ids={dnb_bass} "
        "review_required=29 unknown=11136 proven_contracts=7 bass_semantics=INDEPENDENT"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
