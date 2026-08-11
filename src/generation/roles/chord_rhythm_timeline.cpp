#include "chord_rhythm_timeline.h"

namespace GroovePuterRhythm {
namespace {

uint8_t countOnsets(ChordRhythmTimelineMask mask) {
  uint8_t count = 0;
  while (mask != 0) {
    count = static_cast<uint8_t>(count + (mask & 1u));
    mask >>= 1u;
  }
  return count;
}

bool masksOverlap(const ChordRhythmTimelineRequest& request) {
  return (request.onsets & request.continuations) != 0 ||
         (request.onsets & request.releasePoints) != 0 ||
         (request.continuations & request.releasePoints) != 0;
}

bool validBounds(const ChordRhythmTimelineRequest& request) {
  const ChordRhythmTimelineMask active =
      chordRhythmTimelineActiveMask(request.barCount);
  if (active == 0) return false;
  const ChordRhythmTimelineMask used =
      request.onsets | request.continuations | request.releasePoints;
  return (used & ~active) == 0 && !masksOverlap(request);
}

bool validGateTopology(const ChordRhythmTimelineRequest& request) {
  const uint8_t stepCount =
      static_cast<uint8_t>(request.barCount * kStepsPerBar);
  bool gateActive = false;
  for (uint8_t step = 0; step < stepCount; ++step) {
    const ChordRhythmTimelineMask bit = chordRhythmTimelineStepBit(step);
    if ((request.onsets & bit) != 0) {
      gateActive = true;
      continue;
    }
    if ((request.continuations & bit) != 0) {
      if (!gateActive) return false;
      continue;
    }
    if ((request.releasePoints & bit) != 0) {
      if (!gateActive) return false;
      gateActive = false;
      continue;
    }
    gateActive = false;
  }
  return true;
}

}  // namespace

ChordRhythmTimelinePlan realizeChordRhythmTimeline(
    const ChordRhythmTimelineRequest& request) {
  ChordRhythmTimelinePlan plan{};
  if (!validBounds(request)) return plan;
  if (!validGateTopology(request)) {
    plan.status = ChordRhythmTimelineStatus::InvalidTopology;
    return plan;
  }

  plan.barCount = request.barCount;
  plan.stepCount = static_cast<uint8_t>(request.barCount * kStepsPerBar);
  plan.onsets = request.onsets;
  plan.continuations = request.continuations;
  plan.releasePoints = request.releasePoints;
  plan.onsetCount = countOnsets(request.onsets);

  if ((request.onsets | request.continuations | request.releasePoints) == 0) {
    plan.status = ChordRhythmTimelineStatus::ValidButEmpty;
    return plan;
  }

  plan.status = ChordRhythmTimelineStatus::Ok;
  return plan;
}

}  // namespace GroovePuterRhythm

extern "C" uint8_t grooveputerP2ChordRhythmTimelineProbe() {
  GroovePuterRhythm::ChordRhythmTimelineRequest request{};
  request.barCount = 4;
  for (uint8_t step = 0; step < 64; step = static_cast<uint8_t>(step + 4))
    request.onsets |= GroovePuterRhythm::chordRhythmTimelineStepBit(step);
  const GroovePuterRhythm::ChordRhythmTimelinePlan plan =
      GroovePuterRhythm::realizeChordRhythmTimeline(request);
  return plan.status == GroovePuterRhythm::ChordRhythmTimelineStatus::Ok
      ? plan.onsetCount
      : 0;
}
