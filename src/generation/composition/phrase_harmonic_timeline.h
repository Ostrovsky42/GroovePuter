#ifndef GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_HARMONIC_TIMELINE_H
#define GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_HARMONIC_TIMELINE_H

#include <cstdint>
#include <type_traits>

#include "../rhythm/rhythm_types.h"
#include "phrase_length_request.h"

namespace GroovePuterRhythm {

// Separate from rhythm_types.h::kMaxPhraseBars (the existing four-bar rhythm
// vocabulary capability). This is the converged semantic phrase capacity.
constexpr uint8_t kMaxSemanticPhraseBars = 8;
constexpr uint8_t kMaxHarmonicEventPositionsPerBar = 4;
constexpr uint8_t kMaxPhraseHarmonicEventPositions =
    kMaxSemanticPhraseBars * kMaxHarmonicEventPositionsPerBar;

struct PhraseHarmonicEvent {
  uint8_t sourceOrdinal = 0;
  uint8_t onsetStep = 0;
  uint8_t durationSteps = 0;
};

constexpr uint8_t kMaxHarmonicSegmentsPerBar =
    kMaxHarmonicEventPositionsPerBar + 1;

// Derived physical window. A carry-in anchor reuses the phrase event's WHAT
// source ordinal; it is never counted as a phrase harmonic change.
struct PhraseHarmonicBarMaterialization {
  StepMask segmentOnsets = 0;
  uint8_t segmentCount = 0;
  uint8_t sourceOrdinals[kMaxHarmonicSegmentsPerBar]{};
  bool entersFromPreviousBar = false;
};

struct PhraseHarmonicEventRange {
  uint8_t firstOrdinal = 0;
  uint8_t eventCount = 0;
};

struct PhraseHarmonicEventCoordinate {
  bool valid = false;
  uint8_t phraseHarmonicEventOrdinal = 0;
  uint8_t localStep = 0;
};

enum class PhraseHarmonicTimelineStatus : uint8_t {
  Ok = 0,
  InvalidPhraseLength,
  TooManyEventPositionsInBar,
  InvalidEvents,
  Count,
};

// Canonical WHEN owner. Events refer to the progression source without owning
// its HarmonicEvent values. Masks index actual phrase event starts only.
struct PhraseHarmonicTimeline {
  PhraseHarmonicTimelineStatus status =
      PhraseHarmonicTimelineStatus::InvalidPhraseLength;
  uint8_t phraseBars = 0;
  uint8_t totalEventPositions = 0;
  PhraseHarmonicEvent events[kMaxPhraseHarmonicEventPositions]{};
  StepMask eventPositionsByBar[kMaxSemanticPhraseBars]{};
};

constexpr uint8_t phraseHarmonicPositionCount(StepMask positions) {
  uint8_t count = 0;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if ((positions & stepBit(step)) != 0) ++count;
  }
  return count;
}

inline PhraseHarmonicTimeline makePhraseHarmonicTimeline(
    uint8_t phraseBars,
    const PhraseHarmonicEvent* events,
    uint8_t eventCount) {
  PhraseHarmonicTimeline result{};
  if (!isSupportedPhraseLength(phraseBars)) return result;
  result.phraseBars = phraseBars;
  result.status = PhraseHarmonicTimelineStatus::InvalidEvents;
  if (events == nullptr || eventCount == 0 ||
      eventCount > kMaxPhraseHarmonicEventPositions) return result;

  const uint16_t phraseSteps = static_cast<uint16_t>(phraseBars) * kStepsPerBar;
  uint16_t nextStep = 0;
  for (uint8_t i = 0; i < eventCount; ++i) {
    const PhraseHarmonicEvent& event = events[i];
    const uint16_t end = static_cast<uint16_t>(event.onsetStep) + event.durationSteps;
    if (event.durationSteps == 0 || event.onsetStep >= phraseSteps ||
        event.onsetStep != nextStep || end > phraseSteps) return result;
    const uint8_t bar = event.onsetStep / kStepsPerBar;
    result.eventPositionsByBar[bar] = static_cast<StepMask>(
        result.eventPositionsByBar[bar] | stepBit(event.onsetStep % kStepsPerBar));
    if (phraseHarmonicPositionCount(result.eventPositionsByBar[bar]) >
        kMaxHarmonicEventPositionsPerBar) {
      result.status = PhraseHarmonicTimelineStatus::TooManyEventPositionsInBar;
      return result;
    }
    result.events[i] = event;
    nextStep = end;
  }
  if (nextStep != phraseSteps) return result;
  result.totalEventPositions = eventCount;
  result.status = PhraseHarmonicTimelineStatus::Ok;
  return result;
}

