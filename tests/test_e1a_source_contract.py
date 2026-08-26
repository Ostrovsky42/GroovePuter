#!/usr/bin/env python3
"""E1a source ownership contract: one rhythm mutation implementation."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def source(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def function_body(text: str, marker: str, next_marker: str) -> str:
    start = text.find(marker)
    end = text.find(next_marker, start + 1)
    require(start >= 0 and end > start, f"cannot isolate {marker}")
    return text[start:end]


def main() -> None:
    header = source("src/generation/rhythm/rhythm_realizer.h")
    realizer = source("src/generation/rhythm/rhythm_realizer.cpp")
    mutation = source("src/generation/rhythm/rhythm_realizer_evolution.cpp")
    bar = source("src/generation/rhythm/bar_evolution.cpp")
    phrase = source("src/generation/phrase/phrase_evolution.cpp")
    bridge = source("src/generation/migration/strong_rhythm_live_bridge.cpp")

    require(
        "bool applyRhythmBarFunctionMutation(" in header and
        "bool rhythmMutationPlanValid(" in header,
        "canonical mutation API is not owned by rhythm_realizer",
    )
    require(
        mutation.count("bool applyRhythmBarFunctionMutation(") == 1,
        "canonical BarFunction mutation implementation must be unique",
    )
    require(
        mutation.count("bool dropOneStructuralEvent(") == 1 and
        mutation.count("bool addGhostCue(") == 1 and
        "switch (function)" in mutation,
        "legacy BarFunction primitives were not moved under canonical owner",
    )
    require(
        "dropOneStructuralEvent" not in bar and
        "addGhostCue" not in bar and
        "switch (function)" not in bar,
        "bar_evolution retained a competing mutation implementation",
    )
    require(
        "return rhythmMutationPlanValid(archetype, plan);" in bar and
        "applyRhythmBarFunctionMutation(" in bar,
        "bar_evolution compatibility surface does not delegate",
    )

    variation = function_body(
        realizer,
        "void addVariation(const RhythmArchetype& archetype",
        "bool requestValid(",
    )
    require(
        "secondaryAdds" in variation and "ghostAdds" in variation and
        "addVariationPass(archetype, seed" in variation,
        "characterized production secondary/ghost variation path changed",
    )
    require(
        "dropOneStructuralEvent" not in variation and
        "maxDisplacements" not in variation and
        "maxAccentChanges" not in variation,
        "production addVariation vocabulary was silently expanded",
    )

    require(
        "const uint8_t segmentBars = request.phraseBars == 8 ? 4" in phrase,
        "PhraseEvolution lost 8-bar 4+4 segmentation",
    )
    require(
        "secondGeneration.phraseOrdinal = static_cast<uint16_t>(" in phrase and
        "secondGeneration.phraseOrdinal + 1u" in phrase and
        "request, 4, secondGeneration, &first.identity" in phrase,
        "PhraseEvolution lost N->N+1 identity-reuse contract",
    )
    require(
        "next.roleIdentity = request.roleIdentity;" in phrase and
        "copySegment(first.plan, 0, next);" in phrase and
        "copySegment(second.plan, 4, next);" in phrase,
        "PhraseEvolution lost role continuity or segment placement",
    )
    require(
        "applyRhythmBarFunctionMutation" not in phrase and
        "dropOneStructuralEvent" not in phrase and
        "addGhostCue" not in phrase,
        "PhraseEvolution became a second mutation executor",
    )

    caller_start = bridge.find(
        "PhraseAuditionResult regeneratePhraseAuditionWithProbe("
    )
    require(caller_start >= 0, "real MiniAcid phrase-audition caller missing")
    caller = bridge[caller_start:]
    require(
        "PhraseEvolutionRequest request{}" in caller and
        "evolveMultiBarPhrase(request)" in caller and
        "materializeEvolvedDrumBar" in caller,
        "live bridge no longer executes PhraseEvolution then materialization",
    )

    print("E1A one evolution mutation owner source contract: OK")


if __name__ == "__main__":
    main()
