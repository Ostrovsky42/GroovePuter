#ifndef GROOVEPUTER_GENERATION_ROLES_CHORD_RHYTHM_RETRIGGER_H
#define GROOVEPUTER_GENERATION_ROLES_CHORD_RHYTHM_RETRIGGER_H

#include <cstdint>
#include <type_traits>

#include "../rhythm/rhythm_types.h"

namespace GroovePuterRhythm {

constexpr uint8_t kIncomingChordRhythmSourceOrdinal = 0xFEu;
constexpr uint8_t kNoChordRhythmSourceOrdinal = 0xFFu;

enum class ChordRhythmRetriggerStatus : uint8_t {
  Ok = 0,
  ValidButEmpty,
  InvalidOverlap,
  OrphanRetrigger,
  InvalidGateTopology,
  Count,
};

// P3 adds an action distinction only. Gate semantics remain explicit and
// independent because current ChordRhythm already owns continuation/release.
struct ChordRhythmRetriggerRequest {
  StepMask sourceAdvanceOnsets = 0;
  StepMask sameChordRetriggers = 0;
  StepMask continuations = 0;
  StepMask releasePoints = 0;

  // Explicit composition input for a bar that begins while a harmonic source
  // from a preceding bar is still the current source identity. No global state
  // is consulted. Retriggers before the first local source advance map to the
  // INCOMING sentinel in sourceOrdinalByStep.
  bool sourceAvailableAtStart = false;
};

struct ChordRhythmRetriggerPlan {
  StepMask sourceAdvanceOnsets = 0;
  StepMask sameChordRetriggers = 0;
  StepMask audibleOnsets = 0;
  StepMask continuations = 0;
  StepMask releasePoints = 0;
  ChordRhythmRetriggerStatus status =
      ChordRhythmRetriggerStatus::ValidButEmpty;
  uint8_t sourceAdvanceCount = 0;
  uint8_t audibleOnsetCount = 0;

  // Meaningful only at audible onset steps. Local source advances use 0..15;
  // a retrigger of an explicitly supplied incoming source uses 0xFE.
  uint8_t sourceOrdinalByStep[kStepsPerBar]{};
};

ChordRhythmRetriggerPlan realizeChordRhythmRetriggers(
    const ChordRhythmRetriggerRequest& request);

static_assert(std::is_trivially_copyable<ChordRhythmRetriggerRequest>::value,
              "P3 retrigger request must remain fixed-capacity");
static_assert(std::is_trivially_copyable<ChordRhythmRetriggerPlan>::value,
              "P3 retrigger plan must remain fixed-capacity");
static_assert(sizeof(ChordRhythmRetriggerRequest) <= 12,
              "P3 retrigger request exceeded its budget");
static_assert(sizeof(ChordRhythmRetriggerPlan) <= 32,
              "P3 retrigger plan exceeded its budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_ROLES_CHORD_RHYTHM_RETRIGGER_H
