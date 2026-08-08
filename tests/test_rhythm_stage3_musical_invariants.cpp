#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;
using namespace GroovePuterRhythm::ReferenceVocabulary;

namespace {

constexpr StepMask kFourFloorKick =
    stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12);
constexpr StepMask kBackbeatSteps = stepBit(4) | stepBit(12);

const LaneGrammar* laneFor(const RhythmArchetype& archetype,
                           RhythmRole role) {
  for (uint8_t i = 0; i < archetype.laneCount; ++i) {
    if (archetype.lanes[i].role == role) return &archetype.lanes[i];
  }
  return nullptr;
}

StepMask onsets(const RhythmPhrasePlan& plan, RhythmRole role) {
  const RoleRhythmPlan& lane = plan.bars[0].roles[static_cast<uint8_t>(role)];
  return static_cast<StepMask>(
      lane.structural | lane.secondary | lane.ghosts);
}

bool rolePlanEqual(const RoleRhythmPlan& lhs,
                   const RoleRhythmPlan& rhs) {
  return lhs.structural == rhs.structural &&
         lhs.secondary == rhs.secondary &&
         lhs.ghosts == rhs.ghosts &&
         lhs.shortGate == rhs.shortGate &&
         lhs.heldGate == rhs.heldGate &&
         lhs.tieGate == rhs.tieGate &&
         lhs.accents == rhs.accents;
}

bool planEqual(const RhythmPhrasePlan& lhs,
               const RhythmPhrasePlan& rhs) {
  if (lhs.barCount != rhs.barCount ||
      lhs.trajectoryId != rhs.trajectoryId ||
      lhs.level != rhs.level ||
      lhs.intent != rhs.intent) {
    return false;
  }
  for (uint8_t bar = 0; bar < kMaxPhraseBars; ++bar) {
    if (lhs.bars[bar].function != rhs.bars[bar].function) return false;
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      if (!rolePlanEqual(lhs.bars[bar].roles[role],
                         rhs.bars[bar].roles[role])) {
        return false;
      }
    }
  }
  return true;
}

RhythmRealizationResult realize(const RhythmCatalogView& vocabulary,
                                const RhythmArchetype& archetype,
                                uint32_t seed,
                                uint16_t phraseOrdinal,
                                RealizationLevel level,
                                const PhraseRhythmIdentity* identity = nullptr) {
  RhythmRealizationRequest request{};
  request.catalog = &vocabulary;
  request.archetypeId = archetype.id;
  request.phraseBars = 1;
  request.level = level;
  request.generation = GenerationContext{seed, phraseOrdinal};
  request.reuseIdentity = identity;
  return realizeRhythmPhrase(request);
}

void assertBrokenNotFourFloor(const RhythmCatalogView& vocabulary,
                              Archetype key) {
  const RhythmArchetype* archetype = archetypeFor(key);
  assert(archetype != nullptr);
  for (uint32_t seed = 1; seed <= 64; ++seed) {
    const auto p1 = realize(vocabulary, *archetype, seed,
                            static_cast<uint16_t>(key),
                            RealizationLevel::P1Canonical);
    assert(p1.status != RealizationStatus::InvalidConstraintSet);
    assert(onsets(p1.plan, RhythmRole::Kick) != kFourFloorKick);
  }
}

void assertAcidBassHasIndependentMotion(const RhythmCatalogView& vocabulary,
                                        Archetype key) {
  const RhythmArchetype* archetype = archetypeFor(key);
  assert(archetype != nullptr);
  const LaneGrammar* bass = laneFor(*archetype, RhythmRole::BassRhythm);
  assert(bass != nullptr);
  assert(bass->preferred != 0);
  assert((bass->shortGate | bass->heldGate | bass->tieGate) != 0);

  uint8_t independentSeeds = 0;
  for (uint32_t seed = 1; seed <= 64; ++seed) {
    const auto p1 = realize(vocabulary, *archetype, seed,
                            static_cast<uint16_t>(key),
                            RealizationLevel::P1Canonical);
    assert(p1.status != RealizationStatus::InvalidConstraintSet);
    const StepMask kick = onsets(p1.plan, RhythmRole::Kick);
    const StepMask bassOnsets = onsets(p1.plan, RhythmRole::BassRhythm);
    if (bassOnsets & static_cast<StepMask>(~kick)) ++independentSeeds;
  }
  assert(independentSeeds >= 16);
}

