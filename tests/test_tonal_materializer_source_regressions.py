import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/tonal/tonal_materializer.h").read_text()
SOURCE = (ROOT / "src/generation/tonal/tonal_materializer.cpp").read_text()
CATALOG = (ROOT / "src/generation/tonal/scale_catalog.h").read_text()
TEXT = HEADER + "\n" + SOURCE


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//.*", "", text)


CODE = strip_comments(TEXT)

for forbidden in (
    "Scene",
    "Song",
    "PhraseCore",
    "SynthPattern",
    "GenerativeMode",
    "GenreSettings",
    "new ",
    "malloc(",
    "rand(",
    "std::vector",
    "std::string",
    "float",
    "double",
):
    assert forbidden not in CODE, forbidden

assert "TonalMaterializationRequest" in HEADER
assert "TonalMaterializationPlan" in HEADER
assert "std::is_trivially_copyable<TonalMaterializationRequest>" in HEADER
assert "std::is_trivially_copyable<TonalMaterializationPlan>" in HEADER
assert "sizeof(TonalMaterializationRequest) <= 64" in HEADER
assert "sizeof(TonalMaterializationPlan) <= 40" in HEADER
assert "harmonicEventOnsets" in HEADER
assert "ChordProgressionPlan progression" in HEADER
assert "semitoneOffsetOrdinals" in HEADER
assert "scaleTypeValue" in HEADER
assert "minMidi" in HEADER and "maxMidi" in HEADER

# ChordRhythm remains timing owner: materializer only consumes the event-onset
# mask and may not introduce a rhythm selector/realizer.
assert "realizeChordRhythm" not in CODE
assert "realizeBassRhythm" not in CODE
assert "realizeMelodicMotif" not in CODE
assert "projectLegacyPitchPattern" not in CODE

# All scale-degree conversion comes from the one shared catalog primitive.
assert "scaleDegreeToSemitone(" in SOURCE
assert "scaleDefinitionFor(" not in SOURCE
assert "kScaleIntervals" not in SOURCE
assert "kScaleIntervalsMinor" in CATALOG

# Materializer converts final combined displacements to tagged semitone intent,
# then delegates register/root-anchor selection to Tonal Projector.
assert "projection.semitoneOffsetOrdinals = ordinalMask(onsetCount)" in SOURCE
assert "projectTonalIntent(projectionRequest)" in SOURCE
assert "event.rootOffsetSemitones" in SOURCE
assert "harmonicEventIndexForStep(" in SOURCE
assert "popcount(request.harmonicEventOnsets) != request.progression.eventCount" in SOURCE

# No unbounded retry or hidden octave folding.
assert "while (" not in CODE
assert "+= 12" not in CODE
assert "-= 12" not in CODE

print("Stage 15 Tonal Materializer source regressions: OK")
