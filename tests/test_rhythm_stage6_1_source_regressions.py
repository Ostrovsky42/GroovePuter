#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/generation/rhythm/bar_evolution.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require(
        "!validateRhythmCatalog(*request.catalog)" not in SOURCE,
        "BarEvolution must not perform a duplicate full catalog scan",
    )
    require(
        "Full catalog\n  // validation is intentionally delegated to realizeRhythmPhrase()" in SOURCE,
        "Stage 6.1 must document single-owner catalog validation",
    )
    require(
        "Secondary events are a strict lower-authority removal class" in SOURCE,
        "secondary-before-structural drop precedence is not documented",
    )
    require(
        "Response is metadata-only" in SOURCE,
        "Response v1 metadata-only contract is missing",
    )
    require(
        "Keeping those two salt\n  // spaces disjoint" in SOURCE and
        "TrajectoryId is uint8_t today" in SOURCE,
        "BarEvolution salt-domain invariant is not documented",
    )

    print("Groove Vocabulary Stage 6.1 source regressions: OK")


if __name__ == "__main__":
    main()