void assertProtectedBackbeatSpace(const RhythmCatalogView& vocabulary,
                                  Archetype key,
                                  RhythmRoleMask expectedRoles) {
  const RhythmArchetype* archetype = archetypeFor(key);
  assert(archetype != nullptr);

  RhythmRoleMask rolesAtBackbeatSpace = 0;
  for (uint8_t i = 0; i < archetype->protectedSpaceCount; ++i) {
    if ((archetype->protectedSpaces[i].steps & kBackbeatSteps) ==
        kBackbeatSteps) {
      rolesAtBackbeatSpace = static_cast<RhythmRoleMask>(
          rolesAtBackbeatSpace | archetype->protectedSpaces[i].affectedRoles);
    }
  }
  assert((rolesAtBackbeatSpace & expectedRoles) == expectedRoles);

  for (uint32_t seed = 1; seed <= 64; ++seed) {
    const auto p1 = realize(vocabulary, *archetype, seed,
                            static_cast<uint16_t>(key),
                            RealizationLevel::P1Canonical);
    assert(p1.status != RealizationStatus::InvalidConstraintSet);
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      const RhythmRoleMask bit = rhythmRoleBit(static_cast<RhythmRole>(role));
      if (!(expectedRoles & bit)) continue;
      assert((onsets(p1.plan, static_cast<RhythmRole>(role)) &
              kBackbeatSteps) == 0);
    }
  }
}

}  // namespace

int main() {
  const RhythmCatalogView& vocabulary = catalog();
  assert(validateRhythmCatalog(vocabulary));

  // Broken/2-step reference frames must not silently collapse into a generic
  // four-on-the-floor kick topology merely because all events are legal.
  assertBrokenNotFourFloor(vocabulary, Archetype::BrokenTechno);
  assertBrokenNotFourFloor(vocabulary, Archetype::TwoStepRoll);
  assertBrokenNotFourFloor(vocabulary, Archetype::GhostedRoll);
  assertBrokenNotFourFloor(vocabulary, Archetype::SparseFastBreak);
  assertBrokenNotFourFloor(vocabulary, Archetype::HalftimeSwitch);
  assertBrokenNotFourFloor(vocabulary, Archetype::ClassicTwoStep);
  assertBrokenNotFourFloor(vocabulary, Archetype::SkippyTwoStep);
  assertBrokenNotFourFloor(vocabulary, Archetype::MachineSyncopation);

  // Acid remains a generic rhythm frame plus an independently moving,
  // duration-aware bass-rhythm role. It is deliberately not a RhythmFamily.
  assertAcidBassHasIndependentMotion(vocabulary, Archetype::StraightAcid);
  assertAcidBassHasIndependentMotion(vocabulary, Archetype::RollingAcid);
  assertAcidBassHasIndependentMotion(vocabulary, Archetype::SyncopatedAcid);
  assertAcidBassHasIndependentMotion(vocabulary, Archetype::SparseAcid);

  // Dub space is role-scoped: kick/bass (and for sparse_skank, chord) remain
  // silent on the protected backbeat coordinates while hats/perc may continue.
  const RhythmRoleMask kickBass =
      rhythmRoleBit(RhythmRole::Kick) | rhythmRoleBit(RhythmRole::BassRhythm);
  assertProtectedBackbeatSpace(vocabulary, Archetype::OneDropSpace, kickBass);
  assertProtectedBackbeatSpace(vocabulary, Archetype::ChordResponse, kickBass);
  assertProtectedBackbeatSpace(
      vocabulary, Archetype::SparseSkank,
      static_cast<RhythmRoleMask>(kickBass |
          rhythmRoleBit(RhythmRole::ChordRhythm)));

  uint8_t p2VariedArchetypes = 0;
  uint8_t p3VariedArchetypes = 0;
  for (uint8_t index = 0; index < definitionCount(); ++index) {
    const Definition& def = definition(index);
    const RhythmArchetype* archetype = archetypeFor(def.key);
    assert(archetype != nullptr);

    uint8_t p2ChangedSeeds = 0;
    uint8_t p3ChangedSeeds = 0;
    for (uint32_t seed = 1; seed <= 64; ++seed) {
      const auto p1 = realize(vocabulary, *archetype, seed, index,
                              RealizationLevel::P1Canonical);
      assert(p1.status != RealizationStatus::InvalidConstraintSet);
      const auto p2 = realize(vocabulary, *archetype, seed, index,
                              RealizationLevel::P2Variation, &p1.identity);
      const auto p3 = realize(vocabulary, *archetype, seed, index,
                              RealizationLevel::P3Transformation, &p1.identity);
      assert(p2.status != RealizationStatus::InvalidConstraintSet);
      assert(p3.status != RealizationStatus::InvalidConstraintSet);
      if (!planEqual(p1.plan, p2.plan)) ++p2ChangedSeeds;
      if (!planEqual(p1.plan, p3.plan)) ++p3ChangedSeeds;
    }

    std::fprintf(stderr,
                 "stage3 variation %-20s p2=%u/64 p3=%u/64\n",
                 def.name,
                 static_cast<unsigned>(p2ChangedSeeds),
                 static_cast<unsigned>(p3ChangedSeeds));
    if (p2ChangedSeeds != 0) ++p2VariedArchetypes;
    if (p3ChangedSeeds != 0) ++p3VariedArchetypes;
  }

  // Stage 3 is a reference package, not a fixed-pattern library. Every
  // archetype must expose at least one legal P2 and P3 realization distinct
  // from its P1 statement somewhere in the deterministic seed corpus.
  assert(p2VariedArchetypes == definitionCount());
  assert(p3VariedArchetypes == definitionCount());

  return 0;
}
