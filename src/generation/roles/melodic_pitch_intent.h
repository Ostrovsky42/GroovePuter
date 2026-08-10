#ifndef GROOVEPUTER_GENERATION_ROLES_MELODIC_PITCH_INTENT_H
#define GROOVEPUTER_GENERATION_ROLES_MELODIC_PITCH_INTENT_H

#include <cstdint>
#include <type_traits>

#include "melodic_motif.h"

namespace GroovePuterRhythm {

enum class MelodicRhythmOperationId : uint8_t {
  Auto = 0,
  Preserve,
  ControlledRest,
  ShiftInteriorEarlier,
  ShiftInteriorLater,
  TerminalEcho,
  Count,
};

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
  MelodicRhythmOperationId requestedRhythmOperation =
      MelodicRhythmOperationId::Auto;
  MelodicContourId requestedContour = MelodicContourId::Auto;
  MelodicMotifOperationId requestedOperation = MelodicMotifOperationId::Auto;

  // Transient semantic legality for 15B. Onset legality is intentionally
  // separate from continuation legality because Stage 14 may permit a held
  // melodic note to pass through a bass/chord onset while still forbidding a
  // new melodic onset there. Neither mask is physical Synth B availability:
  // chord-first blocking and voice arbitration remain downstream Stage 14.
  StepMask allowedOnsetSteps = kAllSteps;
  StepMask allowedContinuationSteps = kAllSteps;
  bool allowEmptyBar = false;

  // Scale-degree offsets relative to the current-bar harmonic anchor.
  // Absolute MIDI realization belongs to the downstream tonal materializer.
  int8_t minDegreeOffset = -7;
  int8_t maxDegreeOffset = 7;
  uint8_t maxLeapDegrees = 4;
  uint8_t maxOnsets = kStepsPerBar;
};

struct MelodicPitchIntentPlan {
  MelodicRhythmOperationId rhythmOperation =
      MelodicRhythmOperationId::Preserve;
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

bool isValidMelodicRhythmOperationId(
    MelodicRhythmOperationId id, bool allowAuto = true);
bool isValidMelodicContourId(MelodicContourId id, bool allowAuto = true);
bool isValidMelodicMotifOperationId(
    MelodicMotifOperationId id, bool allowAuto = true);
const char* melodicRhythmOperationName(MelodicRhythmOperationId id);
const char* melodicContourName(MelodicContourId id);
const char* melodicMotifOperationName(MelodicMotifOperationId id);

static_assert(std::is_trivially_copyable<MelodicPitchIntentPlan>::value,
              "MelodicPitchIntentPlan must remain fixed-capacity");
static_assert(sizeof(MelodicPitchIntentPlan) <= 48,
              "MelodicPitchIntentPlan exceeded its command-time budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_ROLES_MELODIC_PITCH_INTENT_H
