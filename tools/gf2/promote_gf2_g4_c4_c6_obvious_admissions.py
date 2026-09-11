#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import re
import sys
from dataclasses import dataclass
from pathlib import Path

CENSUS = "GF2_G4_I6_OWNERSHIP_CENSUS.tsv"
REPORT = "GF2_G4_I6_OWNERSHIP_CENSUS.md"
SUMMARY = "g4-i6-summary.txt"
SOURCE_REVIEW_ID = "I6-REVIEW-RHYTHM-OWNERSHIP"
EXPECTED_ROWS = 384


@dataclass(frozen=True)
class Contract:
    key: str
    mode: str
    recipe: str
    contract_id: str
    candidate_count: int
    archetypes: frozenset[str]
    family: str
    bass_ids: frozenset[str]
    expected_witness: str
    registry_owner: str
    registry_predicate: str


CONTRACTS = (
    Contract(
        "classic_2step", "7", "8", "G4-C4-CLASSIC-2STEP-ADMISSION", 2,
        frozenset({"417", "419"}), "UkTwoStep", frozenset({"3", "4", "5", "9"}),
        "admission={417,419};backbeat=4,12;kick_forms=broken+quarter;shuffle=preferred",
        "Broken / Classic 2-Step (mode=7, recipe=8)",
        "effective admitted set is {417,419}; both preserve backbeat anchors 4/12 and shuffle-oriented timing while retaining distinct broken/quarter kick forms",
    ),
    Contract(
        "psytrance", "4", "4", "G4-C5-PSYTRANCE-FOURFLOOR-ADMISSION", 3,
        frozenset({"401", "402", "406"}), "FourFloor", frozenset({"2", "5", "7", "9"}),
        "admission={401,402,406};quarter_kick=0,4,8,12",
        "Rave / Psytrance (mode=4, recipe=4)",
        "effective admitted set is {401,402,406}; every admitted idea preserves quarter-kick anchors 0/4/8/12",
    ),
    Contract(
        "reggae_base", "5", "0", "G4-C6-REGGAE-BASE-DUBPULSE-ADMISSION", 4,
        frozenset({"409", "410", "411", "412"}), "DubPulse", frozenset({"3", "4", "6", "10"}),
        "admission={409,410,411,412};dubpulse_vocabulary=full",
        "Reggae / BASE (mode=5, recipe=0)",
        "effective admitted set is the full current DubPulse vocabulary {409,410,411,412}",
    ),
    Contract(
        "reggae_minimal", "5", "11", "G4-C6-REGGAE-MINIMAL-SPACE-DUBPULSE-ADMISSION", 3,
        frozenset({"409", "411", "412"}), "DubPulse", frozenset({"3", "4", "6", "10"}),
        "admission={409,411,412};prohibition=410",
        "Reggae / Minimal Space (mode=5, recipe=11)",
        "effective admitted set is {409,411,412}; Steppers archetype 410 is prohibited relative to Reggae BASE",
    ),
)

REGISTRY_ROWS = tuple(
    f"| {c.contract_id} | 1 | {c.registry_owner} | rhythm admission | effective admitted candidate space | "
    f"forall admitted candidates | exact owner (mode={c.mode}, recipe={c.recipe}) | {c.registry_predicate} | "
    f"G4-{c.contract_id.split('-')[1]} | PROVEN |"
    for c in CONTRACTS
)


def die(message: str) -> None:
    print(f"G4_C4_C6_PROMOTION_FAIL reason={message}", file=sys.stderr)
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
            handle, fieldnames=fields, delimiter="\t", lineterminator="\n", extrasaction="raise"
        )
        writer.writeheader()
        writer.writerows(rows)


def owner_rows(rows: list[dict[str, str]], contract: Contract) -> list[dict[str, str]]:
    return [
        row for row in rows
        if row["owner_mode"] == contract.mode and row["recipe_id"] == contract.recipe
    ]


