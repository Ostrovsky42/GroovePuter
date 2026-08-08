#pragma once

#include <cstdint>

#include "../../../scenes.h"
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
  Count,
};

enum class StrongRhythmMigrationStatus : uint8_t {
  Legacy = 0,
  Applied,
  InvalidContext,
  RealizationFailed,
  MaterializationFailed,
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
  RealizationStatus realizationStatus = RealizationStatus::InvalidConstraintSet;
  PatternMaterializeStatus materializationStatus =
      PatternMaterializeStatus::InvalidPlan;
};

// Explicit Stage 5 allow-list. A non-zero recipe is authoritative: unsupported
// recipes remain legacy even when their base generative mode is a strong style.
// Cross-recipe morphs also remain legacy until GenreGenerationProfile owns
// weighted vocabulary selection.
StrongRhythmRoute selectStrongRhythmRoute(const GenreSettings& settings);

// Transactional migration over an already-produced legacy drum pattern. The
// destination is untouched for Legacy/failure. On success only drum voices are
// replaced; legacy automation lanes and PatternGroove remain authoritative.
StrongRhythmMigrationResult migrateStrongRhythmDrums(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& destination);

}  // namespace GroovePuterRhythm
