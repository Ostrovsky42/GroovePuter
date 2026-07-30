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

}  // namespace

int main() {
  constexpr uint8_t kChicagoJackRecipe = 6;
  assert(AtlasRuntime::hasRecipe(kChicagoJackRecipe));
  assert(AtlasRuntime::variationCount(kChicagoJackRecipe) == 3);

  for (uint8_t variation = 0; variation < 3; ++variation) {
    SynthPattern synthA{};
    SynthPattern synthB{};
    DrumPatternSet drums{};
    AtlasRuntimeMetadata metadata{};

    assert(AtlasRuntime::applyRecipe(
        kChicagoJackRecipe, variation, synthA, synthB, drums, &metadata));
    assert(metadata.atlasRecipeId != nullptr);
    assert(std::strcmp(metadata.atlasRecipeId, "REC_ACID_CHICAGO_JACK") == 0);
    assert(metadata.slotId != nullptr);
    assert(metadata.bpm == 124);
    assert(metadata.swingPercent == 52);
    assert(countSynthNotes(synthA) >= 8);
    assert(countSynthNotes(synthB) >= 3);
    assert(countDrumHits(drums) >= 10);
    validateRanges(synthA);
    validateRanges(synthB);
    validateRanges(drums);
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
