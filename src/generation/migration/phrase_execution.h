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
static_assert(std::is_trivially_copyable<PhraseExecutionScratch>::value,
              "phrase execution scratch must remain fixed-capacity");
static_assert(std::is_trivially_copyable<PreparedPhraseExecution>::value,
              "prepared phrase execution must remain fixed-capacity");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_MIGRATION_PHRASE_EXECUTION_H