inline PhraseHarmonicTimeline makePhraseHarmonicTimeline(
    uint8_t phraseBars,
    const StepMask (&eventPositionsByBar)[kMaxSemanticPhraseBars]) {
  PhraseHarmonicTimeline result{};
  if (!isSupportedPhraseLength(phraseBars)) return result;
  result.phraseBars = phraseBars;
  result.totalEventPositions = 0;
  for (uint8_t bar = 0; bar < phraseBars; ++bar) {
    const StepMask positions = eventPositionsByBar[bar];
    const uint8_t count = phraseHarmonicPositionCount(positions);
    if (count > kMaxHarmonicEventPositionsPerBar) {
      result.status = PhraseHarmonicTimelineStatus::TooManyEventPositionsInBar;
      result.totalEventPositions = 0;
      return result;
    }
    result.eventPositionsByBar[bar] = positions;
    result.totalEventPositions =
        static_cast<uint8_t>(result.totalEventPositions + count);
  }
  // Compatibility input: turn positions into contiguous explicit intervals.
  // Empty bars are supported when the preceding phrase event carries through.
  PhraseHarmonicEvent events[kMaxPhraseHarmonicEventPositions]{};
  uint8_t ordinal = 0;
  for (uint8_t bar = 0; bar < phraseBars; ++bar) {
    for (uint8_t step = 0; step < kStepsPerBar; ++step) {
      if ((eventPositionsByBar[bar] & stepBit(step)) == 0) continue;
      events[ordinal] = {ordinal, static_cast<uint8_t>(bar * kStepsPerBar + step), 0};
      if (ordinal > 0) {
        events[ordinal - 1].durationSteps = static_cast<uint8_t>(
            events[ordinal].onsetStep - events[ordinal - 1].onsetStep);
      }
      ++ordinal;
    }
  }
  if (ordinal > 0) {
    events[ordinal - 1].durationSteps = static_cast<uint8_t>(
        phraseBars * kStepsPerBar - events[ordinal - 1].onsetStep);
  }
  return makePhraseHarmonicTimeline(phraseBars, events, ordinal);
}

inline bool validatePhraseHarmonicTimeline(const PhraseHarmonicTimeline& timeline) {
  if (timeline.status != PhraseHarmonicTimelineStatus::Ok) return false;
  const auto checked = makePhraseHarmonicTimeline(
      timeline.phraseBars, timeline.events, timeline.totalEventPositions);
  if (checked.status != PhraseHarmonicTimelineStatus::Ok) return false;
  for (uint8_t bar = 0; bar < kMaxSemanticPhraseBars; ++bar) {
    if (checked.eventPositionsByBar[bar] != timeline.eventPositionsByBar[bar]) return false;
  }
  return true;
}

inline const PhraseHarmonicEvent* activePhraseHarmonicEventAt(
    const PhraseHarmonicTimeline& timeline, uint8_t phraseStep) {
  if (timeline.status != PhraseHarmonicTimelineStatus::Ok ||
      timeline.totalEventPositions > kMaxPhraseHarmonicEventPositions ||
      phraseStep >= timeline.phraseBars * kStepsPerBar) return nullptr;
  for (uint8_t i = 0; i < timeline.totalEventPositions; ++i) {
    const auto& event = timeline.events[i];
    if (phraseStep >= event.onsetStep &&
        phraseStep < static_cast<uint16_t>(event.onsetStep) + event.durationSteps) return &event;
  }
  return nullptr;
}

