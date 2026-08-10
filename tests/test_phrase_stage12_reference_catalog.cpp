#include <cassert>
#include <cstdint>

#include "src/generation/phrase/phrase_evolution.h"
#include "src/generation/rhythm/reference_phrase_vocabulary.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

constexpr RhythmArchetypeId kStage12Ids[] = {
    404, 413, 414, 415, 416, 417, 418, 420, 712, 714,
};
constexpr RhythmArchetypeId kSubtractiveStage12Ids[] = {
    404, 413, 414, 415, 417, 418, 420, 712, 714,
};
constexpr RhythmArchetypeId kProtectedOneBarIds[] = {
    401, 402, 403, 405, 406, 711,
};
constexpr RhythmArchetypeId kHalftimeSwitchId = 416;

bool contains(const RhythmArchetypeId* ids,
              uint8_t count,
              RhythmArchetypeId id) {
  for (uint8_t index = 0; index < count; ++index) {
    if (ids[index] == id) return true;
  }
  return false;
}

bool isStage12Id(RhythmArchetypeId id) {
  return contains(kStage12Ids,
                  static_cast<uint8_t>(sizeof(kStage12Ids) /
                                       sizeof(kStage12Ids[0])),
                  id);
}

bool isSubtractiveStage12Id(RhythmArchetypeId id) {
  return contains(kSubtractiveStage12Ids,
                  static_cast<uint8_t>(sizeof(kSubtractiveStage12Ids) /
                                       sizeof(kSubtractiveStage12Ids[0])),
                  id);
}

const RhythmArchetype* archetypeForId(const RhythmCatalogView& catalog,
                                      RhythmArchetypeId id) {
  for (uint16_t index = 0; index < catalog.archetypeCount; ++index) {
    if (catalog.archetypes[index].id == id) return &catalog.archetypes[index];
  }
  return nullptr;
}

uint8_t bitCount16(StepMask value) {
  uint8_t count = 0;
  while (value) {
    value = static_cast<StepMask>(value & (value - 1u));
    ++count;
  }
  return count;
}

uint16_t onsetCount(const RhythmBarPlan& bar) {
  uint16_t count = 0;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    count += bitCount16(static_cast<StepMask>(
        bar.roles[role].structural |
        bar.roles[role].secondary |
        bar.roles[role].ghosts));
  }
  return count;
}

bool barTopologyDiffers(const RhythmBarPlan& left,
                        const RhythmBarPlan& right) {
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    if (left.roles[role].structural != right.roles[role].structural ||
        left.roles[role].secondary != right.roles[role].secondary ||
        left.roles[role].ghosts != right.roles[role].ghosts) {
      return true;
    }
  }
  return false;
}

bool segmentTopologyDiffers(const PhraseEvolutionResult& phrase) {
  assert(phrase.barCount == 8);
  for (uint8_t bar = 0; bar < 4; ++bar) {
    if (barTopologyDiffers(phrase.bars[bar], phrase.bars[bar + 4])) {
      return true;
    }
  }
  return false;
}

GenerationContext generationFor(RhythmArchetypeId id) {
  GenerationContext generation{};
  generation.projectSeed = 0x12000000u | id;
  generation.phraseOrdinal = static_cast<uint16_t>(id * 3u + 7u);
  return generation;
}

PhraseEvolutionRequest phraseRequest(const RhythmCatalogView& catalog,
                                     RhythmArchetypeId id,
                                     uint8_t bars,
                                     RealizationLevel level,
                                     TrajectoryId trajectory) {
  PhraseEvolutionRequest request{};
  request.catalog = &catalog;
  request.archetypeId = id;
  request.phraseBars = bars;
  request.level = level;
  request.generation = generationFor(id);
  request.requestedTrajectoryId = trajectory;
  return request;
}

RhythmRealizationResult baseRealization(const RhythmCatalogView& catalog,
                                        RhythmArchetypeId id,
                                        RealizationLevel level) {
  RhythmRealizationRequest request{};
  request.catalog = &catalog;
  request.archetypeId = id;
  request.phraseBars = 4;
  request.level = level;
  request.generation = generationFor(id);
  return realizeRhythmPhrase(request);
}

void testProductionCatalogUntouched() {
  const RhythmCatalogView& production = ReferenceVocabulary::catalog();
  assert(production.trajectoryCount == 1);
  for (uint16_t index = 0; index < production.archetypeCount; ++index) {
    assert(production.archetypes[index].allowedPhraseBars == phraseBarsBit(1));
  }
}

