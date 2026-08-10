#include <cassert>
#include <cstdint>

#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;
using namespace GroovePuterRhythm::ReferenceVocabulary;

namespace {

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

}  // namespace

int main() {
  const RhythmCatalogView& vocabulary = catalog();
  assert(validateRhythmCatalog(vocabulary));

  for (uint8_t index = 0; index < definitionCount(); ++index) {
    const RhythmArchetype* archetype = archetypeFor(definition(index).key);
    assert(archetype != nullptr);

    for (uint32_t seed = 1; seed <= 64; ++seed) {
      const auto p1 = realize(vocabulary, *archetype, seed, index,
                              RealizationLevel::P1Canonical);
      assert(p1.status == RealizationStatus::Ok);

      const auto p2 = realize(vocabulary, *archetype, seed, index,
                              RealizationLevel::P2Variation, &p1.identity);
      const auto p3 = realize(vocabulary, *archetype, seed, index,
                              RealizationLevel::P3Transformation, &p1.identity);
      assert(p2.status == RealizationStatus::Ok);
      assert(p3.status == RealizationStatus::Ok);

      bool p2HasGhost = false;
      bool p3HasSecondary = false;
      for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
        const RoleRhythmPlan& base = p1.plan.bars[0].roles[role];
        const RoleRhythmPlan& variation = p2.plan.bars[0].roles[role];
        const RoleRhythmPlan& transformation = p3.plan.bars[0].roles[role];

        // Stage 2/3 do not own BarEvolution or structural anchor mutation.
        assert(variation.structural == base.structural);
        assert(transformation.structural == base.structural);

        // P2 remains ornament-only in the current reference policy.
        assert(variation.secondary == 0);
        if (variation.ghosts != 0) p2HasGhost = true;

        // Stage 14.1 makes P3 cumulative: it retains an ornament budget while
        // adding bounded secondary onsets. Exact P3 ghost placement is allowed
        // to differ from P2 because each level has an independent variation
        // seed and the secondary pass may consume candidate coordinates first.
        if (transformation.secondary != 0) p3HasSecondary = true;

        // No P-level may remove any onset from the shared structural identity.
        assert((base.structural & ~variation.structural) == 0);
        assert((base.structural & ~transformation.structural) == 0);
      }
      assert(p2HasGhost);
      assert(p3HasSecondary);
    }
  }

  return 0;
}
