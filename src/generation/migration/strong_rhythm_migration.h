#pragma once

#include <cstdint>

#include "../../../scenes.h"
#include "../composition/rhythm_selection.h"
#include "../materialization/pattern_materializer.h"
#include "../rhythm/reference_vocabulary.h"

namespace GroovePuterRhythm {

enum class StrongRhythmRoute : uint8_t {
  Legacy = 0,
  AcidBase,
  TechnoBase,
  RaveBase,
  DrumAndBass,
  DubTechno,
  ChicagoJack,
  RollingAcid,
  DeepChord,
  Stage7Composition,
  Count,
};

enum class StrongRhythmMigrationStatus : uint8_t {
  Legacy = 0,
  Applied,
  InvalidContext,
  RealizationFailed,
  MaterializationFailed,
  CompatibilityBindingFailed,
  Count,
};

struct StrongRhythmMigrationContext {
  // Existing pattern address is an explicit deterministic variation coordinate.
  // Stage 5 does not add a persisted backend/seed/ordinal to Scene.
  int16_t patternAddress = 0;
  RealizationLevel level = RealizationLevel::P2Variation;
};

struct StrongRhythmMigrationResult {
  StrongRhythmMigrationStatus status = StrongRhythmMigrationStatus::Legacy;
  StrongRhythmRoute route = StrongRhythmRoute::Legacy;
  ReferenceVocabulary::Archetype archetype =
      ReferenceVocabulary::Archetype::Count;
  RhythmSelectionMode selectionMode = RhythmSelectionMode::Auto;
  bool normalizedSelectionToAuto = false;
  RealizationStatus realizationStatus = RealizationStatus::InvalidConstraintSet;
  PatternMaterializeStatus materializationStatus =
      PatternMaterializeStatus::InvalidPlan;

  // Ephemeral compatibility output from the already-realized plan. It is never
  // persisted and carries no pitch/VoiceRole ownership.
  StepMask chordOnsets = 0;
  bool chordRhythmApplied = false;
};

// Explicit Stage 5 allow-list. A non-zero recipe is authoritative: unsupported
// recipes remain legacy even when their base generative mode is a strong style.
// Cross-recipe morphs also remain legacy until GenreGenerationProfile owns
// weighted vocabulary selection.
StrongRhythmRoute selectStrongRhythmRoute(const GenreSettings& settings);

// Transactional drum-only primitive. The destination is untouched for
// Legacy/failure. On success only drum voices are replaced; legacy automation
// lanes and PatternGroove remain authoritative. chordOnsets exposes only the
// already-realized ChordRhythm topology for the compatibility adapter below.
StrongRhythmMigrationResult migrateStrongRhythmDrums(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& destination);

// Stage 5 compatibility adapter for the two strong Dub routes. Synth B is the
// established legacy lead/stab physical slot (and Atlas Deep Chord target 9).
// Vocabulary owns only ChordRhythm onset placement; pitches, velocity, timing,
// accent, slide and timbre remain sourced from the just-generated legacy Synth B.
// Commit of drums + Synth B is atomic. No semantic VoiceRole is introduced.
StrongRhythmMigrationResult migrateStrongRhythmMaterial(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& drums,
    SynthPattern& synthB);

}  // namespace GroovePuterRhythm
