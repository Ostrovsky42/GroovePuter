#!/usr/bin/env python3
"""Normalize the frozen GF2 reachability table into a JSON report."""

from __future__ import annotations

import argparse
import csv
import json
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT = ROOT / "docs/research/GF2_C1RF_FINAL_SEMANTIC_REACHABILITY.tsv"


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    required = {
        "domain",
        "role",
        "semantic_field",
        "authoritative_owner",
        "terminal_effect",
        "status",
        "blocker",
        "failure_mode",
        "fallback",
    }
    if not rows:
        raise RuntimeError(f"reachability table is empty: {path}")
    missing = required - set(rows[0])
    if missing:
        raise RuntimeError(f"reachability table lacks columns: {sorted(missing)}")
    return rows


def build_report(rows: list[dict[str, str]]) -> dict[str, object]:
    status_counts = Counter(row["status"] for row in rows)
    domain_statuses: dict[str, Counter[str]] = defaultdict(Counter)
    role_statuses: dict[str, Counter[str]] = defaultdict(Counter)
    blockers = Counter(row["blocker"] for row in rows if row["blocker"] != "NONE")
    for row in rows:
        domain_statuses[row["domain"]][row["status"]] += 1
        role_statuses[row["role"]][row["status"]] += 1

    return {
        "schema_version": 1,
        "trace_count": len(rows),
        "status_counts": dict(sorted(status_counts.items())),
        "domain_status_counts": {
            key: dict(sorted(value.items()))
            for key, value in sorted(domain_statuses.items())
        },
        "role_status_counts": {
            key: dict(sorted(value.items()))
            for key, value in sorted(role_statuses.items())
        },
        "blocker_counts": dict(sorted(blockers.items())),
        "traces": sorted(
            (
                {
                    key: row[key]
                    for key in (
                        "domain",
                        "role",
                        "semantic_field",
                        "authoritative_owner",
                        "terminal_effect",
                        "status",
                        "blocker",
                        "failure_mode",
                        "fallback",
                    )
                }
                for row in rows
            ),
            key=lambda row: (row["domain"], row["role"], row["semantic_field"]),
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    report = build_report(read_rows(args.input))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(report, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(f"reachability report: {report['trace_count']} traces")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
