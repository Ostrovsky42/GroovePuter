#ifndef GROOVEPUTER_GENERATION_MIGRATION_PHRASE_EXECUTION_H
#define GROOVEPUTER_GENERATION_MIGRATION_PHRASE_EXECUTION_H

#include <cstdint>
#include <type_traits>

#include "../composition/phrase_harmonic_clock_projection.h"
#include "phrase_semantic_result.h"

namespace GroovePuterRhythm {

enum class PhraseExecutionStatus : uint8_t {
  Ready = 0,
  Rejected,
  InvalidContext,
  ProgressionSourceFailure,
  HarmonicProjectionFailure,
  SemanticProbeFailure,
  SemanticContractFailure,
  Count,
};

// Immutable scalar settings needed to reproduce any physical semantic bar.
// Pointers, physical storage addresses and mutable cursors are deliberately
// excluded so PreparedPhraseExecution remains caller-owned and random-access.
struct PhraseExecutionMaterializationSettings {
  RealizationLevel level = RealizationLevel::P2Variation;
  uint32_t generationAttemptOrdinal = 0;
  FeelProfileId feelProfile = FeelProfileId::Straight;
  uint8_t feelAmount = 0;
  bool tonalMaterializationEnabled = false;
  uint8_t rootPitchClass = 0;
  ScaleTypeValue scaleTypeValue = kDefaultScaleTypeValue;
};

// C2 bootstrap observation for one already-realized semantic bar. This is
// transient preparation state only: it is not persisted in PreparedPhraseExecution
// and it never becomes a runtime playback token.
struct PhraseMelodicBoundaryObservation {
  uint16_t phraseGenerationIdentity = kUnspecifiedPhraseGenerationIdentity;
  uint8_t phraseBarOrdinal = kUnspecifiedPhraseBarOrdinal;
  SemanticSynthBRole role = SemanticSynthBRole::Chord;
  MelodicMotifStatus melodicStatus = MelodicMotifStatus::InvalidRequest;
  StepMask admittedOnsets = 0;
  StepMask admittedContinuations = 0;
};

// PHRASE-C2 bootstrap law. It recognizes only the frozen C2-C0 A_ONSET class.
// It never creates/moves/extends notes and deliberately rejects continuation,
// overlap, hybrid, empty, step-0 replacement, non-sequential and terminal cases.
constexpr bool c2AOnsetBoundaryEligible(
    const PhraseMelodicBoundaryObservation& outgoing,
    const PhraseMelodicBoundaryObservation& incoming,
    uint8_t effectivePhraseBars,
    bool sameLogicalMelodicVoice) {
  if (!sameLogicalMelodicVoice ||
      !isSupportedPhraseLength(effectivePhraseBars) ||
      effectivePhraseBars < 2 ||
      outgoing.phraseGenerationIdentity ==
          kUnspecifiedPhraseGenerationIdentity ||
      outgoing.phraseGenerationIdentity != incoming.phraseGenerationIdentity ||
      outgoing.phraseBarOrdinal >= effectivePhraseBars ||
      incoming.phraseBarOrdinal >= effectivePhraseBars ||
      static_cast<uint8_t>(outgoing.phraseBarOrdinal + 1u) !=
          incoming.phraseBarOrdinal ||
      outgoing.role != SemanticSynthBRole::Melodic ||
      incoming.role != SemanticSynthBRole::Melodic ||
      outgoing.melodicStatus != MelodicMotifStatus::Ok ||
      incoming.melodicStatus != MelodicMotifStatus::Ok ||
      (outgoing.admittedOnsets & outgoing.admittedContinuations) != 0 ||
      (incoming.admittedOnsets & incoming.admittedContinuations) != 0) {
    return false;
  }

  const StepMask step0 = stepBit(0);
  const StepMask step15 = stepBit(15);
  const StepMask laterThanStep0 =
      static_cast<StepMask>(kAllSteps & static_cast<StepMask>(~step0));

  const bool outgoingOnset15 =
      (outgoing.admittedOnsets & step15) != 0;
  const bool outgoingContinuation15 =
      (outgoing.admittedContinuations & step15) != 0;
  const bool incomingOnset0 = (incoming.admittedOnsets & step0) != 0;
  const bool incomingLaterOnset =
      (incoming.admittedOnsets & laterThanStep0) != 0;

  return outgoingOnset15 && !outgoingContinuation15 &&
         !incomingOnset0 && incomingLaterOnset;
}

// One reusable caller-owned physical bar used only while semantic preparation
// probes the actual strong production materializer. No physical bar is retained
// in PreparedPhraseExecution.
struct PhraseExecutionScratch {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
};

struct PreparedPhraseExecution {
  PhraseExecutionStatus status = PhraseExecutionStatus::InvalidContext;
  GenreSettings settings{};
  PhraseExecutionMaterializationSettings materialization{};
  uint16_t phraseGenerationIdentity = kUnspecifiedPhraseGenerationIdentity;
  PhraseLengthRequestResult length{};
  StrongRhythmFrozenSelection selection{};
  ChordProgressionSource progressionSource{};
  PhraseHarmonicClockProjection harmonicClock{};
  PhraseSemanticResult semantic{};
};

PhraseExecutionStatus preparePhraseExecution(
    const GenreSettings& settings,
    const PhraseExecutionMaterializationSettings& materialization,
    uint16_t phraseGenerationIdentity,
    uint8_t requestedPhraseBars,
    PhraseExecutionScratch& scratch,
    PreparedPhraseExecution& destination);

// Random-access physical materialization. physicalPatternAddress is a storage
// destination coordinate only; explicit phraseBarOrdinal remains the musical
// coordinate. On validation failure the caller-owned outputs are untouched.
StrongRhythmMigrationResult materializePreparedPhraseBar(
    const PreparedPhraseExecution& prepared,
    uint8_t phraseBarOrdinal,
    int16_t physicalPatternAddress,
    DrumPatternSet& drums,
    SynthPattern& synthA,
    SynthPattern& synthB);

static_assert(std::is_trivially_copyable<PhraseExecutionMaterializationSettings>::value,
              "phrase execution settings must remain fixed-capacity");
static_assert(std::is_trivially_copyable<PhraseMelodicBoundaryObservation>::value,
              "C2 boundary observation must remain fixed-capacity");
static_assert(std::is_trivially_copyable<PhraseExecutionScratch>::value,
              "phrase execution scratch must remain fixed-capacity");
static_assert(std::is_trivially_copyable<PreparedPhraseExecution>::value,
              "prepared phrase execution must remain fixed-capacity");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_MIGRATION_PHRASE_EXECUTION_H