void testCandidateCatalogContracts() {
  const RhythmCatalogView& catalog =
      ReferenceVocabulary::phraseEvolutionCatalog();
  assert(validateRhythmCatalog(catalog));
  assert(catalog.archetypeCount == ReferenceVocabulary::definitionCount());
  assert(catalog.trajectoryCount == 7);

  constexpr TrajectoryId kExpectedTrajectoryIds[] = {1, 2, 3, 5, 6, 7, 8};
  for (uint8_t index = 0; index < catalog.trajectoryCount; ++index) {
    assert(catalog.trajectories[index].id == kExpectedTrajectoryIds[index]);
  }

  const PhraseBarsMask multiBarMask = static_cast<PhraseBarsMask>(
      phraseBarsBit(1) | phraseBarsBit(2) | phraseBarsBit(4));
  for (uint16_t index = 0; index < catalog.archetypeCount; ++index) {
    const RhythmArchetype& archetype = catalog.archetypes[index];
    if (isStage12Id(archetype.id)) {
      assert(archetype.allowedPhraseBars == multiBarMask);

      const MutationBudget& p2 = archetype.mutation.level[
          static_cast<uint8_t>(RealizationLevel::P2Variation)];
      const MutationBudget& p3 = archetype.mutation.level[
          static_cast<uint8_t>(RealizationLevel::P3Transformation)];

      if (isSubtractiveStage12Id(archetype.id)) {
        assert(archetype.trajectoryCount == 7);
        assert(p2.maxDrops == 1);
        assert((p2.flags & AllowReduction) != 0);
        assert((p2.flags & AllowBreak) == 0);
        assert(p2.allowedIntents ==
               transformationIntentBit(TransformationIntent::Reduce));
        assert(p3.maxDrops == 3);
        assert((p3.flags & AllowReduction) != 0);
        assert((p3.flags & AllowTurnaround) != 0);
        assert((p3.flags & AllowBreak) != 0);
      } else {
        assert(archetype.id == kHalftimeSwitchId);
        assert(archetype.trajectoryCount == 5);
        assert(p2.maxDrops == 0);
        assert((p2.flags & AllowReduction) == 0);
        assert((p2.flags & AllowBreak) == 0);
        assert(p2.allowedIntents == 0);
        assert(p3.maxDrops == 0);
        assert((p3.flags & AllowReduction) == 0);
        assert((p3.flags & AllowBreak) == 0);
        assert((p3.flags & AllowTurnaround) != 0);
        assert(p3.allowedIntents ==
               transformationIntentBit(TransformationIntent::Turnaround));
      }
    } else {
      assert(archetype.allowedPhraseBars == phraseBarsBit(1));
      assert(archetype.trajectoryCount == 1);
      for (uint8_t level = 0;
           level < static_cast<uint8_t>(RealizationLevel::Count);
           ++level) {
        assert(archetype.mutation.level[level].maxDrops == 0);
        assert(archetype.mutation.level[level].allowedIntents == 0);
      }
    }
  }

  for (RhythmArchetypeId id : kProtectedOneBarIds) {
    const RhythmArchetype* archetype = archetypeForId(catalog, id);
    assert(archetype != nullptr);
    assert(archetype->allowedPhraseBars == phraseBarsBit(1));
  }

  const RhythmArchetype* shuffled = archetypeForId(catalog, 419);
  assert(shuffled != nullptr);
  assert(shuffled->allowedPhraseBars == phraseBarsBit(1));
}

void testTwoFourEightBarReachability() {
  const RhythmCatalogView& catalog =
      ReferenceVocabulary::phraseEvolutionCatalog();

  for (RhythmArchetypeId id : kStage12Ids) {
    const PhraseEvolutionResult two = evolveMultiBarPhrase(phraseRequest(
        catalog, id, 2, RealizationLevel::P1Canonical, 2));
    assert(two.status == PhraseEvolutionStatus::Ok);
    assert(two.barCount == 2 && two.segmentCount == 1);
    assert(two.bars[1].function == BarFunction::Repeat);

    const TrajectoryId fourTrajectory =
        isSubtractiveStage12Id(id) ? 6 : 5;
    const RealizationLevel fourLevel =
        isSubtractiveStage12Id(id)
            ? RealizationLevel::P2Variation
            : RealizationLevel::P1Canonical;
    const PhraseEvolutionResult four = evolveMultiBarPhrase(phraseRequest(
        catalog, id, 4, fourLevel, fourTrajectory));
    assert(four.status == PhraseEvolutionStatus::Ok);
    assert(four.barCount == 4 && four.segmentCount == 1);
    assert(four.bars[2].function ==
           (isSubtractiveStage12Id(id)
                ? BarFunction::Reduction
                : BarFunction::Repeat));

    const TrajectoryId eightTrajectory =
        isSubtractiveStage12Id(id) ? 8 : 7;
    const PhraseEvolutionResult eight = evolveMultiBarPhrase(phraseRequest(
        catalog,
        id,
        8,
        RealizationLevel::P3Transformation,
        eightTrajectory));
    assert(eight.status == PhraseEvolutionStatus::Ok);
    assert(eight.barCount == 8 && eight.segmentCount == 2);
    assert(eight.segmentTrajectories[0] == eightTrajectory);
    assert(eight.segmentTrajectories[1] == eightTrajectory);
    assert(eight.bars[2].function ==
           (isSubtractiveStage12Id(id)
                ? BarFunction::Break
                : BarFunction::RepeatWithGhosts));
    assert(eight.bars[6].function == eight.bars[2].function);
    assert(eight.variationHistoryMask != 0);
    assert(segmentTopologyDiffers(eight));
  }
}

