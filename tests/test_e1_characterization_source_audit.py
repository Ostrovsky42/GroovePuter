#!/usr/bin/env python3
"""Static characterization guard for the first E1 checkpoint.

This deliberately records source facts rather than proposing ownership changes.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def source(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")

realizer = source("src/generation/rhythm/rhythm_realizer.cpp")
bar = source("src/generation/rhythm/bar_evolution.cpp")
phrase = source("src/generation/phrase/phrase_evolution.cpp")
bridge = source("src/generation/migration/strong_rhythm_live_bridge.cpp")

assert "void addVariation(const RhythmArchetype& archetype" in realizer
variation = realizer[realizer.index("void addVariation("):realizer.index("bool requestValid(")]
assert "addVariationPass(archetype, seed" in variation
assert "secondaryAdds" in variation and "ghostAdds" in variation
assert "dropOneStructuralEvent" not in variation

assert "BarEvolutionResult evolveRhythmPhrase(const BarEvolutionRequest& request)" in bar
assert "selectTrajectory" in bar and "dropOneStructuralEvent" in bar
assert "const uint8_t segmentBars = request.phraseBars == 8 ? 4" in phrase
assert "secondGeneration.phraseOrdinal = static_cast<uint16_t>(" in phrase
assert "secondGeneration.phraseOrdinal + 1u" in phrase

caller = bridge[bridge.index("PhraseAuditionResult regeneratePhraseAuditionWithProbe") :]
assert "PhraseEvolutionRequest request{}" in caller
assert "phrase = evolveMultiBarPhrase(request)" in caller
assert "materializeEvolvedDrumBar" in caller
assert "for (uint8_t bar = 0; bar < result.requestedBars; ++bar)" in caller

print("E1 source characterization audit: OK")