inline PhraseHarmonicBarMaterialization projectPhraseHarmonicBarMaterialization(
    const PhraseHarmonicTimeline& timeline, uint8_t phraseBarOrdinal) {
  PhraseHarmonicBarMaterialization result{};
  if (!validatePhraseHarmonicTimeline(timeline) || phraseBarOrdinal >= timeline.phraseBars) return result;
  const uint8_t barStart = phraseBarOrdinal * kStepsPerBar;
  const auto* active = activePhraseHarmonicEventAt(timeline, barStart);
  if (active == nullptr) return result;
  result.segmentOnsets = stepBit(0);
  result.sourceOrdinals[0] = active->sourceOrdinal;
  result.segmentCount = 1;
  result.entersFromPreviousBar = active->onsetStep < barStart;
  for (uint8_t i = 0; i < timeline.totalEventPositions; ++i) {
    const auto& event = timeline.events[i];
    if (event.onsetStep <= barStart || event.onsetStep >= barStart + kStepsPerBar) continue;
    result.segmentOnsets = static_cast<StepMask>(result.segmentOnsets | stepBit(event.onsetStep - barStart));
    result.sourceOrdinals[result.segmentCount++] = event.sourceOrdinal;
  }
  return result;
}

constexpr PhraseHarmonicEventRange phraseHarmonicEventRangeForBar(
    const PhraseHarmonicTimeline& timeline,
    uint8_t phraseBarOrdinal) {
  PhraseHarmonicEventRange range{};
  if (timeline.status != PhraseHarmonicTimelineStatus::Ok ||
      phraseBarOrdinal >= timeline.phraseBars) {
    return range;
  }
  for (uint8_t bar = 0; bar < phraseBarOrdinal; ++bar) {
    range.firstOrdinal = static_cast<uint8_t>(
        range.firstOrdinal +
        phraseHarmonicPositionCount(timeline.eventPositionsByBar[bar]));
  }
  range.eventCount =
      phraseHarmonicPositionCount(timeline.eventPositionsByBar[phraseBarOrdinal]);
  return range;
}

constexpr PhraseHarmonicEventCoordinate phraseHarmonicEventCoordinate(
    const PhraseHarmonicTimeline& timeline,
    uint8_t phraseBarOrdinal,
    uint8_t localHarmonicEventOrdinal) {
  PhraseHarmonicEventCoordinate result{};
  const PhraseHarmonicEventRange range =
      phraseHarmonicEventRangeForBar(timeline, phraseBarOrdinal);
  if (localHarmonicEventOrdinal >= range.eventCount) return result;

  uint8_t seen = 0;
  const StepMask positions = timeline.eventPositionsByBar[phraseBarOrdinal];
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if ((positions & stepBit(step)) == 0) continue;
    if (seen == localHarmonicEventOrdinal) {
      result.valid = true;
      result.localStep = step;
      result.phraseHarmonicEventOrdinal = static_cast<uint8_t>(
          range.firstOrdinal + localHarmonicEventOrdinal);
      return result;
    }
    ++seen;
  }
  return result;
}

static_assert(kMaxPhraseHarmonicEventPositions == 32,
              "phrase harmonic time capacity must remain 32 event positions");
static_assert(std::is_trivially_copyable<PhraseHarmonicTimeline>::value,
              "harmonic timeline must remain fixed-capacity");
static_assert(std::is_trivially_copyable<PhraseHarmonicBarMaterialization>::value,
              "bar materialization must remain fixed-capacity");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_HARMONIC_TIMELINE_H
