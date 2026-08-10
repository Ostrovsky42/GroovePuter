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

constexpr uint16_t bassPitchContourBit(BassPitchContourId id) {
  const uint8_t value = static_cast<uint8_t>(id);
  return value == 0 || value >= static_cast<uint8_t>(BassPitchContourId::Count)
      ? 0u
      : static_cast<uint16_t>(1u << value);
}

constexpr uint16_t bassArticulationStyleBit(BassArticulationStyleId id) {
  const uint8_t value = static_cast<uint8_t>(id);
  return value == 0 || value >= static_cast<uint8_t>(BassArticulationStyleId::Count)
      ? 0u
      : static_cast<uint16_t>(1u << value);
}

constexpr uint16_t kAllBassPitchContours =
    bassPitchContourBit(BassPitchContourId::RootAnchor) |
    bassPitchContourBit(BassPitchContourId::RootFifth) |
    bassPitchContourBit(BassPitchContourId::RootOctave) |
    bassPitchContourBit(BassPitchContourId::NeighborReturn) |
    bassPitchContourBit(BassPitchContourId::StepApproach) |
    bassPitchContourBit(BassPitchContourId::LeapReturn) |
    bassPitchContourBit(BassPitchContourId::RootFifthNeighbor) |
    bassPitchContourBit(BassPitchContourId::PedalTurn);

constexpr uint16_t kAllBassArticulationStyles =
    bassArticulationStyleBit(BassArticulationStyleId::Plain) |
    bassArticulationStyleBit(BassArticulationStyleId::AccentPulse) |
    bassArticulationStyleBit(BassArticulationStyleId::LegatoApproach) |
    bassArticulationStyleBit(BassArticulationStyleId::Dynamic);

struct BassBehaviorPolicy {
  // Genre/Variant/composition resolves these transient masks. Preferred masks
  // are optional subsets; AUTO falls back to allowed when preferred is empty.
  // RootAnchor and Plain must stay allowed as compatibility fallbacks.
  uint16_t allowedContours = kAllBassPitchContours;
  uint16_t preferredContours = 0;
  uint16_t allowedArticulations = kAllBassArticulationStyles;
  uint16_t preferredArticulations = 0;
};

struct BassPitchBehaviorRequest {
  BassRhythmPlan rhythmPlan{};
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  GenerationContext generation{};
  uint8_t barOrdinal = 0;
  BassBehaviorPolicy policy{};
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

  // Timing topology is copied from BassRhythmPlan and is immutable in 15C.
  StepMask onsets = 0;
  StepMask continuations = 0;

  // Engine-neutral articulation intent only. A downstream engine adapter may
  // drop an unsupported accent/slide, but must never add/move an onset or
  // extend/create a continuation to force the articulation to happen.
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

static_assert(static_cast<uint8_t>(BassPitchContourId::Count) <= 16,
              "Bass pitch contour mask exceeded uint16_t");
static_assert(static_cast<uint8_t>(BassArticulationStyleId::Count) <= 16,
              "Bass articulation mask exceeded uint16_t");
static_assert(std::is_trivially_copyable<BassBehaviorPolicy>::value,
              "BassBehaviorPolicy must remain fixed-capacity");
static_assert(std::is_trivially_copyable<BassPitchBehaviorPlan>::value,
              "BassPitchBehaviorPlan must remain fixed-capacity");
static_assert(sizeof(BassPitchBehaviorPlan) <= 56,
              "BassPitchBehaviorPlan exceeded its command-time budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_ROLES_BASS_PITCH_BEHAVIOR_H
