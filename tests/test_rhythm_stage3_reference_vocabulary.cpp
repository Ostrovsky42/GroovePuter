#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/rhythm/relationship_resolver.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

uint64_t mix(uint64_t hash, uint64_t value) {
  hash ^= value;
  hash *= 1099511628211ull;
  return hash;
}

uint64_t identityFingerprint(const PhraseRhythmIdentity& identity) {
  uint64_t hash = 1469598103934665603ull;
  hash = mix(hash, identity.archetypeId);
  hash = mix(hash, identity.phraseBars);
  for (uint8_t bar = 0; bar < identity.phraseBars; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      hash = mix(hash, identity.structuralCore[bar][role]);
      hash = mix(hash, identity.canonicalCore[bar][role]);
    }
  }
  return hash;
}

uint64_t grammarFingerprint(const RhythmArchetype& archetype) {
  uint64_t hash = 1469598103934665603ull;
  hash = mix(hash, archetype.activeRoles);
  hash = mix(hash, archetype.laneCount);
  for (uint8_t i = 0; i < archetype.laneCount; ++i) {
    const LaneGrammar& lane = archetype.lanes[i];
    hash = mix(hash, static_cast<uint8_t>(lane.role));
    hash = mix(hash, lane.immutableAnchors);
    hash = mix(hash, lane.canonicalAnchors);
    hash = mix(hash, lane.preferred);
    hash = mix(hash, lane.optional);
    hash = mix(hash, lane.forbidden);
    hash = mix(hash, lane.shortGate);
    hash = mix(hash, lane.heldGate);
    hash = mix(hash, lane.tieGate);
    hash = mix(hash, lane.structuralMin);
    hash = mix(hash, lane.structuralMax);
    hash = mix(hash, lane.ornamentMax);
  }
  for (uint8_t i = 0; i < archetype.protectedSpaceCount; ++i) {
    hash = mix(hash, archetype.protectedSpaces[i].steps);
    hash = mix(hash, archetype.protectedSpaces[i].affectedRoles);
  }
  for (uint8_t i = 0; i < archetype.relationshipCount; ++i) {
    const LaneRelationship& relationship = archetype.relationships[i];
    hash = mix(hash, static_cast<uint8_t>(relationship.source));
    hash = mix(hash, static_cast<uint8_t>(relationship.target));
    hash = mix(hash, static_cast<uint8_t>(relationship.op));
    hash = mix(hash, static_cast<uint8_t>(relationship.strength));
    hash = mix(hash, static_cast<uint8_t>(relationship.scope));
    hash = mix(hash, relationship.zoneMask);
    hash = mix(hash, static_cast<uint8_t>(relationship.minOffset + 32));
    hash = mix(hash, static_cast<uint8_t>(relationship.maxOffset + 32));
    hash = mix(hash, relationship.minMatches);
    hash = mix(hash, relationship.maxMatches);
    hash = mix(hash, relationship.minResponsesPerWindow);
    hash = mix(hash, relationship.maxResponsesPerWindow);
    hash = mix(hash, relationship.weight);
  }
  hash = mix(hash, archetype.density.structuralMin);
  hash = mix(hash, archetype.density.structuralPreferred);
  hash = mix(hash, archetype.density.structuralMax);
  hash = mix(hash, archetype.density.ornamentMax);
  hash = mix(hash, static_cast<uint8_t>(archetype.timing.compatibility));
  hash = mix(hash, archetype.timing.sensitiveSteps);
  hash = mix(hash, archetype.timing.affectedRoles);
  return hash;
}

const LaneGrammar* laneFor(const RhythmArchetype& archetype,
                           RhythmRole role) {
  for (uint8_t i = 0; i < archetype.laneCount; ++i) {
    if (archetype.lanes[i].role == role) return &archetype.lanes[i];
  }
  return nullptr;
}

void assertLane(const RhythmArchetype& archetype,
                RhythmRole role,
                StepMask canonical,
                StepMask preferred,
                StepMask optional,
                uint8_t structuralMin,
                uint8_t structuralMax,
                uint8_t ornamentMax = 1) {
  const LaneGrammar* lane = laneFor(archetype, role);
  assert(lane != nullptr);
  assert(lane->immutableAnchors == 0);
  assert(lane->canonicalAnchors == canonical);
  assert(lane->preferred == preferred);
  assert(lane->optional == optional);
  assert(lane->forbidden == 0);
  assert(lane->structuralMin == structuralMin);
  assert(lane->structuralMax == structuralMax);
  assert(lane->ornamentMax == ornamentMax);
}

