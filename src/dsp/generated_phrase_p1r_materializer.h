#pragma once

#include "atlas_runtime.h"
#include "mode_manager.h"
#include "phrase_generator.h"
#include "src/generation/migration/phrase_execution.h"
#include "src/state/generation_request_state.h"

#include <array>
#include <cstdint>

namespace GeneratedPhraseP1R {

constexpr int kLogicalPhraseAttemptChannel = 0xFFFF;
static_assert(kMaxGlobalPatterns < kLogicalPhraseAttemptChannel,
              "I1 logical phrase attempt channel must not be a physical pattern address");

enum class PreparationDisposition : uint8_t {
  LegacyRoute = 0,
  Ready,
  Failed,
};

struct PreparationEvidence {
  bool usedP1r = false;
  bool attemptAvailable = false;
  GroovePuterRhythm::PhraseExecutionStatus executionStatus =
      GroovePuterRhythm::PhraseExecutionStatus::Count;
  GroovePuterRhythm::StrongRhythmMigrationStatus materializationStatus =
      GroovePuterRhythm::StrongRhythmMigrationStatus::Legacy;
  uint16_t phraseGenerationIdentity =
      GroovePuterRhythm::kUnspecifiedPhraseGenerationIdentity;
  uint32_t generationAttemptOrdinal = 0;
  GroovePuterRhythm::ProgressionId progression =
      GroovePuterRhythm::ProgressionId::Auto;
  uint8_t harmonicEventPositions = 0;
};

inline uint16_t phraseIdentityForAttempt(uint32_t ordinal) {
  return static_cast<uint16_t>(
      ordinal % static_cast<uint32_t>(
          GroovePuterRhythm::kUnspecifiedPhraseGenerationIdentity));
}

inline GroovePuterRhythm::PhraseExecutionMaterializationSettings
materializationSettingsFor(const Scene& scene,
                           GroovePuterRhythm::RealizationLevel level,
                           uint32_t generationAttemptOrdinal) {
  GroovePuterRhythm::PhraseExecutionMaterializationSettings settings{};
  settings.level = level;
  settings.generationAttemptOrdinal = generationAttemptOrdinal;
  settings.feelProfile = static_cast<GroovePuterRhythm::FeelProfileId>(
      scene.feel.timingProfile);

  float feelAmount = scene.generatorParams.microTimingAmount;
  if (feelAmount < 0.0f) feelAmount = 0.0f;
  if (feelAmount > 1.0f) feelAmount = 1.0f;
  settings.feelAmount = static_cast<uint8_t>(feelAmount * 100.0f + 0.5f);

  int root = scene.generatorParams.scaleRoot % 12;
  if (root < 0) root += 12;
  settings.tonalMaterializationEnabled = true;
  settings.rootPitchClass = static_cast<uint8_t>(root);
  settings.scaleTypeValue =
      static_cast<GroovePuterRhythm::ScaleTypeValue>(scene.generatorParams.scale);
  return settings;
}

inline GenreBehavior synthBehaviorForVoice(GenreBehavior behavior,
                                           GenerativeMode mode,
                                           int voiceIndex) {
  if (mode != GenerativeMode::Reggae) return behavior;
  if (voiceIndex <= 0) {
    behavior.stepMask = 0x1111;
    behavior.motifLength = 2;
    behavior.avoidClusters = true;
    behavior.forceOctaveJump = false;
  } else {
    behavior.stepMask = 0xAAAA;
    behavior.motifLength = 4;
    behavior.avoidClusters = false;
    behavior.forceOctaveJump = false;
  }
  return behavior;
}

inline bool prepareDestinationIndependentPitchSource(
    MiniAcid& engine,
    const GroovePuterRhythm::PreparedPhraseExecution& execution,
    PhraseGenerator::PhraseBar& destination) {
  destination = PhraseGenerator::PhraseBar{};

  auto& genreManager = engine.genreManager();
  const GenreRecipeId recipe = genreManager.recipe();
  const GenerativeMode activeGenre = genreManager.generativeMode();
  if (AtlasRuntime::hasRecipe(recipe)) {
    return AtlasRuntime::applyRecipe(
        recipe, 0, destination.synthA, destination.synthB,
        destination.drums, nullptr);
  }

  const GenerativeParams params = genreManager.getCompiledGenerativeParams();
  const GenreBehavior behavior = genreManager.getBehavior();
  GrooveboxModeManager scratchMode(engine);
  scratchMode.setModeLocal(
      GenreManager::grooveboxModeForRecipe(recipe, activeGenre));
  scratchMode.setFlavorLocal(engine.modeManager().flavor());
  scratchMode.setGenerationSeed(
      execution.selection.realizationGeneration.projectSeed);
  scratchMode.generatePattern(
      destination.synthA,
      engine.bpm(),
      params,
      synthBehaviorForVoice(behavior, activeGenre, 0),
      0);
  scratchMode.generatePattern(
      destination.synthB,
      engine.bpm(),
      params,
      synthBehaviorForVoice(behavior, activeGenre, 1),
      1);
  scratchMode.generateDrumPattern(destination.drums, params, behavior);
  return true;
}

inline bool materializePreparedBars(
    const GroovePuterRhythm::PreparedPhraseExecution& execution,
    const PhraseGenerator::PhraseBar& pitchSource,
    int pageIndex,
    int firstLocalSlot,
    std::array<PhraseGenerator::PhraseBar, 8>& destination,
    PreparationEvidence& evidence) {
  if (execution.status != GroovePuterRhythm::PhraseExecutionStatus::Ready ||
      pageIndex < 0 || pageIndex >= kMaxPages ||
      firstLocalSlot < 0 ||
      firstLocalSlot + execution.length.effectivePhraseBars > kPatternsPerPage) {
    evidence.materializationStatus =
        GroovePuterRhythm::StrongRhythmMigrationStatus::InvalidContext;
    return false;
  }

  for (uint8_t bar = 0; bar < execution.length.effectivePhraseBars; ++bar) {
    destination[bar] = pitchSource;
    const int localSlot = firstLocalSlot + bar;
    const int bank = localSlot / Bank<SynthPattern>::kPatterns;
    const int index = localSlot % Bank<SynthPattern>::kPatterns;
    const int globalPattern = songPatternFromPageBankIndex(
        pageIndex, bank, index);
    const auto result = GroovePuterRhythm::materializePreparedPhraseBar(
        execution,
        bar,
        static_cast<int16_t>(globalPattern),
        destination[bar].drums,
        destination[bar].synthA,
        destination[bar].synthB);
    evidence.materializationStatus = result.status;
    if (result.status != GroovePuterRhythm::StrongRhythmMigrationStatus::Applied)
      return false;
  }
  return true;
}

inline PreparationDisposition prepare(
    MiniAcid& engine,
    const Scene& scene,
    const GenreSettings& genre,
    uint8_t bars,
    int pageIndex,
    int firstLocalSlot,
    std::array<PhraseGenerator::PhraseBar, 8>& destination,
    PreparationEvidence& evidence) {
  evidence = PreparationEvidence{};
  if (GroovePuterRhythm::selectStrongRhythmRoute(genre) ==
      GroovePuterRhythm::StrongRhythmRoute::Legacy) {
    return PreparationDisposition::LegacyRoute;
  }

  evidence.usedP1r = true;
  const auto level = GroovePuterState::currentGenerationLevel();
  const auto attempt = GroovePuterState::allocateGenerationAttempt(
      genre.generativeMode,
      genre.recipe,
      level,
      kLogicalPhraseAttemptChannel);
  if (!attempt.ok()) {
    evidence.executionStatus =
        GroovePuterRhythm::PhraseExecutionStatus::InvalidContext;
    return PreparationDisposition::Failed;
  }
  evidence.attemptAvailable = true;
  evidence.generationAttemptOrdinal = attempt.ordinal;
  evidence.phraseGenerationIdentity = phraseIdentityForAttempt(attempt.ordinal);

  GroovePuterRhythm::PhraseExecutionScratch scratch{};
  GroovePuterRhythm::PreparedPhraseExecution execution{};
  evidence.executionStatus = GroovePuterRhythm::preparePhraseExecution(
      genre,
      materializationSettingsFor(scene, level, attempt.ordinal),
      evidence.phraseGenerationIdentity,
      bars,
      scratch,
      execution);
  if (evidence.executionStatus !=
      GroovePuterRhythm::PhraseExecutionStatus::Ready) {
    return PreparationDisposition::Failed;
  }

  evidence.progression = execution.progressionSource.id;
  evidence.harmonicEventPositions =
      execution.semantic.harmonicTimeline.totalEventPositions;

  PhraseGenerator::PhraseBar pitchSource{};
  if (!prepareDestinationIndependentPitchSource(engine, execution, pitchSource)) {
    evidence.materializationStatus =
        GroovePuterRhythm::StrongRhythmMigrationStatus::MaterializationFailed;
    return PreparationDisposition::Failed;
  }

  if (!materializePreparedBars(
          execution,
          pitchSource,
          pageIndex,
          firstLocalSlot,
          destination,
          evidence)) {
    return PreparationDisposition::Failed;
  }

  return PreparationDisposition::Ready;
}

}  // namespace GeneratedPhraseP1R
