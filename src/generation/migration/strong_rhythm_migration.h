#ifndef GROOVEPUTER_GENERATION_MIGRATION_STRONG_RHYTHM_MIGRATION_H
#define GROOVEPUTER_GENERATION_MIGRATION_STRONG_RHYTHM_MIGRATION_H

#include <cstdint>

#include "../../../scenes.h"
#include "../composition/generation_profile.h"
#include "../composition/rhythm_selection.h"
#include "../composition/tonal_profile.h"
#include "../feel/feel_pattern_adapter.h"
#include "../materialization/pattern_materializer.h"
#include "../rhythm/reference_vocabulary.h"
#include "../roles/bass_pitch_behavior.h"
#include "../roles/bass_rhythm.h"
#include "../roles/chord_progression.h"
#include "../roles/chord_rhythm.h"
#include "../roles/melodic_motif.h"
#include "../roles/melodic_pitch_intent.h"
#include "../roles/semantic_pattern_projector.h"
#include "../tonal/tonal_materializer.h"
#include "tonal_pattern_adapter.h"

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
  AttemptUnavailable,
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
  // Existing pattern address remains part of deterministic generation identity.
  int16_t patternAddress = 0;
  RealizationLevel level = RealizationLevel::P2Variation;

  // F-07: assigned when a generation request is accepted. It is transient
  // session/request state, never Scene/project persistence. Ordinal zero is the
  // compatibility path; non-zero ordinals may vary realization while the
  // selected rhythm archetype remains attempt-invariant.
  uint32_t generationAttemptOrdinal = 0;

  FeelProfileId feelProfile = FeelProfileId::Straight;
  uint8_t feelAmount = 0;

  // Stage 15 tonal integration is explicit and transient. Legacy callers that
  // do not provide tonal context keep the established pitch-redistribution path.
  bool tonalMaterializationEnabled = false;
  uint8_t rootPitchClass = 0;
  ScaleTypeValue scaleTypeValue = kDefaultScaleTypeValue;
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
  BassPitchBehaviorStatus bassPitchBehaviorStatus =
      BassPitchBehaviorStatus::InvalidRequest;
  BassPitchContourId bassPitchContour = BassPitchContourId::Auto;
  SemanticPatternProjectStatus bassProjectionStatus =
      SemanticPatternProjectStatus::Ok;
  TonalMaterializationStatus bassTonalStatus =
      TonalMaterializationStatus::InvalidRequest;
  TonalProjectionStatus bassTonalProjectionStatus =
      TonalProjectionStatus::InvalidRequest;
  TonalPatternAdaptStatus bassTonalAdaptStatus =
      TonalPatternAdaptStatus::InvalidPlan;
  FeelInterpretStatus bassFeelStatus = FeelInterpretStatus::Ok;
  ChordRhythmStatus chordRhythmStatus = ChordRhythmStatus::InvalidRequest;
  ChordRhythmId chordRhythmId = ChordRhythmId::Auto;
  ChordProgressionStatus chordProgressionStatus =
      ChordProgressionStatus::InvalidRequest;
  ProgressionId progressionId = ProgressionId::Auto;
  SemanticPatternProjectStatus chordProjectionStatus =
      SemanticPatternProjectStatus::Ok;
  TonalMaterializationStatus chordTonalStatus =
      TonalMaterializationStatus::InvalidRequest;
  TonalProjectionStatus chordTonalProjectionStatus =
      TonalProjectionStatus::InvalidRequest;
  TonalPatternAdaptStatus chordTonalAdaptStatus =
      TonalPatternAdaptStatus::InvalidPlan;
  FeelInterpretStatus chordFeelStatus = FeelInterpretStatus::Ok;
  MelodicMotifStatus melodicMotifStatus =
      MelodicMotifStatus::InvalidRequest;
  MelodicRhythmId melodicRhythmId = MelodicRhythmId::Auto;
  MotifShapeId motifShapeId = MotifShapeId::Auto;
  MelodicPitchIntentStatus melodicPitchIntentStatus =
      MelodicPitchIntentStatus::InvalidRequest;
  MelodicContourId melodicPitchContour = MelodicContourId::Auto;
  SemanticPatternProjectStatus melodicProjectionStatus =
      SemanticPatternProjectStatus::Ok;
  TonalMaterializationStatus melodicTonalStatus =
      TonalMaterializationStatus::InvalidRequest;
  TonalProjectionStatus melodicTonalProjectionStatus =
      TonalProjectionStatus::InvalidRequest;
  TonalPatternAdaptStatus melodicTonalAdaptStatus =
      TonalPatternAdaptStatus::InvalidPlan;
  FeelInterpretStatus melodicFeelStatus = FeelInterpretStatus::Ok;
  SemanticSynthBRole synthBRole = SemanticSynthBRole::Chord;

  // Ephemeral semantic topology. One physical Synth B remains monophonic:
  // hybrid mode gives chord onsets/continuations priority and records only
  // melodic onsets admitted into otherwise free cells.
  StepMask chordOnsets = 0;
  StepMask melodicFillOnsets = 0;
  bool chordRhythmApplied = false;
  bool melodicRhythmApplied = false;
  bool tonalMaterializationApplied = false;
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
