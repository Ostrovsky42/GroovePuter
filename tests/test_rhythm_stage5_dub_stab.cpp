#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "src/generation/migration/strong_rhythm_migration.h"

using namespace GroovePuterRhythm;

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

GenreSettings recipeSettings(uint8_t recipe) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(GenerativeMode::Acid);
  settings.recipe = recipe;
  settings.morphTarget = kBaseRecipeId;
  settings.morphAmount = 0;
  return settings;
}

GenreSettings baseAcid() {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(GenerativeMode::Acid);
  settings.recipe = kBaseRecipeId;
  settings.morphTarget = kBaseRecipeId;
  settings.morphAmount = 0;
  return settings;
}

bool equalDrumStep(const DrumStep& a, const DrumStep& b) {
  return a.hit == b.hit && a.accent == b.accent &&
         a.velocity == b.velocity && a.timing == b.timing &&
         a.fx == b.fx && a.fxParam == b.fxParam &&
         a.probability == b.probability;
}

bool equalDrums(const DrumPatternSet& a, const DrumPatternSet& b) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      if (!equalDrumStep(a.voices[voice].steps[step],
                         b.voices[voice].steps[step])) {
        return false;
      }
    }
  }
  if (a.groove.swing != b.groove.swing ||
      a.groove.humanize != b.groove.humanize) {
    return false;
  }
  for (int lane = 0; lane < DrumPatternSet::kMaxLanes; ++lane) {
    if (a.lanes[lane].targetParam != b.lanes[lane].targetParam ||
        a.lanes[lane].nodeCount != b.lanes[lane].nodeCount) {
      return false;
    }
    for (int node = 0; node < AutomationLane::kMaxNodes; ++node) {
      if (a.lanes[lane].nodes[node].step != b.lanes[lane].nodes[node].step ||
          a.lanes[lane].nodes[node].value != b.lanes[lane].nodes[node].value ||
          a.lanes[lane].nodes[node].curveType != b.lanes[lane].nodes[node].curveType) {
        return false;
      }
    }
  }
  return true;
}

bool equalSynthStep(const SynthStep& a, const SynthStep& b) {
  return a.note == b.note && a.slide == b.slide && a.accent == b.accent &&
         a.ghost == b.ghost && a.velocity == b.velocity &&
         a.timing == b.timing && a.fx == b.fx &&
         a.fxParam == b.fxParam && a.probability == b.probability;
}

bool equalSynth(const SynthPattern& a, const SynthPattern& b) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (!equalSynthStep(a.steps[step], b.steps[step])) return false;
  }
  return true;
}

DrumPatternSet sentinelDrums() {
  DrumPatternSet drums{};
  drums.groove.swing = 0.29f;
  drums.groove.humanize = 0.13f;
  drums.lanes[0].targetParam = 2;
  drums.lanes[0].nodeCount = 1;
  drums.lanes[0].nodes[0].step = 6;
  drums.lanes[0].nodes[0].value = 0.44f;
  drums.voices[7].steps[5].hit = true;
  drums.voices[7].steps[5].velocity = 71;
  return drums;
}

SynthPattern legacyStab() {
  SynthPattern pattern{};

  pattern.steps[1].note = 48;
  pattern.steps[1].accent = true;
  pattern.steps[1].velocity = 82;
  pattern.steps[1].timing = -2;
  pattern.steps[1].probability = 97;

  pattern.steps[8].note = 55;
  pattern.steps[8].slide = true;
  pattern.steps[8].velocity = 74;
  pattern.steps[8].timing = 1;
  pattern.steps[8].fx = 2;
  pattern.steps[8].fxParam = 9;

  pattern.steps[14].note = 67;
  pattern.steps[14].ghost = true;
  pattern.steps[14].velocity = 58;
  pattern.steps[14].probability = 61;

  return pattern;
}

