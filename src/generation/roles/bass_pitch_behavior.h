#ifndef GROOVEPUTER_GENERATION_ROLES_BASS_PITCH_BEHAVIOR_H
#define GROOVEPUTER_GENERATION_ROLES_BASS_PITCH_BEHAVIOR_H

#include <cstdint>
#include <type_traits>

#include "bass_rhythm.h"

namespace GroovePuterRhythm {

enum class BassPitchContourId : uint8_t {
  Auto = 0,
  RootAnchor,
  RootFifth,
  RootOctave,
  NeighborReturn,
  StepApproach,
  LeapReturn,
  RootFifthNeighbor,
  PedalTurn,
  Count,
};

enum class BassArticulationStyleId : uint8_t {
  Auto = 0,
  Plain,
  AccentPulse,
  LegatoApproach,
  Dynamic,
  Count,
};

enum class BassPitchBehaviorStatus : uint8_t {
  Ok = 0,
  ValidButEmpty,
  InvalidRequest,
  Count,
};

struct BassPitchBehaviorRequest {
  BassRhythmPlan rhythmPlan{};
  RhythmFamily family = RhythmFamily::FourFloor;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  GenerationContext generation{};
  uint8_t barOrdinal = 0;
  BassPitchContourId requestedContour = BassPitchContourId::Auto;
  BassArticulationStyleId requestedArticulation =
      BassArticulationStyleId::Auto;

  // Scale-degree offsets relative to the current-bar harmonic root.
  // Absolute MIDI note selection and synth-specific slide implementation are
  // downstream concerns.
  int8_t minDegreeOffset = -7;
  int8_t maxDegreeOffset = 7;
  uint8_t maxLeapDegrees = 7;
};

struct BassPitchBehaviorPlan {
  BassPitchContourId contour = BassPitchContourId::RootAnchor;
  BassArticulationStyleId articulation = BassArticulationStyleId::Plain;
  StepMask onsets = 0;
  StepMask continuations = 0;
  StepMask accentOnsets = 0;
  StepMask slideIntoOnsets = 0;
  uint8_t onsetCount = 0;
  uint8_t onsetSteps[kStepsPerBar]{};
  int8_t degreeOffsets[kStepsPerBar]{};
};

struct BassPitchBehaviorResult {
  BassPitchBehaviorStatus status = BassPitchBehaviorStatus::InvalidRequest;
  BassPitchBehaviorPlan plan{};
};

BassPitchBehaviorResult realizeBassPitchBehavior(
    const BassPitchBehaviorRequest& request);

bool isValidBassPitchContourId(BassPitchContourId id, bool allowAuto = true);
bool isValidBassArticulationStyleId(
    BassArticulationStyleId id, bool allowAuto = true);
const char* bassPitchContourName(BassPitchContourId id);
const char* bassArticulationStyleName(BassArticulationStyleId id);

static_assert(std::is_trivially_copyable<BassPitchBehaviorPlan>::value,
              "BassPitchBehaviorPlan must remain fixed-capacity");
static_assert(sizeof(BassPitchBehaviorPlan) <= 56,
              "BassPitchBehaviorPlan exceeded its command-time budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_ROLES_BASS_PITCH_BEHAVIOR_H
