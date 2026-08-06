#include "atlas_runtime.h"

#include "../generated/atlas_runtime.generated.h"

namespace {

const AtlasGenerated::Recipe* findRecipe(uint8_t runtimeRecipeId) {
  for (size_t i = 0; i < AtlasGenerated::kRecipeCount; ++i) {
    if (AtlasGenerated::kRecipes[i].runtimeRecipeId == runtimeRecipeId) {
      return &AtlasGenerated::kRecipes[i];
    }
  }
  return nullptr;
}

bool validatePattern(const AtlasGenerated::Pattern& pattern) {
  if (!pattern.events && pattern.eventCount != 0) return false;
  for (uint16_t i = 0; i < pattern.eventCount; ++i) {
    const AtlasGenerated::Event& event = pattern.events[i];
    if (event.target > 9 || event.step >= SynthPattern::kSteps) return false;
    if (event.velocity < 1 || event.velocity > 127) return false;
    if (event.timing < -23 || event.timing > 23) return false;
    if (event.probability > 100) return false;
    if (event.target >= 8 && (event.note < 0 || event.note > 127)) return false;
  }
  return true;
}

void fillMetadata(const AtlasGenerated::Recipe& recipe,
                  const AtlasGenerated::Pattern& pattern,
                  AtlasRuntimeMetadata& metadata) {
  metadata.atlasRecipeId = recipe.atlasRecipeId;
  metadata.displayName = recipe.displayName;
  metadata.atlasPatternId = pattern.atlasPatternId;
  metadata.slotId = pattern.slotId;
  metadata.slotFunction = pattern.slotFunction;
  metadata.bpm = recipe.bpm;
  metadata.swingPercent = recipe.swingPercent;
}

void clearSynth(SynthPattern& pattern) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step] = SynthStep{};
    pattern.steps[step].note = -1;
    pattern.steps[step].velocity = 100;
    pattern.steps[step].probability = 100;
  }
}

void clearDrums(DrumPatternSet& pattern) {
  pattern.groove = PatternGroove{};
  for (int lane = 0; lane < DrumPatternSet::kMaxLanes; ++lane) {
    pattern.lanes[lane] = AutomationLane{};
  }
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      pattern.voices[voice].steps[step] = DrumStep{};
      pattern.voices[voice].steps[step].velocity = 100;
      pattern.voices[voice].steps[step].probability = 100;
    }
  }
}

}  // namespace

namespace AtlasRuntime {

bool hasRecipe(uint8_t runtimeRecipeId) {
  return findRecipe(runtimeRecipeId) != nullptr;
}

uint8_t variationCount(uint8_t runtimeRecipeId) {
  const AtlasGenerated::Recipe* recipe = findRecipe(runtimeRecipeId);
  return recipe ? recipe->patternCount : 0;
}

bool describeVariation(uint8_t runtimeRecipeId,
                       uint8_t variationIndex,
                       AtlasRuntimeMetadata& metadata) {
  const AtlasGenerated::Recipe* recipe = findRecipe(runtimeRecipeId);
  if (!recipe || variationIndex >= recipe->patternCount) return false;
  const AtlasGenerated::Pattern& pattern = recipe->patterns[variationIndex];
  if (!validatePattern(pattern)) return false;
  fillMetadata(*recipe, pattern, metadata);
  return true;
}

bool applyRecipe(uint8_t runtimeRecipeId,
                 uint8_t variationIndex,
                 SynthPattern& synthA,
                 SynthPattern& synthB,
                 DrumPatternSet& drums,
                 AtlasRuntimeMetadata* metadata) {
  const AtlasGenerated::Recipe* recipe = findRecipe(runtimeRecipeId);
  if (!recipe || variationIndex >= recipe->patternCount) return false;

  const AtlasGenerated::Pattern& pattern = recipe->patterns[variationIndex];
  if (!validatePattern(pattern)) return false;

  clearSynth(synthA);
  clearSynth(synthB);
  clearDrums(drums);

  for (uint16_t i = 0; i < pattern.eventCount; ++i) {
    const AtlasGenerated::Event& event = pattern.events[i];
    const bool accent = (event.flags & AtlasGenerated::kAccent) != 0;
    const bool slide = (event.flags & AtlasGenerated::kSlide) != 0;

    if (event.target < DrumPatternSet::kVoices) {
      DrumStep& step = drums.voices[event.target].steps[event.step];
      step.hit = true;
      step.accent = accent;
      step.velocity = event.velocity;
      step.timing = event.timing;
      step.probability = event.probability;
      continue;
    }

    SynthPattern& destination = event.target == 8 ? synthA : synthB;
    SynthStep& step = destination.steps[event.step];
    step.note = event.note;
    step.accent = accent;
    step.slide = slide;
    step.velocity = event.velocity;
    step.timing = event.timing;
    step.probability = event.probability;
  }

  if (metadata) fillMetadata(*recipe, pattern, *metadata);
  return true;
}

}  // namespace AtlasRuntime