void assertHardBackbeatKickExclude(const RhythmArchetype& archetype) {
  assert(archetype.relationshipCount == 1);
  const LaneRelationship& relationship = archetype.relationships[0];
  assert(relationship.source == RhythmRole::Backbeat);
  assert(relationship.target == RhythmRole::Kick);
  assert(relationship.op == RelationshipOp::Exclude);
  assert(relationship.strength == ConstraintStrength::Hard);
  assert(relationship.scope == RelationshipScope::BarLocal);
  assert(relationship.zoneMask == kAllSteps);
}

void assertBatch2ProductionContracts() {
  using namespace GroovePuterRhythm::ReferenceVocabulary;

  constexpr RhythmRoleMask drumsOnly =
      rhythmRoleBit(RhythmRole::Kick) |
      rhythmRoleBit(RhythmRole::Backbeat) |
      rhythmRoleBit(RhythmRole::ClosedHat) |
      rhythmRoleBit(RhythmRole::OpenHat) |
      rhythmRoleBit(RhythmRole::Percussion);

  const RhythmArchetype* hard01 = archetypeFor(Archetype::StackedQuarters);
  const RhythmArchetype* hard06 = archetypeFor(Archetype::ElectroBackskip);
  const RhythmArchetype* hard07 = archetypeFor(Archetype::FunkHouseBridge);
  const RhythmArchetype* hard08 = archetypeFor(Archetype::ElectroGapPush);
  assert(hard01 != nullptr);
  assert(hard06 != nullptr);
  assert(hard07 != nullptr);
  assert(hard08 != nullptr);

  assert(hard01->id == 421);
  assert(hard06->id == 422);
  assert(hard07->id == 423);
  assert(hard08->id == 424);

  assert(hard01->family == RhythmFamily::FourFloor);
  assert(hard06->family == RhythmFamily::MachineSyncopation);
  assert(hard07->family == RhythmFamily::Funk16);
  assert(hard08->family == RhythmFamily::HipHopBackbeat);

  for (const RhythmArchetype* archetype : {hard01, hard06, hard07, hard08}) {
    assert(archetype->activeRoles == drumsOnly);
    assert(archetype->laneCount == 5);
    assert(archetype->protectedSpaceCount == 0);
    assert(archetype->allowedPhraseBars == phraseBarsBit(1));
  }

  assertLane(*hard01, RhythmRole::Kick,
             stepBit(0) | stepBit(8),
             stepBit(4) | stepBit(12),
             stepBit(6) | stepBit(14),
             2, 5);
  assertLane(*hard01, RhythmRole::Backbeat,
             stepBit(4) | stepBit(12), 0, 0, 2, 2);
  assertLane(*hard01, RhythmRole::ClosedHat,
             0,
             stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
             stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12),
             2, 6, 2);
  assertLane(*hard01, RhythmRole::OpenHat,
             0, 0, stepBit(6) | stepBit(14), 0, 2);
  assertLane(*hard01, RhythmRole::Percussion,
             0, 0,
             stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
             0, 2, 1);
  assert(hard01->relationshipCount == 0);
  assert(hard01->density.structuralMin == 6);
  assert(hard01->density.structuralPreferred == 9);
  assert(hard01->density.structuralMax == 16);
  assert(hard01->density.ornamentMax == 4);
  assert(hard01->timing.compatibility == TimingCompatibility::StraightOnly);

  assertLane(*hard06, RhythmRole::Kick,
             stepBit(0) | stepBit(10),
             stepBit(3) | stepBit(6) | stepBit(13),
             stepBit(8) | stepBit(15),
             2, 5);
  assertLane(*hard06, RhythmRole::Backbeat,
             stepBit(4) | stepBit(12), 0, 0, 2, 2);
  assertLane(*hard06, RhythmRole::ClosedHat,
             0,
             stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
             stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15),
             2, 6, 2);
  assertLane(*hard06, RhythmRole::OpenHat,
             0, 0, stepBit(6) | stepBit(14), 0, 2);
  assertLane(*hard06, RhythmRole::Percussion,
             0,
             stepBit(1) | stepBit(9),
             stepBit(5) | stepBit(13),
             0, 3, 2);
  assertHardBackbeatKickExclude(*hard06);
  assert(hard06->density.structuralMin == 7);
  assert(hard06->density.structuralPreferred == 10);
  assert(hard06->density.structuralMax == 18);
  assert(hard06->density.ornamentMax == 5);
  assert(hard06->timing.compatibility == TimingCompatibility::StraightOnly);

  assertLane(*hard07, RhythmRole::Kick,
             stepBit(0) | stepBit(8),
             stepBit(3) | stepBit(10) | stepBit(14),
             stepBit(2) | stepBit(6) | stepBit(15),
             2, 5);
  assertLane(*hard07, RhythmRole::Backbeat,
             stepBit(4) | stepBit(12), 0, 0, 2, 2);
  assertLane(*hard07, RhythmRole::ClosedHat,
             0,
             stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
             stepBit(1) | stepBit(5) | stepBit(9) | stepBit(13),
             2, 6, 2);
  assertLane(*hard07, RhythmRole::OpenHat,
             0, 0, stepBit(6) | stepBit(14), 0, 2);
  assertLane(*hard07, RhythmRole::Percussion,
             0,
             stepBit(7) | stepBit(15),
             stepBit(3) | stepBit(11),
             1, 3, 2);
  assert(hard07->relationshipCount == 0);
  assert(hard07->density.structuralMin == 7);
  assert(hard07->density.structuralPreferred == 11);
  assert(hard07->density.structuralMax == 18);
  assert(hard07->density.ornamentMax == 5);
  assert(hard07->timing.compatibility == TimingCompatibility::SwingCompatible);
  assert(hard07->timing.sensitiveSteps ==
         (stepBit(3) | stepBit(7) | stepBit(11) | stepBit(15)));
  assert(hard07->timing.affectedRoles ==
         (rhythmRoleBit(RhythmRole::Kick) |
          rhythmRoleBit(RhythmRole::ClosedHat) |
          rhythmRoleBit(RhythmRole::Percussion)));

  assertLane(*hard08, RhythmRole::Kick,
             stepBit(0) | stepBit(6),
             stepBit(9) | stepBit(14),
             stepBit(3) | stepBit(11) | stepBit(15),
             2, 5);
  assertLane(*hard08, RhythmRole::Backbeat,
             stepBit(4) | stepBit(12), 0, 0, 2, 2);
  assertLane(*hard08, RhythmRole::ClosedHat,
             0,
             stepBit(2) | stepBit(6) | stepBit(10) | stepBit(14),
             stepBit(0) | stepBit(8),
             2, 5, 2);
  assertLane(*hard08, RhythmRole::OpenHat,
             0, 0, stepBit(2) | stepBit(10), 0, 2);
  assertLane(*hard08, RhythmRole::Percussion,
             0,
             stepBit(7) | stepBit(15),
             stepBit(1) | stepBit(9) | stepBit(13),
             0, 3, 2);
  assertHardBackbeatKickExclude(*hard08);
  assert(hard08->density.structuralMin == 6);
  assert(hard08->density.structuralPreferred == 9);
  assert(hard08->density.structuralMax == 17);
  assert(hard08->density.ornamentMax == 5);
  assert(hard08->timing.compatibility == TimingCompatibility::StraightOnly);

  // Hardware audition explicitly admitted HARD_06 and HARD_08 as separate
  // musical identities. Keep a direct regression in addition to the global
  // no-duplicate grammar fingerprint loop below.
  assert(grammarFingerprint(*hard06) != grammarFingerprint(*hard08));

  const Definition* def01 = definitionFor(Archetype::StackedQuarters);
  const Definition* def06 = definitionFor(Archetype::ElectroBackskip);
  const Definition* def07 = definitionFor(Archetype::FunkHouseBridge);
  const Definition* def08 = definitionFor(Archetype::ElectroGapPush);
  assert(def01 != nullptr && std::strcmp(def01->name, "stacked_quarters") == 0);
  assert(def06 != nullptr && std::strcmp(def06->name, "electro_backskip") == 0);
  assert(def07 != nullptr && std::strcmp(def07->name, "funk_house_bridge") == 0);
  assert(def08 != nullptr && std::strcmp(def08->name, "electro_gap_push") == 0);
}

