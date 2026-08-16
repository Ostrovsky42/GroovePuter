#include "src/dsp/genre_manager.h"

#include <cstdint>
#include <type_traits>

static_assert(std::is_same_v<std::underlying_type_t<GenerativeMode>, uint8_t>);
static_assert(static_cast<uint8_t>(GenerativeMode::Acid) == 0);
static_assert(static_cast<uint8_t>(GenerativeMode::Outrun) == 1);
static_assert(static_cast<uint8_t>(GenerativeMode::Darksynth) == 2);
static_assert(static_cast<uint8_t>(GenerativeMode::Electro) == 3);
static_assert(static_cast<uint8_t>(GenerativeMode::Rave) == 4);
static_assert(static_cast<uint8_t>(GenerativeMode::Reggae) == 5);
static_assert(static_cast<uint8_t>(GenerativeMode::TripHop) == 6);
static_assert(static_cast<uint8_t>(GenerativeMode::Broken) == 7);
static_assert(static_cast<uint8_t>(GenerativeMode::Chip) == 8);
static_assert(static_cast<uint8_t>(GenerativeMode::House) == 9);
static_assert(static_cast<uint8_t>(GenerativeMode::Techno) == 10);
static_assert(static_cast<uint8_t>(GenerativeMode::HipHop) == 11);
static_assert(static_cast<uint8_t>(GenerativeMode::FunkSoul) == 12);
static_assert(static_cast<uint8_t>(GenerativeMode::UkGarage) == 13);
static_assert(static_cast<uint8_t>(GenerativeMode::DrumAndBass) == 14);
static_assert(static_cast<uint8_t>(GenerativeMode::LoFi) == 15);
static_assert(kGenerativeModeCount == 16);

static_assert(std::is_same_v<GenreRecipeId, uint8_t>);
static_assert(kBaseRecipeId == 0);
static_assert(kClassicChillRecipeId == 12);
static_assert(kDrunkenGrooveRecipeId == 13);
static_assert(kLoFiHouseRecipeId == 14);
static_assert(kMinimalSleepRecipeId == 15);
static_assert(kGoldenEraRecipeId == 16);
static_assert(kDustyJazzRecipeId == 17);

int main() {
    return 0;
}
