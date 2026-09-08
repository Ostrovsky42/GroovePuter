#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
PROJECTION = ROOT / "src/generation/composition/phrase_harmonic_clock_projection.h"
HARMONIC = ROOT / "src/generation/roles/harmonic_rhythm.h"
TIMELINE = ROOT / "src/generation/composition/phrase_harmonic_timeline.h"
PROGRESSION = ROOT / "src/generation/roles/chord_progression.h"


def fail(message: str) -> None:
    print(f"H2R SOURCE GUARD FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


projection = PROJECTION.read_text(encoding="utf-8")
harmonic = HARMONIC.read_text(encoding="utf-8")
timeline = TIMELINE.read_text(encoding="utf-8")
progression = PROGRESSION.read_text(encoding="utf-8")
combined = "\n".join((projection, harmonic, timeline))

# H2R has accepted descendants. Byte identity with the original replay commit is
# no longer a live contract once GF2 deliberately extends harmonic WHEN policy.
# Preserve the actual compatibility boundary instead: historical two-argument
# callers must still select the accepted {0,8} moving-harmony clock, while any
# explicit musical policy must enter the same bounded one-bar owner.
required_projection = [
    "projectPhraseHarmonicClock(\n    uint8_t phraseBars,\n    ProgressionId progression,\n    HarmonicChangeRateId changeRate)",
    "projectPhraseHarmonicClock(\n    uint8_t phraseBars,\n    ProgressionId progression)",
    "HarmonicChangeRateId::Every2Beats",
    "realizeHarmonicRhythm(request)",
    "request.phraseBarOrdinal = bar",
    "request.phraseHarmonicPosition = nextPhraseOrdinal",
    "makePhraseHarmonicTimeline",
    "phraseHarmonicEventRangeForBar",
]
for token in required_projection:
    if token not in projection:
        fail(f"required live H2 projection contract missing: {token!r}")

required_harmonic = [
    "struct HarmonicRhythmRequest",
    "uint8_t harmonicEventCount = 0;",
    "defaultOneBarHarmonicEventCount",
    "return isStaticHarmonicProgression(id) ? 1 : 2;",
    "evenlySpacedHarmonicOnsets",
    "std::is_trivially_copyable<HarmonicRhythmRequest>",
    "std::is_trivially_copyable<HarmonicRhythmPlan>",
]
for token in required_harmonic:
    if token not in harmonic:
        fail(f"accepted F08 one-bar owner contract missing: {token!r}")

# H2 remains WHEN-only. WHAT, runtime, storage and transport ownership must not
# leak into the projection even though GF2 can now supply a musical change rate.
for token in (
    "ChordRhythm",
    "ChordProgressionSource",
    "realizeChordProgressionSource",
    "chordProgressionSourceEventAt",
    "chordProgressionEventAt",
    "realizeChordProgression(",
    "PatternPlayer",
    "RuntimeSynthEvent",
    "AudioMutationGate",
    "Midi",
    "MIDI",
    "Song",
    "patternAddress",
    "GenreSettings",
    "GenerativeMode",
    "FeelProfile",
):
    if token in projection:
        fail(f"foreign owner leaked into H2 projection: {token!r}")

for pattern in (
    r"\bmalloc\s*\(",
    r"\bcalloc\s*\(",
    r"\brealloc\s*\(",
    r"\bfree\s*\(",
    r"\bnew\s+[A-Za-z_:]",
    r"\bdelete\s+",
    r"std::vector",
    r"std::string",
):
    if re.search(pattern, combined):
        fail(f"dynamic allocation/container leaked into bounded H2 owner: {pattern}")

# H2 still consumes progression identity only. Its WHAT source remains owned by
# chord_progression, and the timeline remains a fixed-capacity WHEN carrier.
if "struct ChordProgressionSource" not in progression:
    fail("H1-F1 progression source owner missing")
if "kMaxPhraseHarmonicEventPositions =" not in timeline:
    fail("fixed-capacity phrase harmonic timeline missing")
if "std::is_trivially_copyable<PhraseHarmonicClockProjection>" not in projection:
    fail("H2 projection lost fixed-capacity value contract")

print("H2R source firewall: OK")
print("legacy two-argument moving clock -> Every2Beats: YES")
print("explicit musical rate reuses F08 one-bar WHEN owner: YES")
print("H1 WHAT / runtime / storage / transport ownership leakage: NO")
