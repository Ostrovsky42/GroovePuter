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

    const char* patternIds[3] = {nullptr, nullptr, nullptr};
    const char* slotIds[3] = {nullptr, nullptr, nullptr};

    for (uint8_t variation = 0; variation < 3; ++variation) {
      AtlasRuntimeMetadata preview{};
      assert(AtlasRuntime::describeVariation(expected.id, variation, preview));
      assert(preview.atlasRecipeId != nullptr);
      assert(std::strcmp(preview.atlasRecipeId, expected.atlasId) == 0);
      assert(preview.displayName != nullptr);
      assert(preview.atlasPatternId != nullptr);
      assert(preview.slotId != nullptr);
      assert(preview.slotFunction != nullptr);
      assert(preview.bpm == expected.bpm);
      assert(preview.swingPercent == expected.swing);
      patternIds[variation] = preview.atlasPatternId;
      slotIds[variation] = preview.slotId;

      SynthPattern synthA{};
      SynthPattern synthB{};
      DrumPatternSet drums{};
      AtlasRuntimeMetadata applied{};

      assert(AtlasRuntime::applyRecipe(
          expected.id, variation, synthA, synthB, drums, &applied));
      assert(std::strcmp(applied.atlasRecipeId, preview.atlasRecipeId) == 0);
      assert(std::strcmp(applied.atlasPatternId, preview.atlasPatternId) == 0);
      assert(std::strcmp(applied.slotId, preview.slotId) == 0);
      assert(std::strcmp(applied.slotFunction, preview.slotFunction) == 0);
      assert(applied.bpm == preview.bpm);
      assert(applied.swingPercent == preview.swingPercent);
      assert(countSynthNotes(synthA) >= expected.minSynthA);
      assert(countSynthNotes(synthB) >= expected.minSynthB);
      assert(countDrumHits(drums) >= expected.minDrums);
      validateRanges(synthA);
      validateRanges(synthB);
      validateRanges(drums);
    }

    for (int lhs = 0; lhs < 3; ++lhs) {
      for (int rhs = lhs + 1; rhs < 3; ++rhs) {
        const bool samePattern = std::strcmp(patternIds[lhs], patternIds[rhs]) == 0;
        const bool sameSlot = std::strcmp(slotIds[lhs], slotIds[rhs]) == 0;
        assert(!(samePattern && sameSlot));
      }
    }
  }

  SynthPattern sentinelA{};
  SynthPattern sentinelB{};
  DrumPatternSet sentinelDrums{};
  sentinelA.steps[0].note = 55;
  sentinelB.steps[1].note = 67;
  sentinelDrums.voices[0].steps[2].hit = true;

  assert(!AtlasRuntime::applyRecipe(
      250, 0, sentinelA, sentinelB, sentinelDrums, nullptr));
  assert(sentinelA.steps[0].note == 55);
  assert(sentinelB.steps[1].note == 67);
  assert(sentinelDrums.voices[0].steps[2].hit);

  assert(!AtlasRuntime::applyRecipe(
      6, 3, sentinelA, sentinelB, sentinelDrums, nullptr));
  assert(sentinelA.steps[0].note == 55);
  assert(sentinelB.steps[1].note == 67);
  assert(sentinelDrums.voices[0].steps[2].hit);

  AtlasRuntimeMetadata sentinelMetadata{};
  sentinelMetadata.displayName = "sentinel";
  sentinelMetadata.bpm = 999;
  assert(!AtlasRuntime::describeVariation(6, 3, sentinelMetadata));
  assert(std::strcmp(sentinelMetadata.displayName, "sentinel") == 0);
  assert(sentinelMetadata.bpm == 999);

  return 0;
}
