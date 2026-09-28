#!/usr/bin/env python3
"""D0-F source audit: pin the minimum P0 seam and its prerequisites.

D0-F is contract/reconciliation only. It must not quietly relax Material
identity or pretend the evidence-export seam already exists.
"""

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]

def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

LINEAGE = read("src/state/material_lineage.h")
SONG = read("src/dsp/generated_phrase_song.h")
PHRASE_GENERATOR = read("src/dsp/phrase_generator.h")
P1R = read("src/dsp/generated_phrase_p1r_materializer.h")
STRONG_H = read("src/generation/migration/strong_rhythm_migration.h")
STRONG_CPP = read("src/generation/migration/strong_rhythm_migration.cpp")
ENGINE = read("src/dsp/miniacid_engine.cpp")
PAGING_H = read("src/audio/pattern_paging.h")
PAGING_CPP = read("src/audio/pattern_paging.cpp")
BASS = read("src/generation/roles/bass_rhythm.h")
HARMONIC = read("src/generation/roles/harmonic_rhythm.h")
PROGRESSION = read("src/generation/roles/chord_progression.h")
SEM = read("src/dsp/development_semantics.h")
P0_TEST = read("tests/test_0_9_14_d0f_p0_preservation_contract.cpp")

failures: list[str] = []

def require(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)

# Existing FS2A lifecycle is deliberately strict. D0-F must not weaken this.
require("return reference.id.valid() && version.valid();" in LINEAGE,
        "D0-F evidence: PreparationBasis no longer requires stable MaterialId + version")
require("if (!current303MaterialReference_(idx, reference))" in ENGINE,
        "D0-F evidence: CURRENT basis no longer resolves through MaterialReference")
require("if (!actualBasis.valid())" in ENGINE and
        "return NextPrepareResult::UnsupportedCurrentState;" in ENGINE,
        "D0-F evidence: NEXT lifecycle no longer fails closed on invalid basis")

# Generated Phrase slot selection is currently physical-byte/reference based,
# not Material-descriptor aware. A physically empty slot may still carry a
# Melody/Material identity descriptor and therefore is not semantically free.
safe_start = PHRASE_GENERATOR.find("inline bool localSlotIsSafeForPhrase(")
safe_end = PHRASE_GENERATOR.find("inline int findSafeContiguousEmptySlots(", safe_start)
require(safe_start >= 0 and safe_end > safe_start,
        "D0-F evidence: Phrase safe-slot predicate moved")
if safe_start >= 0 and safe_end > safe_start:
    safe = PHRASE_GENERATOR[safe_start:safe_end]
    require("localSlotIsEmpty" in safe and "globalPatternIsReferenced" in safe,
            "D0-F evidence: Phrase safe-slot physical/reference guards changed")

# A0 future contract must be able to distinguish canonical free descriptors.
require("MaterialSlotDescriptor" in SONG or "MaterialSlotDescriptor" in LINEAGE or
        "MaterialSlotDescriptor" in read("src/state/material_slot.h"),
        "D0-F evidence: canonical MaterialSlotDescriptor type missing")

# Generated Phrase currently writes physical patterns directly but does not
# establish Material descriptors/ids in the generated commit path.
apply_start = SONG.find("inline void applyPreparedPersistent(")
apply_end = SONG.find("inline GeneratedPhraseUndoPayload captureUndo(", apply_start)
require(apply_start >= 0 and apply_end > apply_start,
        "D0-F evidence: generated Phrase commit seam moved")
if apply_start >= 0 and apply_end > apply_start:
    apply = SONG[apply_start:apply_end]
    require("scene.synthABanks[bank].patterns[index] = scratch.synthA;" in apply,
            "D0-F evidence: generated Synth A physical commit changed")

# P1R Synth A is a genuine bass-role proof vertical.
require("BassRhythmResult bass" in STRONG_CPP,
        "D0-F evidence: bass owner result disappeared")
require("nextSynthA" in STRONG_CPP and
        "bass.plan.onsets" in STRONG_CPP and
        "bass.plan.continuations" in STRONG_CPP,
        "D0-F evidence: Synth A no longer materializes from BassRhythmPlan")
require("synthA = nextSynthA;" in STRONG_CPP,
        "D0-F evidence: Synth A bass-role publication changed")

# Compact owner-derived evidence exists, but full BassRhythmPlan is not exported.
require("struct BassRhythmPlan" in BASS and
        "static_assert(sizeof(BassRhythmPlan) <= 8" in BASS,
        "D0-F evidence: compact BassRhythmPlan witness changed")
