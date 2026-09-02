#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

from extract_atlas_pass2_hardening import extract_hardening

HARDENED_OUTPUTS = (
    "ATLAS_PASS2_CALIBRATION_DISTRIBUTIONS.csv",
    "ATLAS_PASS2_HARDENED_CANDIDATES.csv",
    "ATLAS_PASS2_HARDENING_SUMMARY.json",
    "ATLAS_PASS2_ROLE_MAPPING_AUDIT.csv",
)


def normalize(output_dir: Path) -> None:
    for filename in HARDENED_OUTPUTS:
        path = output_dir / filename
        if not path.is_file():
            raise FileNotFoundError(path)
        text = path.read_text(encoding="utf-8")
        path.write_text(
            text.replace("\r\n", "\n").replace("\r", "\n"),
            encoding="utf-8",
            newline="\n",
        )


def write_manifest(output_dir: Path) -> Path:
    path = output_dir / "ATLAS_PASS2_HARDENING_OUTPUT_HASHES.sha256"
    lines = []
    for filename in HARDENED_OUTPUTS:
        digest = hashlib.sha256((output_dir / filename).read_bytes()).hexdigest()
        lines.append(f"{digest}  {filename}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    return path


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run base Atlas Pass 2 plus adversarial-audit hardening passes."
    )
    parser.add_argument("atlas_zip", type=Path)
    parser.add_argument("runtime_baseline_tsv", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument(
        "--runtime-topology",
        type=Path,
        default=None,
        help="Full V2 runtime topology dump. Defaults to sibling RUNTIME_RHYTHM_TOPOLOGY_V2.tsv.",
    )
    args = parser.parse_args()

    base_runner = Path(__file__).with_name("run_atlas_pass2.py")
    subprocess.run(
        [sys.executable, str(base_runner), str(args.atlas_zip), str(args.runtime_baseline_tsv), str(args.output_dir)],
        check=True,
    )

    runtime_topology = args.runtime_topology or (
        args.runtime_baseline_tsv.parent / "RUNTIME_RHYTHM_TOPOLOGY_V2.tsv"
    )
    summary = extract_hardening(args.atlas_zip, runtime_topology, args.output_dir)
    normalize(args.output_dir)
    manifest = write_manifest(args.output_dir)
    print(json.dumps({
        "hardening_summary": summary,
        "hardening_hash_manifest": str(manifest),
    }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
