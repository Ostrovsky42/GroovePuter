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

for forbidden in (
    "Scene",
    "PhraseCore",
    "StrongRhythmMigration",
    "recentHistory",
    "fingerprintHistory",
    "previousBar",
    "nextBar",
    "SynthPattern",
    "RhythmFamily family",
    "switch (request.family)",
    "std::vector",
    "std::map",
    "std::unordered",
    "rand(",
    "random_device",
):
    assert forbidden not in CODE, forbidden

assert re.search(r"\bnew\s+[A-Za-z_:]", CODE) is None, "heap new"
assert re.search(r"\bdelete\s+[A-Za-z_]", CODE) is None, "heap delete"

assert "struct MelodicIntentPolicy" in HEADER
assert "allowedRhythmOperations =\n      melodicRhythmOperationBit(MelodicRhythmOperationId::Preserve)" in HEADER
assert "allowedContours = melodicContourBit(MelodicContourId::Static)" in HEADER
assert "allowedMotifOperations =\n      melodicMotifOperationBit(MelodicMotifOperationId::None)" in HEADER
assert "preferredRhythmOperations = 0" in HEADER
assert "preferredContours = 0" in HEADER
assert "preferredMotifOperations = 0" in HEADER
assert "MelodicIntentPolicy policy{}" in HEADER
assert "StepMask allowedOnsetSteps = kAllSteps" in HEADER
assert "StepMask allowedContinuationSteps = kAllSteps" in HEADER
assert "int8_t degreeOffsets[kStepsPerBar]" in HEADER

assert "GenerationDomain::MelodicRhythmSelection" in SOURCE
assert "GenerationDomain::LeadPitch" in SOURCE
assert "GenerationDomain::MotifSelection" in SOURCE
assert "preferredOrAllowed(" in SOURCE
assert "validPolicy(" in SOURCE
assert "explicitSelectionsAllowed(" in SOURCE
assert "applyRhythmOperation(" in SOURCE
assert "request.allowedOnsetSteps" in SOURCE
assert "request.allowedContinuationSteps" in SOURCE
assert "validContinuationTopology(onsets, continuations)" in SOURCE

# TerminalEcho is semantically an echo: every candidate is strictly after the
# terminal onset. A left-side fallback must never reappear.
assert "constexpr uint8_t offsets[] = {1, 2, 3};" in SOURCE
assert "target <= terminal" in SOURCE
assert "-1" not in SOURCE[SOURCE.index("bool applyTerminalEcho"):SOURCE.index("MelodicRhythmOperationId applyRhythmOperation")]
assert "while (" not in CODE

print("Generation Stage 15B source regressions: OK")
