import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/roles/bass_pitch_behavior.h").read_text()
SOURCE = (ROOT / "src/generation/roles/bass_pitch_behavior.cpp").read_text()
TEXT = HEADER + "\n" + SOURCE


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//.*", "", text)


CODE = strip_comments(TEXT)

for forbidden in (
    "Scene",
    "PhraseCore",
    "StrongRhythmMigration",
    "MelodicMotif",
    "SynthPattern",
    "request.family",
    "RhythmFamily family",
    "switch (request.family)",
    "degreeOffsets",
    "std::vector",
    "std::map",
    "std::unordered",
    "rand(",
    "random_device",
):
    assert forbidden not in CODE, forbidden

assert re.search(r"\bnew\s+[A-Za-z_:]", CODE) is None, "heap new"
assert re.search(r"\bdelete\s+[A-Za-z_]", CODE) is None, "heap delete"

assert "struct BassBehaviorPolicy" in HEADER
assert "allowedContours =\n      bassPitchContourBit(BassPitchContourId::RootAnchor)" in HEADER
assert "allowedArticulations =\n      bassArticulationStyleBit(BassArticulationStyleId::Plain)" in HEADER
assert "preferredContours = 0" in HEADER
assert "preferredArticulations = 0" in HEADER
assert "BassBehaviorPolicy policy{}" in HEADER

assert "int8_t tonalOffsets[kStepsPerBar]" in HEADER
assert "uint16_t semitoneOffsetOrdinals = 0" in HEADER
assert "int8_t minDegreeOffset = -7" in HEADER
assert "uint8_t maxLeapDegrees = 7" in HEADER

assert "preferredOrAllowed(" in SOURCE
assert "validPolicy(" in SOURCE
assert "explicitSelectionsAllowed(" in SOURCE
assert "request.policy.allowedContours" in SOURCE
assert "request.policy.allowedArticulations" in SOURCE
assert "GenerationDomain::BassPitch" in SOURCE
assert "kBassContourSalt" in SOURCE
assert "kBassArticulationSalt" in SOURCE

# Named fifth/octave relations are chromatic semitone intent and are tagged by
# onset ordinal. Generic neighbor/approach vocabulary remains degree intent.
assert "values[index] = 7;" in SOURCE
assert "values[index] = 12;" in SOURCE
assert "markSemitoneOrdinal(semitoneOrdinals, index)" in SOURCE
assert "BassPitchContourId::NeighborReturn" in SOURCE
assert "BassPitchContourId::StepApproach" in SOURCE
assert "sameTonalUnit(" in SOURCE
assert "isSemitoneOrdinal(" in SOURCE
assert "enforceDegreeBounds(" in SOURCE

assert "result.plan.onsets = request.rhythmPlan.onsets" in SOURCE
assert "result.plan.continuations = request.rhythmPlan.continuations" in SOURCE
assert "result.plan.accentOnsets & result.plan.onsets" in SOURCE
assert "result.plan.slideIntoOnsets & result.plan.onsets" in SOURCE
assert "isLegatoConnected(pitchPlan, ordinal)" in SOURCE
assert "pitchPlan.continuations & stepBit(step)" in SOURCE
assert "while (" not in CODE

print("Generation Stage 15C source regressions: OK")
