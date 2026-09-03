#ifndef GROOVEPUTER_GENERATION_MIGRATION_STRONG_RHYTHM_MIGRATION_H
#define GROOVEPUTER_GENERATION_MIGRATION_STRONG_RHYTHM_MIGRATION_H

#include <cstdint>

#include "../../../scenes.h"
#include "../composition/generation_profile.h"
#include "../composition/phrase_length_request.h"
#include "../composition/rhythm_selection.h"
#include "../composition/tonal_profile.h"
#include "../feel/feel_pattern_adapter.h"
#include "../materialization/pattern_materializer.h"
#include "../rhythm/reference_vocabulary.h"
#include "../roles/bass_pitch_behavior.h"
#include "../roles/bass_rhythm.h"
#include "../roles/chord_progression.h"
#include "../roles/chord_rhythm.h"
#include "../roles/harmonic_rhythm.h"
#include "../roles/melodic_motif.h"
#include "../roles/melodic_pitch_intent.h"
#include "../roles/semantic_pattern_projector.h"
#include "../tonal/tonal_materializer.h"
#include "tonal_pattern_adapter.h"

namespace GroovePuterRhythm {

constexpr uint8_t kGrooveVocabularyPhraseBars = 4;
constexpr uint8_t kUnspecifiedPhraseBarOrdinal = 0xFFu;
constexpr uint16_t kUnspecifiedPhraseGenerationIdentity = 0xFFFFu;

struct PhraseTemporalCoordinates {
  uint8_t phraseBarOrdinal = 0;
  uint8_t evolutionOrdinal = 0;
};

constexpr PhraseTemporalCoordinates phraseTemporalCoordinatesForBar(
    uint8_t phraseBarOrdinal) {
  return PhraseTemporalCoordinates{
      phraseBarOrdinal,
      static_cast<uint8_t>(phraseBarOrdinal / kGrooveVocabularyPhraseBars)};
}

constexpr uint8_t phraseVocabularyBarOrdinal(uint8_t phraseBarOrdinal) {
  return static_cast<uint8_t>(phraseBarOrdinal % kGrooveVocabularyPhraseBars);
}

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

struct StrongRhythmFrozenSelection;

inline bool chordProgressionSourceEventAt(
    const ChordProgressionSource& source,
    uint32_t globalHarmonicOrdinal,
    HarmonicEvent& destination) {
  const ChordProgressionEventResult result =
      chordProgressionEventAt(source, globalHarmonicOrdinal);
  if (result.status != ChordProgressionStatus::Ok &&
      result.status != ChordProgressionStatus::ValidButStatic) {
    return false;
  }
  destination = result.event;
  return true;
}

struct StrongRhythmPhraseExecutionOverride {
  const HarmonicRhythmPlan* harmonicRhythm = nullptr;
  const ChordProgressionSource* progressionSource = nullptr;
  uint16_t firstGlobalHarmonicOrdinal = 0;
  // GF2-I3: the already-evolved rhythm plan for this bar of the phrase. The
  // phrase owner realizes the whole bar-function programme; the shared
  // migration only materializes the bar it is handed.
  const RhythmBarPlan* barPlan = nullptr;
};

struct StrongRhythmMigrationContext {
  int16_t patternAddress = 0;
  RealizationLevel level = RealizationLevel::P2Variation;

  uint32_t generationAttemptOrdinal = 0;

  uint8_t phraseBarOrdinal = kUnspecifiedPhraseBarOrdinal;
  uint8_t evolutionOrdinal = 0;

  uint16_t phraseGenerationIdentity = kUnspecifiedPhraseGenerationIdentity;
  const StrongRhythmFrozenSelection* frozenSelection = nullptr;

  FeelProfileId feelProfile = FeelProfileId::Straight;
  uint8_t feelAmount = 0;

  bool tonalMaterializationEnabled = false;
  uint8_t rootPitchClass = 0;
  ScaleTypeValue scaleTypeValue = kDefaultScaleTypeValue;

  const StrongRhythmPhraseExecutionOverride* phraseExecutionOverride = nullptr;
};

// Bounded migration-owned phrase selection. This is deliberately the existing
// composition result plus the production-owned generation identities, not a
// second composition model. It is caller-owned and ephemeral.
struct StrongRhythmFrozenSelection {
  StrongRhythmRoute route = StrongRhythmRoute::Legacy;
  GenerationCompositionResult composition{};
  GenerationContext selectionGeneration{};
  GenerationContext realizationGeneration{};
  uint16_t phraseGenerationIdentity = kUnspecifiedPhraseGenerationIdentity;
  // GF2-I2: derived once with the composition so every role of one musical
  // decision materializes with the same concrete FEEL. Never persisted.
  FeelProfileId resolvedFeel = FeelProfileId::Straight;
  // GF2-I4: profile activity intent is projected once against the selected
  // archetype on PREPARE/frozen-selection. Every phrase bar forwards this same
  // value; phrase-law remains the sole owner of temporal bar evolution.
  uint8_t structuralDensityTarget = kNoStructuralDensityTarget;
  bool resolved = false;
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
  FeelProfileId resolvedFeel = FeelProfileId::Straight;
  PhraseEvolutionLawId phraseLaw = PhraseEvolutionLawId::Loop;
  BarFunction phraseBarFunction = BarFunction::Statement;
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
  HarmonicRhythmStatus harmonicRhythmStatus = HarmonicRhythmStatus::InvalidRequest;
  StepMask harmonicEventOnsets = 0;
  uint8_t harmonicEventCount = 0;
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

  StepMask chordOnsets = 0;
  StepMask melodicFillOnsets = 0;
  bool chordRhythmApplied = false;
  bool melodicRhythmApplied = false;
  bool tonalMaterializationApplied = false;
};

#ifdef GROOVEPUTER_M1_TEST_PROBE
struct StrongRhythmMelodicRequestProbe {
  bool captured = false;
  MelodicMotifRequest request{};
};

void setStrongRhythmMelodicRequestProbe(
    StrongRhythmMelodicRequestProbe* probe);
#endif

StrongRhythmRoute selectStrongRhythmRoute(const GenreSettings& settings);

TrajectoryId phraseTrajectoryForLaw(PhraseEvolutionLawId law,
                                    RealizationLevel level);

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

StrongRhythmMigrationResult migrateStrongRhythmSynths(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB);

StrongRhythmMigrationResult resolveStrongRhythmFrozenSelection(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    uint16_t phraseGenerationIdentity,
    StrongRhythmFrozenSelection& destination);

StrongRhythmMigrationResult resolveStrongRhythmFrozenSelectionForPhraseBars(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    uint16_t phraseGenerationIdentity,
    uint8_t requestedPhraseBars,
    PhraseLengthRequestResult& lengthResult,
    StrongRhythmFrozenSelection& destination);

StrongRhythmMigrationResult migrateStrongRhythmFrozenMaterial(
    const GenreSettings& settings,
    const StrongRhythmFrozenSelection& selection,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB);

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_MIGRATION_STRONG_RHYTHM_MIGRATION_H
