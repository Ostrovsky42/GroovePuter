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
  const Scene& scene = engine.sceneManager().currentScene();
  context.feelProfile = static_cast<FeelProfileId>(scene.feel.timingProfile);
  float feelAmount = scene.generatorParams.microTimingAmount;
  if (feelAmount < 0.0f) feelAmount = 0.0f;
  if (feelAmount > 1.0f) feelAmount = 1.0f;
  context.feelAmount = static_cast<uint8_t>(feelAmount * 100.0f + 0.5f);

  return migrateStrongRhythmMaterial(
      scene.genre,
      context,
      engine.sceneManager().editCurrentDrumPattern(),
      engine.sceneManager().editCurrentSynthPattern(0),
      engine.sceneManager().editCurrentSynthPattern(1));
}

}  // namespace GroovePuterRhythm
