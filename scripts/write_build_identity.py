#!/usr/bin/env python3
"""Write deterministic compile-time identity for one staged firmware build."""

import argparse
import json
from pathlib import Path
import re


VERSION_RE = re.compile(
    r"\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?(?:\+[A-Za-z0-9.-]+)?\Z"
)
SHA_RE = re.compile(r"(?:[0-9a-fA-F]{40}|[0-9a-fA-F]{64})\Z")


def build_header(version: str, source_sha: str, source_state: str) -> str:
    if not VERSION_RE.fullmatch(version):
        raise ValueError("version must be semantic MAJOR.MINOR.PATCH with optional suffix")
    if not SHA_RE.fullmatch(source_sha):
        raise ValueError("source SHA must be a full 40- or 64-digit hex SHA")
    if source_state not in {"clean", "dirty"}:
        raise ValueError("source state must be 'clean' or 'dirty'")

    dirty = "true" if source_state == "dirty" else "false"
    return (
        "#pragma once\n\n"
        "namespace GroovePuterBuild {\n"
        f"inline constexpr char kVersion[] = {json.dumps(version)};\n"
        f"inline constexpr char kSourceCommit[] = {json.dumps(source_sha.lower())};\n"
        f"inline constexpr bool kSourceDirty = {dirty};\n"
        "}  // namespace GroovePuterBuild\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--source-state", required=True, choices=("clean", "dirty"))
    args = parser.parse_args()

    try:
        header = build_header(args.version, args.source_sha, args.source_state)
    except ValueError as error:
        parser.error(str(error))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(header, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
