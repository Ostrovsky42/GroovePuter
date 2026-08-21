#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PHRASE = (ROOT / "src/dsp/phrase_generator.h").read_text(encoding="utf-8")
MATERIALIZER = (ROOT / "src/dsp/song_pattern_materializer.h").read_text(encoding="utf-8")

required_phrase = [
    '#include "src/dsp/song_pattern_materializer.h"',
    "SongPatternMaterializer::globalPatternIsReferenced(",
    "SongPatternMaterializer::editableTrackForIndex(",
]
for needle in required_phrase:
    if needle not in PHRASE:
        raise SystemExit(f"D1 missing canonical liveness delegation: {needle}")

if "for (int songSlot = 0; songSlot < 2; ++songSlot)" in PHRASE:
    raise SystemExit("D1 regression: PhraseGenerator reintroduced a private Song-only liveness scan")

required_owner = [
    "phraseSlotHasPatternReferences",
    "phrasePatternReferenceCount",
    "globalPatternReferenceCount",
    "return references + phrasePatternReferenceCount(scene, track, globalPattern);",
]
for needle in required_owner:
    if needle not in MATERIALIZER:
        raise SystemExit(f"D1 canonical liveness owner drifted: {needle}")

for forbidden in ("std::vector", "std::deque", "new ", "malloc("):
    if forbidden in PHRASE:
        raise SystemExit(f"D1 added unbounded/dynamic liveness state: {forbidden}")

print("0.9.9-D1 source ownership: PASS")
