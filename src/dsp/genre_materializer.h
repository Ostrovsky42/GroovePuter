#pragma once

#include <cstdint>

#include "atlas_runtime.h"
#include "genre_variant_catalog.h"
#include "miniacid_engine.h"

namespace GenreMaterializer {

struct Result {
  bool ok = false;
  bool usedAtlas = false;
  uint8_t variation = 0;
  AtlasRuntimeMetadata metadata{};

  explicit operator bool() const { return ok; }
};

inline int countSynthNotes(const SynthPattern& pattern) {
  int count = 0;
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (pattern.steps[step].note >= 0) ++count;
  }
  return count;
}

inline void clearSynthStep(SynthStep& step) {
  step = SynthStep{};
  step.note = -1;
  step.velocity = 100;
  step.probability = 100;
}

inline int retentionPriority(const SynthStep& value, int step) {
  int score = static_cast<int>(value.velocity);
  score += static_cast<int>(value.probability) * 4;
  if (value.accent) score += 1024;
  if ((step % 4) == 0) score += 512;
  if (value.slide) score += 128;
  return score;
}

// Allocation-free deterministic repair. It removes the weakest optional event
// first and never adds notes, so a sparse profile cannot become denser.
inline void trimSynthToMax(SynthPattern& pattern, int maxNotes) {
  if (maxNotes < 0) maxNotes = 0;
  if (maxNotes > SynthPattern::kSteps) maxNotes = SynthPattern::kSteps;

  int count = countSynthNotes(pattern);
  while (count > maxNotes) {
    int weakestStep = -1;
    int weakestScore = 0;
    for (int step = 0; step < SynthPattern::kSteps; ++step) {
      const SynthStep& value = pattern.steps[step];
      if (value.note < 0) continue;
      const int score = retentionPriority(value, step);
      if (weakestStep < 0 || score < weakestScore ||
          (score == weakestScore && step > weakestStep)) {
        weakestStep = step;
        weakestScore = score;
      }
    }
    if (weakestStep < 0) break;
    clearSynthStep(pattern.steps[weakestStep]);
    --count;
  }
}

inline int sparseLeadLimit(GenerativeMode genre,
                           GenreRecipeId recipe,
                           uint8_t variation) {
  if (recipe == 10 || recipe == 11) {
    if (variation == 0) return 3;
    if (variation == 1) return 2;
    return recipe == 11 ? 0 : 1;
  }
  if (recipe == 5) return 2;
  if (genre == GenerativeMode::TripHop) return 3;
  return SynthPattern::kSteps;
}

inline void applySparseLeadContract(GenerativeMode genre,
                                    GenreRecipeId recipe,
                                    uint8_t variation,
                                    SynthPattern& synthB) {
  if (!GenreVariantCatalog::sparseLeadProfile(genre, recipe)) return;
  trimSynthToMax(synthB, sparseLeadLimit(genre, recipe, variation));
}

inline Result materializeCurrent(MiniAcid& engine,
                                 GenerativeMode genre,
                                 GenreRecipeId recipe,
                                 uint8_t variation,
                                 bool applyTempo,
                                 uint16_t fallbackBpm) {
  Result result{};
  result.variation = variation;

  if (AtlasRuntime::hasRecipe(recipe)) {
    const uint8_t count = AtlasRuntime::variationCount(recipe);
    if (count == 0) return result;
    if (variation >= count) variation = static_cast<uint8_t>(count - 1);
    result.variation = variation;

    if (!AtlasRuntime::applyRecipe(
            recipe, variation,
            engine.editSynthPattern(0), engine.editSynthPattern(1),
            engine.sceneManager().editCurrentDrumPattern(),
            &result.metadata)) {
      return result;
    }

    applySparseLeadContract(
        genre, recipe, variation, engine.editSynthPattern(1));
    engine.sceneManager().currentScene().feel.swingPct =
        result.metadata.swingPercent;
    if (applyTempo) {
      engine.setBpm(static_cast<float>(result.metadata.bpm));
    }
    result.usedAtlas = true;
    result.ok = true;
    return result;
  }

  if (applyTempo) engine.setBpm(static_cast<float>(fallbackBpm));
  engine.regeneratePatternsWithGenre();
  applySparseLeadContract(
      genre, recipe, 0, engine.editSynthPattern(1));
  result.ok = true;
  return result;
}

}  // namespace GenreMaterializer
