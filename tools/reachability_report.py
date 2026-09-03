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


def trace_key(row: dict[str, object]) -> str:
    return "::".join(
        str(row[field]) for field in ("domain", "role", "semantic_field")
    )


def build_diff(
    baseline: dict[str, object], candidate: dict[str, object]
) -> dict[str, object]:
    old = {trace_key(row): row for row in baseline.get("traces", [])}
    new = {trace_key(row): row for row in candidate.get("traces", [])}
    if len(old) != len(baseline.get("traces", [])):
        raise RuntimeError("duplicate trace identity in reachability baseline")
    if len(new) != len(candidate.get("traces", [])):
        raise RuntimeError("duplicate trace identity in reachability candidate")
    changed = []
    for key in sorted(old.keys() & new.keys()):
        fields = sorted(set(old[key]) | set(new[key]))
        changed_fields = [
            field for field in fields if old[key].get(field) != new[key].get(field)
        ]
        if changed_fields:
            changed.append({"key": key, "changed_fields": changed_fields})
    added = sorted(new.keys() - old.keys())
    removed = sorted(old.keys() - new.keys())
    return {
        "schema_version": 1,
        "has_reachability_changes": bool(added or removed or changed),
        "traces": {"added": added, "removed": removed, "changed": changed},
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--diff-output", type=Path)
    parser.add_argument("--fail-on-change", action="store_true")
    args = parser.parse_args()

    if bool(args.baseline) != bool(args.diff_output):
        parser.error("--baseline and --diff-output must be provided together")

    report = build_report(read_rows(args.input))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(report, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(f"reachability report: {report['trace_count']} traces")
    if args.baseline:
        baseline = json.loads(args.baseline.read_text(encoding="utf-8"))
        diff = build_diff(baseline, report)
        args.diff_output.parent.mkdir(parents=True, exist_ok=True)
        args.diff_output.write_text(
            json.dumps(diff, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
        print(
            "reachability diff: changes="
            f"{str(diff['has_reachability_changes']).lower()}"
        )
        return int(args.fail_on_change and diff["has_reachability_changes"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
