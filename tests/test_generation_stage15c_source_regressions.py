from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/roles/bass_pitch_behavior.h").read_text()
SOURCE = (ROOT / "src/generation/roles/bass_pitch_behavior.cpp").read_text()
TEXT = HEADER + "\n" + SOURCE

# 15C assigns pitch/articulation to an existing bass rhythm. It must not own
# timing, Scene state, Phrase state, Synth B material, or heap-backed retries.
for forbidden in (
    "Scene",
    "PhraseCore",
    "StrongRhythmMigration",
    "MelodicMotif",
    "SynthPattern",
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
assert "StepMask accentOnsets" in HEADER
assert "StepMask slideIntoOnsets" in HEADER
assert "int8_t degreeOffsets[kStepsPerBar]" in HEADER
assert "GenerationDomain::BassPitch" in SOURCE
assert "kBassContourSalt" in SOURCE
assert "kBassArticulationSalt" in SOURCE
assert "result.plan.onsets = request.rhythmPlan.onsets" in SOURCE
assert "result.plan.continuations = request.rhythmPlan.continuations" in SOURCE
assert "result.plan.accentOnsets & result.plan.onsets" in SOURCE
assert "result.plan.slideIntoOnsets & result.plan.onsets" in SOURCE
assert "while (" not in SOURCE

print("Generation Stage 15C source regressions: OK")
