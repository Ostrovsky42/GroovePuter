#ifndef GROOVEPUTER_GENERATION_MIGRATION_P2_P3_HARDWARE_AUDITION_H
#define GROOVEPUTER_GENERATION_MIGRATION_P2_P3_HARDWARE_AUDITION_H

#include <cstdint>

#include "../rhythm/rhythm_types.h"

namespace GroovePuterRhythm {

enum class P2P3HardwareAuditionFixture : uint8_t {
  Retrigger = 1,
  DenseRetrigger,
  CrossBarHold,
  MultiBarNS,
  Count,
};

enum class P2P3HardwareAuditionStatus : uint8_t {
  Ok = 0,
  InvalidFixture,
  TimelineRejected,
  RetriggerRejected,
  Count,
};

struct P2P3HardwareAuditionBar {
  StepMask sourceAdvanceOnsets = 0;
  StepMask sameChordRetriggers = 0;
  StepMask audibleOnsets = 0;
  StepMask continuations = 0;
  StepMask releasePoints = 0;
  uint8_t sourceOrdinalByStep[kStepsPerBar]{};
};

struct P2P3HardwareAuditionPlan {
  P2P3HardwareAuditionStatus status =
      P2P3HardwareAuditionStatus::InvalidFixture;
  P2P3HardwareAuditionFixture fixture =
      P2P3HardwareAuditionFixture::Retrigger;
  uint8_t barCount = 0;
  uint8_t sourceAdvanceCount = 0;
  P2P3HardwareAuditionBar bars[kMaxPhraseBars]{};
};

P2P3HardwareAuditionPlan realizeP2P3HardwareAudition(
    P2P3HardwareAuditionFixture fixture);

const char* p2P3HardwareAuditionFixtureName(
    P2P3HardwareAuditionFixture fixture);

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_MIGRATION_P2_P3_HARDWARE_AUDITION_H