def validate_owner(contract: Contract, rows: list[dict[str, str]]) -> None:
    if len(rows) != EXPECTED_ROWS:
        die(f"{contract.key}_rows:{len(rows)}!={EXPECTED_ROWS}")
    counts = {int(row["effective_candidate_count"]) for row in rows}
    if counts != {contract.candidate_count}:
        die(f"{contract.key}_candidate_count:{sorted(counts)}!=[{contract.candidate_count}]")
    archetypes = {row["selected_archetype_id"] for row in rows}
    if archetypes != set(contract.archetypes):
        die(f"{contract.key}_admission_set:{sorted(archetypes)}!={sorted(contract.archetypes)}")
    families = {row["RhythmFamily"] for row in rows}
    if families != {contract.family}:
        die(f"{contract.key}_family:{sorted(families)}!=['{contract.family}']")
    bass_ids = {row["bass_identity"] for row in rows}
    if bass_ids != set(contract.bass_ids):
        die(f"{contract.key}_bass_space:{sorted(bass_ids)}!={sorted(contract.bass_ids)}")
    for row in rows:
        if (
            row["contract_id"] != SOURCE_REVIEW_ID
            or row["contract_status"] != "REVIEW_REQUIRED"
            or row["evaluation"] != "UNKNOWN"
        ):
            die(
                f"{contract.key}_unexpected_pre_state:contract={row['contract_id']} "
                f"status={row['contract_status']} evaluation={row['evaluation']}"
            )


def validate_cross_owner_relations(grouped: dict[str, list[dict[str, str]]]) -> None:
    base = {row["selected_archetype_id"] for row in grouped["reggae_base"]}
    minimal = {row["selected_archetype_id"] for row in grouped["reggae_minimal"]}
    if base - minimal != {"410"} or minimal - base:
        die(f"reggae_minimal_not_base_minus_steppers:base={sorted(base)} minimal={sorted(minimal)}")
    if "410" in minimal:
        die("reggae_minimal_reintroduced_steppers")


def promote_census(path: Path) -> tuple[int, int]:
    fields, rows = read_tsv(path)
    required = {
        "owner_mode", "recipe_id", "effective_candidate_count", "selected_archetype_id",
        "RhythmFamily", "bass_identity", "contract_id", "contract_status", "contract_scope",
        "contract_quantifier", "expected_witness", "actual_witness", "evaluation", "reason",
        "offending_stage",
    }
    if not required.issubset(fields):
        die("census_schema")

    grouped = {contract.key: owner_rows(rows, contract) for contract in CONTRACTS}
    for contract in CONTRACTS:
        validate_owner(contract, grouped[contract.key])
    validate_cross_owner_relations(grouped)

    promoted = 0
    for contract in CONTRACTS:
        for row in grouped[contract.key]:
            row["contract_id"] = contract.contract_id
            row["contract_status"] = "PROVEN"
            row["contract_scope"] = "one-bar address-0; identities 1..128; P1/P2/P3"
            row["contract_quantifier"] = "forall materialized rows for exact owner under admission predicate"
            row["expected_witness"] = contract.expected_witness
            row["actual_witness"] = f"{contract.family}+archetype={row['selected_archetype_id']}"
            row["evaluation"] = "SATISFIED"
            row["reason"] = (
                "selected archetype belongs to the owner-specific proven admission set; "
                "bass semantics independent"
            )
            row["offending_stage"] = "NONE"
            promoted += 1

    if promoted != len(CONTRACTS) * EXPECTED_ROWS:
        die(f"promoted_rows:{promoted}!={len(CONTRACTS) * EXPECTED_ROWS}")
    write_tsv(path, fields, rows)
    return len(CONTRACTS), promoted


def replace_once(text: str, old: str, new: str, fault: str) -> str:
    if old not in text:
        die(fault)
    return text.replace(old, new, 1)


