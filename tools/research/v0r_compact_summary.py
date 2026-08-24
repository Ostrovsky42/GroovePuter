#!/usr/bin/env python3
"""Create the committed compact V0R JSON summary from deterministic artifacts."""

import argparse
import csv
import hashlib
import json
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw", required=True, type=Path)
    parser.add_argument("--csv", required=True, type=Path)
    parser.add_argument("--full-json", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    with args.csv.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.reader(handle))
    if len(rows) != 19:
        raise SystemExit(f"expected header + 18 graph rows, found {len(rows)}")

    full = json.loads(args.full_json.read_text(encoding="utf-8"))
    detailed = dict(full["detailed_artifacts"])
    detailed["full_metrics_json"] = {
        "bytes": args.full_json.stat().st_size,
        "sha256": sha256(args.full_json),
    }

    payload = {
        "schema": "0.9.9-V0R",
        "authority": "E2c + E2a + E2b canonical-relative legality",
        "previous_v0_numeric_baseline":
            "NOT AVAILABLE — BRANCH CONTAINED NO TOOLING DELTA",
        "graph_count": 18,
        "raw_dataset_sha256": sha256(args.raw),
        "detailed_artifacts": detailed,
        "columns": rows[0],
        "graphs": rows[1:],
    }
    args.output.write_text(
        json.dumps(
            payload,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
        ) + "\n",
        encoding="utf-8",
    )
    print(f"V0R_COMPACT_JSON_SHA256 {sha256(args.output)}")


if __name__ == "__main__":
    main()
