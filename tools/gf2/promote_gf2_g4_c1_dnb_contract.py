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

OWNER_MODE = "14"
OWNER_RECIPE = "0"
EXPECTED_ROWS = 128 * 3
ALLOWED_BASS = {"3", "4", "8", "9"}

ADMISSION_ID = "G4-C1-DNB-ADMISSION-BREAKBEAT"
MATERIALIZED_ID = "G4-C1-DNB-MATERIALIZED-BASS-SPACE"
SOURCE_REVIEW_ID = "I6-REVIEW-RHYTHM-OWNERSHIP"

ADMISSION_ROW = (
    "| G4-C1-DNB-ADMISSION-BREAKBEAT | 1 | Drum&Bass / BASE (mode=14, recipe=0) "
    "| rhythm admission | effective admitted candidate space | forall admitted candidates "
    "| exact owner (mode=14, recipe=0) | RhythmFamily is Breakbeat and admitted space remains plural "
    "| G4-C1 | PROVEN |"
)
MATERIALIZED_ROW = (
    "| G4-C1-DNB-MATERIALIZED-BASS-SPACE | 1 | Drum&Bass / BASE (mode=14, recipe=0) "
    "| bass selection | one-bar address-0; identities 1..128; P1/P2/P3 | forall materialized rows "
    "| exact owner (mode=14, recipe=0) | selected archetype is Breakbeat and bass identity is one of "
    "KickAnswer, GapFill, HalfTimePocket, SyncopatedHook | G4-C1 | PROVEN |"
)


def die(message: str) -> None:
    print(f"G4_C1_PROMOTION_FAIL reason={message}", file=sys.stderr)
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


def promote_census(path: Path) -> tuple[int, int, int]:
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

    dnb = [
        row for row in rows
        if row["owner_mode"] == OWNER_MODE and row["recipe_id"] == OWNER_RECIPE
    ]
    if len(dnb) != EXPECTED_ROWS:
        die(f"dnb_rows:{len(dnb)}!={EXPECTED_ROWS}")

    candidate_counts = {int(row["effective_candidate_count"]) for row in dnb}
    if len(candidate_counts) != 1:
        die(f"effective_candidate_count_drift:{sorted(candidate_counts)}")
    candidate_count = next(iter(candidate_counts))
    archetypes = {row["selected_archetype_id"] for row in dnb}
    if candidate_count < 2:
        die(f"dnb_admission_not_plural:{candidate_count}")
    if len(archetypes) != candidate_count:
        die(
            "dnb_effective_admission_not_fully_observed:"
            f"observed={len(archetypes)} effective={candidate_count}"
        )

    bass_ids: set[str] = set()
    for row in dnb:
        if row["RhythmFamily"] != "Breakbeat":
            die(
                "dnb_non_breakbeat_admission:"
                f"archetype={row['selected_archetype_id']} family={row['RhythmFamily']}"
            )
        if row["bass_identity"] not in ALLOWED_BASS:
            die(
                "dnb_bass_outside_contract:"
                f"identity={row.get('identity_ordinal', '?')} bass={row['bass_identity']}"
            )
        bass_ids.add(row["bass_identity"])
        if (
            row["contract_id"] != SOURCE_REVIEW_ID
            or row["contract_status"] != "REVIEW_REQUIRED"
            or row["evaluation"] != "UNKNOWN"
        ):
            die(
                "unexpected_pre_promotion_state:"
                f"contract={row['contract_id']} status={row['contract_status']} "
                f"evaluation={row['evaluation']}"
            )

    if len(bass_ids) < 2:
        die(f"dnb_bass_space_collapsed:{sorted(bass_ids)}")

    for row in dnb:
        row["contract_id"] = MATERIALIZED_ID
        row["contract_status"] = "PROVEN"
        row["contract_scope"] = "one-bar address-0; identities 1..128; P1/P2/P3"
        row["contract_quantifier"] = "forall materialized rows for exact DnB owner"
        row["expected_witness"] = "Breakbeat+DNB_BASS_SPACE"
        row["actual_witness"] = f"Breakbeat+bass={row['bass_identity']}"
        row["evaluation"] = "SATISFIED"
        row["reason"] = "selected Breakbeat archetype with allowed DnB bass identity"
        row["offending_stage"] = "NONE"

    write_tsv(path, fields, rows)
    return len(dnb), len(archetypes), len(bass_ids)


def replace_once(text: str, old: str, new: str, fault: str) -> str:
    if old not in text:
        die(fault)
    return text.replace(old, new, 1)


