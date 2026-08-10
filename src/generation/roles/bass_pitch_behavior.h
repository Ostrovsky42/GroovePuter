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
  // Fail-safe defaults preserve the Stage 14 bass behavior. Wider vocabulary
  // must be enabled explicitly by the composition/Genre integration layer.
  uint16_t allowedContours =
      bassPitchContourBit(BassPitchContourId::RootAnchor);
  uint16_t preferredContours = 0;
  uint16_t allowedArticulations =
      bassArticulationStyleBit(BassArticulationStyleId::Plain);
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

  // These bounds apply only to entries tagged as scale-degree intent. Entries
  // tagged as semitone intent retain their named chromatic meaning. A common
  // musical leap bound is evaluated later, after Tonal Projector resolves all
  // entries to absolute MIDI notes.
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

  // Engine-neutral articulation intent only. A downstream adapter may drop an
  // unsupported articulation but may not alter timing to force it.
  StepMask accentOnsets = 0;
  StepMask slideIntoOnsets = 0;

  uint8_t onsetCount = 0;
  uint8_t onsetSteps[kStepsPerBar]{};

  // Bit N describes tonalOffsets[N]: set means chromatic semitones, clear means
  // scale degrees. RootFifth/RootOctave therefore remain unambiguous for 5-,
  // 7-, and 12-note scales while neighbor/approach vocabulary stays modal.
  int8_t tonalOffsets[kStepsPerBar]{};
  uint16_t semitoneOffsetOrdinals = 0;
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
static_assert(sizeof(BassPitchBehaviorPlan) <= 60,
              "BassPitchBehaviorPlan exceeded its command-time budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_ROLES_BASS_PITCH_BEHAVIOR_H
