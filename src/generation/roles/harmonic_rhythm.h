#ifndef GROOVEPUTER_GENERATION_ROLES_HARMONIC_RHYTHM_H
#define GROOVEPUTER_GENERATION_ROLES_HARMONIC_RHYTHM_H

#include <cstdint>
#include <type_traits>

#include "chord_progression.h"

namespace GroovePuterRhythm {

enum class HarmonicRhythmStatus : uint8_t {
  Ok = 0,
  InvalidRequest,
  Count,
};

// F08.1 vocabulary is intentionally small and semantic. These are harmonic
// clocks, not physical ChordRhythm articulation patterns.
enum class HarmonicClockId : uint8_t {
  StaticHold = 0,      // {0}
  HalfBarPivot,        // {0,8}
  QuarterCycle,        // {0,4,8,12}
  CadentialThree,      // {0,6,10}
  LateChange,          // {0,12}
  Count,
};

// HarmonicRhythm owns WHEN progression state advances inside the current
// physical bar. It deliberately has no ChordRhythm input: chord articulation
// may retrigger any number of times without becoming a harmonic clock.
//
// harmonicEventCount == 0 selects the bounded F08.1 progression vocabulary.
// A caller may still request another bounded count explicitly; that compatibility
// path remains evenly spaced and does not change the default vocabulary owner.
//
// phraseBarOrdinal / phraseHarmonicPosition remain carried temporal coordinates.
// F08.1 does not invent a scheduler or phrase-position policy merely to vary the
// one-bar clock; a later temporal owner may use those coordinates explicitly.
struct HarmonicRhythmRequest {
  ProgressionId progression = ProgressionId::Auto;
  uint8_t harmonicEventCount = 0;
  uint8_t phraseBarOrdinal = 0;
  uint8_t phraseHarmonicPosition = 0;
};

struct HarmonicRhythmPlan {
  ProgressionId progression = ProgressionId::Auto;
  StepMask onsets = 0;
  uint8_t eventCount = 0;
  uint8_t phraseBarOrdinal = 0;
  uint8_t phraseHarmonicPosition = 0;
};

struct HarmonicRhythmResult {
  HarmonicRhythmStatus status = HarmonicRhythmStatus::InvalidRequest;
  HarmonicRhythmPlan plan{};
};

inline bool isStaticHarmonicProgression(ProgressionId id) {
  return id == ProgressionId::StaticModal || id == ProgressionId::PedalDrone;
}

inline HarmonicClockId defaultOneBarHarmonicClock(ProgressionId id) {
  switch (id) {
    case ProgressionId::StaticModal:
    case ProgressionId::PedalDrone:
      return HarmonicClockId::StaticHold;

    // Four-stage cyclic/falling vocabularies are represented as four harmonic
    // states. This is progression structure, not chord attack density.
    case ProgressionId::PopCycle:
    case ProgressionId::MinorFall:
      return HarmonicClockId::QuarterCycle;

    // II-V-I is a three-stage function. The final tonic receives the longer
    // landing window instead of forcing an equal-grid synthetic clock.
    case ProgressionId::TwoFiveOne:
      return HarmonicClockId::CadentialThree;

    // Parallel color motion keeps the accepted F08 half-bar baseline until a
    // phrase-aware policy can justify a more specific temporal shape.
    case ProgressionId::ParallelShift:
      return HarmonicClockId::HalfBarPivot;

    // The borrowed-color family keeps two states but moves the second state late
    // in the bar, matching the reviewed "localized late change" pressure.
    case ProgressionId::BorrowedLift:
      return HarmonicClockId::LateChange;

    case ProgressionId::Auto:
    case ProgressionId::Count:
      return HarmonicClockId::Count;
  }
  return HarmonicClockId::Count;
}

inline StepMask harmonicClockOnsets(HarmonicClockId clock) {
  switch (clock) {
    case HarmonicClockId::StaticHold:
      return stepBit(0);
    case HarmonicClockId::HalfBarPivot:
      return static_cast<StepMask>(stepBit(0) | stepBit(8));
    case HarmonicClockId::QuarterCycle:
      return static_cast<StepMask>(
          stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12));
    case HarmonicClockId::CadentialThree:
      return static_cast<StepMask>(stepBit(0) | stepBit(6) | stepBit(10));
    case HarmonicClockId::LateChange:
      return static_cast<StepMask>(stepBit(0) | stepBit(12));
    case HarmonicClockId::Count:
      return 0;
  }
  return 0;
}

inline uint8_t harmonicClockEventCount(HarmonicClockId clock) {
  switch (clock) {
    case HarmonicClockId::StaticHold: return 1;
    case HarmonicClockId::HalfBarPivot: return 2;
    case HarmonicClockId::QuarterCycle: return 4;
    case HarmonicClockId::CadentialThree: return 3;
    case HarmonicClockId::LateChange: return 2;
    case HarmonicClockId::Count: return 0;
  }
  return 0;
}

inline uint8_t defaultOneBarHarmonicEventCount(ProgressionId id) {
  if (!isValidProgressionId(id, false)) return 0;
  return harmonicClockEventCount(defaultOneBarHarmonicClock(id));
}

inline StepMask evenlySpacedHarmonicOnsets(uint8_t eventCount) {
  if (eventCount == 0 || eventCount > kMaxHarmonicEvents) return 0;

  StepMask result = 0;
  for (uint8_t ordinal = 0; ordinal < eventCount; ++ordinal) {
    const uint8_t step = static_cast<uint8_t>(
        (static_cast<uint16_t>(ordinal) * kStepsPerBar) / eventCount);
    result = static_cast<StepMask>(result | stepBit(step));
  }
  return result;
}

inline HarmonicRhythmResult realizeHarmonicRhythm(
    const HarmonicRhythmRequest& request) {
  HarmonicRhythmResult result{};
  if (!isValidProgressionId(request.progression, false) ||
      request.harmonicEventCount > kMaxHarmonicEvents) {
    return result;
  }

  result.plan.progression = request.progression;
  result.plan.phraseBarOrdinal = request.phraseBarOrdinal;
  result.plan.phraseHarmonicPosition = request.phraseHarmonicPosition;

  if (isStaticHarmonicProgression(request.progression)) {
    result.plan.eventCount = 1;
    result.plan.onsets = harmonicClockOnsets(HarmonicClockId::StaticHold);
  } else if (request.harmonicEventCount != 0) {
    result.plan.eventCount = request.harmonicEventCount;
    result.plan.onsets = evenlySpacedHarmonicOnsets(request.harmonicEventCount);
  } else {
    const HarmonicClockId clock = defaultOneBarHarmonicClock(request.progression);
    result.plan.eventCount = harmonicClockEventCount(clock);
    result.plan.onsets = harmonicClockOnsets(clock);
  }

  if (result.plan.eventCount == 0 || result.plan.onsets == 0) {
    return HarmonicRhythmResult{};
  }

  result.status = HarmonicRhythmStatus::Ok;
  return result;
}

static_assert(std::is_trivially_copyable<HarmonicRhythmRequest>::value,
              "HarmonicRhythmRequest must remain fixed-capacity");
static_assert(std::is_trivially_copyable<HarmonicRhythmPlan>::value,
              "HarmonicRhythmPlan must remain fixed-capacity");
static_assert(sizeof(HarmonicRhythmPlan) <= 8,
              "HarmonicRhythmPlan exceeded its command-time budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_ROLES_HARMONIC_RHYTHM_H
