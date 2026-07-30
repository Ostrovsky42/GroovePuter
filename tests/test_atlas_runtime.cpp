#include "../src/dsp/atlas_runtime.h"

#include <cassert>
#include <cstring>

namespace {

int countSynthNotes(const SynthPattern& pattern) {
  int count = 0;
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (pattern.steps[step].note >= 0) ++count;
  }
  return count;
}

int countDrumHits(const DrumPatternSet& pattern) {
  int count = 0;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      if (pattern.voices[voice].steps[step].hit) ++count;
    }
  }
  return count;
}

void validateRanges(const SynthPattern& pattern) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& value = pattern.steps[step];
    assert(value.note >= -1 && value.note <= 127);
    assert(value.velocity >= 1 && value.velocity <= 127);
    assert(value.timing >= -23 && value.timing <= 23);
    assert(value.probability <= 100);
  }
}

void validateRanges(const DrumPatternSet& pattern) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& value = pattern.voices[voice].steps[step];
      assert(value.velocity >= 1 && value.velocity <= 127);
      assert(value.timing >= -23 && value.timing <= 23);
      assert(value.probability <= 100);
    }
  }
}

struct RecipeExpectation {
  uint8_t id;
  const char* atlasId;
  uint16_t bpm;
  uint8_t swing;
  int minSynthA;
  int minSynthB;
  int minDrums;
};

constexpr RecipeExpectation kRecipes[] = {
    {6, "REC_ACID_CHICAGO_JACK", 124, 52, 8, 3, 15},
    {7, "REC_ACID_ROLLING", 128, 54, 8, 3, 15},
    {8, "REC_UKG_CLASSIC_2STEP", 134, 66, 3, 3, 16},
    {9, "REC_UKG_DARK_SKIPPY", 136, 68, 3, 3, 16},
    {10, "REC_DUB_DEEP_CHORD", 120, 54, 2, 3, 10},
    {11, "REC_DUB_MINIMAL_SPACE", 116, 51, 2, 3, 9},
};

}  // namespace

int main() {
  for (const RecipeExpectation& expected : kRecipes) {
    assert(AtlasRuntime::hasRecipe(expected.id));
    assert(AtlasRuntime::variationCount(expected.id) == 3);

    for (uint8_t variation = 0; variation < 3; ++variation) {
      SynthPattern synthA{};
      SynthPattern synthB{};
      DrumPatternSet drums{};
      AtlasRuntimeMetadata metadata{};

      assert(AtlasRuntime::applyRecipe(
          expected.id, variation, synthA, synthB, drums, &metadata));
      assert(metadata.atlasRecipeId != nullptr);
      assert(std::strcmp(metadata.atlasRecipeId, expected.atlasId) == 0);
      assert(metadata.slotId != nullptr);
      assert(metadata.bpm == expected.bpm);
      assert(metadata.swingPercent == expected.swing);
      assert(countSynthNotes(synthA) >= expected.minSynthA);
      assert(countSynthNotes(synthB) >= expected.minSynthB);
      assert(countDrumHits(drums) >= expected.minDrums);
      validateRanges(synthA);
      validateRanges(synthB);
      validateRanges(drums);
    }
  }

  SynthPattern sentinelA{};
  SynthPattern sentinelB{};
  DrumPatternSet sentinelDrums{};
  sentinelA.steps[0].note = 55;
  assert(!AtlasRuntime::applyRecipe(
      250, 0, sentinelA, sentinelB, sentinelDrums, nullptr));
  assert(sentinelA.steps[0].note == 55);

  return 0;
}
