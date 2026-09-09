#!/usr/bin/env python3
"""Build a deterministic G4-C0R corpus from the production-backed dump seam.

This tool owns no generation semantics. It only enumerates explicit research
coordinates and concatenates the single-row TSV emitted by --g4-c0r-dump.
"""

from __future__ import annotations

import argparse
import csv
import subprocess
from pathlib import Path

DEPTHS = ("P1", "P2", "P3")
MAX_IDENTITY = 0xFFFE  # 0xFFFF is kUnspecifiedPhraseGenerationIdentity.


def positive_int(text: str) -> int:
    value = int(text, 0)
    if value <= 0:
        raise argparse.ArgumentTypeError("value must be positive")
    return value


def non_negative_int(text: str) -> int:
    value = int(text, 0)
    if value < 0:
        raise argparse.ArgumentTypeError("value must be non-negative")
    return value


def parse_profiles(text: str) -> list[int]:
    if not text:
        raise argparse.ArgumentTypeError("profiles must not be empty")
    values: list[int] = []
    for token in text.split(","):
        try:
            value = int(token, 0)
        except ValueError as exc:
            raise argparse.ArgumentTypeError(f"invalid profile ordinal: {token}") from exc
        if value < 0:
            raise argparse.ArgumentTypeError("profile ordinals must be non-negative")
        values.append(value)
    if len(values) != len(set(values)):
        raise argparse.ArgumentTypeError("profile ordinals must be unique")
    return values


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dump-binary", required=True, type=Path)
    parser.add_argument("--profiles", required=True, type=parse_profiles)
    parser.add_argument("--identity-start", required=True, type=non_negative_int)
    parser.add_argument("--identity-count", required=True, type=positive_int)
    parser.add_argument("--attempt-count", required=True, type=positive_int)
    parser.add_argument("--pattern-address", required=True, type=non_negative_int)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    identity_end = args.identity_start + args.identity_count - 1
    if identity_end > MAX_IDENTITY:
        parser.error(
            f"identity range ends at {identity_end}, but 0xFFFF is reserved as unspecified"
        )
    if not args.dump_binary.is_file():
        parser.error(f"dump binary does not exist: {args.dump_binary}")
    return args


def observe(
    binary: Path,
    profile: int,
    identity: int,
    attempt: int,
    pattern_address: int,
    depth: str,
) -> tuple[list[str], dict[str, str]]:
    completed = subprocess.run(
        [
            str(binary),
            "--g4-c0r-dump",
            str(profile),
            str(identity),
            str(attempt),
            str(pattern_address),
            depth,
        ],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    reader = csv.DictReader(completed.stdout.splitlines(), delimiter="\t")
    rows = list(reader)
    if reader.fieldnames is None or len(rows) != 1:
        raise RuntimeError(
            "--g4-c0r-dump must emit exactly one TSV header and one data row; "
            f"got fieldnames={reader.fieldnames!r} rows={len(rows)}"
        )
    return list(reader.fieldnames), rows[0]


def main() -> int:
    args = parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)

    fieldnames: list[str] | None = None
    rows: list[dict[str, str]] = []
    identity_stop = args.identity_start + args.identity_count

    for profile in args.profiles:
        for identity in range(args.identity_start, identity_stop):
            for attempt in range(args.attempt_count):
                for depth in DEPTHS:
                    current_fields, row = observe(
                        args.dump_binary,
                        profile,
                        identity,
                        attempt,
                        args.pattern_address,
                        depth,
                    )
                    if fieldnames is None:
                        fieldnames = current_fields
                    elif current_fields != fieldnames:
                        raise RuntimeError(
                            "--g4-c0r-dump schema changed inside one corpus run"
                        )
                    rows.append(row)

    if fieldnames is None:
        raise RuntimeError("corpus coordinate set is empty")

    with args.output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=fieldnames,
            delimiter="\t",
            lineterminator="\n",
        )
        writer.writeheader()
        writer.writerows(rows)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