bool identityEqual(const PhraseRhythmIdentity& lhs,
                   const PhraseRhythmIdentity& rhs) {
  if (lhs.archetypeId != rhs.archetypeId ||
      lhs.phraseBars != rhs.phraseBars ||
      lhs.trajectoryId != rhs.trajectoryId ||
      lhs.protectedSpaceCount != rhs.protectedSpaceCount) {
    return false;
  }
  for (uint8_t i = 0; i < lhs.protectedSpaceCount; ++i) {
    if (lhs.protectedSpaces[i].steps != rhs.protectedSpaces[i].steps ||
        lhs.protectedSpaces[i].affectedRoles !=
            rhs.protectedSpaces[i].affectedRoles) {
      return false;
    }
  }
  for (uint8_t bar = 0; bar < kMaxPhraseBars; ++bar) {
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      if (lhs.structuralCore[bar][role] != rhs.structuralCore[bar][role] ||
          lhs.canonicalCore[bar][role] != rhs.canonicalCore[bar][role]) {
        return false;
      }
    }
  }
  return true;
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

uint8_t distinctCount(const std::array<uint64_t, 64>& values) {
  uint8_t distinct = 0;
  for (uint8_t i = 0; i < values.size(); ++i) {
    bool seen = false;
    for (uint8_t j = 0; j < i; ++j) {
      if (values[i] == values[j]) {
        seen = true;
        break;
      }
    }
    if (!seen) ++distinct;
  }
  return distinct;
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
  const PhraseOccupancy occupancy = structuralOccupancy(result.plan);
  assert(hardRelationshipsSatisfied(archetype, occupancy));
}

}  // namespace