def update_report(path: Path) -> None:
    text = path.read_text(encoding="utf-8")
    text = replace_once(text, "proven_contract_owners=2", "proven_contract_owners=3", "report_owner_count")
    text = replace_once(text, "proven_contracts=4", "proven_contracts=6", "report_contract_count")
    text = replace_once(text, "review_required=31", "review_required=30", "report_review_metric")
    text = replace_once(text, "unknown_evaluations=11904", "unknown_evaluations=11520", "report_unknown_metric")

    text = re.sub(
        r"### REVIEW_REQUIRED\n\n31 shipped owners have no applicable PROVEN rhythm admission contract in I6\.",
        "### REVIEW_REQUIRED\n\n30 shipped owners have no applicable PROVEN rhythm admission contract in I6.",
        text,
        count=1,
    )
    text = re.sub(
        r"### UNKNOWN\n\n11904 materialized rows are observations under REVIEW_REQUIRED owners\.",
        "### UNKNOWN\n\n11520 materialized rows are observations under REVIEW_REQUIRED owners.",
        text,
        count=1,
    )
    text = text.replace(
        "None under applicable PROVEN I4/I5 admission contracts.",
        "None under applicable PROVEN admission contracts.",
        1,
    )

    proven_anchor = (
        "- G4-I5: candidate quarter space may be REQUIRED or POSSIBLE_BUT_NOT_REQUIRED via preferred anchors; "
        "materialized I5 separately proves observed quarter-pulse preservation.\n"
    )
    if proven_anchor not in text:
        die("report_proven_contract_anchor")
    text = text.replace(
        proven_anchor,
        proven_anchor
        + "- G4-C1: Drum&Bass / BASE admits only Breakbeat archetypes in a plural space; "
        "materialized bass selection stays inside KickAnswer / GapFill / HalfTimePocket / SyncopatedHook.\n",
        1,
    )

    registry_anchor = "\n\nHouse admission deliberately keeps"
    if registry_anchor not in text:
        die("report_registry_anchor")
    text = text.replace(
        registry_anchor,
        "\n" + ADMISSION_ROW + "\n" + MATERIALIZED_ROW + registry_anchor,
        1,
    )
    text = text.replace(
        "Dub Techno retains the I4 existential dub-dialogue witness separately from its universal techno-skeleton admission contract.",
        "Dub Techno retains the I4 existential dub-dialogue witness separately from its universal techno-skeleton admission contract. "
        "DnB C1 predicates are structural: RhythmFamily and BassRhythmId only; labels and weights are not evidence.",
        1,
    )

    path.write_text(text, encoding="utf-8")


def update_summary(path: Path, rows: int, archetypes: int, bass_ids: int) -> None:
    text = path.read_text(encoding="utf-8")
    findings = re.compile(
        r"(?m)^(G4_I6_FINDINGS .* review_required=)31( unknown=)11904$"
    )
    text, count = findings.subn(r"\g<1>30\g<2>11520", text, count=1)
    if count != 1:
        die("summary_findings")

    coverage = re.compile(
        r"(?m)^G4_I6_CONTRACT_COVERAGE proven=4 provisional=0 review_required=31 unknown=11904$"
    )
    text, count = coverage.subn(
        "G4_I6_CONTRACT_COVERAGE proven=6 provisional=0 review_required=30 unknown=11520",
        text,
        count=1,
    )
    if count != 1:
        die("summary_contract_coverage")

    marker = "G4-I6 global ownership census: PASS\n"
    if marker not in text:
        die("summary_pass_marker")
    c1 = (
        f"G4_C1_DNB_CONTRACT admission=PROVEN materialized=PROVEN rows={rows} "
        f"archetypes={archetypes} bass_ids={bass_ids}\n"
    )
    text = text.replace(marker, c1 + marker, 1)
    path.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-dir", required=True, type=Path)
    args = parser.parse_args()
    run_dir: Path = args.run_dir

    for name in (CENSUS, REPORT, SUMMARY):
        if not (run_dir / name).is_file():
            die(f"missing_artifact:{name}")

    rows, archetypes, bass_ids = promote_census(run_dir / CENSUS)
    update_report(run_dir / REPORT)
    update_summary(run_dir / SUMMARY, rows, archetypes, bass_ids)

    print(
        "G4_C1_PROMOTION_PASS "
        f"rows={rows} archetypes={archetypes} bass_ids={bass_ids} "
        "review_required=30 unknown=11520 proven_contracts=6"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
