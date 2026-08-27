#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/generation/roles/chord_progression.h").read_text(encoding="utf-8")
SOURCE = (ROOT / "src/generation/roles/chord_progression.cpp").read_text(encoding="utf-8")
HARMONIC = (ROOT / "src/generation/roles/harmonic_rhythm.h").read_text(encoding="utf-8")
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
):
    forbid(MODULE, forbidden, f"ChordProgression crossed tonal/genre ownership: {forbidden}")

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
    "id == ProgressionId::ParallelShift ||\n"
    "         id == ProgressionId::BorrowedLift",
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

# Recovered exact accepted F08 ownership. P1R wraps the frozen one-bar path in
# the explicit nullptr override branch so prepared phrase execution can supply
# H2 WHEN/WHAT. The legacy branch must still realize HarmonicRhythm before
# ChordProgression and remain independent of ChordRhythm physical articulation.
require(
    MATERIAL_BRIDGE,
    "if (context.phraseExecutionOverride == nullptr) {",
    "P1R legacy nullptr override branch disappeared from the strong migration bridge",
)
require(
    MATERIAL_BRIDGE,
    "harmonic = realizeHarmonicRhythm(harmonicRequest);",
    "F08 harmonic rhythm is no longer production-reachable in the legacy nullptr branch",
)
require(
    MATERIAL_BRIDGE,
    "harmonicRequest.progression = result.progressionId;",
    "HarmonicRhythm stopped consuming the selected progression identity",
)
forbid(
    MATERIAL_BRIDGE,
    "progressionRequest.harmonicEventCount = onsetCount(chord.plan.onsets);",
    "Stage 15 restored the forbidden ChordRhythm -> progression-count coupling",
)
require(
    MATERIAL_BRIDGE,
    "progressionRequest.harmonicEventCount = harmonic.plan.eventCount;",
    "ChordProgression stopped consuming HarmonicRhythm-owned cardinality",
)
require(
    MATERIAL_BRIDGE,
    "progression = realizeChordProgression(progressionRequest);",
    "Stage 15 is no longer production-reachable from the legacy nullptr branch",
)
require(
    MATERIAL_BRIDGE,
    "progression.plan.eventCount != harmonic.plan.eventCount",
    "production bridge stopped validating HarmonicRhythm/ChordProgression cardinality agreement",
)
require(
    MATERIAL_BRIDGE,
    "harmonic.plan.onsets, bassPitch.plan.onsets",
    "bass TonalMaterializer path stopped using HarmonicRhythm as its harmonic clock",
)
require(
    MATERIAL_BRIDGE,
    "harmonic.plan.onsets, chord.plan.onsets, chord.plan.continuations",
    "chord TonalMaterializer path stopped separating harmonic clock from chord articulation",
)
require(
    MATERIAL_BRIDGE,
    "harmonic.plan.onsets, melodicPitch.plan.onsets",
    "melodic TonalMaterializer path stopped using HarmonicRhythm as its harmonic clock",
)
require(
    MATERIAL_BRIDGE,
    "harmonic.plan.onsets, admittedOnsets, admittedContinuations",
    "hybrid TonalMaterializer path stopped using HarmonicRhythm as its harmonic clock",
)
require(
    MATERIAL_BRIDGE,
    "progressionRequest.phraseBars = 1;",
    "Stage 15 crossed the one-bar production hardware guard",
)
require(
    BRIDGE,
    "request.phraseBars = 1;",
    "Stage 12 one-bar production guard was modified by Stage 15",
)

for forbidden in (
    "ChordRhythmId",
    "ChordRhythmPlan",
    "chordOnsets",
):
    forbid(
        HARMONIC,
        forbidden,
        f"HarmonicRhythm acquired forbidden ChordRhythm input: {forbidden}",
    )
require(
    HARMONIC,
    "StepMask onsets = 0;",
    "HarmonicRhythm plan lost its explicit harmonic timing mask",
)
require(
    HARMONIC,
    "uint8_t eventCount = 0;",
    "HarmonicRhythm plan lost ownership of harmonic event cardinality",
)

# Preserve the existing Stage15 ordering while allowing the P1R override path
# after the frozen legacy F08/H1 realization. This is not phrase-wide policy.
legacy_branch = MATERIAL_BRIDGE.index("if (context.phraseExecutionOverride == nullptr) {")
harmonic_call = MATERIAL_BRIDGE.index("harmonic = realizeHarmonicRhythm(harmonicRequest)")
progression_call = MATERIAL_BRIDGE.index("progression = realizeChordProgression(progressionRequest)")
override_branch = MATERIAL_BRIDGE.index(
    "const StrongRhythmPhraseExecutionOverride& execution ="
)
pitch_path_branch = MATERIAL_BRIDGE.index("if (!context.tonalMaterializationEnabled)")
legacy_projection = MATERIAL_BRIDGE.index(
    "result.bassProjectionStatus = projectLegacyPitchPattern("
)
tonal_projection = MATERIAL_BRIDGE.index(
    "const TonalMaterializationResult bassTonal = materializeRole("
)
if not (
    legacy_branch < harmonic_call < progression_call < override_branch < pitch_path_branch
    and progression_call < legacy_projection
    and progression_call < tonal_projection
):
    raise AssertionError(
        "legacy HarmonicRhythm/ChordProgression ordering moved outside the P1R nullptr branch or after production pitch materialization"
    )

for changed_layer in (PROFILE_H, PROFILE_CPP, BRIDGE):
    forbid(
        changed_layer,
        "src/generation/tonal/",
        "Stage 15 gained a forbidden direct Tonal Projector include",
    )

require(RUNNER, "-Wvla", "Stage 15 runner lost VLA rejection")
require(RUNNER, "-fstack-usage", "Stage 15 runner lost stack-usage measurement")
require(RUNNER, "test_generation_stage15_chord_progression", "Stage 15 executable left its runner")

print("Generation Stage 15 chord-progression source regressions: OK (F08 ownership recovered)")
