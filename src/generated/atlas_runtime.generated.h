#pragma once

#include "atlas_runtime_types.generated.h"
#include "rec_acid_chicago_jack.generated.h"

namespace AtlasGenerated {

inline constexpr Recipe kRecipes[] = {
  kRecipe_REC_ACID_CHICAGO_JACK,
};

inline constexpr size_t kRecipeCount = sizeof(kRecipes) / sizeof(kRecipes[0]);
inline constexpr uint16_t kIgnoredSamplerEvents = 5;
inline constexpr uint16_t kIgnoredUnsupportedTracks = 0;
inline constexpr uint16_t kIgnoredUnrepresentablePitchEvents = 0;

}  // namespace AtlasGenerated
