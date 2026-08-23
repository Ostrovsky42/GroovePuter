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

// HarmonicRhythm owns WHEN progression state advances inside the current
// physical bar. It deliberately has no ChordRhythm input: chord articulation
// may retrigger any number of times without becoming a harmonic clock.
//
// harmonicEventCount == 0 selects the bounded F08 one-bar default:
// static progressions expose one state, moving progressions expose two states.
// A caller may request another bounded count explicitly; this is useful for
// tests and for future phrase policy without changing chord articulation.
//
// phraseBarOrdinal / phraseHarmonicPosition are an API boundary for E3a. F08
// carries them as coordinates only. They do not create a temporal carrier,
// scheduler, lifecycle, or cross-bar progression state.
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

inline uint8_t defaultOneBarHarmonicEventCount(ProgressionId id) {
  if (!isValidProgressionId(id, false)) return 0;
  return isStaticHarmonicProgression(id) ? 1 : 2;
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

  uint8_t count = request.harmonicEventCount;
  if (count == 0) count = defaultOneBarHarmonicEventCount(request.progression);
  if (isStaticHarmonicProgression(request.progression)) count = 1;
  if (count == 0 || count > kMaxHarmonicEvents) return HarmonicRhythmResult{};

  result.plan.eventCount = count;
  result.plan.onsets = evenlySpacedHarmonicOnsets(count);
  if (result.plan.onsets == 0) return HarmonicRhythmResult{};

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
