#!/usr/bin/env python3
from __future__ import annotations

import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CSV_PATH = ROOT / "docs/architecture/ATLAS_PASS1_CANDIDATES.csv"

EXPECTED_COLUMNS = [
    "candidate_id",
    "domain",
    "primary_evidence_class",
    "supporting_evidence",
    "distinctness",
    "confidence",
    "runtime_owner",
    "decision",
    "reason",
]

ALLOWED_EVIDENCE = {
    "MEASURED",
    "EDITORIAL_CURATED",
    "PROJECT_OWNED_EXACT",
    "RESEARCH_AGGREGATE",
    "INSUFFICIENT",
}

ALLOWED_DECISIONS = {
    "ACCEPT_BASELINE",
    "ACCEPT_TRANSFORM",
    "REVIEW",
    "HOLD",
}

# Review-only annotation. This field is deliberately non-normative; keeping
# the labels bounded prevents accidental schema drift without turning them
# into an admission score.
ALLOWED_CONFIDENCE_LABELS = {
    "high",
    "medium_high",
    "medium",
    "low_medium",
    "low",
}

ALLOWED_DOMAINS = {
    "Rhythm",
    "PhraseTransform",
    "PhraseTrajectory",
    "BassRhythm",
    "BassPitch",
    "Motif",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require(CSV_PATH.is_file(), f"missing Atlas Pass 1 CSV: {CSV_PATH}")

    with CSV_PATH.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        require(reader.fieldnames == EXPECTED_COLUMNS,
                f"Atlas Pass 1 CSV columns changed: {reader.fieldnames}")
        rows = list(reader)

    require(rows, "Atlas Pass 1 CSV must contain candidates")

    seen_ids: set[str] = set()
    for line_number, row in enumerate(rows, start=2):
        candidate_id = row["candidate_id"].strip()
        require(candidate_id != "", f"line {line_number}: empty candidate_id")
        require(candidate_id not in seen_ids,
                f"line {line_number}: duplicate candidate_id {candidate_id}")
        seen_ids.add(candidate_id)

        require(row["domain"] in ALLOWED_DOMAINS,
                f"line {line_number}: unsupported domain {row['domain']}")
        require(row["primary_evidence_class"] in ALLOWED_EVIDENCE,
                f"line {line_number}: invalid primary evidence class "
                f"{row['primary_evidence_class']}")
        require(row["decision"] in ALLOWED_DECISIONS,
                f"line {line_number}: invalid decision {row['decision']}")
        require(row["confidence"] in ALLOWED_CONFIDENCE_LABELS,
                f"line {line_number}: invalid confidence label "
                f"{row['confidence']}")

        for field in (
            "supporting_evidence",
            "distinctness",
            "runtime_owner",
            "reason",
        ):
            require(row[field].strip() != "",
                    f"line {line_number}: empty {field}")

        # Orthogonal extraction: a candidate row has exactly one runtime owner.
        owner = row["runtime_owner"]
        require("+" not in owner and "/" not in owner and "Split" not in owner,
                f"line {line_number}: fused runtime owner {owner}")

    print(f"Atlas Pass 1 schema: OK ({len(rows)} candidates)")


if __name__ == "__main__":
    main()
