#ifndef GROOVEPUTER_GENERATION_MIGRATION_PHRASE_SEMANTIC_RESULT_H
#define GROOVEPUTER_GENERATION_MIGRATION_PHRASE_SEMANTIC_RESULT_H

#include <cstdint>
#include <type_traits>

#include "../composition/phrase_harmonic_timeline.h"
#include "../composition/phrase_length_request.h"
#include "../composition/phrase_semantic_contract.h"
#include "strong_rhythm_migration.h"

namespace GroovePuterRhythm {

enum class PhraseSemanticContractStatus : uint8_t {
  Ready = 0,
  Rejected,
  InvalidContract,
  Count,
};

struct PhraseSemanticBarState {
  PhraseTemporalCoordinates temporal{};
  PhraseHarmonicEventRange harmonicEvents{};
  MelodicCrossBarLifetime melodicLifetime{};
  MelodicMotifStatus melodicStatus = MelodicMotifStatus::InvalidRequest;
};

struct PhraseSemanticResult {
  PhraseSemanticContractStatus status =
      PhraseSemanticContractStatus::InvalidContract;
  PhraseLengthRejectReason rejectReason =
      PhraseLengthRejectReason::CompositionResolutionFailed;
  uint16_t phraseGenerationIdentity = kUnspecifiedPhraseGenerationIdentity;
  uint8_t requestedPhraseBars = 0;
  uint8_t effectivePhraseBars = 0;
  PhraseHarmonicTimeline harmonicTimeline{};
  PhraseSemanticBarState bars[kMaxSemanticPhraseBars]{};
};

inline PhraseSemanticResult makePhraseSemanticResult(
    uint16_t phraseGenerationIdentity,
    const PhraseLengthRequestResult& length,
    const PhraseHarmonicTimeline& harmonicTimeline,
    const MelodicMotifStatus (&melodicStatus)[kMaxSemanticPhraseBars],
    const MelodicCrossBarLifetime (&melodicLifetime)[kMaxSemanticPhraseBars]) {
  PhraseSemanticResult result{};
  result.phraseGenerationIdentity = phraseGenerationIdentity;
  result.requestedPhraseBars = length.requestedPhraseBars;
  result.effectivePhraseBars = length.effectivePhraseBars;
  result.rejectReason = length.rejectReason;

  if (length.status != PhraseLengthRequestStatus::Accepted) {
    result.status = PhraseSemanticContractStatus::Rejected;
    return result;
  }
  if (phraseGenerationIdentity == kUnspecifiedPhraseGenerationIdentity ||
      length.requestedPhraseBars != length.effectivePhraseBars ||
      !isSupportedPhraseLength(length.effectivePhraseBars) ||
      harmonicTimeline.status != PhraseHarmonicTimelineStatus::Ok ||
      harmonicTimeline.phraseBars != length.effectivePhraseBars) {
    result.status = PhraseSemanticContractStatus::InvalidContract;
    return result;
  }

  result.harmonicTimeline = harmonicTimeline;
  for (uint8_t bar = 0; bar < length.effectivePhraseBars; ++bar) {
    result.bars[bar].temporal = phraseTemporalCoordinatesForBar(bar);
    result.bars[bar].harmonicEvents =
        phraseHarmonicEventRangeForBar(harmonicTimeline, bar);
    result.bars[bar].melodicLifetime = melodicLifetime[bar];
    result.bars[bar].melodicStatus = melodicStatus[bar];
  }
  result.rejectReason = PhraseLengthRejectReason::None;
  result.status = PhraseSemanticContractStatus::Ready;
  return result;
}

static_assert(std::is_trivially_copyable<PhraseSemanticBarState>::value,
              "semantic phrase bar must remain fixed-capacity");
static_assert(std::is_trivially_copyable<PhraseSemanticResult>::value,
              "semantic phrase result must remain fixed-capacity");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_MIGRATION_PHRASE_SEMANTIC_RESULT_H
