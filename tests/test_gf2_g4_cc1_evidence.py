#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path

EXPECTED_SUMMARY_LINES = (
    "G4_CC1_PASS adversarial_contract_fixtures",
    "G4_CC1_ACID owners=3 candidates=8 violations=0 weight_independence_violations=0",
    "G4_CC1_PASS acid_bass_articulation_contract",
    "G4_CC1_TECHNO progression_candidates=2 candidate_violations=0 selected=384 selected_violations=0",
    "G4_CC1_PASS techno_no_harmonic_motion_contract",
    "G4_CC1_FUNK candidates=3 admission_violations=0 weight_independence_violations=0 materialized=384 materialized_violations=0",
    "G4_CC1_PASS funk_the_one_contract",
    "G4-CC1 Acid/Techno/Funk contract wave: PASS",
)

EXPECTED_DOC_MARKERS = (
    "G4-CC1-ACID-BASS-ARTICULATION",
    "G4-CC1-TECHNO-NO-HARMONIC-MOTION",
    "G4-CC1-FUNK-THE-ONE",
    "I6 remains `proven=11`, `review_required=25`, `unknown=9600`",
    "current I6 materialized row has one `contract_id` slot",
    "does **not** rewrite the existing I6 census totals",
)


def fail(reason: str) -> None:
    print(f"G4_CC1_EVIDENCE_FAIL reason={reason}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    if len(sys.argv) != 3:
        fail("usage: test_gf2_g4_cc1_evidence.py SUMMARY DOC")

    summary_path = Path(sys.argv[1])
    doc_path = Path(sys.argv[2])
    if not summary_path.is_file():
        fail(f"summary_missing:{summary_path}")
    if not doc_path.is_file():
        fail(f"doc_missing:{doc_path}")

    summary = summary_path.read_text(encoding="utf-8")
    doc = doc_path.read_text(encoding="utf-8")

    for line in EXPECTED_SUMMARY_LINES:
        if line not in summary:
            fail(f"summary_marker_missing:{line}")
    for marker in EXPECTED_DOC_MARKERS:
        if marker not in doc:
            fail(f"doc_marker_missing:{marker}")

    if "G4_CC1_FAIL" in summary:
        fail("summary_contains_failure")

    print(
        "G4_CC1_EVIDENCE_PASS contracts=3 acid_owners=3 "
        "techno_owners=1 funk_owners=1 i6_totals_unchanged=1"
    )


if __name__ == "__main__":
    main()
