#pragma once

#include <cstdint>

#include "../generation_backend.h"
#include "../materialization/pattern_materializer.h"
#include "../rhythm/rhythm_catalog.h"
#include "../rhythm/rhythm_realizer.h"
#include "pattern_shadow_metrics.h"

namespace GroovePuterRhythm {

enum class VocabularyShadowStatus : uint8_t {
  Ok = 0,
  InvalidRequest,
  RealizationFailed,
  MaterializationFailed,
  Count,
};

struct VocabularyShadowRequest {
  const RhythmCatalogView* catalog = nullptr;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  RealizationLevel level = RealizationLevel::P1Canonical;
  GenerationContext generation{};

  // Early shadowing compares migrated rhythm topology while legacy synth pitch
  // remains authoritative. Deferred roles are explicit, never silently lost.
  RhythmRoleMask ignoredRoles = static_cast<RhythmRoleMask>(
      rhythmRoleBit(RhythmRole::BassRhythm) |
      rhythmRoleBit(RhythmRole::ChordRhythm) |
      rhythmRoleBit(RhythmRole::MelodicRhythm));

  uint8_t compareTargets = ShadowDrums;
};

struct VocabularyShadowResult {
  VocabularyShadowStatus status = VocabularyShadowStatus::InvalidRequest;
  RealizationStatus realizationStatus = RealizationStatus::InvalidConstraintSet;
  PatternMaterializeStatus materializationStatus =
      PatternMaterializeStatus::InvalidPlan;
  PatternMaterializationDiagnostics materialization{};
  ShadowPatternMetrics metrics{};
};

// Runs Vocabulary entirely in scratch state against an already-produced legacy
// snapshot. The legacy patterns are const and no candidate pattern escapes this
// function. Stage 4 therefore cannot accidentally become the applied backend.
VocabularyShadowResult runVocabularyShadow(
    const VocabularyShadowRequest& request,
    const DrumPatternSet& legacyDrums,
    const SynthPattern& legacySynthA,
    const SynthPattern& legacySynthB);

}  // namespace GroovePuterRhythm
