#include "chord_rhythm_retrigger.h"

namespace GroovePuterRhythm {
namespace {

uint8_t countOnsets(StepMask mask) {
  uint8_t count = 0;
  while (mask != 0) {
    count = static_cast<uint8_t>(count + (mask & 1u));
    mask = static_cast<StepMask>(mask >> 1u);
  }
  return count;
}

bool masksOverlap(const ChordRhythmRetriggerRequest& request) {
  const StepMask masks[] = {
      request.sourceAdvanceOnsets,
      request.sameChordRetriggers,
      request.continuations,
      request.releasePoints,
  };
  for (uint8_t a = 0; a < 4; ++a) {
    for (uint8_t b = static_cast<uint8_t>(a + 1u); b < 4; ++b) {
      if ((masks[a] & masks[b]) != 0) return true;
    }
  }
  return false;
}

bool hasOrphanRetrigger(const ChordRhythmRetriggerRequest& request) {
  bool sourceKnown = request.sourceAvailableAtStart;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    const StepMask bit = stepBit(step);
    if ((request.sourceAdvanceOnsets & bit) != 0) sourceKnown = true;
    if ((request.sameChordRetriggers & bit) != 0 && !sourceKnown) return true;
  }
  return false;
}

bool validGateTopology(const ChordRhythmRetriggerRequest& request) {
  const StepMask audible = static_cast<StepMask>(
      request.sourceAdvanceOnsets | request.sameChordRetriggers);
  bool gateActive = false;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    const StepMask bit = stepBit(step);
    if ((audible & bit) != 0) {
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

void initializeOrdinals(ChordRhythmRetriggerPlan& plan) {
  for (uint8_t step = 0; step < kStepsPerBar; ++step)
    plan.sourceOrdinalByStep[step] = kNoChordRhythmSourceOrdinal;
}

}  // namespace

ChordRhythmRetriggerPlan realizeChordRhythmRetriggers(
    const ChordRhythmRetriggerRequest& request) {
  ChordRhythmRetriggerPlan plan{};
  initializeOrdinals(plan);

  if (masksOverlap(request)) {
    plan.status = ChordRhythmRetriggerStatus::InvalidOverlap;
    return plan;
  }
  if (hasOrphanRetrigger(request)) {
    plan.status = ChordRhythmRetriggerStatus::OrphanRetrigger;
    return plan;
  }
  if (!validGateTopology(request)) {
    plan.status = ChordRhythmRetriggerStatus::InvalidGateTopology;
    return plan;
  }

  const StepMask audible = static_cast<StepMask>(
      request.sourceAdvanceOnsets | request.sameChordRetriggers);
  const StepMask used = static_cast<StepMask>(
      audible | request.continuations | request.releasePoints);
  if (used == 0) {
    plan.status = ChordRhythmRetriggerStatus::ValidButEmpty;
    return plan;
  }

  plan.sourceAdvanceOnsets = request.sourceAdvanceOnsets;
  plan.sameChordRetriggers = request.sameChordRetriggers;
  plan.audibleOnsets = audible;
  plan.continuations = request.continuations;
  plan.releasePoints = request.releasePoints;
  plan.sourceAdvanceCount = countOnsets(request.sourceAdvanceOnsets);
  plan.audibleOnsetCount = countOnsets(audible);

  uint8_t currentSourceOrdinal = request.sourceAvailableAtStart
      ? kIncomingChordRhythmSourceOrdinal
      : kNoChordRhythmSourceOrdinal;
  uint8_t nextLocalSourceOrdinal = 0;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    const StepMask bit = stepBit(step);
    if ((request.sourceAdvanceOnsets & bit) != 0) {
      currentSourceOrdinal = nextLocalSourceOrdinal;
      nextLocalSourceOrdinal = static_cast<uint8_t>(nextLocalSourceOrdinal + 1u);
      plan.sourceOrdinalByStep[step] = currentSourceOrdinal;
    } else if ((request.sameChordRetriggers & bit) != 0) {
      plan.sourceOrdinalByStep[step] = currentSourceOrdinal;
    }
  }

  plan.status = ChordRhythmRetriggerStatus::Ok;
  return plan;
}

}  // namespace GroovePuterRhythm

extern "C" uint8_t grooveputerP3SameChordRetriggerProbe() {
  GroovePuterRhythm::ChordRhythmRetriggerRequest request{};
  request.sourceAdvanceOnsets = static_cast<GroovePuterRhythm::StepMask>(
      GroovePuterRhythm::stepBit(0) | GroovePuterRhythm::stepBit(8));
  request.sameChordRetriggers = static_cast<GroovePuterRhythm::StepMask>(
      GroovePuterRhythm::stepBit(4) | GroovePuterRhythm::stepBit(12));
  const GroovePuterRhythm::ChordRhythmRetriggerPlan plan =
      GroovePuterRhythm::realizeChordRhythmRetriggers(request);
  return plan.status == GroovePuterRhythm::ChordRhythmRetriggerStatus::Ok
      ? plan.audibleOnsetCount
      : 0;
}
