#!/usr/bin/env python3
"""E2b ownership/source regression checks."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def source(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    header = source("src/generation/rhythm/rhythm_canonical_diff.h")
    impl = source("src/generation/rhythm/rhythm_canonical_diff.cpp")
    realizer = source("src/generation/rhythm/rhythm_realizer.h")
    bar_evolution = source("src/generation/rhythm/bar_evolution.cpp")

    for symbol in (
        "CanonicalRhythmDiffStats",
        "canonicalRhythmBarDiff",
        "canonicalRhythmBudgetValid",
        "canonicalRhythmCandidateValid",
    ):
        require(symbol in header, f"missing E2b API: {symbol}")

    require("MutationPolicy" in header and "MutationBudget" in impl,
            "E2b stopped consuming the existing mutation policy")
    require("struct CanonicalRhythmBudget" not in header + impl,
            "E2b introduced a second budget hierarchy")
    require("rhythmMutationDisplacementGrammarLegal(" in impl,
            "E2b duplicated or bypassed E2c displacement grammar")
    require("rhythmMutationDeltaLess(" in impl,
            "E2b does not use E2c canonical delta ordering")
    require(impl.count("rhythmMutationPlanValid(") == 2,
            "E2b must delegate canonical/candidate music legality to realizer")
    require("applyRhythmBarFunctionMutation(" not in impl,
            "E2b became a mutation producer/executor")
    require("deterministicValue(" not in impl and "random" not in impl.lower(),
            "E2b must be a pure deterministic diff, not a candidate chooser")

    forbidden_state = (
        "previousVariant",
        "previous_variant",
        "mutationHistory",
        "candidateCache",
        "candidate_cache",
        "generationAttemptOrdinal",
        "transportState",
        "SongPage",
        "PhraseEvolutionRequest",
    )
    for token in forbidden_state:
        require(token not in header + impl,
                f"E2b acquired forbidden lifecycle/transport state: {token}")

    require("canonical" in header and "candidate" in header,
            "E2b API lost explicit canonical-relative inputs")
    require("HiddenPhraseChange" in header + impl,
            "E2b can hide mutations outside the diffed bar")

    # E1a owner remains unique and BarEvolution remains a delegating planner.
    require("bool applyRhythmBarFunctionMutation(" in realizer,
            "rhythm_realizer lost canonical mutation ownership")
    require("applyRhythmBarFunctionMutation(" in bar_evolution,
            "BarEvolution no longer delegates to realizer")
    require("canonicalRhythmBarDiff" not in bar_evolution,
            "BarEvolution incorrectly acquired canonical diff ownership")

    print("E2B canonical-relative source contract: OK")


if __name__ == "__main__":
    main()
