from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/generation/rhythm/reference_vocabulary.h"
SOURCE = ROOT / "src/generation/rhythm/reference_vocabulary.cpp"

header = HEADER.read_text()
source = SOURCE.read_text()
combined = header + "\n" + source

required = [
    "enum class Archetype",
    "const RhythmCatalogView& catalog()",
    "static_assert(sizeof(kArchetypes) / sizeof(kArchetypes[0]) == 20",
    '"straight_drive"',
    '"rolling_acid"',
    '"classic_2step"',
    '"two_step_roll"',
    '"sparse_skank"',
    '"machine_syncopation"',
    "RhythmFamily::FourFloor",
    "RhythmFamily::MachineSyncopation",
    "RhythmFamily::Breakbeat",
    "RhythmFamily::UkTwoStep",
    "RhythmFamily::DubPulse",
    "RhythmFamily::SparsePulse",
]

for token in required:
    assert token in combined, f"missing Stage 3 contract token: {token}"

for forbidden in [
    "GenreManager",
    "GenreSettings",
    "SceneManager",
    "PatternPlayer",
    "MiniAcid",
    "SynthA",
    "SynthB",
    "applyGenreTimbre",
    "Arduino.h",
    "M5Cardputer",
    "std::vector",
    "std::string",
    "new ",
    "malloc(",
]:
    assert forbidden not in combined, f"Stage 3 ownership/heap leak: {forbidden}"

# Acid remains a genre/bass/articulation interpretation over generic rhythm
# families. It must not be introduced as a RhythmFamily in the reference pack.
assert "RhythmFamily::Acid" not in combined

# Stage 3 is a reference catalog only. Bar evolution stays in Stage 6.
assert "phraseBarsBit(1)" in source
assert "BarFunction::Repeat" not in source
assert "BarFunction::Break" not in source
assert "BarFunction::Turnaround" not in source

print("Groove Vocabulary Stage 3 source regressions: OK")
