import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/tonal/tonal_projector.h").read_text()
SOURCE = (ROOT / "src/generation/tonal/tonal_projector.cpp").read_text()
SCENES = (ROOT / "scenes.h").read_text()
TEXT = HEADER + "\n" + SOURCE


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//.*", "", text)


CODE = strip_comments(TEXT)
SCENES_CODE = strip_comments(SCENES)

# Pin the exact existing global ScaleType ABI without importing the heavyweight
# Scene header into the projector C++ API and without defining a second enum.
match = re.search(r"enum\s+ScaleType\s*\{([^}]*)\};", SCENES_CODE, re.DOTALL)
assert match is not None, "ScaleType enum must remain discoverable"
enum_body = match.group(1)
# The lightweight ABI constants below depend on the existing implicit 0..9
# values. An explicit numeric assignment is therefore ABI drift and must fail.
assert "=" not in enum_body, "ScaleType numeric assignments changed"
entries = [entry.strip() for entry in enum_body.split(",") if entry.strip()]
assert entries == [
    "MINOR",
    "MAJOR",
    "DORIAN",
    "PHRYGIAN",
    "LYDIAN",
    "MIXOLYDIAN",
    "LOCRIAN",
    "PENTATONIC_MJ",
    "PENTATONIC_MN",
    "CHROMATIC",
], entries
assert "ScaleType scale = DORIAN" in SCENES_CODE

assert '#include "scenes.h"' not in HEADER
assert "enum ScaleType" not in HEADER
assert "enum class ScaleType" not in HEADER
assert "using ScaleTypeValue = uint8_t" in HEADER
assert "kDefaultScaleTypeValue = 2u" in HEADER
assert "ScaleTypeValue scaleTypeValue = kDefaultScaleTypeValue" in HEADER
assert "sizeof(TonalProjectionRequest) == 24" in HEADER

for expected in (
    "constexpr ScaleTypeValue kScaleMinor = 0;",
    "constexpr ScaleTypeValue kScaleMajor = 1;",
    "constexpr ScaleTypeValue kScaleDorian = 2;",
    "constexpr ScaleTypeValue kScalePhrygian = 3;",
    "constexpr ScaleTypeValue kScaleLydian = 4;",
    "constexpr ScaleTypeValue kScaleMixolydian = 5;",
    "constexpr ScaleTypeValue kScaleLocrian = 6;",
    "constexpr ScaleTypeValue kScalePentatonicMajor = 7;",
    "constexpr ScaleTypeValue kScalePentatonicMinor = 8;",
    "constexpr ScaleTypeValue kScaleChromatic = 9;",
):
    assert expected in SOURCE, expected

# Shared transient materialization adapter only. It consumes the compact
# ScaleType ABI value but must not become a Genre/policy/rhythm/voice/persistence
# owner.
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
assert "case kScalePentatonicMajor: return {kMajorPentatonic, 5};" in SOURCE
assert "case kScalePentatonicMinor: return {kMinorPentatonic, 5};" in SOURCE
assert "case kScaleChromatic: return {kChromatic, 12};" in SOURCE
assert "scale % 7" not in CODE
assert "% 7" not in CODE
assert "AdvancedPatternGenerator" not in CODE
assert "quantizeToScale" not in CODE

# Tagged semitone intent and scale-degree intent are converted to displacement
# first. Root selection then considers the complete bar and chooses the root
# nearest register center among anchors that can materialize every note.
assert "resolveDisplacements(" in SOURCE
assert "isSemitoneOrdinal(request, ordinal)" in SOURCE
assert "degreeToSemitone(request.tonalOffsets[ordinal], scale)" in SOURCE
assert "anchorFitsAllNotes(" in SOURCE
assert "findFeasibleRootAnchor(" in SOURCE
assert "rootPitchClassPresent" in SOURCE
assert "request.maxAdjacentLeapSemitones" in SOURCE

# Projection result is atomic: register/leap validation operates on local root
# and local notes. Result fields are published only after the full loop succeeds.
projector = SOURCE[SOURCE.index("TonalProjectionResult projectTonalIntent") :]
loop_pos = projector.index("for (uint8_t ordinal = 0;")
root_publish_pos = projector.index("result.rootAnchorMidi = rootAnchor;")
note_count_pos = projector.index("result.noteCount = request.onsetCount;")
midi_publish_pos = projector.index("result.midiNotes[ordinal] = midiNotes[ordinal];")
ok_pos = projector.index("result.status = TonalProjectionStatus::Ok;")
assert root_publish_pos > loop_pos
assert note_count_pos > root_publish_pos
assert midi_publish_pos > note_count_pos
assert ok_pos > midi_publish_pos
assert projector.count("result.noteCount =") == 1
assert projector.count("result.rootAnchorMidi =") == 1

# No retry loop or hidden register octave-folding.
assert "while (" not in CODE
assert "+= 12" not in CODE
assert "-= 12" not in CODE

print("Tonal Projector source regressions: OK")
