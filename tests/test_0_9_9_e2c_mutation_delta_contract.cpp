#include <cassert>
#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

constexpr RhythmMutationDelta delta(RhythmMutationOp operation,
                                    RhythmRole role,
                                    uint8_t sourceStep,
                                    uint8_t targetStep) {
  return RhythmMutationDelta{operation, role, sourceStep, targetStep};
}

static_assert(static_cast<uint8_t>(RhythmMutationOp::KEEP) == 0);
static_assert(static_cast<uint8_t>(RhythmMutationOp::ADD) == 1);
static_assert(static_cast<uint8_t>(RhythmMutationOp::DROP) == 2);
static_assert(static_cast<uint8_t>(RhythmMutationOp::DISPLACE) == 3);
static_assert(static_cast<uint8_t>(RhythmMutationOp::ACCENT) == 4);
static_assert(static_cast<uint8_t>(RhythmMutationOp::GHOST) == 5);
static_assert(static_cast<uint8_t>(RhythmMutationOp::Count) == 6);

static_assert(kNoMutationStep == 0xFFu);
static_assert(kDisplaceRadius == 2u);
static_assert(kMaxRhythmMutationDeltasPerBar == 256u);
static_assert(kMaxRhythmMutationDeltasPerPhrase == 1024u);
static_assert(static_cast<uint8_t>(GenerationDomain::BarEvolution) == 12u);
static_assert(sizeof(RhythmMutationDelta) == 4u);
static_assert(std::is_trivially_copyable<RhythmMutationDelta>::value);

static_assert(rhythmMutationDeltaShapeValid(
    delta(RhythmMutationOp::KEEP, RhythmRole::Kick, 4, 4)));
static_assert(!rhythmMutationDeltaShapeValid(
    delta(RhythmMutationOp::KEEP, RhythmRole::Kick, 4, 5)));

static_assert(rhythmMutationDeltaShapeValid(
    delta(RhythmMutationOp::ADD, RhythmRole::Kick,
          kNoMutationStep, 5)));
static_assert(!rhythmMutationDeltaShapeValid(
    delta(RhythmMutationOp::ADD, RhythmRole::Kick, 4, 5)));

static_assert(rhythmMutationDeltaShapeValid(
    delta(RhythmMutationOp::DROP, RhythmRole::Kick,
          5, kNoMutationStep)));
static_assert(!rhythmMutationDeltaShapeValid(
    delta(RhythmMutationOp::DROP, RhythmRole::Kick, 5, 6)));

static_assert(rhythmMutationDeltaShapeValid(
    delta(RhythmMutationOp::DISPLACE, RhythmRole::Kick, 5, 7)));
static_assert(!rhythmMutationDeltaShapeValid(
    delta(RhythmMutationOp::DISPLACE, RhythmRole::Kick, 5, 8)));
static_assert(!rhythmMutationDeltaShapeValid(
    delta(RhythmMutationOp::DISPLACE, RhythmRole::Kick, 15, 0)));
static_assert(!rhythmMutationDeltaShapeValid(
    delta(RhythmMutationOp::DISPLACE, RhythmRole::Kick, 5, 5)));

static_assert(rhythmMutationDeltaShapeValid(
    delta(RhythmMutationOp::ACCENT, RhythmRole::Kick, 5, 5)));
static_assert(!rhythmMutationDeltaShapeValid(
    delta(RhythmMutationOp::ACCENT, RhythmRole::Kick,
          kNoMutationStep, 5)));

static_assert(rhythmMutationDeltaShapeValid(
    delta(RhythmMutationOp::GHOST, RhythmRole::Kick,
          kNoMutationStep, 5)));
static_assert(!rhythmMutationDeltaShapeValid(
    delta(RhythmMutationOp::GHOST, RhythmRole::Kick, 5, 5)));

static_assert(!rhythmMutationDeltaShapeValid(
    delta(RhythmMutationOp::KEEP, RhythmRole::Count, 5, 5)));
static_assert(!rhythmMutationDeltaShapeValid(
    delta(RhythmMutationOp::Count, RhythmRole::Kick, 5, 5)));

static_assert(rhythmMutationDeltaLess(
    delta(RhythmMutationOp::DROP, RhythmRole::Kick,
          3, kNoMutationStep),
    delta(RhythmMutationOp::ADD, RhythmRole::Kick,
          kNoMutationStep, 5)));
static_assert(rhythmMutationDeltaLess(
    delta(RhythmMutationOp::ADD, RhythmRole::Kick,
          kNoMutationStep, 5),
    delta(RhythmMutationOp::ACCENT, RhythmRole::Kick, 5, 5)));
static_assert(rhythmMutationDeltaLess(
    delta(RhythmMutationOp::ACCENT, RhythmRole::Kick, 5, 5),
    delta(RhythmMutationOp::GHOST, RhythmRole::Kick,
          kNoMutationStep, 5)));
