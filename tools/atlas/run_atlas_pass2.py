#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path

from extract_atlas_pass2 import extract
from extract_atlas_pass2_negative_space import extract_negative_space


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run all Atlas Pass 2 computational extraction passes."
    )
    parser.add_argument("atlas_zip", type=Path)
    parser.add_argument("runtime_tsv", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()

    summary = extract(args.atlas_zip, args.runtime_tsv, args.output_dir)
    negative_space = extract_negative_space(
        args.atlas_zip,
        args.output_dir / "ATLAS_PASS2_NEGATIVE_SPACE.csv",
    )
    print(json.dumps({
        "summary": summary,
        "negative_space_rows": len(negative_space),
    }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
