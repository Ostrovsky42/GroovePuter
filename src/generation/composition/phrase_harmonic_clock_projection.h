#ifndef GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_HARMONIC_CLOCK_PROJECTION_H
#define GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_HARMONIC_CLOCK_PROJECTION_H

#include <cstdint>
#include <type_traits>

#include "phrase_harmonic_timeline.h"
#include "../roles/harmonic_rhythm.h"

namespace GroovePuterRhythm {

enum class PhraseHarmonicClockProjectionStatus : uint8_t {
  Ok = 0,
  InvalidRequest,
  HarmonicRhythmFailure,
  TimelineFailure,
  Count,
};

struct PhraseHarmonicBarProjection {
  uint8_t phraseBarOrdinal = 0;
  HarmonicRhythmPlan harmonicRhythm{};
  PhraseHarmonicEventRange eventRange{};
};

struct PhraseHarmonicClockProjection {
  PhraseHarmonicClockProjectionStatus status =
      PhraseHarmonicClockProjectionStatus::InvalidRequest;
  uint8_t phraseBars = 0;
  uint8_t harmonicRhythmRealizationCount = 0;
  PhraseHarmonicTimeline timeline{};
  PhraseHarmonicBarProjection bars[kMaxSemanticPhraseBars]{};
};

// H2 phrase policy: realize the accepted F08 one-bar WHEN owner exactly once
// for each semantic bar, then concatenate those local event positions into the
// existing C1 phrase timeline. phraseHarmonicPosition carries the phrase-global
// first ordinal for that bar; it does not alter the F08 local clock.
//
// This function deliberately does not select or materialize ChordProgression
// WHAT. H1 remains the single phrase-global WHAT source; production execution
// wiring is deferred to PHRASE-P1R.
inline PhraseHarmonicClockProjection projectPhraseHarmonicClock(
    uint8_t phraseBars,
    ProgressionId progression) {
  PhraseHarmonicClockProjection result{};
  if (!isSupportedPhraseLength(phraseBars) ||
      !isValidProgressionId(progression, false)) {
    return result;
  }

  StepMask eventPositionsByBar[kMaxSemanticPhraseBars]{};
  uint8_t nextPhraseOrdinal = 0;

  result.phraseBars = phraseBars;
  for (uint8_t bar = 0; bar < phraseBars; ++bar) {
    HarmonicRhythmRequest request{};
    request.progression = progression;
    request.phraseBarOrdinal = bar;
    request.phraseHarmonicPosition = nextPhraseOrdinal;

    const HarmonicRhythmResult realized = realizeHarmonicRhythm(request);
    result.harmonicRhythmRealizationCount = static_cast<uint8_t>(
        result.harmonicRhythmRealizationCount + 1u);
    if (realized.status != HarmonicRhythmStatus::Ok) {
      result.status = PhraseHarmonicClockProjectionStatus::HarmonicRhythmFailure;
      return result;
    }

    PhraseHarmonicBarProjection& projected = result.bars[bar];
    projected.phraseBarOrdinal = bar;
    projected.harmonicRhythm = realized.plan;
    eventPositionsByBar[bar] = realized.plan.onsets;
    nextPhraseOrdinal = static_cast<uint8_t>(
        nextPhraseOrdinal + realized.plan.eventCount);
  }

  result.timeline = makePhraseHarmonicTimeline(phraseBars, eventPositionsByBar);
  if (result.timeline.status != PhraseHarmonicTimelineStatus::Ok ||
      result.timeline.totalEventPositions != nextPhraseOrdinal) {
    result.status = PhraseHarmonicClockProjectionStatus::TimelineFailure;
    return result;
  }

  for (uint8_t bar = 0; bar < phraseBars; ++bar) {
    PhraseHarmonicBarProjection& projected = result.bars[bar];
    projected.eventRange = phraseHarmonicEventRangeForBar(result.timeline, bar);
    if (projected.eventRange.firstOrdinal !=
            projected.harmonicRhythm.phraseHarmonicPosition ||
        projected.eventRange.eventCount != projected.harmonicRhythm.eventCount) {
      result.status = PhraseHarmonicClockProjectionStatus::TimelineFailure;
      return result;
    }
  }

  result.status = PhraseHarmonicClockProjectionStatus::Ok;
  return result;
}

static_assert(std::is_trivially_copyable<PhraseHarmonicBarProjection>::value,
              "H2 bar projection must remain fixed-capacity");
static_assert(std::is_trivially_copyable<PhraseHarmonicClockProjection>::value,
              "H2 phrase projection must remain fixed-capacity");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_HARMONIC_CLOCK_PROJECTION_H