void testEveryAdvertisedSubtractiveTransformRemovesMaterial() {
  const RhythmCatalogView& catalog =
      ReferenceVocabulary::phraseEvolutionCatalog();

  for (RhythmArchetypeId id : kSubtractiveStage12Ids) {
    const RhythmRealizationResult p2Base = baseRealization(
        catalog, id, RealizationLevel::P2Variation);
    assert(p2Base.status == RealizationStatus::Ok ||
           p2Base.status == RealizationStatus::ValidButSparse);

    const PhraseEvolutionResult reduction = evolveMultiBarPhrase(phraseRequest(
        catalog, id, 4, RealizationLevel::P2Variation, 6));
    assert(reduction.status == PhraseEvolutionStatus::Ok);
    assert(onsetCount(reduction.bars[2]) < onsetCount(p2Base.plan.bars[2]));

    const RhythmRealizationResult p3Base = baseRealization(
        catalog, id, RealizationLevel::P3Transformation);
    assert(p3Base.status == RealizationStatus::Ok ||
           p3Base.status == RealizationStatus::ValidButSparse);

    const PhraseEvolutionResult broken = evolveMultiBarPhrase(phraseRequest(
        catalog, id, 4, RealizationLevel::P3Transformation, 8));
    assert(broken.status == PhraseEvolutionStatus::Ok);
    assert(onsetCount(broken.bars[2]) < onsetCount(p3Base.plan.bars[2]));
  }
}

void testHalftimeSwitchRejectsInertSubtractiveTrajectories() {
  const RhythmCatalogView& catalog =
      ReferenceVocabulary::phraseEvolutionCatalog();

  const PhraseEvolutionResult reduction = evolveMultiBarPhrase(phraseRequest(
      catalog,
      kHalftimeSwitchId,
      4,
      RealizationLevel::P2Variation,
      6));
  assert(reduction.status == PhraseEvolutionStatus::CoreEvolutionFailed);
  assert(reduction.coreStatus == BarEvolutionStatus::NoEligibleTrajectory);

  const PhraseEvolutionResult broken = evolveMultiBarPhrase(phraseRequest(
      catalog,
      kHalftimeSwitchId,
      4,
      RealizationLevel::P3Transformation,
      8));
  assert(broken.status == PhraseEvolutionStatus::CoreEvolutionFailed);
  assert(broken.coreStatus == BarEvolutionStatus::NoEligibleTrajectory);

  const PhraseEvolutionResult response = evolveMultiBarPhrase(phraseRequest(
      catalog,
      kHalftimeSwitchId,
      4,
      RealizationLevel::P1Canonical,
      5));
  assert(response.status == PhraseEvolutionStatus::Ok);

  const PhraseEvolutionResult build = evolveMultiBarPhrase(phraseRequest(
      catalog,
      kHalftimeSwitchId,
      4,
      RealizationLevel::P3Transformation,
      7));
  assert(build.status == PhraseEvolutionStatus::Ok);
}

void testCapabilityApi() {
  for (uint8_t index = 0;
       index < ReferenceVocabulary::definitionCount();
       ++index) {
    const ReferenceVocabulary::Definition& definition =
        ReferenceVocabulary::definition(index);
    assert(ReferenceVocabulary::phraseEvolutionEnabled(definition.key) ==
           isStage12Id(definition.archetypeId));
  }
}

}  // namespace

int main() {
  testProductionCatalogUntouched();
  testCandidateCatalogContracts();
  testTwoFourEightBarReachability();
  testEveryAdvertisedSubtractiveTransformRemovesMaterial();
  testHalftimeSwitchRejectsInertSubtractiveTrajectories();
  testCapabilityApi();
  return 0;
}
