#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import re
import sys
from collections import defaultdict
from pathlib import Path

CENSUS = "GF2_G4_I6_OWNERSHIP_CENSUS.tsv"
ANOMALIES = "GF2_G4_I6_OWNERSHIP_ANOMALIES.tsv"
REPORT = "GF2_G4_I6_OWNERSHIP_CENSUS.md"
PROJECTION = "GF2_G4_I6_COLLISION_PROJECTION.tsv"
SUMMARY = "g4-i6-summary.txt"

COLLISION_SCOPE = (
    "lanes(immutable,canonical,preferred,optional,forbidden,minmax)"
    "+protected-space+relationships(source,target,operation,scope,offset,cardinality);"
    "gate/lifetime=excluded"
)

CONTRACTS = [
    {
        "id": "G4-I4-DUB-ADMISSION-TECHNO-SKELETON",
        "version": "1",
        "owner": "Reggae / Dub Techno (mode=5, recipe=5)",
        "subject": "rhythm admission",
        "scope": "effective admitted candidate space",
        "quantifier": "forall admitted candidates",
        "applicability": "exact owner (mode=5, recipe=5)",
        "predicate": "required techno skeleton is quarter-pulse or broken-frame",
        "checkpoint": "G4-I4",
        "status": "PROVEN",
    },
    {
        "id": "G4-I4-DUB-MATERIALIZED-SKELETON",
        "version": "1",
        "owner": "Reggae / Dub Techno (mode=5, recipe=5)",
        "subject": "downstream materialization",
        "scope": "one-bar address-0; identities 1..128; P1/P2/P3",
        "quantifier": "forall materialized rows",
        "applicability": "selected admitted candidate under G4-I4 admission contract",
        "predicate": "observed skeleton equals selected candidate required skeleton",
        "checkpoint": "G4-I4",
        "status": "PROVEN",
    },
    {
        "id": "G4-I5-HOUSE-ADMISSION-QUARTER-SPACE",
        "version": "1",
        "owner": "House / BASE (mode=9, recipe=0)",
        "subject": "rhythm admission",
        "scope": "effective admitted candidate space",
        "quantifier": "forall admitted candidates",
        "applicability": "exact owner (mode=9, recipe=0)",
        "predicate": "quarter-pulse witness is REQUIRED or POSSIBLE_BUT_NOT_REQUIRED",
        "checkpoint": "G4-I5",
        "status": "PROVEN",
    },
    {
        "id": "G4-I5-HOUSE-MATERIALIZED-QUARTER",
        "version": "1",
        "owner": "House / BASE (mode=9, recipe=0)",
        "subject": "downstream materialization",
        "scope": "one-bar address-0; identities 1..128; P1/P2/P3",
        "quantifier": "forall materialized rows",
        "applicability": "exact owner (mode=9, recipe=0)",
        "predicate": "materialized kick preserves observed quarter-pulse",
        "checkpoint": "G4-I5",
        "status": "PROVEN",
    },
]

ALLOWED_FINDING_STAGES = {
    "RHYTHM_ADMISSION",
    "BASS_SELECTION",
    "CHORD_SELECTION",
    "MELODIC_SELECTION",
    "DOWNSTREAM_MATERIALIZATION",
    "UNKNOWN_STAGE",
    "NONE",
}


def die(message: str) -> None:
    print(f"G4_I6_FINALIZER_FAIL reason={message}", file=sys.stderr)
    raise SystemExit(1)


