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

bool sameVoices(const DrumPatternSet& a, const DrumPatternSet& b) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& x = a.voices[voice].steps[step];
      const DrumStep& y = b.voices[voice].steps[step];
      if (x.hit != y.hit || x.accent != y.accent ||
          x.velocity != y.velocity || x.timing != y.timing ||
          x.fx != y.fx || x.fxParam != y.fxParam ||
          x.probability != y.probability) {
        return false;
      }
    }
  }
  return true;
}

DrumPatternSet sentinel() {
  DrumPatternSet drums{};
  drums.voices[0].steps[3].hit = true;
  drums.voices[0].steps[3].velocity = 73;
  drums.voices[7].steps[14].hit = true;
  drums.voices[7].steps[14].accent = true;
  drums.groove.swing = 0.27f;
  return drums;
}

}  // namespace

int main() {
  GenreSettings invalidMode{};
  invalidMode.generativeMode = 255;
  invalidMode.recipe = kBaseRecipeId;
  require(selectStrongRhythmRoute(invalidMode) == StrongRhythmRoute::Legacy,
          "invalid generative mode must not normalize into Vocabulary");

  GenreSettings acid{};
  acid.generativeMode = static_cast<uint8_t>(GenerativeMode::Acid);
  acid.recipe = kBaseRecipeId;

  StrongRhythmMigrationContext context{};
  context.patternAddress = static_cast<int16_t>(kMaxGlobalPatterns);
  DrumPatternSet before = sentinel();
  DrumPatternSet after = before;
  const StrongRhythmMigrationResult addressResult =
      migrateStrongRhythmDrums(acid, context, after);
  require(addressResult.status == StrongRhythmMigrationStatus::InvalidContext,
          "out-of-range pattern address must be rejected");
  require(sameVoices(before, after),
          "out-of-range context mutated legacy drum voices");
  require(before.groove.swing == after.groove.swing,
          "out-of-range context mutated PatternGroove");

  context.patternAddress = 0;
  context.level = static_cast<RealizationLevel>(255);
  after = before;
  const StrongRhythmMigrationResult levelResult =
      migrateStrongRhythmDrums(acid, context, after);
  require(levelResult.status == StrongRhythmMigrationStatus::InvalidContext,
          "invalid P-level must be rejected");
  require(sameVoices(before, after),
          "invalid P-level mutated legacy drum voices");

  std::puts("Groove Vocabulary Stage 5 invalid-context gates: OK");
  return 0;
}