result_start = STRONG_H.find("struct StrongRhythmMigrationResult")
result_end = STRONG_H.find("#ifdef GROOVEPUTER_M1_TEST_PROBE", result_start)
require(result_start >= 0 and result_end > result_start,
        "D0-F evidence: StrongRhythmMigrationResult moved")
if result_start >= 0 and result_end > result_start:
    result_block = STRONG_H[result_start:result_end]
    require("BassRhythmId bassRhythmId" in result_block,
            "D0-F evidence: existing bass id export changed")
    # (gap assertion "plan not exported" moved to D1-B, which tests the seam)

# materializeOneBar currently discards the detailed migration result.
m1_start = P1R.find("inline bool materializeOneBar(")
m1_end = P1R.find("inline PreparationDisposition prepare(", m1_start)
require(m1_start >= 0 and m1_end > m1_start,
        "D0-F evidence: P1R one-bar seam moved")
if m1_start >= 0 and m1_end > m1_start:
    m1 = P1R[m1_start:m1_end]
    require("materializePreparedPhraseBar(" in m1,
            "D0-F evidence: one-bar materializer no longer calls the migration owner")

# Harmonic WHAT/WHEN and tonal frame are already available before COMMIT.
for token in (
    "ChordProgressionSource progressionSource",
    "PhraseHarmonicClockProjection harmonicClock",
    "rootPitchClass",
    "scaleTypeValue",
):
    require(token in read("src/generation/migration/phrase_execution.h"),
            f"D0-F evidence: PreparedPhraseExecution missing {token!r}")
require("struct HarmonicRhythmPlan" in HARMONIC and
        "static_assert(sizeof(HarmonicRhythmPlan) <= 8" in HARMONIC,
        "D0-F evidence: compact HarmonicRhythmPlan changed")
require("struct ChordProgressionSource" in PROGRESSION and
        "static_assert(sizeof(ChordProgressionSource) <= 16" in PROGRESSION,
        "D0-F evidence: ChordProgressionSource compact bound changed")

# P1R production always supplies explicit tonal materialization context.
require("settings.tonalMaterializationEnabled = true;" in P1R and
        "settings.rootPitchClass" in P1R and
        "settings.scaleTypeValue" in P1R,
        "D0-F evidence: generated P1R tonal frame is no longer explicit")

# Existing Material identity allocator is durable SD-backed, so generated-Phrase
# identity closure must use that owner rather than minting a second id namespace.
require("static GroovePuterMaterial::MaterialId allocateMaterialId();" in PAGING_H,
        "D0-F evidence: canonical MaterialId allocator missing")
require("writeIdentityHighWater" in PAGING_CPP and
        "PatternPagingService::allocateMaterialId()" in PAGING_CPP,
        "D0-F evidence: MaterialId high-water owner changed")

# D0-C/D0-D1 semantic boundaries remain frozen.
for token in (
    "LineageStatus",
    "StateRelation",
    "TrajectoryRole",
    "GenreStatus",
    "OperationConformance",
    "CapabilityStatus",
):
    require(token in SEM, f"D0-F regression: D0-C semantic axis lost: {token}")
require("PrimaryDownbeatOnsetPresence" in SEM and
        "MetricAnchorTheOne" not in SEM,
        "D0-F regression: D0-D1 claim-strength repair lost")

# P0 witnesses must consume transformation primitives but never legacy
# classification/disposition authority.
require("developCandidate(" not in P0_TEST and
        "evaluateClassificationAndG4(" not in P0_TEST and
        "evaluateDisposition(" not in P0_TEST,
        "D0-F test imported legacy semantic authority")

# No production workaround is allowed during the D0-F contract checkpoint.
for forbidden in (
    "GeneratedSynthADevelopmentOrigin",
    "P0LineageEvaluator",
    "P0PreservationEvaluator",
    "SessionMaterialReference",
    "TransientMaterialId",
):
    found = any(
        forbidden in read(path)
        for path in (
            "src/state/material_lineage.h",
            "src/dsp/generated_phrase_song.h",
            "src/dsp/miniacid_engine.cpp",
            "src/dsp/development_semantics.h",
        )
    )
    require(not found,
            f"D0-F ERROR: production workaround appeared during contract checkpoint: {forbidden}")

if failures:
    for failure in failures:
        print(failure)
    print(f"D0-F source audit: FAIL ({len(failures)} issues)")
    sys.exit(1)

print("D0-F source audit: PASS")
print("D0-F established prerequisites:")
print("  A0. generated Phrase safe-slot selection must respect Material descriptors")
print("  A1. generated Phrase Synth A needs canonical MaterialId closure")
print("  B. StrongRhythmMigrationResult must export resolved BassRhythmPlan")
print("  C. P1R one-bar seam must forward that owner-derived plan")
