#ifndef GROOVEPUTER_GENERATION_ROLES_MELODIC_PITCH_INTENT_H
#define GROOVEPUTER_GENERATION_ROLES_MELODIC_PITCH_INTENT_H

#include <cstdint>
#include <type_traits>

#include "melodic_motif.h"

namespace GroovePuterRhythm {

enum class MelodicContourId : uint8_t {
  Auto = 0,
  Static,
  StepUp,
  StepDown,
  Arch,
  InvertedArch,
  LeapReturn,
  Neighbor,
  RepeatThenUp,
  RepeatThenDown,
  Count,
};

enum class MelodicMotifOperationId : uint8_t {
  Auto = 0,
  None,
  ChangeTerminal,
  InvertLocal,
  PivotRepeat,
  TerminalReturn,
  Count,
};

enum class MelodicPitchIntentStatus : uint8_t {
  Ok = 0,
  ValidButEmpty,
  InvalidRequest,
  Count,
};

struct MelodicPitchIntentRequest {
  MelodicMotifPlan rhythmPlan{};
  RhythmFamily family = RhythmFamily::FourFloor;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  GenerationContext generation{};
  uint8_t barOrdinal = 0;
  MelodicContourId requestedContour = MelodicContourId::Auto;
  MelodicMotifOperationId requestedOperation = MelodicMotifOperationId::Auto;

  // Scale-degree offsets relative to the current-bar harmonic anchor.
  // Absolute MIDI realization belongs to the downstream tonal materializer.
  int8_t minDegreeOffset = -7;
  int8_t maxDegreeOffset = 7;
  uint8_t maxLeapDegrees = 4;
  uint8_t maxOnsets = kStepsPerBar;
};

struct MelodicPitchIntentPlan {
  MelodicContourId contour = MelodicContourId::Static;
  MelodicMotifOperationId operation = MelodicMotifOperationId::None;
  StepMask onsets = 0;
  StepMask continuations = 0;
  uint8_t onsetCount = 0;
  uint8_t onsetSteps[kStepsPerBar]{};
  int8_t degreeOffsets[kStepsPerBar]{};
};

struct MelodicPitchIntentResult {
  MelodicPitchIntentStatus status = MelodicPitchIntentStatus::InvalidRequest;
  MelodicPitchIntentPlan plan{};
};

MelodicPitchIntentResult realizeMelodicPitchIntent(
    const MelodicPitchIntentRequest& request);

bool isValidMelodicContourId(MelodicContourId id, bool allowAuto = true);
bool isValidMelodicMotifOperationId(
    MelodicMotifOperationId id, bool allowAuto = true);
const char* melodicContourName(MelodicContourId id);
const char* melodicMotifOperationName(MelodicMotifOperationId id);

static_assert(std::is_trivially_copyable<MelodicPitchIntentPlan>::value,
              "MelodicPitchIntentPlan must remain fixed-capacity");
static_assert(sizeof(MelodicPitchIntentPlan) <= 48,
              "MelodicPitchIntentPlan exceeded its command-time budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_ROLES_MELODIC_PITCH_INTENT_H
