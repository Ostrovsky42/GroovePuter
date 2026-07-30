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

// Apply one compiled P1/P2/P3 Atlas pattern. Validation happens before any
// destination pattern is modified.
bool applyRecipe(uint8_t runtimeRecipeId,
                 uint8_t variationIndex,
                 SynthPattern& synthA,
                 SynthPattern& synthB,
                 DrumPatternSet& drums,
                 AtlasRuntimeMetadata* metadata = nullptr);

}  // namespace AtlasRuntime
