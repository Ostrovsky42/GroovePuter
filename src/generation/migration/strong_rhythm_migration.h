#ifndef GROOVEPUTER_GENERATION_MIGRATION_STRONG_RHYTHM_MIGRATION_H
#define GROOVEPUTER_GENERATION_MIGRATION_STRONG_RHYTHM_MIGRATION_H

#include <cstdint>

#include "../../../scenes.h"
#include "../composition/generation_profile.h"
#include "../composition/rhythm_selection.h"
#include "../feel/feel_pattern_adapter.h"
#include "../materialization/pattern_materializer.h"
#include "../rhythm/reference_vocabulary.h"
#include "../roles/bass_rhythm.h"
#include "../roles/chord_progression.h"
#include "../roles/chord_rhythm.h"
#include "../roles/melodic_motif.h"
#include "../roles/semantic_pattern_projector.h"

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
  FeelApplyFailed,
  Count,
};

enum class SemanticSynthBRole : uint8_t {
  Chord = 0,
  Melodic,
  ChordWithMelodicFill,
  Count,
};

struct StrongRhythmMigrationContext {
  // Existing pattern address is an explicit deterministic variation coordinate.
  // Stage 5 does not add a persisted backend/seed/ordinal to Scene.
  int16_t patternAddress = 0;
  RealizationLevel level = RealizationLevel::P2Variation;
  FeelProfileId feelProfile = FeelProfileId::Straight;
  uint8_t feelAmount = 0;
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
  FeelPatternApplyStatus feelStatus = FeelPatternApplyStatus::Ok;
  GenerationCompositionStatus compositionStatus =
      GenerationCompositionStatus::NoProfile;
  FeelProfileId suggestedFeel = FeelProfileId::Straight;
  // Planning metadata only until Stage 12's physical reachability gate clears.
  PhraseEvolutionLawId phraseLaw = PhraseEvolutionLawId::Loop;
  uint8_t phraseBars = 1;
  GenerationCorridor corridor{};
  BassRhythmStatus bassRhythmStatus = BassRhythmStatus::InvalidRequest;
  BassRhythmId bassRhythmId = BassRhythmId::Auto;
  SemanticPatternProjectStatus bassProjectionStatus =
      SemanticPatternProjectStatus::Ok;
  FeelInterpretStatus bassFeelStatus = FeelInterpretStatus::Ok;
  ChordRhythmStatus chordRhythmStatus = ChordRhythmStatus::InvalidRequest;
  ChordRhythmId chordRhythmId = ChordRhythmId::Auto;
  ChordProgressionStatus chordProgressionStatus =
      ChordProgressionStatus::InvalidRequest;
  ProgressionId progressionId = ProgressionId::Auto;
  // Production-visible semantic output. Absolute pitch stays with the existing
  // compatibility projector until Tonal Projector integration is available.
  ChordProgressionPlan chordProgressionPlan{};
  SemanticPatternProjectStatus chordProjectionStatus =
      SemanticPatternProjectStatus::Ok;
  FeelInterpretStatus chordFeelStatus = FeelInterpretStatus::Ok;
  MelodicMotifStatus melodicMotifStatus =
      MelodicMotifStatus::InvalidRequest;
  MelodicRhythmId melodicRhythmId = MelodicRhythmId::Auto;
  MotifShapeId motifShapeId = MotifShapeId::Auto;
  SemanticPatternProjectStatus melodicProjectionStatus =
      SemanticPatternProjectStatus::Ok;
  FeelInterpretStatus melodicFeelStatus = FeelInterpretStatus::Ok;
  SemanticSynthBRole synthBRole = SemanticSynthBRole::Chord;

  // Ephemeral semantic topology. One physical Synth B remains monophonic:
  // hybrid mode gives chord onsets/continuations priority and records only
  // melodic onsets admitted into otherwise free cells.
  StepMask chordOnsets = 0;
  StepMask melodicFillOnsets = 0;
  bool chordRhythmApplied = false;
  bool melodicRhythmApplied = false;
};

StrongRhythmRoute selectStrongRhythmRoute(const GenreSettings& settings);

StrongRhythmMigrationResult migrateStrongRhythmDrums(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& destination);

StrongRhythmMigrationResult migrateStrongRhythmMaterial(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB);

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_MIGRATION_STRONG_RHYTHM_MIGRATION_H
