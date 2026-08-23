#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLANNER = (ROOT / "src/generation/rhythm/bar_evolution.cpp").read_text(encoding="utf-8")
MUTATION = (ROOT / "src/generation/rhythm/rhythm_realizer_evolution.cpp").read_text(
    encoding="utf-8"
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require(
        "!validateRhythmCatalog(*request.catalog)" not in PLANNER,
        "BarEvolution must not perform a duplicate full catalog scan",
    )
    require(
        "Stage 2 owns full catalog/archetype/phrase-length validation" in PLANNER,
        "Stage 6.1 must document Stage 2 catalog-validation ownership",
    )

    realization_index = PLANNER.find(
        "const RhythmRealizationResult base = realizeRhythmPhrase(baseRequest);"
    )
    lookup_index = PLANNER.find(
        "const RhythmArchetype* archetype =\n      archetypeFor(*request.catalog, request.archetypeId);"
    )
    require(realization_index >= 0 and lookup_index >= 0,
            "Stage 6.1 validation/lookup markers are missing")
    require(realization_index < lookup_index,
            "BarEvolution must not dereference catalog arrays before Stage 2 validation")

    require(
        "Secondary events are a strict lower-authority removal class" in MUTATION,
        "secondary-before-structural drop precedence is not documented by canonical mutation owner",
    )
    require(
        "Response is metadata-only" in MUTATION,
        "Response v1 metadata-only contract is missing from canonical mutation owner",
    )
    require(
        "Keeping those two salt\n  // spaces disjoint" in PLANNER and
        "TrajectoryId is uint8_t today" in PLANNER,
        "BarEvolution salt-domain invariant is not documented",
    )
    require(
        "dropOneStructuralEvent" not in PLANNER and
        "addGhostCue" not in PLANNER and
        "switch (function)" not in PLANNER,
        "BarEvolution retained a second mutation executor",
    )
    require(
        "applyRhythmBarFunctionMutation(" in PLANNER,
        "BarEvolution does not delegate to canonical rhythm mutation owner",
    )

    print("Groove Vocabulary Stage 6.1 source regressions: OK")


if __name__ == "__main__":
    main()