int main() {
  using namespace GroovePuterRhythm::ReferenceVocabulary;

  const RhythmCatalogView& vocabulary = catalog();
  assert(vocabulary.archetypeCount == 24);
  assert(definitionCount() == 24);
  assert(validateRhythmCatalog(vocabulary));
  assertBatch2ProductionContracts();

  std::array<uint64_t, 24> grammarFingerprints{};
  uint8_t familyPresence[static_cast<uint8_t>(RhythmFamily::Count)]{};
  uint16_t totalRelationships = 0;
  uint8_t durationAwareArchetypes = 0;
  uint8_t collapsedArchetypes = 0;

  for (uint8_t index = 0; index < definitionCount(); ++index) {
    const Definition& def = definition(index);
    assert(def.name != nullptr);
    assert(def.name[0] != '\0');
    assert(def.archetypeId != kNoArchetypeId);
    assert(def.suggestedBpmMin > 0);
    assert(def.suggestedBpmMin <= def.suggestedBpmMax);

    const RhythmArchetype* archetype = archetypeFor(def.key);
    assert(archetype != nullptr);
    assert(archetype->id == def.archetypeId);
    assert(archetype->family == def.family);
    assert(archetype->allowedPhraseBars == phraseBarsBit(1));
    totalRelationships += archetype->relationshipCount;
    ++familyPresence[static_cast<uint8_t>(archetype->family)];

    bool hasDurationIntent = false;
    for (uint8_t laneIndex = 0; laneIndex < archetype->laneCount; ++laneIndex) {
      const LaneGrammar& lane = archetype->lanes[laneIndex];
      if (lane.shortGate || lane.heldGate || lane.tieGate) {
        hasDurationIntent = true;
      }
    }
    if (hasDurationIntent) ++durationAwareArchetypes;

    grammarFingerprints[index] = grammarFingerprint(*archetype);
    for (uint8_t previous = 0; previous < index; ++previous) {
      assert(grammarFingerprints[index] != grammarFingerprints[previous]);
    }

    std::array<uint64_t, 64> identities{};
    for (uint8_t seedIndex = 0; seedIndex < identities.size(); ++seedIndex) {
      const GenerationContext generation{
          static_cast<uint32_t>(seedIndex + 1u),
          static_cast<uint16_t>(index)};

      RhythmRealizationRequest request{};
      request.catalog = &vocabulary;
      request.archetypeId = archetype->id;
      request.phraseBars = 1;
      request.level = RealizationLevel::P1Canonical;
      request.generation = generation;

      const RhythmRealizationResult p1 = realizeRhythmPhrase(request);
      assertPlanLegal(*archetype, p1);
      assert(p1.identity.archetypeId == archetype->id);
      assert(p1.identity.phraseBars == 1);
      identities[seedIndex] = identityFingerprint(p1.identity);

      request.level = RealizationLevel::P2Variation;
      request.reuseIdentity = &p1.identity;
      const RhythmRealizationResult p2 = realizeRhythmPhrase(request);
      assertPlanLegal(*archetype, p2);
      assert(identityEqual(p1.identity, p2.identity));

      request.level = RealizationLevel::P3Transformation;
      const RhythmRealizationResult p3 = realizeRhythmPhrase(request);
      assertPlanLegal(*archetype, p3);
      assert(identityEqual(p1.identity, p3.identity));

      const RhythmRealizationResult p3Repeat = realizeRhythmPhrase(request);
      assert(p3Repeat.status == p3.status);
      assert(planEqual(p3Repeat.plan, p3.plan));
      assert(identityEqual(p3Repeat.identity, p3.identity));
    }

    const uint8_t distinct = distinctCount(identities);
    std::fprintf(stderr, "reference %-20s valid=64 distinct=%u\n",
                 def.name, static_cast<unsigned>(distinct));
    if (distinct < 2) ++collapsedArchetypes;
  }

  uint8_t familiesUsed = 0;
  for (uint8_t count : familyPresence) {
    if (count != 0) ++familiesUsed;
  }
  assert(collapsedArchetypes == 0);
  assert(familiesUsed >= 7);
  assert(totalRelationships >= 30);
  assert(durationAwareArchetypes >= 12);

  return 0;
}
