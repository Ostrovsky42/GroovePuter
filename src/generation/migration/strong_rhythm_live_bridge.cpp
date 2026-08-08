#include "strong_rhythm_live_bridge.h"

#include "../../dsp/miniacid_engine.h"

namespace GroovePuterRhythm {

StrongRhythmMigrationResult regenerateWithStrongRhythmMigration(
    MiniAcid& engine) {
  // Legacy remains the rollback source and still owns synth pitch/timbre,
  // Atlas tempo metadata and every unsupported genre/recipe.
  engine.regeneratePatternsWithGenre();

  StrongRhythmMigrationContext context{};
  context.patternAddress = engine.currentDrumPatternIndex();
  context.level = RealizationLevel::P2Variation;

  return migrateStrongRhythmDrums(
      engine.sceneManager().currentScene().genre,
      context,
      engine.sceneManager().editCurrentDrumPattern());
}

}  // namespace GroovePuterRhythm
