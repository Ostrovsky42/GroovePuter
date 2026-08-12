#pragma once

#include "../../scenes.h"
#include "atlas_runtime.h"
#include "mode_manager.h"
#include "phrase_generator.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/state/generation_request_state.h"
#include "src/state/scene_revision.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace GeneratedPhraseSong {

inline uint8_t atlasVariationForRole(PhraseGenerator::PhraseBarRole role) {
  switch (role) {
    case PhraseGenerator::PhraseBarRole::Base:
    case PhraseGenerator::PhraseBarRole::Return:
      return 0;
    case PhraseGenerator::PhraseBarRole::MicroVariation:
    case PhraseGenerator::PhraseBarRole::Development:
    case PhraseGenerator::PhraseBarRole::Build:
      return 1;
    case PhraseGenerator::PhraseBarRole::Breakdown:
    case PhraseGenerator::PhraseBarRole::Fill:
    case PhraseGenerator::PhraseBarRole::EndingFill:
      return 2;
  }
  return 0;
}

inline uint32_t phraseSeed(MiniAcid& engine,
                           int pageIndex,
                           int songStart,
                           int bars) {
  uint32_t seed = engine.modeManager().generationSeed();
  seed ^= static_cast<uint32_t>(pageIndex + 1) * 0x9E3779B9u;
  seed ^= static_cast<uint32_t>(songStart + 1) * 0x85EBCA6Bu;
  seed ^= static_cast<uint32_t>(bars + 1) * 0xC2B2AE35u;
  return seed == 0 ? 0x47525048u : seed;
}

inline GroovePuterRhythm::StrongRhythmMigrationContext migrationContextFor(
    const Scene& scene,
    int variationCoordinate) {
  GroovePuterRhythm::StrongRhythmMigrationContext context{};
  context.patternAddress = static_cast<int16_t>(variationCoordinate);
  context.level = GroovePuterState::currentGenerationLevel();
  context.feelProfile = static_cast<GroovePuterRhythm::FeelProfileId>(
      scene.feel.timingProfile);

  float feelAmount = scene.generatorParams.microTimingAmount;
  if (feelAmount < 0.0f) feelAmount = 0.0f;
  if (feelAmount > 1.0f) feelAmount = 1.0f;
  context.feelAmount = static_cast<uint8_t>(feelAmount * 100.0f + 0.5f);

  int root = scene.generatorParams.scaleRoot % 12;
  if (root < 0) root += 12;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = static_cast<uint8_t>(root);
  context.scaleTypeValue = static_cast<GroovePuterRhythm::ScaleTypeValue>(
      scene.generatorParams.scale);
  return context;
}

inline void applyCurrentMigration(
    const Scene& scene,
    const GenreSettings& genre,
    int variationCoordinate,
    PhraseGenerator::PhraseBar& bar) {
  const auto context = migrationContextFor(scene, variationCoordinate);
  (void)GroovePuterRhythm::migrateStrongRhythmMaterial(
      genre, context, bar.drums, bar.synthA, bar.synthB);
}

template <typename Commit>
PhraseGenerator::PhraseResult generate(
    MiniAcid& engine,
    uint8_t bars,
    int songStart,
    Commit&& commit) {
  Scene& scene = engine.sceneManager().currentScene();

  PhraseGenerator::PhraseRequest request{};
  request.bars = bars;
  request.songStart = songStart;
  request.pageIndex = engine.currentPageIndex();
  request.seed = phraseSeed(engine, request.pageIndex, songStart, bars);
  request.forceSingleBarRows = true;

  PhraseGenerator::PhraseResult result{};
  result.bars = bars;
  result.songStart = songStart;

  const GenreSettings genre = scene.genre;
  auto& genreManager = engine.genreManager();
  const GenreRecipeId recipe = genreManager.recipe();
  const GenerativeMode activeGenre = genreManager.generativeMode();
  const GenerativeParams params = genreManager.getCompiledGenerativeParams();
  const GenreBehavior behavior = genreManager.getBehavior();
  const GrooveboxMode mappedMode = GenreManager::grooveboxModeForRecipe(
      recipe, activeGenre);
  const bool atlasPhrase = AtlasRuntime::hasRecipe(recipe) &&
      AtlasRuntime::variationCount(recipe) >= 3;

  PhraseGenerator::PhraseBar proceduralBase{};
  bool proceduralBaseReady = false;
  GrooveboxModeManager scratchMode(engine);
  scratchMode.setModeLocal(mappedMode);
  scratchMode.setFlavorLocal(engine.modeManager().flavor());
  scratchMode.setGenerationSeed(request.seed);

  auto generateBar = [&](PhraseGenerator::PhraseBar& bar,
                         PhraseGenerator::PhraseBarRole role,
                         int barIndex) {
    if (atlasPhrase) {
      const uint8_t variation = atlasVariationForRole(role);
      if (!AtlasRuntime::applyRecipe(
              recipe, variation,
              bar.synthA, bar.synthB, bar.drums, nullptr)) {
        return false;
      }
      // Keep P1/P2/P1/P3 identity deterministic through the current strong
      // rhythm + Stage 15 tonal path by using the Atlas variation as the
      // migration variation coordinate. A Return therefore materializes from
      // exactly the same coordinate as Base.
      applyCurrentMigration(scene, genre, variation, bar);
      return true;
    }

    if (!proceduralBaseReady) {
      scratchMode.generatePattern(
          proceduralBase.synthA, engine.bpm(), params, behavior, 0);
      scratchMode.generatePattern(
          proceduralBase.synthB, engine.bpm(), params, behavior, 1);
      scratchMode.generateDrumPattern(
          proceduralBase.drums, params, behavior);
      applyCurrentMigration(scene, genre, 0, proceduralBase);
      proceduralBaseReady = true;
    }

    // Stage 15 is applied to the base material first. Phrase roles then make
    // bounded relationship edits only: accents, density, fills and octave
    // development. The octave development preserves pitch class, so the tonal
    // materialization contract remains valid across the derived phrase.
    PhraseGenerator::deriveBar(
        proceduralBase, role, request.seed, barIndex, bar);
    return true;
  };

  auto&& commitPrepared = commit;
  commitPrepared([&]() {
    result = PhraseGenerator::generateBarsToSong(
        scene, request, generateBar);
    if (!result) return;

    engine.setSongMode(true);
    engine.setSongPlaybackSlot(std::clamp(scene.activeSongSlot, 0, 1));
    engine.setSongPosition(result.songStart);
  });

  if (result) GroovePuterState::markSceneMutated();
  return result;
}

}  // namespace GeneratedPhraseSong
