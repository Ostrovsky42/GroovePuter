#pragma once

#include <cstddef>
#include <cstdint>

#include "genre_manager.h"

namespace GenreVariantCatalog {

// Variant IDs remain the persisted GenreRecipeId values. The UI index is only
// a view into one of these genre-owned lists, so old scenes keep their codec.
inline constexpr GenreRecipeId kAcidVariants[] = {
    kBaseRecipeId, 6, 7,
};
inline constexpr GenreRecipeId kSynthwaveVariants[] = {
    kBaseRecipeId,
};
inline constexpr GenreRecipeId kTechnoVariants[] = {
    kBaseRecipeId,
};
inline constexpr GenreRecipeId kElectroVariants[] = {
    kBaseRecipeId,
};
inline constexpr GenreRecipeId kRaveVariants[] = {
    kBaseRecipeId, 4,
};
inline constexpr GenreRecipeId kDubReggaeVariants[] = {
    kBaseRecipeId, 5, 10, 11,
};
inline constexpr GenreRecipeId kTripHopVariants[] = {
    kBaseRecipeId,
};
inline constexpr GenreRecipeId kBreaksVariants[] = {
    kBaseRecipeId, 1, 2, 3, 8, 9,
};
inline constexpr GenreRecipeId kChipVariants[] = {
    kBaseRecipeId,
};

struct VariantList {
  const GenreRecipeId* ids = nullptr;
  uint8_t count = 0;
};

template <std::size_t N>
constexpr VariantList listOf(const GenreRecipeId (&ids)[N]) {
  return {ids, static_cast<uint8_t>(N)};
}

inline VariantList variantsFor(GenerativeMode genre) {
  switch (genre) {
    case GenerativeMode::Acid: return listOf(kAcidVariants);
    case GenerativeMode::Outrun: return listOf(kSynthwaveVariants);
    case GenerativeMode::Darksynth: return listOf(kTechnoVariants);
    case GenerativeMode::Electro: return listOf(kElectroVariants);
    case GenerativeMode::Rave: return listOf(kRaveVariants);
    case GenerativeMode::Reggae: return listOf(kDubReggaeVariants);
    case GenerativeMode::TripHop: return listOf(kTripHopVariants);
    case GenerativeMode::Broken: return listOf(kBreaksVariants);
    case GenerativeMode::Chip: return listOf(kChipVariants);
  }
  return listOf(kSynthwaveVariants);
}

inline uint8_t variantCount(GenerativeMode genre) {
  return variantsFor(genre).count;
}

inline GenreRecipeId recipeAt(GenerativeMode genre, int index) {
  const VariantList list = variantsFor(genre);
  if (!list.ids || list.count == 0) return kBaseRecipeId;
  if (index < 0) index = 0;
  if (index >= list.count) index = list.count - 1;
  return list.ids[index];
}

inline int indexOf(GenerativeMode genre, GenreRecipeId recipe) {
  const VariantList list = variantsFor(genre);
  for (uint8_t index = 0; index < list.count; ++index) {
    if (list.ids[index] == recipe) return static_cast<int>(index);
  }
  return -1;
}

inline bool isAllowed(GenerativeMode genre, GenreRecipeId recipe) {
  return indexOf(genre, recipe) >= 0;
}

inline const char* genreDisplayName(GenerativeMode genre) {
  switch (genre) {
    case GenerativeMode::Acid: return "Acid";
    case GenerativeMode::Outrun: return "Synthwave";
    case GenerativeMode::Darksynth: return "Techno";
    case GenerativeMode::Electro: return "Electro";
    case GenerativeMode::Rave: return "Rave";
    case GenerativeMode::Reggae: return "Dub / Reggae";
    case GenerativeMode::TripHop: return "TripHop";
    case GenerativeMode::Broken: return "Breaks";
    case GenerativeMode::Chip: return "Chip";
  }
  return "Synthwave";
}

inline const char* recipeDisplayName(GenreRecipeId recipe) {
  switch (recipe) {
    case 0: return "BASE";
    case 1: return "UK Garage";
    case 2: return "Drum&Bass";
    case 3: return "Footwork";
    case 4: return "Psytrance";
    case 5: return "Dub Techno";
    case 6: return "Chicago Jack";
    case 7: return "Rolling Acid";
    case 8: return "Classic 2-Step";
    case 9: return "Dark Skippy";
    case 10: return "Deep Stab";
    case 11: return "Minimal Space";
    default: return "BASE";
  }
}

inline bool sparseLeadProfile(GenerativeMode genre, GenreRecipeId recipe) {
  return genre == GenerativeMode::TripHop ||
         recipe == 5 || recipe == 10 || recipe == 11;
}

// Automatic Atlas phrase grammar. P1 is the statement, P2 development and P3
// the ending/break role. The result is always a valid variation index 0..2.
inline uint8_t variationForPhraseBar(int bars, int barIndex) {
  if (barIndex < 0) barIndex = 0;
  if (bars <= 1) return 0;
  if (bars == 2) {
    static constexpr uint8_t kPlan[2] = {0, 2};
    return kPlan[barIndex < 2 ? barIndex : 1];
  }
  if (bars == 4) {
    static constexpr uint8_t kPlan[4] = {0, 0, 1, 2};
    return kPlan[barIndex < 4 ? barIndex : 3];
  }
  static constexpr uint8_t kPlan[8] = {0, 0, 1, 0, 1, 0, 1, 2};
  return kPlan[barIndex < 8 ? barIndex : 7];
}

}  // namespace GenreVariantCatalog