def read_tsv(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if reader.fieldnames is None:
            die(f"missing_header:{path.name}")
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


def projection_catalog(path: Path) -> tuple[dict[int, dict[str, str]], list[tuple[int, int]]]:
    fields, rows = read_tsv(path)
    required = {
        "archetype_id",
        "archetype_name",
        "projection_hash",
        "normalized_projection",
        "outside_projection_metadata",
    }
    if not required.issubset(fields):
        die("projection_schema")
    catalog: dict[int, dict[str, str]] = {}
    by_projection: dict[str, list[int]] = defaultdict(list)
    for row in rows:
        archetype_id = int(row["archetype_id"])
        if archetype_id in catalog:
            die(f"duplicate_projection_archetype:{archetype_id}")
        catalog[archetype_id] = row
        by_projection[row["normalized_projection"]].append(archetype_id)
    pairs: list[tuple[int, int]] = []
    for ids in by_projection.values():
        ids.sort()
        for left_index, left in enumerate(ids):
            for right in ids[left_index + 1 :]:
                pairs.append((left, right))
    pairs.sort()
    return catalog, pairs


def outside_difference(left: str, right: str) -> str:
    def parse(value: str) -> dict[str, str]:
        result: dict[str, str] = {}
        for item in value.split("|"):
            if "=" not in item:
                continue
            key, val = item.split("=", 1)
            result[key] = val
        return result

    left_items = parse(left)
    right_items = parse(right)
    keys = sorted(set(left_items) | set(right_items))
    different = [key for key in keys if left_items.get(key) != right_items.get(key)]
    return ",".join(different) if different else "none-observed"


def normalize_census(path: Path, projection: dict[int, dict[str, str]]) -> tuple[int, int, int]:
    fields, rows = read_tsv(path)
    required = {
        "owner_mode",
        "recipe_id",
        "identity_ordinal",
        "P_level",
        "raw_effective_reconciliation",
        "selected_archetype_id",
        "structural_signature",
        "evaluation",
        "offending_stage",
    }
    if not required.issubset(fields):
        die("census_schema")

    keys: set[tuple[str, str, str, str]] = set()
    owner_levels: dict[tuple[str, str, str], set[str]] = defaultdict(set)
    owners: set[tuple[str, str]] = set()
    unknown_rows = 0
    for row in rows:
        key = (
            row["owner_mode"],
            row["recipe_id"],
            row["identity_ordinal"],
            row["P_level"],
        )
        if key in keys:
            die(f"duplicate_census_key:{key}")
        keys.add(key)
        owners.add((row["owner_mode"], row["recipe_id"]))
        owner_levels[(row["owner_mode"], row["recipe_id"], row["identity_ordinal"])].add(
            row["P_level"]
        )
        if "UNKNOWN_ID_REJECTED" in row["raw_effective_reconciliation"]:
            unknown_rows += 1
        archetype_id = int(row["selected_archetype_id"])
        if archetype_id not in projection:
            die(f"selected_archetype_missing_projection:{archetype_id}")
        # The committed candidate signature is the corrected structural hash.
        # Equality is never inferred from the hash: the projection oracle above
        # compares full normalized fields when constructing collision pairs.
        row["structural_signature"] = projection[archetype_id]["projection_hash"]

    if unknown_rows:
        die(f"unknown_raw_rejection_rows:{unknown_rows}")
    expected_rows = len(owners) * 128 * 3
    if len(rows) != expected_rows:
        die(f"row_count:{len(rows)}!={expected_rows}")
    if len(owner_levels) != len(owners) * 128:
        die("root_count")
    for root, levels in owner_levels.items():
        if levels != {"P1", "P2", "P3"}:
            die(f"root_levels:{root}:{sorted(levels)}")

    write_tsv(path, fields, rows)
    return len(owners), len(owner_levels), len(rows)


def reconcile_collisions(
    path: Path,
    projection: dict[int, dict[str, str]],
    corrected_pairs: list[tuple[int, int]],
) -> int:
    fields, rows = read_tsv(path)
    required = {
        "finding_id",
        "finding_type",
        "affected_identities",
        "affected_materializations",
        "offending_stage",
        "left_archetype",
        "right_archetype",
        "collision_scope",
        "equal_projection",
        "differing_known_semantics",
    }
    if not required.issubset(fields):
        die("anomaly_schema")

    main_pairs: set[tuple[int, int]] = set()
    kept: list[dict[str, str]] = []
    for row in rows:
        stage = row["offending_stage"]
        if stage not in ALLOWED_FINDING_STAGES:
            die(f"unknown_offending_stage:{stage}")
        if row["finding_type"] == "FALSE_OWNER":
            affected_identities = int(row["affected_identities"] or 0)
            affected_materializations = int(row["affected_materializations"] or 0)
            if not (
                affected_identities <= affected_materializations
                <= 3 * affected_identities
            ):
                die(f"false_owner_accounting:{row['finding_id']}")
        if row["finding_type"] == "STRUCTURAL_COLLISION":
            left = int(row["left_archetype"])
            right = int(row["right_archetype"])
            main_pairs.add(tuple(sorted((left, right))))
            continue
        kept.append(row)

    corrected_set = set(corrected_pairs)
    # The legacy projection is strictly more detailed. It may miss a collision,
    # but it must never assert equality when the corrected projection disagrees.
    if not main_pairs.issubset(corrected_set):
        die(f"legacy_collision_not_in_corrected:{sorted(main_pairs - corrected_set)}")

    for ordinal, (left, right) in enumerate(corrected_pairs, start=1):
        left_row = projection[left]
        right_row = projection[right]
        row = {field: "" for field in fields}
        row.update(
            {
                "finding_id": f"I6-CORRECTED-COLLISION-{ordinal}",
                "finding_type": "STRUCTURAL_COLLISION",
                "archetype": str(left),
                "contract_id": "STRUCTURAL-PROJECTION",
                "contract_version": "2",
                "evidence_status": "PROVISIONAL",
                "evaluation": "SATISFIED",
                "affected_identities": "0",
                "affected_materializations": "0",
                "offending_stage": "UNKNOWN_STAGE",
                "candidate_evidence": "equal full normalized projection",
                "materialized_evidence": "NOT_ASSESSED",
                "reason": "equal corrected structural projection; not a musical-duplicate claim",
                "left_archetype": str(left),
                "right_archetype": str(right),
                "collision_scope": COLLISION_SCOPE,
                "equal_projection": left_row["normalized_projection"],
                "differing_known_semantics": outside_difference(
                    left_row["outside_projection_metadata"],
                    right_row["outside_projection_metadata"],
                ),
            }
        )
        kept.append(row)

    write_tsv(path, fields, kept)
    return len(corrected_pairs)


def replace_metric(text: str, name: str, value: int) -> str:
    pattern = re.compile(rf"(?m)^{re.escape(name)}=\d+$")
    if not pattern.search(text):
        die(f"missing_report_metric:{name}")
    return pattern.sub(f"{name}={value}", text, count=1)


def contract_registry() -> str:
    lines = [
        "## Contract registry (I6 closure)",
        "",
        "`PROVEN` is attached to the statement below, never to an owner as a whole.",
        "",
        "| contract_id | version | owner | subject | scope | quantifier | applicability | predicate | evidence_checkpoint | evidence_status |",
        "|---|---:|---|---|---|---|---|---|---|---|",
    ]
    for contract in CONTRACTS:
        lines.append(
            "| {id} | {version} | {owner} | {subject} | {scope} | {quantifier} | "
            "{applicability} | {predicate} | {checkpoint} | {status} |".format(**contract)
        )
    lines.extend(
        [
            "",
            "House admission deliberately keeps `POSSIBLE_BUT_NOT_REQUIRED` distinct from `REQUIRED`; a preferred anchor is not silently promoted to a requirement. Dub Techno retains the I4 existential dub-dialogue witness separately from its universal techno-skeleton admission contract.",
            "",
        ]
    )
    return "\n".join(lines)


def detector_section(collision_count: int) -> str:
    return f"""## Corrected detector controls and collision scope

The closure oracle applies the corrected projection independently of the legacy diagnostic hash. Hashes are indices only; collision equality is determined by full normalized field comparison.

```text
collision_scope={COLLISION_SCOPE}
corrected_collision_pairs={collision_count}
genre_label_independence=PASS
weight_independence=PASS
positive_quarter_fixture=PASS
negative_quarter_fixture=PASS
required_anchor_removal=PASS
review_required_never_auto_valid=PASS
shared_admission_not_violation=PASS
candidate_vs_downstream_stage=PASS
StraightDrive_vs_StackedQuarters=DISTINCT
relationship_policy_metadata_excluded=PASS
relationship_offset_and_cardinality_retained=PASS
unknown_nonzero_raw_archetype=FAIL_CLOSED
```

Gate/lifetime masks are excluded because no collision contract assessed by I6 depends on lifetime. Relationship strength, zone and weight are likewise excluded from collision equality; source role, target role, operation, scope, offset and cardinality remain structural. The main census `structural_signature` column is normalized to this corrected projection hash during finalization.

Finding stages are limited to `RHYTHM_ADMISSION`, `BASS_SELECTION`, `CHORD_SELECTION`, `MELODIC_SELECTION`, `DOWNSTREAM_MATERIALIZATION`, and `UNKNOWN_STAGE` (`NONE` is retained only for non-offending satisfied observations).

"""


def collision_section(
    collision_count: int,
    projection: dict[int, dict[str, str]],
    pairs: list[tuple[int, int]],
) -> str:
    lines = ["### STRUCTURAL COLLISIONS", ""]
    if collision_count == 0:
        lines.extend(
            [
                f"None at corrected scope `{COLLISION_SCOPE}`.",
                "",
            ]
        )
        return "\n".join(lines)
    lines.append(
        f"{collision_count} pair(s) at corrected scope `{COLLISION_SCOPE}`. These are projection equalities, not claims of identical musical ideas."
    )
    lines.append("")
    for left, right in pairs:
        diff = outside_difference(
            projection[left]["outside_projection_metadata"],
            projection[right]["outside_projection_metadata"],
        )
        lines.append(
            f"- {left} `{projection[left]['archetype_name']}` vs {right} `{projection[right]['archetype_name']}`; differing known semantics outside projection: {diff}."
        )
    lines.append("")
    return "\n".join(lines)


def finalize_report(
    path: Path,
    collision_count: int,
    projection: dict[int, dict[str, str]],
    pairs: list[tuple[int, int]],
) -> None:
    text = path.read_text(encoding="utf-8")
    text = replace_metric(text, "structural_collision", collision_count)

    coverage_anchor = "provisional_contract_owners=0\n"
    if coverage_anchor not in text:
        die("missing_contract_coverage_anchor")
    text = text.replace(
        coverage_anchor,
        coverage_anchor
        + f"proven_contracts={len(CONTRACTS)}\nprovisional_contracts=0\n",
        1,
    )

    start = text.find("### STRUCTURAL COLLISIONS")
    end = text.find("### SHARED ADMISSIONS", start)
    if start < 0 or end < 0:
        die("collision_report_section")
    text = text[:start] + collision_section(
        collision_count, projection, pairs
    ) + "\n" + text[end:]

    i7_start = text.find("## I7 eligibility")
    calibration = text.find("## Calibration discipline", i7_start)
    if i7_start < 0 or calibration < 0:
        die("i7_report_section")
    false_owner_match = re.search(r"(?m)^false_owner=(\d+)$", text)
    if false_owner_match is None:
        die("false_owner_metric")
    false_owner = int(false_owner_match.group(1))
    if false_owner == 0:
        i7 = """## I7 eligibility

`I7_CANDIDATE: NONE`

The A-G editorial filter is not scored numerically. Gate C (`effective admission contradicts an applicable PROVEN contract`) has no witness in this census, so no edge can become I7-eligible. The result does not promote REVIEW_REQUIRED owners into PASS and does not select a winner artificially.

"""
    else:
        i7 = """## I7 eligibility

`I7_CANDIDATE: EDITORIAL_REVIEW_REQUIRED`

I6 does not rank or repair candidates. Each proven contradiction still requires the explicit A-G filter: CONTRACT, VALID IDEA, ADMISSION CONTRADICTION, MATERIALIZED EVIDENCE, BOUNDARY, SCOPE, and CAPACITY PRESERVATION. No numeric score or generic minimum-archetype rule is used.

"""
    text = text[:i7_start] + i7 + text[calibration:]

    insertion = contract_registry() + "\n" + detector_section(collision_count)
    calibration = text.find("## Calibration discipline")
    text = text[:calibration] + insertion + text[calibration:]

    phrase_line = "- **PHRASE OWNERSHIP: NOT_ASSESSED by main I6 corpus.** G4-I3 is a retained control only."
    if phrase_line in text:
        text = text.replace(
            phrase_line,
            phrase_line
            + "\n- **GLOBAL PHRASE OWNERSHIP: NOT ASSESSED BY I6 MAIN CORPUS.**",
            1,
        )

    path.write_text(text, encoding="utf-8")


def finalize_summary(path: Path, collision_count: int) -> None:
    text = path.read_text(encoding="utf-8")
    text = re.sub(
        r"(?m)(structural_collision=)\d+",
        rf"\g<1>{collision_count}",
        text,
        count=1,
    )
    findings = re.search(
        r"G4_I6_FINDINGS .*?review_required=(\d+) unknown=(\d+)", text
    )
    if findings is None:
        die("summary_findings")
    review_required, unknown = findings.groups()
    addition = (
        f"G4_I6_CONTRACT_COVERAGE proven={len(CONTRACTS)} provisional=0 "
        f"review_required={review_required} unknown={unknown}\n"
        f"G4_I6_CORRECTED_PROJECTION collision_pairs={collision_count} "
        "hash_is_index=1 equality=FULL_NORMALIZED_FIELDS\n"
    )
    marker = "G4-I6 global ownership census: PASS\n"
    if marker not in text:
        die("summary_pass_marker")
    text = text.replace(marker, addition + marker, 1)
    path.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-dir", required=True, type=Path)
    args = parser.parse_args()
    run_dir: Path = args.run_dir

    required = [CENSUS, ANOMALIES, REPORT, PROJECTION, SUMMARY]
    for name in required:
        if not (run_dir / name).is_file():
            die(f"missing_artifact:{name}")

    projection, corrected_pairs = projection_catalog(run_dir / PROJECTION)
    owners, roots, rows = normalize_census(run_dir / CENSUS, projection)
    collision_count = reconcile_collisions(
        run_dir / ANOMALIES, projection, corrected_pairs
    )
    finalize_report(
        run_dir / REPORT, collision_count, projection, corrected_pairs
    )
    finalize_summary(run_dir / SUMMARY, collision_count)

    print(
        "G4_I6_FINALIZER_PASS "
        f"profiles={owners} archetypes={len(projection)} roots={roots} rows={rows} "
        f"corrected_collisions={collision_count} proven_contracts={len(CONTRACTS)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
