#!/usr/bin/env python3
"""Resolve a reproducible firmware version from a release ref and source SHA."""

import argparse
import re


SHA_RE = re.compile(r"(?:[0-9a-fA-F]{40}|[0-9a-fA-F]{64})\Z")
TAG_RE = re.compile(r"v(\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?)\Z")
RELEASE_BRANCH_RE = re.compile(r"release/(\d+\.\d+\.\d+)\Z")


def resolve_version(ref_name: str, ref_type: str, source_sha: str) -> str:
    if not SHA_RE.fullmatch(source_sha):
        raise ValueError("source SHA must be a full 40- or 64-digit hex SHA")

    if ref_type == "tag":
        match = TAG_RE.fullmatch(ref_name)
        if match:
            return match.group(1)

    if ref_type == "branch":
        match = RELEASE_BRANCH_RE.fullmatch(ref_name)
        if match:
            return f"{match.group(1)}-rc+{source_sha[:7].lower()}"

    return f"0.0.0-dev+{source_sha[:7].lower()}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ref-name", required=True)
    parser.add_argument("--ref-type", required=True, choices=("tag", "branch"))
    parser.add_argument("--source-sha", required=True)
    args = parser.parse_args()

    try:
        version = resolve_version(args.ref_name, args.ref_type, args.source_sha)
    except ValueError as error:
        parser.error(str(error))
    print(f"GROOVEPUTER_BUILD_VERSION={version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
