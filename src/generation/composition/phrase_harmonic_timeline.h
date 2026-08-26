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
  Count,
};

// WHEN-only phrase harmonic timeline. Each StepMask stores positions 0..15 for
// one global semantic phrase bar using the repository's established stepBit()
// convention. It deliberately stores no ChordProgression HarmonicEvent values
// and therefore does not own WHAT.
struct PhraseHarmonicTimeline {
  PhraseHarmonicTimelineStatus status =
      PhraseHarmonicTimelineStatus::InvalidPhraseLength;
  uint8_t phraseBars = 0;
  uint8_t totalEventPositions = 0;
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
  result.status = PhraseHarmonicTimelineStatus::Ok;
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

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_HARMONIC_TIMELINE_H
