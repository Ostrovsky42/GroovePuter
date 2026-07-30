#pragma once

#include "atlas_runtime_types.generated.h"
#include "rec_acid_chicago_jack.generated.h"
#include "rec_acid_rolling.generated.h"
#include "rec_ukg_classic_2step.generated.h"
#include "rec_ukg_dark_skippy.generated.h"
#include "rec_dub_deep_chord.generated.h"
#include "rec_dub_minimal_space.generated.h"

namespace AtlasGenerated {

inline constexpr Recipe kRecipes[] = {
  kRecipe_REC_ACID_CHICAGO_JACK,
  kRecipe_REC_ACID_ROLLING,
  kRecipe_REC_UKG_CLASSIC_2STEP,
  kRecipe_REC_UKG_DARK_SKIPPY,
  kRecipe_REC_DUB_DEEP_CHORD,
  kRecipe_REC_DUB_MINIMAL_SPACE,
};

inline constexpr size_t kRecipeCount = sizeof(kRecipes) / sizeof(kRecipes[0]);
inline constexpr uint16_t kIgnoredSamplerEvents = 40;
inline constexpr uint16_t kIgnoredUnsupportedTracks = 0;
inline constexpr uint16_t kIgnoredUnrepresentablePitchEvents = 0;

}  // namespace AtlasGenerated
