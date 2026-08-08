#include <cassert>
#include <cstdint>

#include "src/generation/audition/rhythm_audition_catalog.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

bool identityEqual(const PhraseRhythmIdentity& a,
                   const PhraseRhythmIdentity& b) {
  if (a.archetypeId != b.archetypeId ||
      a.phraseBars != b.phraseBars ||
      a.trajectoryId != b.trajectoryId ||
      a.protectedSpaceCount != b.protectedSpaceCount) {
    return false;
  }
  for (uint8_t i = 0; i < a.protectedSpaceCount; ++i) {
    if (a.protectedSpaces[i].steps != b.protectedSpaces[i].steps ||
        a.protectedSpaces[i].affectedRoles !=
            b.protectedSpaces[i].affectedRoles) {
      return false;
    }
  }
  for (uint8_t bar = 0; bar < kMaxPhraseBars; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      if (a.structuralCore[bar][role] != b.structuralCore[bar][role] ||
          a.canonicalCore[bar][role] != b.canonicalCore[bar][role]) {
        return false;
      }
    }
  }
  return true;
}

uint64_t signature(const RhythmPhrasePlan& plan) {
  uint64_t hash = 1469598103934665603ull;
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      const RoleRhythmPlan& value = plan.bars[bar].roles[role];
      hash ^= value.structural;
      hash *= 1099511628211ull;
      hash ^= value.secondary;
      hash *= 1099511628211ull;
      hash ^= value.ghosts;
      hash *= 1099511628211ull;
    }
  }
  return hash;
}

uint8_t distinctCount(const uint64_t* values, uint8_t count) {
  uint8_t distinct = 0;
  for (uint8_t i = 0; i < count; ++i) {
    bool seen = false;
    for (uint8_t j = 0; j < i; ++j) {
      if (values[j] == values[i]) {
        seen = true;
        break;
      }
    }
    if (!seen) ++distinct;
  }
  return distinct;
}

const RhythmArchetype& archetypeFor(RhythmArchetypeId id) {
  const RhythmCatalogView& view = Audition::catalog();
  for (uint16_t i = 0; i < view.archetypeCount; ++i) {
    if (view.archetypes[i].id == id) return view.archetypes[i];
  }
  assert(false && "missing audition archetype");
  return view.archetypes[0];
}

RhythmRealizationResult realize(const Audition::Definition& definition,
                                uint32_t seed,
                                RealizationLevel level,
                                const PhraseRhythmIdentity* identity = nullptr) {
  RhythmRealizationRequest request{};
  request.catalog = &Audition::catalog();
  request.archetypeId = definition.archetypeId;
  request.phraseBars = 1;
  request.level = level;
  request.generation.projectSeed = seed;
  request.generation.phraseOrdinal = 0;
  request.reuseIdentity = identity;
  return realizeRhythmPhrase(request);
}

void assertPlanLegal(const RhythmArchetype& archetype,
                     const RhythmRealizationResult& result) {
  assert(result.status != RealizationStatus::InvalidConstraintSet);
  assert(result.plan.barCount == 1);
  assert(result.plan.trajectoryId == kNoTrajectoryId);
  assert(result.plan.intent == TransformationIntent::Auto);
  assert(result.plan.bars[0].function == BarFunction::Statement);
  assert(planRespectsProtectedSpace(archetype, result.plan));
  assert(planRespectsLaneBounds(archetype, result.plan));
  assert(hardRelationshipsSatisfied(archetype,
                                    structuralOccupancy(result.plan)));
}

void assertArchetypeIdentityRules(const Audition::Definition& definition,
                                  const RhythmPhrasePlan& plan) {
  const PhraseOccupancy occupancy = structuralOccupancy(plan);
  const uint8_t kick = static_cast<uint8_t>(RhythmRole::Kick);
  const uint8_t backbeat = static_cast<uint8_t>(RhythmRole::Backbeat);

  if (definition.key == Audition::Archetype::StraightDrive ||
      definition.key == Audition::Archetype::RollingAcid) {
    const StepMask requiredFour = static_cast<StepMask>(
        stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12));
    assert((occupancy.roleMasks[0][kick] & requiredFour) == requiredFour);
  }

  if (definition.key == Audition::Archetype::ClassicTwoStep ||
      definition.key == Audition::Archetype::TwoStepRoll) {
    const StepMask backbeatMask =
        static_cast<StepMask>(stepBit(4) | stepBit(12));
    assert((occupancy.roleMasks[0][backbeat] & backbeatMask) == backbeatMask);
    assert((occupancy.roleMasks[0][kick] & backbeatMask) == 0);
  }

  if (definition.key == Audition::Archetype::SparseSkank) {
    assert((occupancy.roleMasks[0][backbeat] & stepBit(8)) != 0);
    const StepMask protectedSteps =
        static_cast<StepMask>(stepBit(4) | stepBit(12));
    const uint8_t bass = static_cast<uint8_t>(RhythmRole::BassRhythm);
    const uint8_t chord = static_cast<uint8_t>(RhythmRole::ChordRhythm);
    assert((occupancy.roleMasks[0][kick] & protectedSteps) == 0);
    assert((occupancy.roleMasks[0][bass] & protectedSteps) == 0);
    assert((occupancy.roleMasks[0][chord] & protectedSteps) == 0);
  }
}

void testCatalogAndRealizationCorpus() {
  assert(Audition::definitionCount() == 5);
  assert(validateRhythmCatalog(Audition::catalog()));

  for (uint8_t definitionIndex = 0;
       definitionIndex < Audition::definitionCount();
       ++definitionIndex) {
    const Audition::Definition& definition =
        Audition::definition(definitionIndex);
    const RhythmArchetype& archetype =
        archetypeFor(definition.archetypeId);

    uint64_t signatures[32]{};
    uint32_t p2GhostEvents = 0;
    uint32_t p3SecondaryEvents = 0;

    for (uint32_t seed = 1; seed <= 32; ++seed) {
      const RhythmRealizationResult p1 = realize(
          definition, seed, RealizationLevel::P1Canonical);
      assertPlanLegal(archetype, p1);
      assertArchetypeIdentityRules(definition, p1.plan);
      signatures[seed - 1] = signature(p1.plan);

      const RhythmRealizationResult p2 = realize(
          definition, seed, RealizationLevel::P2Variation, &p1.identity);
      assertPlanLegal(archetype, p2);
      assert(identityEqual(p1.identity, p2.identity));
      assertArchetypeIdentityRules(definition, p2.plan);

      const RhythmRealizationResult p3 = realize(
          definition, seed, RealizationLevel::P3Transformation, &p1.identity);
      assertPlanLegal(archetype, p3);
      assert(identityEqual(p1.identity, p3.identity));
      assertArchetypeIdentityRules(definition, p3.plan);

      for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
        StepMask ghosts = p2.plan.bars[0].roles[role].ghosts;
        StepMask secondary = p3.plan.bars[0].roles[role].secondary;
        while (ghosts) {
          ghosts = static_cast<StepMask>(ghosts & (ghosts - 1u));
          ++p2GhostEvents;
        }
        while (secondary) {
          secondary = static_cast<StepMask>(secondary & (secondary - 1u));
          ++p3SecondaryEvents;
        }
      }
    }

    // The listening harness is useless if a reference grammar collapses to
    // one preset or if P2/P3 cannot exercise their intended variation lanes.
    assert(distinctCount(signatures, 32) >= 2);
    assert(p2GhostEvents > 0);
    assert(p3SecondaryEvents > 0);
  }
}

}  // namespace

int main() {
  testCatalogAndRealizationCorpus();
  return 0;
}
