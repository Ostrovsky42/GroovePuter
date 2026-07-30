#pragma once

#include "../../scenes.h"

#include <cstdint>

struct AtlasRuntimeMetadata {
  const char* atlasRecipeId = nullptr;
  const char* displayName = nullptr;
  const char* atlasPatternId = nullptr;
  const char* slotId = nullptr;
  const char* slotFunction = nullptr;
  uint16_t bpm = 120;
  uint8_t swingPercent = 50;
};

namespace AtlasRuntime {

bool hasRecipe(uint8_t runtimeRecipeId);
uint8_t variationCount(uint8_t runtimeRecipeId);

// Read variation metadata without touching active or destination patterns.
// The output is assigned only after the recipe, variation and event payload
// have passed the same validation used by applyRecipe().
bool describeVariation(uint8_t runtimeRecipeId,
                       uint8_t variationIndex,
                       AtlasRuntimeMetadata& metadata);

// Apply one compiled P1/P2/P3 Atlas pattern. Validation and materialization
// happen in temporary patterns before all destinations are committed together.
bool applyRecipe(uint8_t runtimeRecipeId,
                 uint8_t variationIndex,
                 SynthPattern& synthA,
                 SynthPattern& synthB,
                 DrumPatternSet& drums,
                 AtlasRuntimeMetadata* metadata = nullptr);

}  // namespace AtlasRuntime
