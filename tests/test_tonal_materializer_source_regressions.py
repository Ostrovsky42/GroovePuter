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

# Materializer groups role onsets by harmonic event, resolves each event root in
# the global scale, converts all event-local role intent to exact semitone
# displacement, then delegates only register/root-anchor selection to projector.
assert "eventSemitoneFromGlobalRoot(" in SOURCE
assert "targetSemitoneFromGlobalRoot(" in SOURCE
assert "harmonicEventIndexForStep(" in SOURCE
assert "projection.rootPitchClass = normalizePitchClass(" in SOURCE
assert "projection.scaleTypeValue = request.scaleTypeValue" in SOURCE
assert "projection.semitoneOffsetOrdinals = ordinalMask(localOrdinal)" in SOURCE
assert "for (uint8_t eventIndex = 0; eventIndex < eventCount; ++eventIndex)" in SOURCE
assert "projectTonalIntent(projectionRequest)" in SOURCE
assert "event.rootOffsetSemitones" in SOURCE
assert "popcount(request.harmonicEventOnsets) != request.progression.eventCount" in SOURCE

# No unbounded retry, hidden octave folding or phrase-global root anchor.
# `normalizePitchClass()` may add 12 solely to normalize a negative modulo; the
# emitted/relative pitch paths themselves may never octave-fold to force fit.
assert "while (" not in CODE
for pitch_name in (
    "relativeSemitone",
    "targetSemitone",
    "projection.tonalOffsets",
    "result.plan.midiNotes",
):
    assert f"{pitch_name} += 12" not in CODE
    assert f"{pitch_name} -= 12" not in CODE
assert "projection.rootPitchClass = request.rootPitchClass;" not in SOURCE

print("Stage 15 Tonal Materializer source regressions: OK")
