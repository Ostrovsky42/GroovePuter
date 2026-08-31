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

// Fresh-P1R corrected-ancestry adapter. The old frozen P1R executor consumed
// the H1-F1 WHAT source through a bool/out-param helper. Finalized H1-F1 exposes
// ChordProgressionEventResult instead. Keep the P1R execution algorithm frozen
// while translating only that API shape inside the allowed P1R owner.
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

// P1R transient execution seam. It carries already-frozen H2 WHEN and one
// H1-F1 WHAT source into the existing one-bar materializer. It owns neither
// policy nor storage and is valid only for the duration of one call.
struct StrongRhythmPhraseExecutionOverride {
  const HarmonicRhythmPlan* harmonicRhythm = nullptr;
  const ChordProgressionSource* progressionSource = nullptr;
  uint16_t firstGlobalHarmonicOrdinal = 0;
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

  // E0a: PREPARE-owned semantic coordinates for one physical Phrase bar.
  // Unspecified keeps non-Phrase callers on the exact compatibility path.
  // evolutionOrdinal is explicit context for the existing 4+4 structure; E0a
  // does not force it into generation that does not already consume it.
  uint8_t phraseBarOrdinal = kUnspecifiedPhraseBarOrdinal;
  uint8_t evolutionOrdinal = 0;

  // M1 phrase materialization supplies one logical identity shared by all
  // physical destination bars. Unspecified preserves ordinary one-bar callers.
  uint16_t phraseGenerationIdentity = kUnspecifiedPhraseGenerationIdentity;
  const StrongRhythmFrozenSelection* frozenSelection = nullptr;

  FeelProfileId feelProfile = FeelProfileId::Straight;
  uint8_t feelAmount = 0;

  // Stage 15 tonal integration is explicit and transient. Legacy callers that
  // do not provide tonal context keep the established pitch-redistribution path.
  bool tonalMaterializationEnabled = false;
  uint8_t rootPitchClass = 0;
  ScaleTypeValue scaleTypeValue = kDefaultScaleTypeValue;

  // P1R appends this optional seam so all ordinary aggregate/default callers
  // retain the legacy path when no prepared phrase execution is supplied.
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

  // Ephemeral semantic topology. One physical Synth B remains monophonic:
  // hybrid mode gives chord onsets/continuations priority and records only
  // melodic onsets admitted into otherwise free cells.
  StepMask chordOnsets = 0;
  StepMask melodicFillOnsets = 0;
  bool chordRhythmApplied = false;
  bool melodicRhythmApplied = false;
  bool tonalMaterializationApplied = false;
};

#ifdef GROOVEPUTER_M1_TEST_PROBE
// Focused host-test observation only.  Normal firmware neither declares nor
// links this type or its storage; migration semantics remain untouched.
struct StrongRhythmMelodicRequestProbe {
  bool captured = false;
  MelodicMotifRequest request{};
};

void setStrongRhythmMelodicRequestProbe(
    StrongRhythmMelodicRequestProbe* probe);
#endif

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

// Genre-aware synth-only materialization. Drums are rhythm context and remain
// byte-for-byte unchanged; both synth candidates are produced so the caller can
// atomically publish only the selected physical voice.
StrongRhythmMigrationResult migrateStrongRhythmSynths(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB);

// M1 four-bar path: resolve composition once under an explicit logical phrase
// identity, then materialize each explicit phraseBarOrdinal into independent
// physical storage. The ordinary one-bar APIs above retain compatibility.
StrongRhythmMigrationResult resolveStrongRhythmFrozenSelection(
    const GenreSettings& settings,
    const StrongRhythmMigrationContext& context,
    uint16_t phraseGenerationIdentity,
    StrongRhythmFrozenSelection& destination);

// P1R exact-length sibling. It performs the same strong-rhythm selection setup
// but calls the frozen M4 exact phrase-length resolver exactly once. Legacy
// callers remain on resolveStrongRhythmFrozenSelection().
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
