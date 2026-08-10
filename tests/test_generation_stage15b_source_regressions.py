from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/roles/melodic_pitch_intent.h").read_text()
SOURCE = (ROOT / "src/generation/roles/melodic_pitch_intent.cpp").read_text()
TEXT = HEADER + "\n" + SOURCE

# 15B is a fixed-capacity one-bar melodic-intent layer. It must not become a
# persistence, phrase, full-groove diversity, voice-allocation, or heap owner.
for forbidden in (
    "Scene",
    "PhraseCore",
    "StrongRhythmMigration",
    "recentHistory",
    "fingerprintHistory",
    "std::vector",
    "std::map",
    "std::unordered",
    "new ",
    "delete ",
    "rand(",
    "random_device",
):
    assert forbidden not in TEXT, forbidden

assert "StepMask onsets" in HEADER
assert "StepMask continuations" in HEADER
assert "int8_t degreeOffsets[kStepsPerBar]" in HEADER
assert "GenerationDomain::LeadPitch" in SOURCE
assert "GenerationDomain::MotifSelection" in SOURCE
assert "kContourSalt" in SOURCE
assert "kOperationSalt" in SOURCE
assert "result.plan.onsets = request.rhythmPlan.onsets" in SOURCE
assert "result.plan.continuations = request.rhythmPlan.continuations" in SOURCE
assert "request.maxOnsets > kStepsPerBar" in SOURCE
assert "while (" not in SOURCE

print("Generation Stage 15B source regressions: OK")
