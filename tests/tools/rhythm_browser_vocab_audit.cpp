#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/rhythm/rhythm_realizer.h"
#include "src/generation/materialization/pattern_materializer.h"
#include <cstdio>
using namespace GroovePuterRhythm;
int main() {
  const auto ignored = rhythmRoleBit(RhythmRole::BassRhythm) |
      rhythmRoleBit(RhythmRole::ChordRhythm) | rhythmRoleBit(RhythmRole::MelodicRhythm);
  const auto binding = standardDrumPatternBinding(ignored);
  for (int i = 0; i < ReferenceVocabulary::definitionCount(); ++i) {
    const auto& d = ReferenceVocabulary::definition(i);
    RhythmRealizationRequest request{};
    request.catalog = &ReferenceVocabulary::catalog();
    request.archetypeId = d.archetypeId;
    request.phraseBars = 1;
    int success = 0, minHits = 129, maxHits = 0;
    for (uint32_t seed = 1; seed <= 16; ++seed) {
      request.generation = GenerationContext{seed, 0};
      const auto result = realizeRhythmPhrase(request);
      MaterializedPatterns material{};
      const auto status = materializeRhythmPattern(result.plan, binding, material);
      int hits = 0;
      for (const auto& voice : material.drums.voices)
        for (const auto& step : voice.steps) hits += step.hit;
      if ((result.status == RealizationStatus::Ok ||
           result.status == RealizationStatus::ValidButSparse) &&
          status == PatternMaterializeStatus::Ok && hits > 0) ++success;
      if (hits < minHits) minHits = hits;
      if (hits > maxHits) maxHits = hits;
    }
    std::printf("%u %s success=%d/16 hits=%d..%d\n",
                unsigned(d.archetypeId), d.name, success, minHits, maxHits);
  }
}
