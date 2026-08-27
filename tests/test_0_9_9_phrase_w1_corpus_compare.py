#!/usr/bin/env python3
import csv
import hashlib
import sys
from pathlib import Path

EXPECTED_F08_SHA = "bbc1544bf289c7ef7f062997bde3f0b8dae3a317ace54b0998cef6649872ac3f"
COLUMNS = ("topology", "pitch", "articulation", "full")


def read_rows(path: Path):
    with path.open(newline="") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    assert len(rows) == 256, f"{path}: expected 256 rows, got {len(rows)}"
    return rows


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    digest.update(path.read_bytes())
    return digest.hexdigest()


def compare(left, right):
    assert len(left) == len(right)
    counts = {column: 0 for column in COLUMNS}
    changed_rows = 0
    for index, (a, b) in enumerate(zip(left, right)):
        for key in ("mode", "ordinal", "voice", "status", "secondary_role"):
            assert a[key] == b[key], f"row {index + 1}: corpus key {key} changed {a[key]} -> {b[key]}"
        row_changed = False
        for column in COLUMNS:
            if a[column] != b[column]:
                counts[column] += 1
                row_changed = True
        changed_rows += int(row_changed)
    return changed_rows, counts


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit(
            "usage: corpus_compare.py F08_GOLDEN F08_ACTUAL H1_ACTUAL W1_ACTUAL"
        )

    f08_golden, f08_actual, h1_actual, w1_actual = map(Path, sys.argv[1:])
    golden_rows = read_rows(f08_golden)
    f08_rows = read_rows(f08_actual)
    h1_rows = read_rows(h1_actual)
    w1_rows = read_rows(w1_actual)

    actual_sha = sha256(f08_actual)
    assert actual_sha == EXPECTED_F08_SHA, (
        f"exact F08 replay SHA mismatch: {actual_sha} != {EXPECTED_F08_SHA}"
    )
    f08_changed, f08_counts = compare(golden_rows, f08_rows)
    assert f08_changed == 0, "exact F08 source no longer reproduces accepted F08 golden"

    h1_changed, h1_counts = compare(h1_rows, w1_rows)
    assert h1_counts["topology"] == 0, "W1 changed physical topology"
    assert h1_counts["articulation"] == 0, "W1 changed physical articulation"
    assert h1_counts["pitch"] == h1_changed, "W1 compatibility delta is not pitch-only"

    print("EXACT REPLAYABLE F08 CORPUS rows=256 byte_equivalent=YES")
    print(f"EXACT F08 generated SHA-256={actual_sha}")
    print("EXACT F08 accepted golden delta rows=0")
    print(
        "CURRENT H1 COMPATIBILITY CORPUS "
        f"changed={h1_changed} topology={h1_counts['topology']} "
        f"articulation={h1_counts['articulation']} pitch={h1_counts['pitch']} "
        f"full={h1_counts['full']}"
    )
    print("CURRENT H1 compatibility causal class=PITCH_ONLY")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