StepMask synthOnsets(const SynthPattern& pattern) {
  StepMask mask = 0;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if (pattern.steps[step].note >= 0) {
      mask = static_cast<StepMask>(mask | stepBit(step));
    }
  }
  return mask;
}

void requireLegacySequencePreserved(const SynthPattern& remapped) {
  const SynthStep expected[] = {
      legacyStab().steps[1],
      legacyStab().steps[8],
      legacyStab().steps[14],
  };
  uint8_t eventIndex = 0;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if (remapped.steps[step].note < 0) continue;
    require(equalSynthStep(remapped.steps[step],
                           expected[eventIndex % 3]),
            "Dub adapter changed legacy pitch/performance event data");
    ++eventIndex;
  }
  require(eventIndex > 0, "Dub adapter produced no stab events");
}

void testDubRoute(uint8_t recipe, StrongRhythmRoute expectedRoute) {
  DrumPatternSet drums = sentinelDrums();
  SynthPattern synthB = legacyStab();
  StrongRhythmMigrationContext context{};
  context.patternAddress = 5;
  context.level = RealizationLevel::P2Variation;

  const StrongRhythmMigrationResult result = migrateStrongRhythmMaterial(
      recipeSettings(recipe), context, drums, synthB);

  require(result.status == StrongRhythmMigrationStatus::Applied,
          "approved Dub material route did not apply");
  require(result.route == expectedRoute,
          "approved Dub route changed identity");
  require(result.chordOnsets != 0,
          "Dub reference archetype produced no ChordRhythm onsets");
  require(result.chordRhythmApplied,
          "Dub compatibility adapter did not report chord rhythm application");
  require(synthOnsets(synthB) == result.chordOnsets,
          "Synth B onset topology does not match realized ChordRhythm");
  requireLegacySequencePreserved(synthB);
}

void testDubBindingFailureRollsBackEverything() {
  DrumPatternSet drums = sentinelDrums();
  const DrumPatternSet beforeDrums = drums;
  SynthPattern synthB{};
  const SynthPattern beforeSynthB = synthB;
  StrongRhythmMigrationContext context{};
  context.patternAddress = 3;

  const StrongRhythmMigrationResult result = migrateStrongRhythmMaterial(
      recipeSettings(5), context, drums, synthB);

  require(result.status ==
              StrongRhythmMigrationStatus::CompatibilityBindingFailed,
          "empty Dub pitch source did not fail compatibility binding");
  require(!result.chordRhythmApplied,
          "failed Dub binding incorrectly reported chord application");
  require(equalDrums(drums, beforeDrums),
          "failed Dub binding leaked Vocabulary drums");
  require(equalSynth(synthB, beforeSynthB),
          "failed Dub binding changed Synth B");
}

void testNonDubLeavesSynthBByteForBehavior() {
  DrumPatternSet drums = sentinelDrums();
  SynthPattern synthB = legacyStab();
  const SynthPattern beforeSynthB = synthB;
  StrongRhythmMigrationContext context{};
  context.patternAddress = 7;

  const StrongRhythmMigrationResult result = migrateStrongRhythmMaterial(
      baseAcid(), context, drums, synthB);

  require(result.status == StrongRhythmMigrationStatus::Applied,
          "base Acid drum migration failed through material adapter");
  require(!result.chordRhythmApplied,
          "non-Dub route unexpectedly claimed chord ownership");
  require(result.chordOnsets == 0,
          "non-Dub reference unexpectedly emitted ChordRhythm");
  require(equalSynth(synthB, beforeSynthB),
          "non-Dub migration changed Synth B");
}

}  // namespace

int main() {
  testDubRoute(5, StrongRhythmRoute::DubTechno);
  testDubRoute(10, StrongRhythmRoute::DeepChord);
  testDubBindingFailureRollsBackEverything();
  testNonDubLeavesSynthBByteForBehavior();
  std::puts("Groove Vocabulary Stage 5 Dub stab compatibility: OK");
  return 0;
}
