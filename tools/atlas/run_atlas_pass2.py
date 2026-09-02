#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from extract_atlas_pass2 import extract
from extract_atlas_pass2_negative_space import extract_negative_space
from extract_atlas_pass2_phrase import extract_phrase

GENERATED_OUTPUTS = (
    "ATLAS_PASS2_DISTANCE_DISTRIBUTIONS.csv",
    "ATLAS_PASS2_EFFECTIVE_VARIATION_BASELINE.csv",
    "ATLAS_PASS2_EVIDENCE_COVERAGE.csv",
    "ATLAS_PASS2_NEGATIVE_SPACE.csv",
    "ATLAS_PASS2_PHRASE_TRANSITIONS.csv",
    "ATLAS_PASS2_PITCH_CONTOURS.csv",
    "ATLAS_PASS2_RELATIONSHIPS.csv",
    "ATLAS_PASS2_SUMMARY.json",
    "ATLAS_PASS2_TOPOLOGY_CANDIDATES.csv",
)


def normalize_generated_text(output_dir: Path) -> None:
    """Canonicalize generated text artifacts to UTF-8 + LF before hashing."""
    for filename in GENERATED_OUTPUTS:
        path = output_dir / filename
        if not path.is_file():
            raise FileNotFoundError(path)
        text = path.read_text(encoding="utf-8")
        path.write_text(
            text.replace("\r\n", "\n").replace("\r", "\n"),
            encoding="utf-8",
            newline="\n",
        )


def write_hash_manifest(output_dir: Path) -> Path:
    manifest = output_dir / "ATLAS_PASS2_OUTPUT_HASHES.sha256"
    lines = []
    for filename in GENERATED_OUTPUTS:
        path = output_dir / filename
        if not path.is_file():
            raise FileNotFoundError(path)
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        lines.append(f"{digest}  {filename}")
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run all Atlas Pass 2 computational extraction passes."
    )
    parser.add_argument("atlas_zip", type=Path)
    parser.add_argument("runtime_tsv", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()

    summary = extract(args.atlas_zip, args.runtime_tsv, args.output_dir)
    phrase_counts = extract_phrase(
        args.atlas_zip,
        args.output_dir / "ATLAS_PASS2_PHRASE_TRANSITIONS.csv",
        args.output_dir / "ATLAS_PASS2_SUMMARY.json",
    )
    negative_space = extract_negative_space(
        args.atlas_zip,
        args.output_dir / "ATLAS_PASS2_NEGATIVE_SPACE.csv",
    )
    normalize_generated_text(args.output_dir)
    manifest = write_hash_manifest(args.output_dir)
    print(json.dumps({
        "summary": summary,
        "phrase_transition_counts": phrase_counts,
        "negative_space_rows": len(negative_space),
        "hash_manifest": str(manifest),
    }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
