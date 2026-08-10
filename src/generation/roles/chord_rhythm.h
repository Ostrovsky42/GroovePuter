#pragma once

#include <cstdint>
#include <type_traits>

#include "../generation_context.h"

namespace GroovePuterRhythm {

enum class ChordRhythmId : uint8_t {
  Auto = 0,
  HeldPad,
  WholeBarHold,
  HalfBarChange,
  OffbeatStab,
  BackbeatStab,
  AnticipatedChange,
  SparseChordReply,
  DubChordSpace,
  SyncopatedComp,
  Count,
};

enum class ChordRhythmStatus : uint8_t {
  Ok = 0,
  ValidButEmpty,
  InvalidRequest,
  Count,
};

struct ChordRhythmRequest {
  ChordRhythmId requestedId = ChordRhythmId::Auto;
  RhythmFamily family = RhythmFamily::FourFloor;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  StepMask bassOnsets = 0;
  StepMask protectedSpace = 0;
  GenerationContext generation{};
  uint8_t barOrdinal = 0;
  bool allowEmptyBar = false;
};

struct ChordRhythmPlan {
  ChordRhythmId id = ChordRhythmId::Auto;
  StepMask onsets = 0;
  StepMask continuations = 0;
  StepMask releasePoints = 0;
};

struct ChordRhythmResult {
  ChordRhythmStatus status = ChordRhythmStatus::InvalidRequest;
  ChordRhythmPlan plan{};
};

ChordRhythmResult realizeChordRhythm(const ChordRhythmRequest& request);
const char* chordRhythmName(ChordRhythmId id);
bool isValidChordRhythmId(ChordRhythmId id, bool allowAuto = true);

static_assert(std::is_trivially_copyable<ChordRhythmPlan>::value,
              "ChordRhythmPlan must remain fixed-capacity");
static_assert(sizeof(ChordRhythmPlan) <= 8,
              "ChordRhythmPlan exceeded its command-time budget");

}  // namespace GroovePuterRhythm
