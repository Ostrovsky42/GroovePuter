#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/roles/chord_progression.h").read_text(encoding="utf-8")
SOURCE = (ROOT / "src/generation/roles/chord_progression.cpp").read_text(encoding="utf-8")
PROFILE_H = (ROOT / "src/generation/composition/generation_profile.h").read_text(encoding="utf-8")
PROFILE_CPP = (ROOT / "src/generation/composition/generation_profile.cpp").read_text(encoding="utf-8")
BRIDGE = (ROOT / "src/generation/migration/strong_rhythm_migration.cpp").read_text(encoding="utf-8")
RUNNER = (ROOT / "tests/run_generation_stage15_tests.sh").read_text(encoding="utf-8")
MODULE = HEADER + "\n" + SOURCE
MATERIAL_BRIDGE = BRIDGE[
    BRIDGE.index("StrongRhythmMigrationResult migrateStrongRhythmMaterial(") :
]


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        raise AssertionError(message)


def forbid(text: str, needle: str, message: str) -> None:
    if needle in text:
        raise AssertionError(message)


for forbidden in (
    "Scene",
    "Song",
    "PhraseCore",
    "new ",
    "malloc(",
    "rand(",
    "std::vector",
    "std::string",
    "float",
    "double",
):
    forbid(MODULE, forbidden, f"ChordProgression acquired forbidden token: {forbidden}")

for forbidden in (
    "ScaleType",
    "quantizeToScale",
    "intervals[",
    "midiNotes",
    "MidiNote",
    "tonal_projector",
    "src/generation/tonal/",
    "GenerativeMode",
    "StepMask",
    "ChordRhythmPlan",
    "sourceAdvanceOnsets",
    "sameChordRetriggers",
):
    forbid(MODULE, forbidden, f"ChordProgression crossed tonal/rhythm ownership: {forbidden}")

require(
    SOURCE,
    "GenerationDomain::ChordPitch",
    "ChordProgression stopped using the existing ChordPitch RNG domain",
)
require(
    SOURCE,
    "static_cast<uint8_t>(id)",
    "ChordProgression grammar salt is no longer the append-only progression id",
)
require(
    SOURCE,
    "id == ProgressionId::ParallelShift ||\n         id == ProgressionId::BorrowedLift",
    "chromatic root-offset allowlist changed",
)
require(
    HEADER,
    "constexpr int8_t kMaxRootOffsetSemitones = 2;",
    "root offset bound changed without an explicit contract update",
)
require(
    PROFILE_H,
    "ProgressionId progression = ProgressionId::Auto;",
    "composition result lost its progression identity",
)
require(
    PROFILE_CPP,
    "WeightedIdentityView progression;",
    "profile data stopped owning progression selection",
)
require(
    PROFILE_CPP,
    "GenerationDomain::ChordPitch, rhythm.archetypeId, static_cast<uint8_t>(ProgressionId::Auto)",
    "profile progression selection stopped using the stable Auto salt inside ChordPitch",
)
forbid(
    PROFILE_CPP,
    "GenerationDomain::ChordPitch, rhythm.archetypeId, baseSalt | chord",
    "profile progression selection became coupled to chord-rhythm salt",
)
for palette in (
    "kProgressionStatic",
    "kProgressionPop",
    "kProgressionDark",
    "kProgressionBroken",
    "kProgressionDub",
    "kProgressionTrip",
    "kProgressionHipHop",
    "kProgressionFunk",
    "kProgressionLoFi",
    "kProgressionChip",
):
    require(PROFILE_CPP, palette, f"editorial progression palette disappeared: {palette}")

require(
    MATERIAL_BRIDGE,
    "const ChordProgressionResult progression =\n      realizeChordProgression(progressionRequest);",
    "Stage 15 is no longer production-reachable from the strong migration bridge",
)

# Harmonic timing/count ownership remains outside ChordProgression. The bridge
# defaults to the historical one-bar onset clock, while the explicit tonal
# P2/P3 planner may replace those external request values with its bounded N
# clock. ChordProgression itself still receives only count + phraseBars.
require(
    MATERIAL_BRIDGE,
    "uint8_t harmonicEventCount = onsetCount(chord.plan.onsets);",
    "legacy harmonic event count is no longer externally derived",
)
require(
    MATERIAL_BRIDGE,
    "uint8_t progressionPhraseBars = 1;",
    "legacy one-bar progression default disappeared",
)
require(
    MATERIAL_BRIDGE,
    "harmonicEventCount = harmonic.currentBar.sourceAdvanceCount;",
    "P3 source-advance count is no longer supplied by the external planner",
)
require(
    MATERIAL_BRIDGE,
    "progressionPhraseBars = harmonic.boundedPhraseBars;",
    "P2 bounded phrase length is no longer supplied by the external planner",
)
require(
    MATERIAL_BRIDGE,
    "progressionRequest.harmonicEventCount = harmonicEventCount;",
    "ChordProgression request stopped consuming the externally-owned count",
)
require(
    MATERIAL_BRIDGE,
    "progressionRequest.phraseBars = progressionPhraseBars;",
    "ChordProgression request stopped consuming the externally-owned phrase bound",
)
require(
    BRIDGE,
    "request.phraseBars = 1;",
    "Stage 12 rhythm realization stopped being a one-bar physical pattern surface",
)

# ChordProgression must be realized before pitch-path arbitration. The final
# Stage 15 bridge has two downstream owners: the frozen legacy projector when
# tonal materialization is disabled, and TonalMaterializer when it is enabled.
progression_call = MATERIAL_BRIDGE.index("realizeChordProgression(progressionRequest)")
pitch_path_branch = MATERIAL_BRIDGE.index("if (!context.tonalMaterializationEnabled)")
legacy_projection = MATERIAL_BRIDGE.index(
    "result.bassProjectionStatus = projectLegacyPitchPattern("
)
tonal_projection = MATERIAL_BRIDGE.index(
    "const TonalMaterializationResult bassTonal = materializeRole("
)
if not (
    progression_call < pitch_path_branch
    and progression_call < legacy_projection
    and progression_call < tonal_projection
):
    raise AssertionError("ChordProgression moved after production pitch materialization")

for changed_layer in (PROFILE_H, PROFILE_CPP, BRIDGE):
    forbid(
        changed_layer,
        "src/generation/tonal/",
        "Stage 15 gained a forbidden direct Tonal Projector include",
    )

require(RUNNER, "-Wvla", "Stage 15 runner lost VLA rejection")
require(RUNNER, "-fstack-usage", "Stage 15 runner lost stack-usage measurement")
require(RUNNER, "test_generation_stage15_chord_progression", "Stage 15 executable left its runner")

print("Generation Stage 15 chord-progression source regressions: OK")