static_assert(rhythmMutationDeltaLess(
    delta(RhythmMutationOp::GHOST, RhythmRole::Kick,
          kNoMutationStep, 5),
    delta(RhythmMutationOp::ADD, RhythmRole::Backbeat,
          kNoMutationStep, 0)));
static_assert(!rhythmMutationDeltaLess(
    delta(RhythmMutationOp::ADD, RhythmRole::Kick,
          kNoMutationStep, 5),
    delta(RhythmMutationOp::ADD, RhythmRole::Kick,
          kNoMutationStep, 5)));

}  // namespace

int main() {
  LaneGrammar lane{};
  lane.role = RhythmRole::Kick;
  lane.immutableAnchors = stepBit(0);
  lane.canonicalAnchors = stepBit(4);
  lane.preferred = static_cast<StepMask>(
      stepBit(5) | stepBit(9) | stepBit(15));
  lane.optional = static_cast<StepMask>(
      stepBit(2) | stepBit(6) | stepBit(8) |
      stepBit(10) | stepBit(13));
  lane.forbidden = stepBit(7);

  ProtectedSpace protectedSpace{};
  protectedSpace.steps = stepBit(8);
  protectedSpace.affectedRoles = rhythmRoleBit(RhythmRole::Kick);

  AnchorTransformRule transform{};
  transform.role = RhythmRole::Kick;
  transform.barFunction = BarFunction::Break;
  transform.intent = TransformationIntent::Break;
  transform.suppressibleCanonical = 0;
  transform.displaceableCanonical = stepBit(4);

  RhythmArchetype archetype{};
  archetype.activeRoles = rhythmRoleBit(RhythmRole::Kick);
  archetype.lanes = &lane;
  archetype.laneCount = 1;
  archetype.protectedSpaces = &protectedSpace;
  archetype.protectedSpaceCount = 1;
  archetype.anchorTransformRules = &transform;
  archetype.anchorTransformRuleCount = 1;

  const RhythmMutationDelta optionalMove =
      delta(RhythmMutationOp::DISPLACE, RhythmRole::Kick, 5, 6);
  assert(rhythmMutationDisplacementGrammarLegal(
      archetype, optionalMove,
      BarFunction::Statement, TransformationIntent::Auto));

  const RhythmMutationDelta tooFar =
      delta(RhythmMutationOp::DISPLACE, RhythmRole::Kick, 5, 8);
  assert(!rhythmMutationDisplacementGrammarLegal(
      archetype, tooFar,
      BarFunction::Statement, TransformationIntent::Auto));

  const RhythmMutationDelta immutableMove =
      delta(RhythmMutationOp::DISPLACE, RhythmRole::Kick, 0, 2);
  assert(!rhythmMutationDisplacementGrammarLegal(
      archetype, immutableMove,
      BarFunction::Break, TransformationIntent::Break));

  const RhythmMutationDelta canonicalMove =
      delta(RhythmMutationOp::DISPLACE, RhythmRole::Kick, 4, 6);
  assert(rhythmMutationDisplacementGrammarLegal(
      archetype, canonicalMove,
      BarFunction::Break, TransformationIntent::Break));
  assert(!rhythmMutationDisplacementGrammarLegal(
      archetype, canonicalMove,
      BarFunction::Response, TransformationIntent::Break));
  assert(!rhythmMutationDisplacementGrammarLegal(
      archetype, canonicalMove,
      BarFunction::Break, TransformationIntent::Reduce));

  const RhythmMutationDelta canonicalTarget =
      delta(RhythmMutationOp::DISPLACE, RhythmRole::Kick, 6, 4);
  assert(!rhythmMutationDisplacementGrammarLegal(
      archetype, canonicalTarget,
      BarFunction::Statement, TransformationIntent::Auto));

  const RhythmMutationDelta forbiddenTarget =
      delta(RhythmMutationOp::DISPLACE, RhythmRole::Kick, 6, 7);
  assert(!rhythmMutationDisplacementGrammarLegal(
      archetype, forbiddenTarget,
      BarFunction::Statement, TransformationIntent::Auto));

  const RhythmMutationDelta protectedTarget =
      delta(RhythmMutationOp::DISPLACE, RhythmRole::Kick, 6, 8);
  assert(!rhythmMutationDisplacementGrammarLegal(
      archetype, protectedTarget,
      BarFunction::Statement, TransformationIntent::Auto));

  const RhythmMutationDelta absentLane =
      delta(RhythmMutationOp::DISPLACE, RhythmRole::Backbeat, 5, 6);
  assert(!rhythmMutationDisplacementGrammarLegal(
      archetype, absentLane,
      BarFunction::Statement, TransformationIntent::Auto));

  std::puts("E2C canonical rhythm mutation delta contract: OK");
  return 0;
}
