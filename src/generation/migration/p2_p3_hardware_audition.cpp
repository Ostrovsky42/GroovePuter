#include "p2_p3_hardware_audition.h"

#include "../roles/chord_rhythm_retrigger.h"
#include "../roles/chord_rhythm_timeline.h"

namespace GroovePuterRhythm {
namespace {

using TimelineMask = ChordRhythmTimelineMask;

void setTimelineStep(TimelineMask& mask, uint8_t step) {
  mask |= chordRhythmTimelineStepBit(step);
}

StepMask timelineBarMask(TimelineMask mask, uint8_t barIndex) {
  StepMask result = 0;
  const uint8_t first = static_cast<uint8_t>(barIndex * kStepsPerBar);
  for (uint8_t local = 0; local < kStepsPerBar; ++local) {
    const uint8_t timelineStep = static_cast<uint8_t>(first + local);
    if ((mask & chordRhythmTimelineStepBit(timelineStep)) != 0)
      result = static_cast<StepMask>(result | stepBit(local));
  }
  return result;
}

bool buildFixture(P2P3HardwareAuditionFixture fixture,
                  uint8_t& barCount,
                  TimelineMask& sourceAdvance,
                  TimelineMask& retrigger,
                  TimelineMask& continuations,
                  TimelineMask& releases) {
  barCount = 0;
  sourceAdvance = 0;
  retrigger = 0;
  continuations = 0;
  releases = 0;

  switch (fixture) {
    case P2P3HardwareAuditionFixture::Retrigger:
      barCount = 1;
      setTimelineStep(sourceAdvance, 0);
      setTimelineStep(retrigger, 4);
      setTimelineStep(sourceAdvance, 8);
      setTimelineStep(retrigger, 12);
      return true;

    case P2P3HardwareAuditionFixture::DenseRetrigger:
      barCount = 1;
      setTimelineStep(sourceAdvance, 0);
      for (uint8_t step = 1; step < 16; ++step)
        setTimelineStep(retrigger, step);
      return true;

    case P2P3HardwareAuditionFixture::CrossBarHold:
      barCount = 2;
      setTimelineStep(sourceAdvance, 12);
      for (uint8_t step = 13; step <= 19; ++step)
        setTimelineStep(continuations, step);
      setTimelineStep(releases, 20);
      return true;

    case P2P3HardwareAuditionFixture::MultiBarNS:
      barCount = 4;
      // Bar 0: local N0,S4,N8,S12.
      setTimelineStep(sourceAdvance, 0);
      setTimelineStep(retrigger, 4);
      setTimelineStep(sourceAdvance, 8);
      setTimelineStep(retrigger, 12);
      // Bar 1 deliberately begins with S against the incoming source.
      setTimelineStep(retrigger, 16);
      setTimelineStep(sourceAdvance, 24);
      setTimelineStep(retrigger, 28);
      // Bar 2.
      setTimelineStep(sourceAdvance, 32);
      setTimelineStep(retrigger, 36);
      setTimelineStep(sourceAdvance, 40);
      setTimelineStep(retrigger, 44);
      // Bar 3 again begins with incoming-source S.
      setTimelineStep(retrigger, 48);
      setTimelineStep(sourceAdvance, 56);
      setTimelineStep(retrigger, 60);
      return true;

    case P2P3HardwareAuditionFixture::Count:
      break;
  }
  return false;
}

void initializeSourceOrdinals(P2P3HardwareAuditionPlan& plan) {
  for (uint8_t bar = 0; bar < kMaxPhraseBars; ++bar) {
    for (uint8_t step = 0; step < kStepsPerBar; ++step)
      plan.bars[bar].sourceOrdinalByStep[step] =
          kNoChordRhythmSourceOrdinal;
  }
}

}  // namespace

P2P3HardwareAuditionPlan realizeP2P3HardwareAudition(
    P2P3HardwareAuditionFixture fixture) {
  P2P3HardwareAuditionPlan result{};
  result.fixture = fixture;
  initializeSourceOrdinals(result);

  uint8_t barCount = 0;
  TimelineMask sourceAdvance = 0;
  TimelineMask retrigger = 0;
  TimelineMask continuations = 0;
  TimelineMask releases = 0;
  if (!buildFixture(fixture, barCount, sourceAdvance, retrigger,
                    continuations, releases)) {
    return result;
  }

  ChordRhythmTimelineRequest timelineRequest{};
  timelineRequest.barCount = barCount;
  timelineRequest.onsets = sourceAdvance | retrigger;
  timelineRequest.continuations = continuations;
  timelineRequest.releasePoints = releases;
  const ChordRhythmTimelinePlan timeline =
      realizeChordRhythmTimeline(timelineRequest);
  if (timeline.status != ChordRhythmTimelineStatus::Ok &&
      timeline.status != ChordRhythmTimelineStatus::ValidButEmpty) {
    result.status = P2P3HardwareAuditionStatus::TimelineRejected;
    return result;
  }

  result.barCount = barCount;
  uint8_t globalSourceBase = 0;
  uint8_t currentGlobalSource = kNoChordRhythmSourceOrdinal;
  bool sourceAvailable = false;

  for (uint8_t bar = 0; bar < barCount; ++bar) {
    P2P3HardwareAuditionBar& out = result.bars[bar];
    out.sourceAdvanceOnsets = timelineBarMask(sourceAdvance, bar);
    out.sameChordRetriggers = timelineBarMask(retrigger, bar);
    out.audibleOnsets = static_cast<StepMask>(
        out.sourceAdvanceOnsets | out.sameChordRetriggers);
    out.continuations = timelineBarMask(continuations, bar);
    out.releasePoints = timelineBarMask(releases, bar);

    // P2 is the sole gate-topology validator for the composed multi-bar
    // phrase. P3 is asked only to resolve N/S source identity, so it cannot
    // become a second continuation/release owner at a bar boundary.
    ChordRhythmRetriggerRequest retriggerRequest{};
    retriggerRequest.sourceAdvanceOnsets = out.sourceAdvanceOnsets;
    retriggerRequest.sameChordRetriggers = out.sameChordRetriggers;
    retriggerRequest.sourceAvailableAtStart = sourceAvailable;
    const ChordRhythmRetriggerPlan retriggerPlan =
        realizeChordRhythmRetriggers(retriggerRequest);
    if (retriggerPlan.status != ChordRhythmRetriggerStatus::Ok &&
        retriggerPlan.status != ChordRhythmRetriggerStatus::ValidButEmpty) {
      result.status = P2P3HardwareAuditionStatus::RetriggerRejected;
      return result;
    }

    for (uint8_t step = 0; step < kStepsPerBar; ++step) {
      const uint8_t local = retriggerPlan.sourceOrdinalByStep[step];
      if (local == kNoChordRhythmSourceOrdinal) continue;
      if (local == kIncomingChordRhythmSourceOrdinal) {
        out.sourceOrdinalByStep[step] = currentGlobalSource;
      } else {
        out.sourceOrdinalByStep[step] =
            static_cast<uint8_t>(globalSourceBase + local);
      }
    }

    if (retriggerPlan.sourceAdvanceCount > 0) {
      globalSourceBase = static_cast<uint8_t>(
          globalSourceBase + retriggerPlan.sourceAdvanceCount);
      currentGlobalSource = static_cast<uint8_t>(globalSourceBase - 1u);
      sourceAvailable = true;
    }
  }

  result.sourceAdvanceCount = globalSourceBase;
  result.status = P2P3HardwareAuditionStatus::Ok;
  return result;
}

const char* p2P3HardwareAuditionFixtureName(
    P2P3HardwareAuditionFixture fixture) {
  switch (fixture) {
    case P2P3HardwareAuditionFixture::Retrigger: return "RETRIGGER";
    case P2P3HardwareAuditionFixture::DenseRetrigger: return "DENSE_RETRIGGER";
    case P2P3HardwareAuditionFixture::CrossBarHold: return "CROSS_BAR_HOLD";
    case P2P3HardwareAuditionFixture::MultiBarNS: return "MULTI_BAR_NS";
    case P2P3HardwareAuditionFixture::Count: break;
  }
  return "INVALID";
}

}  // namespace GroovePuterRhythm
