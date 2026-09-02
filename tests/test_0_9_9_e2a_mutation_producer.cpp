#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

struct OperationCounts {
  uint16_t keep = 0;
  uint16_t add = 0;
  uint16_t drop = 0;
  uint16_t displace = 0;
  uint16_t accent = 0;
  uint16_t ghost = 0;
};

OperationCounts countOperations(const RhythmMutationDelta* deltas,
                                uint16_t count) {
  OperationCounts result{};
  for (uint16_t i = 0; i < count; ++i) {
    switch (deltas[i].operation) {
      case RhythmMutationOp::KEEP: ++result.keep; break;
      case RhythmMutationOp::ADD: ++result.add; break;
      case RhythmMutationOp::DROP: ++result.drop; break;
      case RhythmMutationOp::DISPLACE: ++result.displace; break;
      case RhythmMutationOp::ACCENT: ++result.accent; break;
      case RhythmMutationOp::GHOST: ++result.ghost; break;
      case RhythmMutationOp::Count: assert(false); break;
    }
  }
  return result;
}

uint32_t sequenceFingerprint(const RhythmMutationDelta* deltas,
                             uint16_t count) {
  uint32_t hash = 2166136261u;
  for (uint16_t i = 0; i < count; ++i) {
    const uint8_t bytes[] = {
        static_cast<uint8_t>(deltas[i].operation),
        static_cast<uint8_t>(deltas[i].role),
        deltas[i].sourceStep,
        deltas[i].targetStep,
    };
    for (uint8_t byte : bytes) {
      hash ^= byte;
      hash *= 16777619u;
    }
  }
  return hash;
}

bool sameSequence(const RhythmMutationDelta* first,
                  const RhythmMutationDelta* second,
                  uint16_t count) {
  for (uint16_t i = 0; i < count; ++i) {
    if (first[i].operation != second[i].operation ||
        first[i].role != second[i].role ||
        first[i].sourceStep != second[i].sourceStep ||
        first[i].targetStep != second[i].targetStep) {
      return false;
    }
  }
  return true;
}

bool containsDelta(const RhythmMutationDelta* deltas,
                   uint16_t count,
                   RhythmMutationOp operation,
                   uint8_t source,
                   uint8_t target) {
  for (uint16_t i = 0; i < count; ++i) {
    if (deltas[i].operation == operation &&
        deltas[i].sourceStep == source &&
        deltas[i].targetStep == target) {
      return true;
    }
  }
  return false;
}

void assertCanonicalSequence(const RhythmMutationDelta* deltas,
                             uint16_t count) {
  assert(count <= kMaxRhythmMutationDeltasPerBar);
  for (uint16_t i = 0; i < count; ++i) {
    assert(rhythmMutationDeltaShapeValid(deltas[i]));
    assert(deltas[i].operation != RhythmMutationOp::KEEP);
    if (deltas[i].operation == RhythmMutationOp::DISPLACE) {
      assert(deltas[i].sourceStep != deltas[i].targetStep);
      assert(rhythmMutationDisplacementDistance(
                 deltas[i].sourceStep, deltas[i].targetStep) <=
             kDisplaceRadius);
      assert(!(deltas[i].sourceStep == 15 && deltas[i].targetStep == 0));
      assert(!(deltas[i].sourceStep == 0 && deltas[i].targetStep == 15));
    }
    if (i != 0) {
      assert(rhythmMutationDeltaLess(deltas[i - 1], deltas[i]));
    }
  }
}

RhythmPhrasePlan canonicalPlanFor(const RhythmArchetype& archetype,
                                  const RhythmCatalogView& catalog) {
  RhythmRealizationRequest request{};
  request.catalog = &catalog;
  request.archetypeId = archetype.id;
  request.phraseBars = 1;
  request.level = RealizationLevel::P1Canonical;
  request.generation.projectSeed = 0xE2A09901u;
  request.generation.phraseOrdinal = 7;
  const RhythmRealizationResult realized = realizeRhythmPhrase(request);
  assert(realized.status == RealizationStatus::Ok ||
         realized.status == RealizationStatus::ValidButSparse);
  return realized.plan;
}

