#ifndef GROOVEPUTER_GENERATION_ROLES_CHORD_RHYTHM_TIMELINE_H
#define GROOVEPUTER_GENERATION_ROLES_CHORD_RHYTHM_TIMELINE_H

#include <cstdint>
#include <type_traits>

#include "../rhythm/rhythm_types.h"

namespace GroovePuterRhythm {

// P2 capability boundary. It is deliberately separate from the established
// one-bar ChordRhythmPlan so live Stage15 remains unchanged while the exact
// onset/continuation/release topology is lifted to a bounded phrase timeline.
using ChordRhythmTimelineMask = uint64_t;

constexpr uint8_t kMaxChordRhythmTimelineBars = kMaxPhraseBars;
constexpr uint8_t kMaxChordRhythmTimelineSteps =
    static_cast<uint8_t>(kStepsPerBar * kMaxChordRhythmTimelineBars);

// Match StepMask: logical step 0 is the most significant active bit.
constexpr ChordRhythmTimelineMask chordRhythmTimelineStepBit(uint8_t step) {
  return step < kMaxChordRhythmTimelineSteps
      ? (ChordRhythmTimelineMask{1} <<
         (kMaxChordRhythmTimelineSteps - 1u - step))
      : ChordRhythmTimelineMask{0};
}

constexpr ChordRhythmTimelineMask chordRhythmTimelineActiveMask(uint8_t bars) {
  if (bars == 0 || bars > kMaxChordRhythmTimelineBars) return 0;
  const uint8_t steps = static_cast<uint8_t>(bars * kStepsPerBar);
  return steps == kMaxChordRhythmTimelineSteps
      ? ~ChordRhythmTimelineMask{0}
      : static_cast<ChordRhythmTimelineMask>(
            ~ChordRhythmTimelineMask{0} <<
            (kMaxChordRhythmTimelineSteps - steps));
}

constexpr ChordRhythmTimelineMask chordRhythmTimelineFromBar(
    StepMask barMask, uint8_t barIndex) {
  if (barIndex >= kMaxChordRhythmTimelineBars) return 0;
  ChordRhythmTimelineMask result = 0;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if ((barMask & stepBit(step)) != 0) {
      const uint8_t timelineStep =
          static_cast<uint8_t>(barIndex * kStepsPerBar + step);
      result |= chordRhythmTimelineStepBit(timelineStep);
    }
  }
  return result;
}

enum class ChordRhythmTimelineStatus : uint8_t {
  Ok = 0,
  ValidButEmpty,
  InvalidRequest,
  InvalidTopology,
  Count,
};

struct ChordRhythmTimelineRequest {
  ChordRhythmTimelineMask onsets = 0;
  ChordRhythmTimelineMask continuations = 0;
  ChordRhythmTimelineMask releasePoints = 0;
  uint8_t barCount = 1;
};

struct ChordRhythmTimelinePlan {
  ChordRhythmTimelineMask onsets = 0;
  ChordRhythmTimelineMask continuations = 0;
  ChordRhythmTimelineMask releasePoints = 0;
  ChordRhythmTimelineStatus status = ChordRhythmTimelineStatus::InvalidRequest;
  uint8_t barCount = 0;
  uint8_t stepCount = 0;
  uint8_t onsetCount = 0;
};

ChordRhythmTimelinePlan realizeChordRhythmTimeline(
    const ChordRhythmTimelineRequest& request);

static_assert(kMaxChordRhythmTimelineBars == 4,
              "P2 bounded ChordRhythm phrase length changed");
static_assert(kMaxChordRhythmTimelineSteps == 64,
              "P2 timeline no longer fits uint64_t");
static_assert(std::is_trivially_copyable<ChordRhythmTimelineRequest>::value,
              "P2 timeline request must remain fixed-capacity");
static_assert(std::is_trivially_copyable<ChordRhythmTimelinePlan>::value,
              "P2 timeline plan must remain fixed-capacity");
static_assert(sizeof(ChordRhythmTimelineRequest) <= 32,
              "P2 timeline request exceeded its budget");
static_assert(sizeof(ChordRhythmTimelinePlan) <= 32,
              "P2 timeline plan exceeded its budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_ROLES_CHORD_RHYTHM_TIMELINE_H
