#!/usr/bin/env python3

import csv
import sys
from pathlib import Path

IDENTITY_COLUMNS = (
    "mode",
    "ordinal",
    "voice",
    "status",
    "secondary_role",
    "topology",
    "pitch",
)
EXPECTED_COLUMNS = IDENTITY_COLUMNS + ("articulation", "full")
EXPECTED_ROWS = 256


def load_rows(path: Path):
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if tuple(reader.fieldnames or ()) != EXPECTED_COLUMNS:
            raise AssertionError(
                f"{path}: unexpected columns {reader.fieldnames!r}; "
                f"expected {EXPECTED_COLUMNS!r}"
            )
        rows = list(reader)
    if len(rows) != EXPECTED_ROWS:
        raise AssertionError(
            f"{path}: expected {EXPECTED_ROWS} data rows, got {len(rows)}"
        )
    return rows


def main() -> int:
    if len(sys.argv) != 3:
        print(
            "usage: test_stage15_tonal_f13_corpus.py OLD_BASELINE NEW_ACTUAL",
            file=sys.stderr,
        )
        return 2

    old_path = Path(sys.argv[1])
    actual_path = Path(sys.argv[2])
    old_rows = load_rows(old_path)
    actual_rows = load_rows(actual_path)

    articulation_changes = 0
    full_changes = 0
    for index, (old, actual) in enumerate(zip(old_rows, actual_rows), start=1):
        for column in IDENTITY_COLUMNS:
            if old[column] != actual[column]:
                raise AssertionError(
                    f"row {index} column {column}: F-13 moved frozen tonal "
                    f"identity {old[column]!r} -> {actual[column]!r}"
                )
        articulation_changes += old["articulation"] != actual["articulation"]
        full_changes += old["full"] != actual["full"]

    if articulation_changes == 0:
        raise AssertionError(
            "F-13 produced no articulation fingerprint changes; expected removal "
            "of inherited destination dynamics to be observable"
        )
    if full_changes == 0:
        raise AssertionError(
            "F-13 produced no full fingerprint changes; expected self-contained "
            "tonal steps to replace inherited destination state"
        )

    print(
        "F-13 tonal corpus: identity/topology/pitch frozen; "
        f"articulation_changed={articulation_changes}/{EXPECTED_ROWS}; "
        f"full_changed={full_changes}/{EXPECTED_ROWS}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
