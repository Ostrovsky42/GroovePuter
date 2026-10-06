#!/usr/bin/env python3
"""D0-E repository evidence/source firewall.

D0-E is research-only. These checks pin the repository facts used by the
provenance-survival decision and prevent the checkpoint from silently becoming
a production provenance implementation.
"""

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]

def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

WORKING = read("src/state/working_material_storage.h")
SLOT = read("src/state/material_slot.h")
VERSION = read("src/state/material_version.h")
EXECUTION = read("src/generation/migration/phrase_execution.h")
P1R = read("src/dsp/generated_phrase_p1r_materializer.h")
PRODUCT = read("src/state/generated_phrase_product_state.h")
RUNTIME_EDIT = read("src/phrase/runtime_phrase_edit.h")
RUNTIME_PROJECTION = read("src/phrase/runtime_synth_events.h")
PATTERN_EDIT = read("src/state/synth_pattern_edit.h")
BASS = read("src/generation/roles/bass_rhythm.h")
HARMONIC = read("src/generation/roles/harmonic_rhythm.h")
PROGRESSION = read("src/generation/roles/chord_progression.h")
STRONG = read("src/generation/migration/strong_rhythm_migration.h")
IDENTITY = read("src/state/material_identity.h")
SEM = read("src/dsp/development_semantics.h")

failures: list[str] = []

def require(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)

# Existing Material owner remains compact and does not become a second semantic
# payload owner during D0-E.
require("struct MaterialSlotDescriptor" in SLOT and
        "MaterialKind kind" in SLOT and "MaterialId id" in SLOT,
        "D0-E evidence: MaterialSlotDescriptor contract changed")
require("static_assert(sizeof(WorkingMaterialStorage) <=" in WORKING and
        "sizeof(WorkingMaterialStorage::MelodyBuffer)" in WORKING,
        "D0-E evidence: WorkingMaterialStorage no longer bounded by Melody payload")
require("SemanticProvenance" not in SLOT and
        "DevelopmentBasis" not in SLOT,
        "D0-E ERROR: production provenance was added to MaterialSlot")

# Exact byte/field version is a stale-binding guard, not lineage.
require("struct MaterialVersionToken" in VERSION and
        "static_assert(sizeof(MaterialVersionToken) == 8" in VERSION,
        "D0-E evidence: exact MaterialVersionToken contract changed")
require("This is deliberately NOT MaterialId and NOT lineage" in VERSION,
        "D0-E evidence: MaterialVersionToken semantics drifted")

# Rich semantic provenance is present at PREPARE.
for token in (
    "struct PreparedPhraseExecution",
    "PhraseExecutionMaterializationSettings materialization",
    "StrongRhythmFrozenSelection selection",
    "ChordProgressionSource progressionSource",
    "PhraseHarmonicTimeline harmonicTimeline",
    "PhraseSemanticResult semantic",
    "TrajectoryId phraseTrajectory",
    "RhythmPhrasePlan phrasePlan",
):
    require(token in EXECUTION,
            f"D0-E evidence: PreparedPhraseExecution missing {token!r}")

# COMMIT still has PreparedPhraseExecution but materializeOneBar only returns a
# boolean; resolved per-bar bass-plan evidence is not exported today.
require("materializeOneBar(" in P1R and
        "materializePreparedPhraseBar(" in P1R,
        "D0-E evidence: P1R commit seam changed")
# (Historical "result dropped / BassRhythmPlan not exported" assertions were
# gap-state and moved to the D1-B gate, which tests the implemented seam.)
require("BassRhythmId bassRhythmId" in STRONG,
        "D0-E evidence: bass id export changed")

# Existing product state is intentionally tiny/coarse, not a Material provenance
# repository.
require("struct GeneratedPhraseProductState" in PRODUCT and
        "sizeof(GeneratedPhraseProductState) <= 24" in PRODUCT,
        "D0-E evidence: GeneratedPhraseProductState budget changed")
