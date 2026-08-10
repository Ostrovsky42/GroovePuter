#include <cassert>
#include <cstdint>

#include "src/generation/phrase/phrase_evolution.h"
#include "src/generation/rhythm/reference_phrase_vocabulary.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

constexpr RhythmArchetypeId kStage12Ids[] = {
    404,  // broken_techno
    413,  // two_step_roll
    414,  // ghosted_roll
    415,  // sparse_fast_break
    416,  // halftime_switch
    417,  // classic_2step
    418,  // skippy_2step
    420,  // machine_syncopation
    712,  // electro_backskip
    714,  // electro_gap_push
};

bool isStage12Id(RhythmArchetypeId id) {
  for (RhythmArchetypeId candidate : kStage12Ids) {
    if (candidate == id) return true;
  }
  return false;
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

void testProductionCatalogIsUntouched() {
  const RhythmCatalogView& production = ReferenceVocabulary::catalog();
  assert(production.trajectoryCount == 1);
  assert(production.trajectories[0].barCount == 1);
  for (uint16_t index = 0; index < production.archetypeCount; ++index) {
    assert(production.archetypes[index].allowedPhraseBars == phraseBarsBit(1));
  }
}

void testCandidateCatalogValidates() {
  const RhythmCatalogView& catalog = ReferenceVocabulary::phraseEvolutionCatalog();
  const CatalogValidationResult validation = validateRhythmCatalog(catalog);
  assert(validation);
  assert(catalog.archetypeCount == ReferenceVocabulary::definitionCount());
  assert(catalog.trajectoryCount == 7);

  constexpr TrajectoryId expectedIds[] = {1, 2, 3, 5, 6, 7, 8};
  for (uint8_t index = 0; index < catalog.trajectoryCount; ++index) {
    assert(catalog.trajectories[index].id == expectedIds[index]);
  }
}

void testOnlyEvidenceTargetedArchetypesGainPhraseCapability() {
  const RhythmCatalogView& catalog = ReferenceVocabulary::phraseEvolutionCatalog();
  const PhraseBarsMask multiBarMask = static_cast<PhraseBarsMask>(
      phraseBarsBit(1) | phraseBarsBit(2) | phraseBarsBit(4));

  for (uint16_t index = 0; index < catalog.archetypeCount; ++index) {
    const RhythmArchetype& archetype = catalog.archetypes[index];
    const bool enabled = isStage12Id(archetype.id);
    if (enabled) {
      assert(archetype.allowedPhraseBars == multiBarMask);
      assert(archetype.trajectoryCount == 7);

      const MutationBudget& p2 = archetype.mutation.level[
          static_cast<uint8_t>(RealizationLevel::P2Variation)];
      assert(p2.maxDrops == 1);
      assert((p2.flags & AllowReduction) != 0);
      assert((p2.flags & AllowBreak) == 0);
      assert(p2.allowedIntents ==
             transformationIntentBit(TransformationIntent::Reduce));

      const MutationBudget& p3 = archetype.mutation.level[
          static_cast<uint8_t>(RealizationLevel::P3Transformation)];
      assert(p3.maxDrops == 3);
      assert((p3.flags & AllowReduction) != 0);
      assert((p3.flags & AllowTurnaround) != 0);
      assert((p3.flags & AllowBreak) != 0);
      assert((p3.allowedIntents & transformationIntentBit(
                 TransformationIntent::Reduce)) != 0);
      assert((p3.allowedIntents & transformationIntentBit(
                 TransformationIntent::Turnaround)) != 0);
      assert((p3.allowedIntents & transformationIntentBit(
                 TransformationIntent::Break)) != 0);
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

  // Preserve the strongest current loop identities exactly as one-bar data.
  for (RhythmArchetypeId id : {401, 402, 403, 405, 406, 711}) {
    const RhythmArchetype* archetype = archetypeForId(catalog, id);
    assert(archetype != nullptr);
    assert(archetype->allowedPhraseBars == phraseBarsBit(1));
  }

  // 419 has phrase-wide hard Coincide maxMatches and cannot be admitted by
  // merely repeating its current one-bar contract.
  const RhythmArchetype* shuffled = archetypeForId(catalog, 419);
  assert(shuffled != nullptr);
  assert(shuffled->allowedPhraseBars == phraseBarsBit(1));
}

void testAdmittedArchetypesReachTwoFourAndEightBars() {
  const RhythmCatalogView& catalog = ReferenceVocabulary::phraseEvolutionCatalog();

  for (RhythmArchetypeId id : kStage12Ids) {
    const PhraseEvolutionResult two = evolveMultiBarPhrase(phraseRequest(
        catalog, id, 2, RealizationLevel::P1Canonical, 2));
    assert(two.status == PhraseEvolutionStatus::Ok);
    assert(two.barCount == 2 && two.segmentCount == 1);
    assert(two.bars[1].function == BarFunction::Repeat);

    const PhraseEvolutionResult four = evolveMultiBarPhrase(phraseRequest(
        catalog, id, 4, RealizationLevel::P2Variation, 6));
    assert(four.status == PhraseEvolutionStatus::Ok);
    assert(four.barCount == 4 && four.segmentCount == 1);
    assert(four.bars[2].function == BarFunction::Reduction);
    assert(four.bars[3].function == BarFunction::Return);

    const PhraseEvolutionResult eight = evolveMultiBarPhrase(phraseRequest(
        catalog, id, 8, RealizationLevel::P3Transformation, 8));
    assert(eight.status == PhraseEvolutionStatus::Ok);
    assert(eight.barCount == 8 && eight.segmentCount == 2);
    assert(eight.segmentTrajectories[0] == 8);
    assert(eight.segmentTrajectories[1] == 8);
    assert(eight.bars[2].function == BarFunction::Break);
    assert(eight.bars[6].function == BarFunction::Break);
    assert(eight.variationHistoryMask != 0);
  }
}

void testReductionAndBreakAreActuallySubtractive() {
  const RhythmCatalogView& catalog = ReferenceVocabulary::phraseEvolutionCatalog();

  // Use two rhythmically elastic representatives. Their preferred density is
  // above the structural minimum, so bounded reduction must have removable
  // non-anchor material without attacking the identity anchors.
  for (RhythmArchetypeId id : {413, 417}) {
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
    const PhraseEvolutionResult breakResult = evolveMultiBarPhrase(phraseRequest(
        catalog, id, 4, RealizationLevel::P3Transformation, 8));
    assert(breakResult.status == PhraseEvolutionStatus::Ok);
    assert(onsetCount(breakResult.bars[2]) < onsetCount(p3Base.plan.bars[2]));
  }
}

void testCapabilityApiMatchesDefinitions() {
  for (uint8_t index = 0; index < ReferenceVocabulary::definitionCount(); ++index) {
    const ReferenceVocabulary::Definition& definition =
        ReferenceVocabulary::definition(index);
    assert(ReferenceVocabulary::phraseEvolutionEnabled(definition.key) ==
           isStage12Id(definition.archetypeId));
  }
}

}  // namespace

int main() {
  testProductionCatalogIsUntouched();
  testCandidateCatalogValidates();
  testOnlyEvidenceTargetedArchetypesGainPhraseCapability();
  testAdmittedArchetypesReachTwoFourAndEightBars();
  testReductionAndBreakAreActuallySubtractive();
  testCapabilityApiMatchesDefinitions();
  return 0;
}
