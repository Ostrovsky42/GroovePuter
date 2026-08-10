#ifndef GROOVEPUTER_GENERATION_ROLES_MELODIC_MOTIF_H
#define GROOVEPUTER_GENERATION_ROLES_MELODIC_MOTIF_H

#include <cstdint>
#include <type_traits>

#include "../generation_context.h"

namespace GroovePuterRhythm {

enum class MelodicRhythmId : uint8_t {
  Auto = 0,
  SparseCall,
  DelayedAnswer,
  TwoNoteHook,
  PickupPhrase,
  LongTone,
  RestHeavy,
  BarEndResponse,
  SyncopatedMotif,
  DriftPhrase,
  RepeatedCell,
  Count,
};

enum class MotifShapeId : uint8_t {
  Auto = 0,
  SourceOrder,
  TwoNoteCell,
  Mirror,
  CallResponse,
  Pivot,
  Count,
};

enum class MelodicMotifStatus : uint8_t {
  Ok = 0,
  ValidButEmpty,
  InvalidRequest,
  Count,
};

struct MotifIdentity {
  MotifShapeId shape = MotifShapeId::Auto;
  uint8_t sourceOrder[8]{};
  uint8_t sourceOrderCount = 0;
};

struct MelodicMotifRequest {
  MelodicRhythmId requestedRhythm = MelodicRhythmId::Auto;
  MotifShapeId requestedShape = MotifShapeId::Auto;
  RhythmFamily family = RhythmFamily::FourFloor;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  StepMask bassOnsets = 0;
  StepMask chordOnsets = 0;
  StepMask protectedSpace = 0;
  GenerationContext generation{};
  uint8_t barOrdinal = 0;
  bool allowEmptyBar = false;
};

struct MelodicMotifPlan {
  MelodicRhythmId rhythmId = MelodicRhythmId::Auto;
  StepMask onsets = 0;
  StepMask continuations = 0;
  MotifIdentity motif{};
};

struct MelodicMotifResult {
  MelodicMotifStatus status = MelodicMotifStatus::InvalidRequest;
  MelodicMotifPlan plan{};
};

MelodicMotifResult realizeMelodicMotif(const MelodicMotifRequest& request);
const char* melodicRhythmName(MelodicRhythmId id);
const char* motifShapeName(MotifShapeId id);
bool isValidMelodicRhythmId(MelodicRhythmId id, bool allowAuto = true);
bool isValidMotifShapeId(MotifShapeId id, bool allowAuto = true);

static_assert(std::is_trivially_copyable<MelodicMotifPlan>::value,
              "MelodicMotifPlan must remain fixed-capacity");
static_assert(sizeof(MelodicMotifPlan) <= 16,
              "MelodicMotifPlan exceeded its command-time budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_ROLES_MELODIC_MOTIF_H