def update_report(path: Path) -> None:
    text = path.read_text(encoding="utf-8")
    replacements = (
        ("proven_contract_owners=4", "proven_contract_owners=8", "report_owner_count"),
        ("proven_contracts=7", "proven_contracts=11", "report_contract_count"),
        ("review_required=29", "review_required=25", "report_review_metric"),
        ("unknown_evaluations=11136", "unknown_evaluations=9600", "report_unknown_metric"),
        (
            "29 shipped owners have no applicable PROVEN rhythm admission contract in I6.",
            "25 shipped owners have no applicable PROVEN rhythm admission contract in I6.",
            "report_review_required_text",
        ),
        (
            "11136 materialized rows are observations under REVIEW_REQUIRED owners.",
            "9600 materialized rows are observations under REVIEW_REQUIRED owners.",
            "report_unknown_text",
        ),
    )
    for old, new, fault in replacements:
        text = replace_once(text, old, new, fault)

    c3_bullet = (
        "- G4-C3: Broken / Drum&Bass has the same four-member Breakbeat rhythm-admission set "
        "as canonical DnB; bass semantics remain independent and are not inherited.\n"
    )
    new_bullets = (
        "- G4-C4: Broken / Classic 2-Step admits exactly ClassicTwoStep + ShuffledFourFour: "
        "two kick organizations under a shared 4/12 backbeat and shuffle-oriented timing; 418/420 stay outside the recipe.\n"
        "- G4-C5: Rave / Psytrance admits exactly 401/402/406; each preserves the quarter-kick skeleton 0/4/8/12.\n"
        "- G4-C6: Reggae / BASE owns the full current DubPulse admission set 409/410/411/412, while Minimal Space is exactly 409/411/412 and prohibits Steppers 410.\n"
    )
    text = replace_once(text, c3_bullet, c3_bullet + new_bullets, "report_proven_bullet_anchor")

    c3_registry = (
        "| G4-C3-BROKEN-DNB-ADMISSION-BREAKBEAT-ALIAS | 1 | Broken / Drum&Bass (mode=7, recipe=2) "
        "| rhythm admission | effective admitted candidate space | forall admitted candidates | exact owner (mode=7, recipe=2) "
        "| effective admitted archetype set equals {413,414,415,416}; all candidates are RhythmFamily Breakbeat | G4-C3 | PROVEN |\n"
    )
    text = replace_once(
        text, c3_registry, c3_registry + "\n".join(REGISTRY_ROWS) + "\n", "report_registry_anchor"
    )

    boundary = (
        "C3 aliases only the Broken / Drum&Bass rhythm-admission set; bass semantics remain independent, "
        "so no C3 bass or downstream-materialization contract exists."
    )
    addition = (
        boundary
        + " C4-C6 add owner-specific admission predicates only; bass semantics remain independent, and "
        "RhythmFamily labels alone are not used as genre proofs."
    )
    text = replace_once(text, boundary, addition, "report_boundary_anchor")
    path.write_text(text, encoding="utf-8")


def update_summary(path: Path, contracts: int, promoted_rows: int) -> None:
    text = path.read_text(encoding="utf-8")
    findings = re.compile(r"(?m)^(G4_I6_FINDINGS .* review_required=)29( unknown=)11136$")
    text, count = findings.subn(r"\g<1>25\g<2>9600", text, count=1)
    if count != 1:
        die("summary_findings")

    coverage = re.compile(
        r"(?m)^G4_I6_CONTRACT_COVERAGE proven=7 provisional=0 review_required=29 unknown=11136$"
    )
    text, count = coverage.subn(
        "G4_I6_CONTRACT_COVERAGE proven=11 provisional=0 review_required=25 unknown=9600",
        text,
        count=1,
    )
    if count != 1:
        die("summary_contract_coverage")

    marker = "G4-I6 global ownership census: PASS\n"
    if marker not in text:
        die("summary_pass_marker")
    wave = (
        f"G4_C4_C6_OBVIOUS_ADMISSIONS contracts={contracts} owners={contracts} rows={promoted_rows} "
        "review_required=25 unknown=9600 proven_contracts=11 bass_semantics=INDEPENDENT\n"
    )
    text = text.replace(marker, wave + marker, 1)
    path.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-dir", required=True, type=Path)
    args = parser.parse_args()
    run_dir: Path = args.run_dir
    for name in (CENSUS, REPORT, SUMMARY):
        if not (run_dir / name).is_file():
            die(f"missing_artifact:{name}")

    contracts, promoted_rows = promote_census(run_dir / CENSUS)
    update_report(run_dir / REPORT)
    update_summary(run_dir / SUMMARY, contracts, promoted_rows)
    print(
        "G4_C4_C6_PROMOTION_PASS "
        f"contracts={contracts} owners={contracts} rows={promoted_rows} "
        "review_required=25 unknown=9600 proven_contracts=11 bass_semantics=INDEPENDENT"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
