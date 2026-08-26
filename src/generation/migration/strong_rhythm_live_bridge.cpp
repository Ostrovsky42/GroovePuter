#include "strong_rhythm_live_bridge.h"

#include "../../dsp/miniacid_engine.h"
#include "../../state/generation_request_state.h"
#include "../feel/feel_pattern_adapter.h"
#include "../materialization/pattern_materializer.h"
#include "../phrase/phrase_evolution.h"
#include "../rhythm/reference_phrase_vocabulary.h"

#if defined(ARDUINO_M5STACK_CARDPUTER)
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace GroovePuterRhythm {
namespace {

constexpr int kPhraseAuditionBank = 1;
constexpr int kPhraseAuditionSongSlot = 1;
constexpr uint16_t kPhraseAuditionGenerationIdentity =
    static_cast<uint16_t>(kPhraseAuditionSongSlot);
constexpr uint16_t kM1SparsePhraseIdentity = 19;
constexpr uint16_t kM1CallPhraseIdentity = 7;
constexpr RhythmRoleMask kDeferredSynthRoles =
    rhythmRoleBit(RhythmRole::BassRhythm) |
    rhythmRoleBit(RhythmRole::ChordRhythm) |
    rhythmRoleBit(RhythmRole::MelodicRhythm);

constexpr RhythmArchetypeId kSubtractiveProbeIds[] = {
    404, 413, 414, 415, 417, 418, 420, 712, 714,
};

StrongRhythmMigrationContext liveMigrationContext(MiniAcid& engine) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = engine.currentDrumPatternIndex();
  context.level = GroovePuterState::currentGenerationLevel();
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

bool assignGenerationAttempt(const GenreSettings& settings,
                             StrongRhythmMigrationContext& context,
                             StrongRhythmMigrationResult& failure) {
  failure.route = selectStrongRhythmRoute(settings);
  if (failure.route == StrongRhythmRoute::Legacy) return true;

  const auto allocation = GroovePuterState::allocateGenerationAttempt(
      settings.generativeMode,
      settings.recipe,
      context.level,
      context.patternAddress);
  if (!allocation.ok()) {
    failure.status = StrongRhythmMigrationStatus::AttemptUnavailable;
    return false;
  }
  context.generationAttemptOrdinal = allocation.ordinal;
  return true;
}

uint8_t normalizedPhraseBars(uint8_t bars) {
  if (bars == 1 || bars == 2 || bars == 4 || bars == 8) return bars;
  return 1;
}

bool m1ListeningCase(PhraseAuditionListeningCase listeningCase) {
  return listeningCase != PhraseAuditionListeningCase::CurrentWired;
}

uint8_t synthNoteCount(const SynthPattern& pattern) {
  uint8_t count = 0;
  for (const SynthStep& step : pattern.steps)
    if (step.note >= 0) ++count;
  return count;
}

uint32_t auditionSeed(const GenreSettings& settings,
                      RhythmArchetypeId archetypeId,
                      int patternAddress) {
  uint32_t value = 2166136261u;
  const uint32_t words[] = {
      settings.generativeMode,
      settings.recipe,
      settings.morphTarget,
      settings.morphAmount,
      archetypeId,
      static_cast<uint32_t>(patternAddress),
      0x41554449u,  // "AUDI"
  };
  for (uint32_t word : words) {
    value ^= word;
    value *= 16777619u;
  }
  return value;
}

GenerationContext auditionGenerationContext(const GenreSettings& settings,
                                             RhythmArchetypeId archetypeId,
                                             int patternAddress,
                                             uint16_t phraseOrdinal) {
  GenerationContext generation{};
  generation.projectSeed = auditionSeed(settings, archetypeId, patternAddress);
  generation.phraseOrdinal = phraseOrdinal;
  return generation;
}

bool materializeEvolvedDrumBar(const RhythmBarPlan& source,
                               RealizationLevel level,
                               FeelProfileId feelProfile,
                               uint8_t feelAmount,
                               const GenerationContext& generation,
                               DrumPatternSet& destination) {
  RhythmPhrasePlan oneBar{};
  oneBar.barCount = 1;
  oneBar.level = level;
  oneBar.bars[0] = source;

  // Stage 4 materializes topology after BarEvolution has already applied the
  // function. Normalizing the function tag to Statement does not change masks;
  // it adapts the evolved bar to the existing one-bar physical materializer.
  oneBar.bars[0].function = BarFunction::Statement;

  const PatternMaterializerBinding binding =
      standardDrumPatternBinding(kDeferredSynthRoles);
  MaterializedPatterns candidate{};
  if (materializeRhythmPattern(oneBar, binding, candidate) !=
      PatternMaterializeStatus::Ok) {
    return false;
  }
  if (applyFeelToMaterializedPattern(
          oneBar,
          binding,
          feelProfile,
          feelAmount,
          generation,
          candidate) != FeelPatternApplyStatus::Ok) {
    return false;
  }
  destination = candidate.drums;
  return true;
}

#if defined(ARDUINO_M5STACK_CARDPUTER)
uint32_t elapsedMicros(uint32_t started) {
  return static_cast<uint32_t>(micros() - started);
}

void captureProbeBefore(PhraseAuditionProbe& probe) {
  constexpr uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  probe.available = true;
  probe.stackBeforeWords =
      static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
  probe.freeInternalBefore =
      static_cast<uint32_t>(heap_caps_get_free_size(caps));
  probe.largestInternalBefore =
      static_cast<uint32_t>(heap_caps_get_largest_free_block(caps));
}

void captureProbeAfter(PhraseAuditionProbe& probe) {
  constexpr uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  probe.stackAfterWords =
      static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
  probe.stackAfterBytes = static_cast<uint32_t>(
      probe.stackAfterWords * sizeof(StackType_t));
  probe.freeInternalAfter =
      static_cast<uint32_t>(heap_caps_get_free_size(caps));
  probe.largestInternalAfter =
      static_cast<uint32_t>(heap_caps_get_largest_free_block(caps));
}

void runSubtractiveRuntimeProbe(const GenreSettings& settings,
                                int patternAddress,
                                PhraseAuditionProbe& probe) {
  const RhythmCatalogView& catalog =
      ReferenceVocabulary::phraseEvolutionCatalog();

  for (RhythmArchetypeId id : kSubtractiveProbeIds) {
    GenerationContext generation = auditionGenerationContext(
        settings,
        id,
        patternAddress,
        static_cast<uint16_t>(id));

    PhraseEvolutionRequest reduction{};
    reduction.catalog = &catalog;
    reduction.archetypeId = id;
    reduction.phraseBars = 4;
    reduction.level = RealizationLevel::P2Variation;
    reduction.generation = generation;
    reduction.requestedTrajectoryId = 6;
    uint32_t started = micros();
    const PhraseEvolutionResult reductionResult =
        evolveMultiBarPhrase(reduction);
    const uint32_t reductionUs = elapsedMicros(started);
    if (reductionResult.status == PhraseEvolutionStatus::Ok &&
        reductionUs >= probe.maxReductionDurationUs) {
      probe.maxReductionDurationUs = reductionUs;
      probe.maxReductionArchetypeId = id;
    }

    generation.phraseOrdinal = static_cast<uint16_t>(
        generation.phraseOrdinal + 1u);
    PhraseEvolutionRequest broken{};
    broken.catalog = &catalog;
    broken.archetypeId = id;
    broken.phraseBars = 4;
    broken.level = RealizationLevel::P3Transformation;
    broken.generation = generation;
    broken.requestedTrajectoryId = 8;
    started = micros();
    const PhraseEvolutionResult breakResult =
        evolveMultiBarPhrase(broken);
    const uint32_t breakUs = elapsedMicros(started);
    if (breakResult.status == PhraseEvolutionStatus::Ok &&
        breakUs >= probe.maxBreakDurationUs) {
      probe.maxBreakDurationUs = breakUs;
      probe.maxBreakArchetypeId = id;
    }
  }
}

void printProbe(const PhraseAuditionResult& result) {
  const PhraseAuditionProbe& probe = result.probe;
  Serial.printf(
      "[PHRASE-PROBE] status=%s level=%s bars=%u profileBars=%u archetype=%u "
      "traj=%u/%u command_us=%lu reduce_max_us=%lu reduce_id=%u "
      "break_max_us=%lu break_id=%u stack_before_words=%lu "
      "stack_after_words=%lu stack_after_bytes=%lu free_internal=%lu->%lu "
      "largest_internal=%lu->%lu\n",
      phraseAuditionStatusName(result.status),
      GroovePuterState::generationLevelCode(result.level),
      static_cast<unsigned>(result.requestedBars),
      static_cast<unsigned>(result.profileBars),
      static_cast<unsigned>(result.archetypeId),
      static_cast<unsigned>(result.firstTrajectoryId),
      static_cast<unsigned>(result.secondTrajectoryId),
      static_cast<unsigned long>(probe.commandDurationUs),
      static_cast<unsigned long>(probe.maxReductionDurationUs),
      static_cast<unsigned>(probe.maxReductionArchetypeId),
      static_cast<unsigned long>(probe.maxBreakDurationUs),
      static_cast<unsigned>(probe.maxBreakArchetypeId),
      static_cast<unsigned long>(probe.stackBeforeWords),
      static_cast<unsigned long>(probe.stackAfterWords),
      static_cast<unsigned long>(probe.stackAfterBytes),
      static_cast<unsigned long>(probe.freeInternalBefore),
      static_cast<unsigned long>(probe.freeInternalAfter),
      static_cast<unsigned long>(probe.largestInternalBefore),
      static_cast<unsigned long>(probe.largestInternalAfter));
}
#endif

}  // namespace

StrongRhythmMigrationResult regenerateWithStrongRhythmMigration(
    MiniAcid& engine) {
  StrongRhythmMigrationContext context = liveMigrationContext(engine);
  const Scene& scene = engine.sceneManager().currentScene();
  StrongRhythmMigrationResult allocationFailure{};
  if (!assignGenerationAttempt(scene.genre, context, allocationFailure))
    return allocationFailure;

  // Legacy generation remains the rollback/metadata source and still owns
  // timbre, Atlas tempo metadata and every unsupported genre/recipe. For a
  // supported strong-rhythm route, Stage 15 now owns the final semantic pitch.
  // The attempt is allocated before this mutation (GA-03).
  engine.regeneratePatternsWithGenre();

  return migrateStrongRhythmMaterial(
      scene.genre,
      context,
      engine.sceneManager().editCurrentDrumPattern(),
      engine.sceneManager().editCurrentSynthPattern(0),
      engine.sceneManager().editCurrentSynthPattern(1));
}

StrongRhythmMigrationResult regenerateDrumsWithStrongRhythmMigration(
    MiniAcid& engine) {
  StrongRhythmMigrationContext context = liveMigrationContext(engine);
  const Scene& scene = engine.sceneManager().currentScene();
  StrongRhythmMigrationResult allocationFailure{};
  if (!assignGenerationAttempt(scene.genre, context, allocationFailure))
    return allocationFailure;

  // Whole-pattern DRUMS generation must preserve its historical fallback and
  // must not regenerate either synth voice. Allocation precedes mutation so a
  // rejected request cannot modify the live drum pattern.
  engine.randomizeDrumPattern();

  return migrateStrongRhythmDrums(
      scene.genre,
      context,
      engine.sceneManager().editCurrentDrumPattern());
}

const char* phraseAuditionStatusName(PhraseAuditionStatus status) {
  switch (status) {
    case PhraseAuditionStatus::AppliedEvolved: return "EVOLVED";
    case PhraseAuditionStatus::AppliedVariationFallback: return "VARIATION";
    case PhraseAuditionStatus::SelectionFailed: return "SELECT_FAIL";
    case PhraseAuditionStatus::MaterializationFailed: return "MATERIAL_FAIL";
    case PhraseAuditionStatus::Count: return "INVALID";
  }
  return "INVALID";
}

PhraseAuditionResult regeneratePhraseAuditionWithProbe(
    MiniAcid& engine, PhraseAuditionListeningCase listeningCase) {
  PhraseAuditionResult result{};
  result.listeningCase = listeningCase;
#if defined(ARDUINO_M5STACK_CARDPUTER)
  captureProbeBefore(result.probe);
  const uint32_t commandStarted = micros();
#endif

  SceneManager& manager = engine.sceneManager();
  Scene& scene = manager.currentScene();
  result.requestedBars = m1ListeningCase(listeningCase)
      ? kGrooveVocabularyPhraseBars
      : normalizedPhraseBars(scene.feel.patternBars);

  GenreSettings auditionSettings = scene.genre;
  uint16_t phraseIdentity = kPhraseAuditionGenerationIdentity;
  if (m1ListeningCase(listeningCase)) {
    auditionSettings = GenreSettings{};
    auditionSettings.generativeMode =
        static_cast<uint8_t>(GenerativeMode::Electro);
    auditionSettings.recipe = kBaseRecipeId;
    auditionSettings.rhythmSelectionMode = static_cast<uint8_t>(
        RhythmSelectionMode::Auto);
    auditionSettings.rhythmArchetypeId = kNoArchetypeId;
    phraseIdentity = listeningCase == PhraseAuditionListeningCase::M1CallWired
        ? kM1CallPhraseIdentity : kM1SparsePhraseIdentity;
  }

  const int page = engine.currentPageIndex();
  const int basePatternAddress = songPatternFromPageBankIndex(
      page, kPhraseAuditionBank, 0);
  StrongRhythmMigrationContext baseContext = liveMigrationContext(engine);
  baseContext.patternAddress = basePatternAddress;
  result.level = baseContext.level;

  // Resolve exactly one Stage 14 rhythm identity before mutating audition
  // storage. The selected identity is then locked across every bar. Phrase
  // audition deliberately remains attemptOrdinal=0; the production G reroll
  // surface does not leak into the destructive audition harness.
  DrumPatternSet selectionScratch{};
  StrongRhythmFrozenSelection frozenSelection{};
  const bool requestedFrozenFourBarPath = result.requestedBars == 4;
  const StrongRhythmMigrationResult selection = requestedFrozenFourBarPath
      ? resolveStrongRhythmFrozenSelection(
            auditionSettings, baseContext, phraseIdentity,
            frozenSelection)
      : migrateStrongRhythmDrums(auditionSettings, baseContext, selectionScratch);
  result.selectionStatus = selection.status;
  result.profileBars = normalizedPhraseBars(selection.phraseBars);
  if (selection.status != StrongRhythmMigrationStatus::Applied) {
#if defined(ARDUINO_M5STACK_CARDPUTER)
    runSubtractiveRuntimeProbe(scene.genre, basePatternAddress, result.probe);
    result.probe.commandDurationUs = elapsedMicros(commandStarted);
    captureProbeAfter(result.probe);
    printProbe(result);
#endif
    return result;
  }
  const bool useFrozenFourBarPath =
      requestedFrozenFourBarPath && selection.phraseBars == 4;

  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionFor(selection.archetype);
  if (definition == nullptr) {
#if defined(ARDUINO_M5STACK_CARDPUTER)
    runSubtractiveRuntimeProbe(scene.genre, basePatternAddress, result.probe);
    result.probe.commandDurationUs = elapsedMicros(commandStarted);
    captureProbeAfter(result.probe);
    printProbe(result);
#endif
    return result;
  }
  result.archetypeId = definition->archetypeId;

  GenreSettings lockedSettings = auditionSettings;
  lockedSettings.morphTarget = 0;
  lockedSettings.morphAmount = 0;
  lockedSettings.rhythmSelectionMode = static_cast<uint8_t>(
      RhythmSelectionMode::Manual);
  lockedSettings.rhythmArchetypeId = result.archetypeId;

  PhraseEvolutionResult phrase{};
  const bool canEvolve = result.requestedBars > 1 &&
      ReferenceVocabulary::phraseEvolutionEnabled(selection.archetype);
  if (canEvolve) {
    PhraseEvolutionRequest request{};
    request.catalog = &ReferenceVocabulary::phraseEvolutionCatalog();
    request.archetypeId = result.archetypeId;
    request.phraseBars = result.requestedBars;
    request.level = baseContext.level;
    request.generation = auditionGenerationContext(
        lockedSettings,
        result.archetypeId,
        basePatternAddress,
        static_cast<uint16_t>(basePatternAddress));
    phrase = evolveMultiBarPhrase(request);
  }
  const bool evolved =
      canEvolve && phrase.status == PhraseEvolutionStatus::Ok;
  if (evolved) {
    result.firstTrajectoryId = phrase.segmentTrajectories[0];
    result.secondTrajectoryId = phrase.segmentTrajectories[1];
  }

  const bool previousSongMode = engine.songModeEnabled();
  const bool previousLoopMode = engine.loopModeEnabled();
  const int previousLoopStart = engine.loopStartRow();
  const int previousLoopEnd = engine.loopEndRow();
  const int previousActiveSongSlot = engine.activeSongSlot();
  const int previousPlaybackSlot = engine.songPlaybackSlot();
  const int previousSongPosition = engine.currentSongPosition();
  const int previousDrumBank = engine.currentDrumBankIndex();
  const int previousDrumPattern = engine.currentDrumPatternIndex();
  const int previousSynthBankA = engine.current303BankIndex(0);
  const int previousSynthBankB = engine.current303BankIndex(1);
  const int previousSynthPatternA = engine.current303PatternIndex(0);
  const int previousSynthPatternB = engine.current303PatternIndex(1);

  auto restoreSelectionState = [&]() {
    engine.setSongMode(false);
    engine.setDrumBankIndex(previousDrumBank);
    engine.setDrumPatternIndex(previousDrumPattern);
    engine.set303BankIndex(0, previousSynthBankA);
    engine.set303BankIndex(1, previousSynthBankB);
    engine.set303PatternIndex(0, previousSynthPatternA);
    engine.set303PatternIndex(1, previousSynthPatternB);
    engine.setActiveSongSlot(previousActiveSongSlot);
    engine.setSongPlaybackSlot(previousPlaybackSlot);
    engine.setSongPosition(previousSongPosition);
    engine.setLoopRange(previousLoopStart, previousLoopEnd);
    engine.setLoopMode(previousLoopMode);
    engine.setSongMode(previousSongMode);
  };

  // M1's four-bar path is prepared in the existing Bank B slots but restores
  // all candidates on failure before the Song B publish step. Storage is fixed
  // and ephemeral; no phrase cache is retained after this command.
  DrumPatternSet previousPhraseDrums[kGrooveVocabularyPhraseBars]{};
  SynthPattern previousPhraseSynthA[kGrooveVocabularyPhraseBars]{};
  SynthPattern previousPhraseSynthB[kGrooveVocabularyPhraseBars]{};
  if (useFrozenFourBarPath) {
    for (uint8_t bar = 0; bar < kGrooveVocabularyPhraseBars; ++bar) {
      previousPhraseDrums[bar] = manager.editDrumPatternSet(bar);
      previousPhraseSynthA[bar] = manager.editSynthPattern(0, bar);
      previousPhraseSynthB[bar] = manager.editSynthPattern(1, bar);
    }
  }

  engine.setSongMode(false);
  bool materialized = true;
  SynthPattern sparseControl{};
  SynthPattern sparseControlA{};
  for (uint8_t bar = 0; bar < result.requestedBars; ++bar) {
    engine.setDrumBankIndex(kPhraseAuditionBank);
    engine.setDrumPatternIndex(bar);
    engine.set303BankIndex(0, kPhraseAuditionBank);
    engine.set303BankIndex(1, kPhraseAuditionBank);
    engine.set303PatternIndex(0, bar);
    engine.set303PatternIndex(1, bar);

    const bool copySparseControl =
        listeningCase == PhraseAuditionListeningCase::M1SparseControl &&
        bar != 0;
    // C is deliberately generated once. Later slots retain their normal drum
    // materialization but receive the exact first physical synth patterns.
    if (!copySparseControl) {
      engine.regeneratePatternsWithGenre();
    }

    const int patternAddress = songPatternFromPageBankIndex(
        page, kPhraseAuditionBank, bar);
    StrongRhythmMigrationContext context = baseContext;
    context.patternAddress = patternAddress;
    context.phraseBarOrdinal = bar;

    // Bind audition output explicitly to the reserved local slot. This keeps
    // Stage 5's live current-pattern binding unique to normal production and
    // makes the destructive Bank B scope auditable in source.
    DrumPatternSet& auditionDrums = manager.editDrumPatternSet(bar);
    SynthPattern& auditionSynthA = manager.editSynthPattern(0, bar);
    SynthPattern& auditionSynthB = manager.editSynthPattern(1, bar);
    const StrongRhythmMigrationResult barResult = copySparseControl
        ? [&]() {
            StrongRhythmMigrationContext drumContext = context;
            drumContext.frozenSelection = &frozenSelection;
            drumContext.phraseGenerationIdentity =
                frozenSelection.phraseGenerationIdentity;
            return migrateStrongRhythmDrums(
                auditionSettings, drumContext, auditionDrums);
          }()
        : useFrozenFourBarPath
        ? migrateStrongRhythmFrozenMaterial(
              auditionSettings, frozenSelection, context, auditionDrums,
              auditionSynthA, auditionSynthB)
        : migrateStrongRhythmMaterial(
              lockedSettings, context, auditionDrums, auditionSynthA,
              auditionSynthB);
    if (barResult.status != StrongRhythmMigrationStatus::Applied) {
      materialized = false;
      break;
    }

    if (listeningCase == PhraseAuditionListeningCase::M1SparseControl) {
      if (bar == 0) {
        sparseControlA = auditionSynthA;
        sparseControl = auditionSynthB;
      } else {
        auditionSynthA = sparseControlA;
        auditionSynthB = sparseControl;
      }
    }

    if (evolved) {
      const GenerationContext feelGeneration = auditionGenerationContext(
          lockedSettings,
          result.archetypeId,
          patternAddress,
          static_cast<uint16_t>(basePatternAddress + bar));
      DrumPatternSet evolvedDrums{};
      if (!materializeEvolvedDrumBar(
              phrase.bars[bar],
              context.level,
              context.feelProfile,
              context.feelAmount,
              feelGeneration,
              evolvedDrums)) {
        materialized = false;
        break;
      }
      auditionDrums = evolvedDrums;
    }
  }

  if (!materialized) {
    if (useFrozenFourBarPath) {
      for (uint8_t bar = 0; bar < kGrooveVocabularyPhraseBars; ++bar) {
        manager.editDrumPatternSet(bar) = previousPhraseDrums[bar];
        manager.editSynthPattern(0, bar) = previousPhraseSynthA[bar];
        manager.editSynthPattern(1, bar) = previousPhraseSynthB[bar];
      }
    }
    result.status = PhraseAuditionStatus::MaterializationFailed;
    restoreSelectionState();
#if defined(ARDUINO_M5STACK_CARDPUTER)
    runSubtractiveRuntimeProbe(lockedSettings, basePatternAddress, result.probe);
    result.probe.commandDurationUs = elapsedMicros(commandStarted);
    captureProbeAfter(result.probe);
    printProbe(result);
#endif
    return result;
  }

  // Song B is the explicit audition transport surface. Song A and Bank A stay
  // untouched. All tracks point at the same current-page Bank B slot per bar.
  engine.setActiveSongSlot(kPhraseAuditionSongSlot);
  for (int row = 0; row < Song::kMaxPositions; ++row) {
    engine.clearSongPattern(row, SongTrack::SynthA);
    engine.clearSongPattern(row, SongTrack::SynthB);
    engine.clearSongPattern(row, SongTrack::Drums);
  }
  for (uint8_t bar = 0; bar < result.requestedBars; ++bar) {
    const int patternAddress = songPatternFromPageBankIndex(
        page, kPhraseAuditionBank, bar);
    engine.setSongPattern(bar, SongTrack::SynthA, patternAddress);
    engine.setSongPattern(bar, SongTrack::SynthB, patternAddress);
    engine.setSongPattern(bar, SongTrack::Drums, patternAddress);
  }
  engine.setSongLength(result.requestedBars);
  engine.setSongPosition(0);
  engine.setLoopRange(0, result.requestedBars - 1);
  engine.setLoopMode(true);
  engine.setSongPlaybackSlot(kPhraseAuditionSongSlot);
  engine.setSongMode(true);

  for (uint8_t bar = 0; bar < result.requestedBars && bar < 4; ++bar)
    result.synthBNoteCounts[bar] = synthNoteCount(
        manager.editSynthPattern(1, bar));

  result.status = evolved
      ? PhraseAuditionStatus::AppliedEvolved
      : PhraseAuditionStatus::AppliedVariationFallback;

#if defined(ARDUINO_M5STACK_CARDPUTER)
  runSubtractiveRuntimeProbe(lockedSettings, basePatternAddress, result.probe);
  result.probe.commandDurationUs = elapsedMicros(commandStarted);
  captureProbeAfter(result.probe);
  printProbe(result);
#endif
  return result;
}

}  // namespace GroovePuterRhythm
