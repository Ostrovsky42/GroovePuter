#ifndef GROOVEPUTER_GENERATION_ROLES_BASS_RHYTHM_H
#define GROOVEPUTER_GENERATION_ROLES_BASS_RHYTHM_H

#include <cstdint>
#include <type_traits>

#include "../generation_context.h"

namespace GroovePuterRhythm {

enum class BassRhythmId : uint8_t {
  Auto = 0,
  RootPulse,
  KickLock,
  KickAnswer,
  GapFill,
  OffbeatPush,
  SparseAnchor,
  RollingDrive,
  HalfTimePocket,
  SyncopatedHook,
  SustainAndDrop,
  Count,
};

enum class BassRhythmStatus : uint8_t {
  Ok = 0,
  ValidButEmpty,
  InvalidRequest,
  Count,
};

struct BassRhythmRequest {
  BassRhythmId requestedId = BassRhythmId::Auto;
  RhythmFamily family = RhythmFamily::FourFloor;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  StepMask kickOnsets = 0;
  StepMask protectedSpace = 0;
  GenerationContext generation{};
  uint8_t barOrdinal = 0;
  bool allowEmptyBar = false;
};

struct BassRhythmPlan {
  BassRhythmId id = BassRhythmId::Auto;
  RelationshipOp kickRelationship = RelationshipOp::Exclude;
  StepMask onsets = 0;
  StepMask continuations = 0;
};

struct BassRhythmResult {
  BassRhythmStatus status = BassRhythmStatus::InvalidRequest;
  BassRhythmPlan plan{};
};

BassRhythmResult realizeBassRhythm(const BassRhythmRequest& request);
const char* bassRhythmName(BassRhythmId id);
bool isValidBassRhythmId(BassRhythmId id, bool allowAuto = true);

static_assert(std::is_trivially_copyable<BassRhythmPlan>::value,
              "BassRhythmPlan must remain fixed-capacity");
static_assert(sizeof(BassRhythmPlan) <= 8,
              "BassRhythmPlan exceeded its command-time budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_ROLES_BASS_RHYTHM_H
