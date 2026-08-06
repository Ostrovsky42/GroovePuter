#pragma once

#include <cstdint>

#include "atlas_runtime.h"
#include "genre_sparse_repair.h"
#include "miniacid_engine.h"

namespace GenreMaterializer {

struct Result {
  bool ok = false;
  bool usedAtlas = false;
  uint8_t variation = 0;
  AtlasRuntimeMetadata metadata{};

  explicit operator bool() const { return ok; }
};

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

    GenreSparseRepair::applySparseLeadContract(
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
  GenreSparseRepair::applySparseLeadContract(
      genre, recipe, 0, engine.editSynthPattern(1));
  result.ok = true;
  return result;
}

}  // namespace GenreMaterializer
