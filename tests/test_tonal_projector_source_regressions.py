import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/tonal/tonal_projector.h").read_text()
SOURCE = (ROOT / "src/generation/tonal/tonal_projector.cpp").read_text()
TEXT = HEADER + "\n" + SOURCE


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//.*", "", text)


CODE = strip_comments(TEXT)

# Shared transient materialization adapter only. It may consume ScaleType but
# must not become a Genre/policy/rhythm/voice/persistence owner.
for forbidden in (
    "GenreSettings",
    "GenreManager",
    "RhythmFamily",
    "RhythmSelectionMode",
    "PhraseCore",
    "SceneManager",
    "Persisted",
    "SynthPattern",
    "VoiceId",
    "std::vector",
    "std::map",
    "std::unordered",
    "rand(",
    "random_device",
    "new ",
    "delete ",
):
    assert forbidden not in CODE, forbidden

assert "ScaleType scale = DORIAN" in HEADER
assert "int8_t tonalOffsets[kStepsPerBar]" in HEADER
assert "uint16_t semitoneOffsetOrdinals = 0" in HEADER
assert "uint8_t rootPitchClass = 0" in HEADER
assert "uint8_t minMidi = 36" in HEADER
assert "uint8_t maxMidi = 84" in HEADER
assert "uint8_t maxAdjacentLeapSemitones = 127" in HEADER
assert "TonalProjectionStatus::RootOutOfRegister" in SOURCE
assert "TonalProjectionStatus::NoteOutOfRegister" in SOURCE
assert "TonalProjectionStatus::LeapExceeded" in SOURCE

# Real scale cardinality must remain 5 / 7 / 12; do not reintroduce modulo-7
# handling for pentatonic/chromatic modes.
assert "case PENTATONIC_MJ: return {kMajorPentatonic, 5};" in SOURCE
assert "case PENTATONIC_MN: return {kMinorPentatonic, 5};" in SOURCE
assert "case CHROMATIC: return {kChromatic, 12};" in SOURCE
assert "scale % 7" not in CODE
assert "% 7" not in CODE
assert "AdvancedPatternGenerator" not in CODE
assert "quantizeToScale" not in CODE

# Tagged semitone intent and scale-degree intent are resolved before the common
# MIDI leap check.
assert "isSemitoneOrdinal(request, ordinal)" in SOURCE
assert "degreeToSemitone(request.tonalOffsets[ordinal], scale)" in SOURCE
assert "request.maxAdjacentLeapSemitones" in SOURCE

# No retry loop or hidden register octave-folding.
assert "while (" not in CODE
assert "+= 12" not in CODE
assert "-= 12" not in CODE

print("Tonal Projector source regressions: OK")
