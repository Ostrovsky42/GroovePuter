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
        "Stage 2 owns full catalog/archetype/phrase-length validation" in SOURCE,
        "Stage 6.1 must document Stage 2 catalog-validation ownership",
    )

    realization_index = SOURCE.find(
        "const RhythmRealizationResult base = realizeRhythmPhrase(baseRequest);"
    )
    lookup_index = SOURCE.find(
        "const RhythmArchetype* archetype =\n      archetypeFor(*request.catalog, request.archetypeId);"
    )
    require(realization_index >= 0 and lookup_index >= 0,
            "Stage 6.1 validation/lookup markers are missing")
    require(realization_index < lookup_index,
            "BarEvolution must not dereference catalog arrays before Stage 2 validation")

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
