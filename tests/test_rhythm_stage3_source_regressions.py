from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/generation/rhythm/reference_vocabulary.h"
SOURCE = ROOT / "src/generation/rhythm/reference_vocabulary.cpp"
STRONG_MIGRATION = ROOT / "src/generation/migration/strong_rhythm_migration.cpp"
RHYTHM_SELECTION = ROOT / "src/generation/composition/rhythm_selection.cpp"

header = HEADER.read_text()
source = SOURCE.read_text()
combined = header + "\n" + source
strong_migration = STRONG_MIGRATION.read_text()
rhythm_selection = RHYTHM_SELECTION.read_text()

required = [
    "enum class Archetype",
    "const RhythmCatalogView& catalog()",
    "static_assert(sizeof(kArchetypes) / sizeof(kArchetypes[0]) == 24",
    '"straight_drive"',
    '"rolling_acid"',
    '"classic_2step"',
    '"two_step_roll"',
    '"sparse_skank"',
    '"machine_syncopation"',
    '"stacked_quarters"',
    '"electro_backskip"',
    '"funk_house_bridge"',
    '"electro_gap_push"',
    "archetype(711",
    "archetype(712",
    "archetype(713",
    "archetype(714",
    '{Archetype::StackedQuarters, 711, "stacked_quarters", RhythmFamily::FourFloor, 122, 122}',
    '{Archetype::ElectroBackskip, 712, "electro_backskip", RhythmFamily::MachineSyncopation, 116, 116}',
    '{Archetype::FunkHouseBridge, 713, "funk_house_bridge", RhythmFamily::Funk16, 112, 112}',
    '{Archetype::ElectroGapPush, 714, "electro_gap_push", RhythmFamily::HipHopBackbeat, 114, 114}',
    "RhythmFamily::FourFloor",
    "RhythmFamily::MachineSyncopation",
    "RhythmFamily::Breakbeat",
    "RhythmFamily::UkTwoStep",
    "RhythmFamily::DubPulse",
    "RhythmFamily::Funk16",
    "RhythmFamily::HipHopBackbeat",
    "RhythmFamily::SparsePulse",
]

for token in required:
    assert token in combined, f"missing reference vocabulary contract token: {token}"

for forbidden in [
    "archetype(421",
    "archetype(422",
    "archetype(423",
    "archetype(424",
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
    "Stage7AAudition",
    "stage7a_catalog",
]:
    assert forbidden not in combined, f"reference vocabulary ownership/heap leak: {forbidden}"

# Generation Stage 7C makes the four curated identities reachable only through
# the data-only compatibility layer, never by adding Genre ownership to the
# vocabulary catalog or realizer.
for token in [
    "Archetype::StackedQuarters",
    "Archetype::ElectroBackskip",
    "Archetype::FunkHouseBridge",
    "Archetype::ElectroGapPush",
]:
    assert token in rhythm_selection, f"Stage 7 identity is not production reachable: {token}"
    assert token not in strong_migration, f"Stage 7 identity bypassed composition routing: {token}"

# Acid remains a genre/bass/articulation interpretation over generic rhythm
# families. It must not be introduced as a RhythmFamily in the reference pack.
assert "RhythmFamily::Acid" not in combined

# Stage 7 Batch 2 promotes only the four already-auditioned one-bar structural
# grammars. The audition IDs are intentionally retained because archetype.id is
# part of the deterministic RhythmIdentity seed domain. Bar evolution and any
# temporary audition routing stay outside the production ReferenceVocabulary.
assert "phraseBarsBit(1)" in source
assert "BarFunction::Repeat" not in source
assert "BarFunction::Break" not in source
assert "BarFunction::Turnaround" not in source
assert "IDs 711..714 are retained" in source

print("Groove Vocabulary reference vocabulary source regressions: OK")