OperationCounts runReferenceLevel(const RhythmArchetype& archetype,
                                  const RhythmPhrasePlan& canonical,
                                  RealizationLevel level,
                                  const char* label) {
  RhythmMutationDelta deltas[kMaxRhythmMutationDeltasPerBar]{};
  RhythmMutationProducerRequest request{};
  request.archetype = &archetype;
  request.canonical = &canonical;
  request.current = &canonical;
  request.bar = 0;
  request.roles = kAllRhythmRoles;
  request.function = BarFunction::Statement;
  request.intent = TransformationIntent::Auto;
  request.level = level;
  request.generation.projectSeed = 0xE2A09901u;
  request.generation.phraseOrdinal = 7;

  const RhythmMutationProducerResult produced =
      produceRhythmMutationCandidates(
          request, deltas, kMaxRhythmMutationDeltasPerBar);
  assert(produced.status == RhythmMutationProducerStatus::Ok);
  assert(!produced.truncated);
  assertCanonicalSequence(deltas, produced.count);

  const OperationCounts counts = countOperations(deltas, produced.count);
  std::printf(
      "E2A-CORPUS production level=%s count=%u KEEP=%u ADD=%u DROP=%u "
      "DISPLACE=%u ACCENT=%u GHOST=%u hash=%08x\n",
      label,
      static_cast<unsigned>(produced.count),
      static_cast<unsigned>(counts.keep),
      static_cast<unsigned>(counts.add),
      static_cast<unsigned>(counts.drop),
      static_cast<unsigned>(counts.displace),
      static_cast<unsigned>(counts.accent),
      static_cast<unsigned>(counts.ghost),
      static_cast<unsigned>(sequenceFingerprint(deltas, produced.count)));
  return counts;
}

struct Fixture {
  LaneGrammar lane{};
  ProtectedSpace protectedSpace{};
  AnchorTransformRule transform{};
  RhythmArchetype archetype{};
  RhythmPhrasePlan canonical{};
  RhythmPhrasePlan current{};
};

Fixture contractFixture() {
  Fixture fixture{};
  fixture.lane.role = RhythmRole::Kick;
  fixture.lane.immutableAnchors = stepBit(0);
  fixture.lane.canonicalAnchors = stepBit(4);
  fixture.lane.preferred = static_cast<StepMask>(
      stepBit(5) | stepBit(9) | stepBit(15));
  fixture.lane.optional = static_cast<StepMask>(
      stepBit(2) | stepBit(6) | stepBit(8) | stepBit(10) |
      stepBit(13) | stepBit(14));
  fixture.lane.forbidden = stepBit(7);
  fixture.lane.structuralMin = 2;
  fixture.lane.structuralMax = 8;
  fixture.lane.ornamentMax = 4;

  fixture.protectedSpace.steps = stepBit(8);
  fixture.protectedSpace.affectedRoles = rhythmRoleBit(RhythmRole::Kick);

  fixture.transform.role = RhythmRole::Kick;
  fixture.transform.barFunction = BarFunction::Break;
  fixture.transform.intent = TransformationIntent::Break;
  fixture.transform.suppressibleCanonical = stepBit(4);
  fixture.transform.displaceableCanonical = stepBit(4);

  fixture.archetype.id = 900;
  fixture.archetype.allowedPhraseBars = phraseBarsBit(1);
  fixture.archetype.activeRoles = rhythmRoleBit(RhythmRole::Kick);
  fixture.archetype.lanes = &fixture.lane;
  fixture.archetype.laneCount = 1;
  fixture.archetype.protectedSpaces = &fixture.protectedSpace;
  fixture.archetype.protectedSpaceCount = 1;
  fixture.archetype.anchorTransformRules = &fixture.transform;
  fixture.archetype.anchorTransformRuleCount = 1;
  fixture.archetype.density.structuralMin = 2;
  fixture.archetype.density.structuralPreferred = 4;
  fixture.archetype.density.structuralMax = 8;
  fixture.archetype.density.ornamentMax = 4;

  MutationBudget& budget = fixture.archetype.mutation.level[
      static_cast<uint8_t>(RealizationLevel::P3Transformation)];
  budget.maxAdds = 1;
  budget.maxDrops = 1;
  budget.maxDisplacements = 1;
  budget.maxAccentChanges = 1;
  budget.maxSecondaryAdds = 1;
  budget.maxGhostAdds = 1;
  budget.flags = static_cast<uint16_t>(
      AllowOptionalAdds | AllowPreferredDrops | AllowGhostConversion |
      AllowOptionalDisplace | AllowAccentVariation | AllowBreak);
  budget.allowedIntents = transformationIntentBit(TransformationIntent::Break);

  fixture.canonical.barCount = 1;
  fixture.canonical.level = RealizationLevel::P1Canonical;
  fixture.canonical.bars[0].function = BarFunction::Statement;
  fixture.canonical.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)]
      .structural = static_cast<StepMask>(
          stepBit(0) | stepBit(4) | stepBit(5) | stepBit(15));

  fixture.current = fixture.canonical;
  fixture.current.level = RealizationLevel::P3Transformation;
  fixture.current.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)]
      .ghosts = stepBit(10);
  return fixture;
}

