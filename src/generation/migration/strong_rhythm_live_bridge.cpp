#include "strong_rhythm_live_bridge.h"

#include "../../dsp/miniacid_engine.h"

namespace GroovePuterRhythm {
namespace {

StrongRhythmMigrationContext liveMigrationContext(MiniAcid& engine) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = engine.currentDrumPatternIndex();
  context.level = RealizationLevel::P2Variation;
  const Scene& scene = engine.sceneManager().currentScene();
  context.feelProfile = static_cast<FeelProfileId>(scene.feel.timingProfile);
  float feelAmount = scene.generatorParams.microTimingAmount;
  if (feelAmount < 0.0f) feelAmount = 0.0f;
  if (feelAmount > 1.0f) feelAmount = 1.0f;
  context.feelAmount = static_cast<uint8_t>(feelAmount * 100.0f + 0.5f);

  // Scene remains the owner of key/scale. Stage 15 receives only a transient
  // compact tonal context and never imports Scene into roles/tonal code.
  int root = scene.generatorParams.scaleRoot % 12;
  if (root < 0) root += 12;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = static_cast<uint8_t>(root);
  context.scaleTypeValue =
      static_cast<ScaleTypeValue>(scene.generatorParams.scale);
  return context;
}

}  // namespace

StrongRhythmMigrationResult regenerateWithStrongRhythmMigration(
    MiniAcid& engine) {
  // Legacy generation remains the rollback/metadata source and still owns
  // timbre, Atlas tempo metadata and every unsupported genre/recipe. For a
  // supported strong-rhythm route, Stage 15 now owns the final semantic pitch.
  engine.regeneratePatternsWithGenre();

  const StrongRhythmMigrationContext context = liveMigrationContext(engine);
  const Scene& scene = engine.sceneManager().currentScene();
  return migrateStrongRhythmMaterial(
      scene.genre,
      context,
      engine.sceneManager().editCurrentDrumPattern(),
      engine.sceneManager().editCurrentSynthPattern(0),
      engine.sceneManager().editCurrentSynthPattern(1));
}

StrongRhythmMigrationResult regenerateDrumsWithStrongRhythmMigration(
    MiniAcid& engine) {
  // Whole-pattern DRUMS generation must preserve its historical fallback and
  // must not regenerate either synth voice.
  engine.randomizeDrumPattern();

  const StrongRhythmMigrationContext context = liveMigrationContext(engine);
  const Scene& scene = engine.sceneManager().currentScene();
  return migrateStrongRhythmDrums(
      scene.genre,
      context,
      engine.sceneManager().editCurrentDrumPattern());
}

}  // namespace GroovePuterRhythm
