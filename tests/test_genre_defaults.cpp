#include "../src/dsp/genre_catalog.h"

#include <cassert>
#include <cmath>
#include <type_traits>

namespace {

bool inUnitRange(float value) {
  return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

}  // namespace

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

static_assert(std::is_same_v<std::underlying_type_t<TextureMode>, uint8_t>);
static_assert(static_cast<uint8_t>(TextureMode::Clean) == 0);
static_assert(static_cast<uint8_t>(TextureMode::Dub) == 1);
static_assert(static_cast<uint8_t>(TextureMode::LoFi) == 2);
static_assert(static_cast<uint8_t>(TextureMode::Industrial) == 3);
static_assert(static_cast<uint8_t>(TextureMode::Psychedelic) == 4);

static_assert(std::is_same_v<GenreRecipeId, uint8_t>);
static_assert(kBaseRecipeId == 0);
static_assert(kGenerativeModeCount == 9);
static_assert(kTextureModeCount == 5);

int main() {
  const GenerativeParams params{};

  assert(params.minNotes >= 0);
  assert(params.maxNotes >= params.minNotes);
  assert(params.maxNotes <= 16);

  assert(params.minOctave >= 0);
  assert(params.maxOctave >= params.minOctave);
  assert(params.maxOctave <= 127);

  assert(inUnitRange(params.slideProbability));
  assert(inUnitRange(params.accentProbability));
  assert(std::isfinite(params.gateLengthMultiplier));
  assert(params.gateLengthMultiplier >= 0.1f);
  assert(params.gateLengthMultiplier <= 1.0f);

  assert(std::isfinite(params.swingAmount));
  assert(params.swingAmount >= 0.0f);
  assert(params.swingAmount <= 0.66f);
  assert(inUnitRange(params.microTimingAmount));

  assert(params.velocityMin >= 1);
  assert(params.velocityMax >= params.velocityMin);
  assert(params.velocityMax <= 127);

  assert(inUnitRange(params.rootNoteBias));
  assert(inUnitRange(params.ghostProbability));
  assert(inUnitRange(params.chromaticProbability));
  assert(inUnitRange(params.fillProbability));
  assert(inUnitRange(params.drumSyncopation));
  assert(params.drumVoiceCount >= 1);
  assert(params.drumVoiceCount <= 8);

  return 0;
}