RhythmMutationProducerRequest fixtureRequest(const Fixture& fixture) {
  RhythmMutationProducerRequest request{};
  request.archetype = &fixture.archetype;
  request.canonical = &fixture.canonical;
  request.current = &fixture.current;
  request.bar = 0;
  request.roles = rhythmRoleBit(RhythmRole::Kick);
  request.function = BarFunction::Break;
  request.intent = TransformationIntent::Break;
  request.level = RealizationLevel::P3Transformation;
  request.generation.projectSeed = 0x12345678u;
  request.generation.phraseOrdinal = 12;
  return request;
}

}  // namespace

int main() {
  const RhythmCatalogView& catalog = ReferenceVocabulary::catalog();
  const RhythmArchetype* production =
      ReferenceVocabulary::archetypeFor(
          ReferenceVocabulary::Archetype::BrokenTechno);
  assert(production != nullptr);
  const RhythmPhrasePlan productionCanonical =
      canonicalPlanFor(*production, catalog);

  const OperationCounts p1 = runReferenceLevel(
      *production, productionCanonical,
      RealizationLevel::P1Canonical, "P1");
  const OperationCounts p2 = runReferenceLevel(
      *production, productionCanonical,
      RealizationLevel::P2Variation, "P2");
  const OperationCounts p3 = runReferenceLevel(
      *production, productionCanonical,
      RealizationLevel::P3Transformation, "P3");

  assert(p1.keep == 0 && p1.add == 0 && p1.drop == 0 &&
         p1.displace == 0 && p1.accent == 0 && p1.ghost == 0);
  assert(p2.keep == 0 && p2.add == 0 && p2.drop == 0 &&
         p2.displace == 0 && p2.accent == 0 && p2.ghost > 0);
  assert(p3.keep == 0 && p3.add > 0 && p3.drop == 0 &&
         p3.displace == 0 && p3.accent == 0 && p3.ghost > 0);

  Fixture fixture = contractFixture();
  RhythmMutationProducerRequest request = fixtureRequest(fixture);
  RhythmMutationDelta first[kMaxRhythmMutationDeltasPerBar]{};
  RhythmMutationDelta second[kMaxRhythmMutationDeltasPerBar]{};
  const RhythmMutationProducerResult firstResult =
      produceRhythmMutationCandidates(
          request, first, kMaxRhythmMutationDeltasPerBar);
  const RhythmMutationProducerResult secondResult =
      produceRhythmMutationCandidates(
          request, second, kMaxRhythmMutationDeltasPerBar);
  assert(firstResult.status == RhythmMutationProducerStatus::Ok);
  assert(secondResult.status == RhythmMutationProducerStatus::Ok);
  assert(firstResult.count == secondResult.count);
  assert(!firstResult.truncated && !secondResult.truncated);
  assert(sameSequence(first, second, firstResult.count));
  assertCanonicalSequence(first, firstResult.count);

  const OperationCounts fixtureCounts =
      countOperations(first, firstResult.count);
  assert(fixtureCounts.keep == 0);
  assert(fixtureCounts.add > 0);
  assert(fixtureCounts.drop > 0);
  assert(fixtureCounts.displace > 0);
  assert(fixtureCounts.accent > 0);
  assert(fixtureCounts.ghost > 0);

  for (uint16_t i = 0; i < firstResult.count; ++i) {
    assert(first[i].role == RhythmRole::Kick);
    assert(first[i].sourceStep != 0);
    assert(first[i].sourceStep != 8);
    assert(first[i].targetStep != 0);
    assert(first[i].targetStep != 4);
    assert(first[i].targetStep != 7);
    assert(first[i].targetStep != 8);
  }
  assert(containsDelta(first, firstResult.count,
                       RhythmMutationOp::DROP, 4, kNoMutationStep));
  assert(containsDelta(first, firstResult.count,
                       RhythmMutationOp::DISPLACE, 4, 2));
  assert(containsDelta(first, firstResult.count,
                       RhythmMutationOp::DROP, 10, kNoMutationStep));
  assert(!containsDelta(first, firstResult.count,
                        RhythmMutationOp::DISPLACE, 15, 0));

  std::printf(
      "E2A-CORPUS contract-fixture count=%u KEEP=%u ADD=%u DROP=%u "
      "DISPLACE=%u ACCENT=%u GHOST=%u hash=%08x\n",
      static_cast<unsigned>(firstResult.count),
      static_cast<unsigned>(fixtureCounts.keep),
      static_cast<unsigned>(fixtureCounts.add),
      static_cast<unsigned>(fixtureCounts.drop),
      static_cast<unsigned>(fixtureCounts.displace),
      static_cast<unsigned>(fixtureCounts.accent),
      static_cast<unsigned>(fixtureCounts.ghost),
      static_cast<unsigned>(sequenceFingerprint(first, firstResult.count)));

  // GenerationContext is an input identity, not an E2a cadence/ranking owner.
  RhythmMutationProducerRequest differentGeneration = request;
  differentGeneration.generation.projectSeed ^= 0xFFFFFFFFu;
  differentGeneration.generation.phraseOrdinal += 31u;
  RhythmMutationDelta contextChanged[kMaxRhythmMutationDeltasPerBar]{};
  const RhythmMutationProducerResult contextResult =
      produceRhythmMutationCandidates(
          differentGeneration, contextChanged,
          kMaxRhythmMutationDeltasPerBar);
  assert(contextResult.count == firstResult.count);
  assert(sameSequence(first, contextChanged, firstResult.count));

  // E2b boundary: the current material already contains one non-canonical
  // addition while maxAdds/maxSecondaryAdds are one. E2a must still enumerate
  // another structurally possible ADD; it does not reset or spend a local
  // canonical-relative budget.
  Fixture alreadyMutated = contractFixture();
  alreadyMutated.current.bars[0]
      .roles[static_cast<uint8_t>(RhythmRole::Kick)]
      .secondary = stepBit(9);
  RhythmMutationProducerRequest alreadyMutatedRequest =
      fixtureRequest(alreadyMutated);
  RhythmMutationDelta afterOneAdd[kMaxRhythmMutationDeltasPerBar]{};
  const RhythmMutationProducerResult afterOneAddResult =
      produceRhythmMutationCandidates(
          alreadyMutatedRequest, afterOneAdd,
          kMaxRhythmMutationDeltasPerBar);
  assert(afterOneAddResult.status == RhythmMutationProducerStatus::Ok);
  assert(containsDelta(afterOneAdd, afterOneAddResult.count,
                       RhythmMutationOp::ADD, kNoMutationStep, 2));

  // Canonical anchor DROP/DISPLACE require an explicit existing transform rule.
  Fixture noCanonicalRule = contractFixture();
  noCanonicalRule.archetype.anchorTransformRules = nullptr;
  noCanonicalRule.archetype.anchorTransformRuleCount = 0;
  RhythmMutationProducerRequest noRuleRequest = fixtureRequest(noCanonicalRule);
  RhythmMutationDelta noRule[kMaxRhythmMutationDeltasPerBar]{};
  const RhythmMutationProducerResult noRuleResult =
      produceRhythmMutationCandidates(
          noRuleRequest, noRule, kMaxRhythmMutationDeltasPerBar);
  assert(noRuleResult.status == RhythmMutationProducerStatus::Ok);
  assert(!containsDelta(noRule, noRuleResult.count,
                        RhythmMutationOp::DROP, 4, kNoMutationStep));
  for (uint16_t i = 0; i < noRuleResult.count; ++i) {
    assert(!(noRule[i].operation == RhythmMutationOp::DISPLACE &&
             noRule[i].sourceStep == 4));
  }

  // Bounded output is a canonical prefix and reports truncation without heap
  // allocation or hidden caching.
  RhythmMutationDelta prefix[3]{};
  const RhythmMutationProducerResult prefixResult =
      produceRhythmMutationCandidates(request, prefix, 3);
  assert(prefixResult.status == RhythmMutationProducerStatus::Ok);
  assert(prefixResult.count == 3);
  assert(prefixResult.truncated);
  assert(sameSequence(first, prefix, 3));

  RhythmMutationProducerRequest invalid = request;
  invalid.current = nullptr;
  RhythmMutationDelta invalidOutput[1]{};
  const RhythmMutationProducerResult invalidResult =
      produceRhythmMutationCandidates(invalid, invalidOutput, 1);
  assert(invalidResult.status == RhythmMutationProducerStatus::InvalidRequest);
  assert(invalidResult.count == 0);

  std::puts("E2A canonical rhythm mutation producer: OK");
  return 0;
}
