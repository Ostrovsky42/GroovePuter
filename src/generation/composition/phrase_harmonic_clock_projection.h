#ifndef GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_HARMONIC_CLOCK_PROJECTION_H
#define GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_HARMONIC_CLOCK_PROJECTION_H

#include <cstdint>
#include <type_traits>

#include "phrase_harmonic_timeline.h"
#include "phrase_harmonic_policy.h"
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
    ProgressionId progression,
    PhraseHarmonicPolicyId policy = PhraseHarmonicPolicyId::HalfBar) {
  PhraseHarmonicClockProjection result{};
  if (!isSupportedPhraseLength(phraseBars) ||
      !isValidProgressionId(progression, false) ||
      static_cast<uint8_t>(policy) >=
          static_cast<uint8_t>(PhraseHarmonicPolicyId::Count)) {
    return result;
  }

  // Non-F08 policies construct phrase events directly. Do not ask the legacy
  // one-bar materializer for views that would then be discarded.
  if (policy != PhraseHarmonicPolicyId::HalfBar) {
    PhraseHarmonicEvent events[kMaxPhraseHarmonicEventPositions]{};
    uint8_t eventCount = 0;
    switch (policy) {
      case PhraseHarmonicPolicyId::Static:
        events[0] = {0, 0, static_cast<uint8_t>(phraseBars * kStepsPerBar)};
        eventCount = 1;
        break;
      case PhraseHarmonicPolicyId::Slow:
        if (phraseBars != 4) return result;
        for (uint8_t bar = 0; bar < 4; ++bar) {
          events[eventCount++] = {
              bar, static_cast<uint8_t>(bar * kStepsPerBar), kStepsPerBar};
        }
        break;
      case PhraseHarmonicPolicyId::Prolong:
        if (phraseBars != 4) return result;
        events[0] = {0, 0, 32};
        events[1] = {1, 32, 16};
        events[2] = {2, 48, 16};
        eventCount = 3;
        break;
      case PhraseHarmonicPolicyId::HalfBar:
      case PhraseHarmonicPolicyId::Syncopated:
      case PhraseHarmonicPolicyId::Count:
        return result;
    }

    result.phraseBars = phraseBars;
    result.timeline = makePhraseHarmonicTimeline(phraseBars, events, eventCount);
    if (!validatePhraseHarmonicTimeline(result.timeline)) {
      result.status = PhraseHarmonicClockProjectionStatus::TimelineFailure;
      return result;
    }
    for (uint8_t bar = 0; bar < phraseBars; ++bar) {
      const PhraseHarmonicBarMaterialization materialization =
          projectPhraseHarmonicBarMaterialization(result.timeline, bar);
      if (materialization.segmentCount == 0 ||
          materialization.segmentCount > kMaxHarmonicEvents) {
        result.status = PhraseHarmonicClockProjectionStatus::TimelineFailure;
        return result;
      }
      PhraseHarmonicBarProjection& projected = result.bars[bar];
      projected.phraseBarOrdinal = bar;
      projected.harmonicRhythm.progression = progression;
      projected.harmonicRhythm.onsets = materialization.segmentOnsets;
      projected.harmonicRhythm.eventCount = materialization.segmentCount;
      projected.harmonicRhythm.phraseBarOrdinal = bar;
      projected.harmonicRhythm.phraseHarmonicPosition =
          materialization.sourceOrdinals[0];
      projected.eventRange = phraseHarmonicEventRangeForBar(result.timeline, bar);
    }
    result.status = PhraseHarmonicClockProjectionStatus::Ok;
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

// Derive the physical one-bar harmonic clock view from the canonical phrase
// timeline. Carry-in contributes a local anchor while retaining the source
// ordinal of the already-active phrase event.
inline HarmonicRhythmPlan projectPhraseHarmonicRhythmForBar(
    const PhraseHarmonicTimeline& timeline,
    ProgressionId progression,
    uint8_t phraseBarOrdinal) {
  HarmonicRhythmPlan result{};
  if (!validatePhraseHarmonicTimeline(timeline) ||
      !isValidProgressionId(progression, false) ||
      phraseBarOrdinal >= timeline.phraseBars) return result;
  const PhraseHarmonicBarMaterialization bar =
      projectPhraseHarmonicBarMaterialization(timeline, phraseBarOrdinal);
  if (bar.segmentCount == 0 || bar.segmentCount > kMaxHarmonicEvents) return result;
  result.progression = progression;
  result.onsets = bar.segmentOnsets;
  result.eventCount = bar.segmentCount;
  result.phraseBarOrdinal = phraseBarOrdinal;
  result.phraseHarmonicPosition = bar.sourceOrdinals[0];
  return result;
}

inline ChordProgressionResult materializePhraseHarmonicProgression(
    const ChordProgressionSource& source,
    const PhraseHarmonicBarMaterialization& bar) {
  ChordProgressionResult result{};
  if (source.period == 0 || bar.segmentCount == 0 ||
      bar.segmentCount > kMaxHarmonicEvents ||
      phraseHarmonicPositionCount(bar.segmentOnsets) != bar.segmentCount) {
    return result;
  }
  ChordProgressionPlan plan{};
  plan.id = source.id;
  plan.eventCount = bar.segmentCount;
  ChordProgressionStatus status = ChordProgressionStatus::InvalidRequest;
  for (uint8_t ordinal = 0; ordinal < bar.segmentCount; ++ordinal) {
    const ChordProgressionEventResult event = chordProgressionEventAt(
        source, bar.sourceOrdinals[ordinal]);
    if (event.status != ChordProgressionStatus::Ok &&
        event.status != ChordProgressionStatus::ValidButStatic) return result;
    if (ordinal != 0 && event.status != status) return result;
    status = event.status;
    plan.events[ordinal] = event.event;
  }
  result.status = status;
  result.plan = plan;
  return result;
}

static_assert(std::is_trivially_copyable<PhraseHarmonicBarProjection>::value,
              "H2 bar projection must remain fixed-capacity");
static_assert(std::is_trivially_copyable<PhraseHarmonicClockProjection>::value,
              "H2 phrase projection must remain fixed-capacity");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_HARMONIC_CLOCK_PROJECTION_H
