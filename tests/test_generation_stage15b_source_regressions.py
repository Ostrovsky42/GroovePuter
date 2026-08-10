import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/roles/melodic_pitch_intent.h").read_text()
SOURCE = (ROOT / "src/generation/roles/melodic_pitch_intent.cpp").read_text()
TEXT = HEADER + "\n" + SOURCE


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//.*", "", text)


CODE = strip_comments(TEXT)

# 15B is a fixed-capacity one-bar melodic-intent layer. It may transform the
# current bar's melodic topology inside explicit semantic legality masks, then
# assign contour and a local motif operation. It must not become a persistence,
# Phrase, full-groove diversity, physical voice-allocation, or heap owner.
for forbidden in (
    "Scene",
    "PhraseCore",
    "StrongRhythmMigration",
    "recentHistory",
    "fingerprintHistory",
    "previousBar",
    "nextBar",
    "SynthPattern",
    "std::vector",
    "std::map",
    "std::unordered",
    "rand(",
    "random_device",
):
    assert forbidden not in CODE, forbidden

assert re.search(r"\bnew\s+[A-Za-z_:]", CODE) is None, "heap new"
assert re.search(r"\bdelete\s+[A-Za-z_]", CODE) is None, "heap delete"

assert "enum class MelodicRhythmOperationId" in HEADER
assert "ControlledRest" in HEADER
assert "ShiftInteriorEarlier" in HEADER
assert "ShiftInteriorLater" in HEADER
assert "TerminalEcho" in HEADER
assert "StepMask allowedOnsetSteps = kAllSteps" in HEADER
assert "StepMask allowedContinuationSteps = kAllSteps" in HEADER
assert "bool allowEmptyBar = false" in HEADER
assert "StepMask onsets" in HEADER
assert "StepMask continuations" in HEADER
assert "int8_t degreeOffsets[kStepsPerBar]" in HEADER

assert "GenerationDomain::MelodicRhythmSelection" in SOURCE
assert "GenerationDomain::LeadPitch" in SOURCE
assert "GenerationDomain::MotifSelection" in SOURCE
assert "kRhythmSalt" in SOURCE
assert "kContourSalt" in SOURCE
assert "kOperationSalt" in SOURCE
assert "applyRhythmOperation(" in SOURCE
assert "request.allowedOnsetSteps" in SOURCE
assert "request.allowedContinuationSteps" in SOURCE
assert "request.maxOnsets" in SOURCE
assert "validContinuationTopology(onsets, continuations)" in SOURCE
assert "MelodicRhythmOperationId::Preserve" in SOURCE
assert "while (" not in CODE

print("Generation Stage 15B source regressions: OK")
