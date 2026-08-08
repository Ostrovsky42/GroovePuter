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
    hash = mix(hash, relationship.zoneMask);
    hash = mix(hash, static_cast<uint8_t>(relationship.minOffset + 32));
    hash = mix(hash, static_cast<uint8_t>(relationship.maxOffset + 32));
  }
  return hash;
}

bool identityEqual(const PhraseRhythmIdentity& lhs,
                   const PhraseRhythmIdentity& rhs) {
  return std::memcmp(&lhs, &rhs, sizeof(PhraseRhythmIdentity)) == 0;
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
  assert(vocabulary.archetypeCount == 20);
  assert(definitionCount() == 20);
  assert(validateRhythmCatalog(vocabulary));

  std::array<uint64_t, 20> grammarFingerprints{};
  uint8_t familyPresence[static_cast<uint8_t>(RhythmFamily::Count)]{};
  uint16_t totalRelationships = 0;
  uint8_t durationAwareArchetypes = 0;

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
    assert(archetype->relationshipCount >= 1);
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
      assert(std::memcmp(&p3Repeat.plan, &p3.plan,
                         sizeof(RhythmPhrasePlan)) == 0);
      assert(identityEqual(p3Repeat.identity, p3.identity));
    }

    const uint8_t distinct = distinctCount(identities);
    std::fprintf(stderr, "stage3 %-20s valid=64 distinct=%u\n",
                 def.name, static_cast<unsigned>(distinct));

    // Stage 3 prefers controlled variation over novelty, but no reference
    // archetype may collapse to a single P1 identity across the seed corpus.
    assert(distinct >= 2);
  }

  uint8_t familiesUsed = 0;
  for (uint8_t count : familyPresence) {
    if (count != 0) ++familiesUsed;
  }
  assert(familiesUsed >= 6);
  assert(totalRelationships >= 30);
  assert(durationAwareArchetypes >= 12);

  return 0;
}