require("rootPitchClass" not in PRODUCT and
        "ChordProgressionSource" not in PRODUCT and
        "BassRhythmPlan" not in PRODUCT,
        "D0-E evidence: rich provenance unexpectedly moved into product state")

# Compact owner-derived witnesses exist.
require("struct BassRhythmPlan" in BASS and
        "sizeof(BassRhythmPlan) <= 8" in BASS,
        "D0-E evidence: BassRhythmPlan bounded witness changed")
require("struct HarmonicRhythmPlan" in HARMONIC and
        "sizeof(HarmonicRhythmPlan) <= 8" in HARMONIC,
        "D0-E evidence: HarmonicRhythmPlan bounded witness changed")
require("struct ChordProgressionSource" in PROGRESSION and
        "sizeof(ChordProgressionSource) <= 16" in PROGRESSION,
        "D0-E evidence: ChordProgressionSource bounded witness changed")
require("struct MaterialReference" in IDENTITY,
        "D0-E evidence: MaterialReference binding missing")

# Pattern -> Runtime projection has a second bounded identity surface: physical
# projection depends on explicit swing/gate settings, while an existing helper
# can expose authoritative source-step ownership without reverse analysis.
for token in (
    "struct PatternProjectionSettings",
    "swingPercent",
    "swingEnabled",
    "gateLengthRatio",
    "projectPatternToRuntimeEventsWithSourceSteps",
):
    require(token in RUNTIME_PROJECTION,
            f"D0-E evidence: Pattern projection contract missing {token!r}")

# Melody edit boundary carries mechanically useful effect classes.
require("Pitch is orthogonal to time. This never touches startTick" in RUNTIME_EDIT,
        "D0-E evidence: transposeEvent no longer guarantees pitch-only edit")
require("phrase.events[eventIndex].note =" in RUNTIME_EDIT,
        "D0-E evidence: transposeEvent pitch mutation missing")
require("phrase.events[eventIndex].durationSubticks =" in RUNTIME_EDIT,
        "D0-E evidence: duration edit primitive missing")
require("deleteEvent(phrase" in RUNTIME_EDIT,
        "D0-E evidence: join/delete onset mutation boundary changed")
require("phrase.lengthTicks = nextLengthTicks" in RUNTIME_EDIT,
        "D0-E evidence: explicit phrase-length mutation missing")

# Pattern edit primitives are centralized, but arbitrary/compound lambdas remain
# possible above them, so P0 must have an unknown-mutation fail-closed path.
for token in (
    "inline void clearStep",
    "inline void adjustNote",
    "inline void toggleAccent",
    "inline void toggleSlide",
    "inline void cycleFx",
    "inline void rotate",
):
    require(token in PATTERN_EDIT,
            f"D0-E evidence: PatternEdit primitive missing: {token}")

# D0-D1 truthfulness remains intact.
require("PrimaryDownbeatOnsetPresence" in SEM and
        "MetricAnchorTheOne" not in SEM,
        "D0-E regression: D0-D1 claim-strength repair lost")

# No reverse-analysis/global-authority implementation may sneak into D0-E.
for forbidden in (
    "MaterialSemanticProvenance",
    "LineageEvaluator",
    "DevelopmentCoordinator",
    "IdeaFingerprint",
    "DevelopmentDistance",
    "ReverseHarmonicAnalysis",
):
    # The research doc/tests may name candidates, but production state/source
    # owners under src/ must not contain them.
    found = any(
        forbidden in read(path)
        for path in (
            "src/state/material_slot.h",
            "src/state/working_material_storage.h",
            "src/dsp/development_semantics.h",
            "src/dsp/development_semantic_adapter.h",
        )
    )
    require(not found, f"D0-E ERROR: forbidden production authority appeared: {forbidden}")

if failures:
    for failure in failures:
        print(failure)
    print(f"D0-E source regressions: FAIL ({len(failures)} issues)")
    sys.exit(1)

print("D0-E source regressions: PASS")
