#include "bar_material_commit.h"

#include "atlas_runtime.h"
#include "miniacid_engine.h"
#include "../debug_log.h"
#include "src/state/scene_revision.h"

#include <atomic>
#include <cstdint>

namespace {

constexpr uint8_t kSynthAMask = 1u << 0;
constexpr uint8_t kSynthBMask = 1u << 1;
constexpr uint8_t kDrumsMask = 1u << 2;

struct PatternTarget {
  int bank = -1;
  int slot = -1;
};

struct PendingMaterial {
  MiniAcid* owner = nullptr;
  MaterialAction action = MaterialAction::None;
  int page = -1;
  uint8_t recipe = 0;
  uint8_t atlasVariation = 0;
  bool atlasBacked = false;
  uint8_t mask = 0;
  PatternTarget synthTarget[2];
  PatternTarget drumTarget;
  SynthPattern synth[2];
  DrumPatternSet drums;
};

PendingMaterial g_pending;
std::atomic<bool> g_pendingValid{false};
std::atomic<uint8_t> g_status{
    static_cast<uint8_t>(MaterialCommitStatus::Idle)};
std::atomic<uint32_t> g_commitSerial{0};
std::atomic<uint32_t> g_stageSerial{0};

bool validBankSlot(const PatternTarget& target) {
  return target.bank >= 0 && target.bank < kBankCount &&
         target.slot >= 0 && target.slot < Bank<SynthPattern>::kPatterns;
}

PatternTarget currentSynthTarget(MiniAcid& engine, int voiceIndex) {
  PatternTarget target;
  target.bank = engine.current303BankIndex(voiceIndex);
  target.slot = engine.current303PatternIndex(voiceIndex);
  return target;
}

PatternTarget currentDrumTarget(MiniAcid& engine) {
  PatternTarget target;
  target.bank = engine.currentDrumBankIndex();
  target.slot = engine.currentDrumPatternIndex();
  return target;
}

uint8_t synthMask(int voiceIndex) {
  return voiceIndex == 0 ? kSynthAMask : kSynthBMask;
}

uint32_t mix32(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7FEB352Du;
  value ^= value >> 15;
  value *= 0x846CA68Bu;
  value ^= value >> 16;
  return value;
}

uint8_t nextAtlasVariation(MiniAcid& engine, uint8_t recipe) {
  const uint8_t count = AtlasRuntime::variationCount(recipe);
  if (count == 0) return 0;
  const uint32_t serial = g_stageSerial.fetch_add(1, std::memory_order_relaxed) + 1;
  const uint32_t mixed = mix32(
      engine.modeManager().generationSeed() ^
      (serial * 0x9E3779B9u) ^
      (static_cast<uint32_t>(recipe) * 0x85EBCA6Bu));
  return static_cast<uint8_t>(mixed % count);
}

bool realizeAtlas(MiniAcid& engine,
                  uint8_t variation,
                  SynthPattern& synthA,
                  SynthPattern& synthB,
                  DrumPatternSet& drums) {
  const uint8_t recipe = engine.genreManager().recipe();
  if (!AtlasRuntime::hasRecipe(recipe)) return false;
  return AtlasRuntime::applyRecipe(
      recipe, variation, synthA, synthB, drums, nullptr);
}

GenreBehavior synthBehaviorForVoice(MiniAcid& engine, int voiceIndex) {
  GenreBehavior behavior = engine.genreManager().getBehavior();
  if (engine.genreManager().generativeMode() == GenerativeMode::Reggae) {
    if (voiceIndex == 0) {
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
  }
  return behavior;
}

bool compatiblePending(MiniAcid& engine,
                       MaterialAction action,
                       int page,
                       uint8_t recipe) {
  if (!g_pendingValid.load(std::memory_order_acquire)) return false;
  return g_pending.owner == &engine &&
         g_pending.action == action &&
         g_pending.page == page &&
         g_pending.recipe == recipe;
}

uint8_t atlasVariationForLane(MiniAcid& engine,
                              uint8_t recipe,
                              uint8_t requestedMask) {
  const int page = engine.currentPageIndex();
  if (compatiblePending(
          engine, MaterialAction::Generation, page, recipe) &&
      g_pending.atlasBacked &&
      (g_pending.mask & requestedMask) == 0) {
    // Adding another lane to an Atlas-backed pending transaction must use the
    // same P-variation. Otherwise an atomic swap could still be musically
    // incoherent across A/B/Drums.
    return g_pending.atlasVariation;
  }
  return nextAtlasVariation(engine, recipe);
}

bool preserveCompatibleAtlasTransaction(MiniAcid& engine,
                                        MaterialAction action,
                                        int page,
                                        uint8_t recipe,
                                        bool atlasBacked,
                                        uint8_t variation) {
  if (!compatiblePending(engine, action, page, recipe)) return true;
  if (!atlasBacked || !g_pending.atlasBacked) return true;
  // A repeated request for an already-pending Atlas lane deliberately selects
  // a new variation. Do not keep other lanes from the previous variation in
  // that transaction; clearing them is safer than committing a mixed Atlas.
  return g_pending.atlasVariation == variation;
}

void beginStage(MiniAcid& engine,
                MaterialAction action,
                int page,
                uint8_t recipe,
                bool preserveCompatible) {
  const bool preserve = preserveCompatible &&
      compatiblePending(engine, action, page, recipe);
  g_pendingValid.store(false, std::memory_order_release);
  if (!preserve) {
    g_pending = PendingMaterial{};
    g_pending.owner = &engine;
    g_pending.action = action;
    g_pending.page = page;
    g_pending.recipe = recipe;
  }
}

MaterialQueueResult finishStage() {
  if (g_pending.mask == 0) {
    g_status.store(
        static_cast<uint8_t>(MaterialCommitStatus::CancelledInvalidTarget),
        std::memory_order_release);
    return MaterialQueueResult::Failed;
  }
  g_pendingValid.store(true, std::memory_order_release);
  g_status.store(
      static_cast<uint8_t>(MaterialCommitStatus::PendingNextBar),
      std::memory_order_release);
  LOG_DEBUG("MaterialCommit",
            "staged action=%s page=%d mask=0x%02x -> NEXT_BAR\n",
            materialActionLabel(g_pending.action),
            g_pending.page,
            static_cast<unsigned>(g_pending.mask));
  return MaterialQueueResult::PendingNextBar;
}

void clearPendingForImmediateCommit() {
  g_pendingValid.store(false, std::memory_order_release);
  g_pending = PendingMaterial{};
}

MaterialQueueResult commitSynthNow(MiniAcid& engine,
                                   int voiceIndex,
                                   const PatternTarget& target,
                                   const SynthPattern& material) {
  if (!validBankSlot(target)) return MaterialQueueResult::Failed;
  clearPendingForImmediateCommit();
  Scene& scene = engine.sceneManager().currentScene();
  if (voiceIndex == 0) {
    scene.synthABanks[target.bank].patterns[target.slot] = material;
  } else {
    scene.synthBBanks[target.bank].patterns[target.slot] = material;
  }
  GroovePuterState::markSceneMutated();
  g_commitSerial.fetch_add(1, std::memory_order_release);
  g_status.store(
      static_cast<uint8_t>(MaterialCommitStatus::Committed),
      std::memory_order_release);
  return MaterialQueueResult::CommittedNow;
}

MaterialQueueResult commitDrumsNow(MiniAcid& engine,
                                   const PatternTarget& target,
                                   const DrumPatternSet& material) {
  if (!validBankSlot(target)) return MaterialQueueResult::Failed;
  clearPendingForImmediateCommit();
  Scene& scene = engine.sceneManager().currentScene();
  scene.drumBanks[target.bank].patterns[target.slot] = material;
  GroovePuterState::markSceneMutated();
  g_commitSerial.fetch_add(1, std::memory_order_release);
  g_status.store(
      static_cast<uint8_t>(MaterialCommitStatus::Committed),
      std::memory_order_release);
  return MaterialQueueResult::CommittedNow;
}

MaterialQueueResult stageSynth(MiniAcid& engine,
                               int voiceIndex,
                               const PatternTarget& target,
                               const SynthPattern& material,
                               bool atlasBacked,
                               uint8_t variation) {
  if (!validBankSlot(target)) return MaterialQueueResult::Failed;
  if (!engine.isPlaying()) {
    return commitSynthNow(engine, voiceIndex, target, material);
  }

  const int page = engine.currentPageIndex();
  const uint8_t recipe = engine.genreManager().recipe();
  const bool preserve = preserveCompatibleAtlasTransaction(
      engine, MaterialAction::Generation, page, recipe,
      atlasBacked, variation);
  beginStage(engine, MaterialAction::Generation, page, recipe, preserve);
  const uint8_t bit = synthMask(voiceIndex);
  g_pending.synthTarget[voiceIndex] = target;
  g_pending.synth[voiceIndex] = material;
  g_pending.mask = static_cast<uint8_t>(g_pending.mask | bit);
  g_pending.atlasBacked = atlasBacked;
  g_pending.atlasVariation = variation;
  return finishStage();
}

MaterialQueueResult stageDrums(MiniAcid& engine,
                               const PatternTarget& target,
                               const DrumPatternSet& material,
                               MaterialAction action,
                               bool atlasBacked,
                               uint8_t variation) {
  if (!validBankSlot(target)) return MaterialQueueResult::Failed;
  if (!engine.isPlaying()) {
    return commitDrumsNow(engine, target, material);
  }

  const int page = engine.currentPageIndex();
  const uint8_t recipe = engine.genreManager().recipe();
  bool preserve = action == MaterialAction::Generation;
  if (preserve) {
    preserve = preserveCompatibleAtlasTransaction(
        engine, action, page, recipe, atlasBacked, variation);
  }
  beginStage(engine, action, page, recipe, preserve);
  g_pending.drumTarget = target;
  g_pending.drums = material;
  g_pending.mask = static_cast<uint8_t>(g_pending.mask | kDrumsMask);
  g_pending.atlasBacked = atlasBacked;
  g_pending.atlasVariation = variation;
  return finishStage();
}

DrumPatternSet pendingOrCurrentDrums(MiniAcid& engine,
                                    const PatternTarget& target) {
  if (g_pendingValid.load(std::memory_order_acquire) &&
      g_pending.owner == &engine &&
      (g_pending.mask & kDrumsMask) != 0 &&
      g_pending.page == engine.currentPageIndex() &&
      g_pending.drumTarget.bank == target.bank &&
      g_pending.drumTarget.slot == target.slot) {
    return g_pending.drums;
  }
  return engine.sceneManager().getCurrentDrumPattern();
}

}  // namespace

const char* materialActionLabel(MaterialAction action) {
  switch (action) {
    case MaterialAction::Generation: return "GEN";
    case MaterialAction::Variation: return "VAR";
    case MaterialAction::Phrase: return "PHRASE";
    case MaterialAction::Fill: return "FILL";
    case MaterialAction::Section: return "SECTION";
    case MaterialAction::SongMaterialization: return "SONG GEN";
    case MaterialAction::RhythmArchetype: return "ARCHETYPE";
    case MaterialAction::Chaos: return "CHAOS";
    case MaterialAction::None:
    default:
      return "MATERIAL";
  }
}

bool hasPendingMaterialCommit() {
  return g_pendingValid.load(std::memory_order_acquire);
}

MaterialAction pendingMaterialAction() {
  if (!g_pendingValid.load(std::memory_order_acquire)) {
    return MaterialAction::None;
  }
  return g_pending.action;
}

MaterialCommitStatus materialCommitStatus() {
  return static_cast<MaterialCommitStatus>(
      g_status.load(std::memory_order_acquire));
}

uint32_t materialCommitSerial() {
  return g_commitSerial.load(std::memory_order_acquire);
}

MaterialQueueResult queueSynthGenerationForBar(MiniAcid& engine,
                                               int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    return MaterialQueueResult::Failed;
  }

  const PatternTarget target = currentSynthTarget(engine, voiceIndex);
  if (!validBankSlot(target)) return MaterialQueueResult::Failed;

  SynthPattern candidate = engine.sceneManager().getCurrentSynthPattern(voiceIndex);
  SynthPattern atlasA{};
  SynthPattern atlasB{};
  DrumPatternSet atlasDrums{};
  const uint8_t recipe = engine.genreManager().recipe();
  const uint8_t variation = atlasVariationForLane(
      engine, recipe, synthMask(voiceIndex));
  const bool atlasBacked = realizeAtlas(
      engine, variation, atlasA, atlasB, atlasDrums);

  if (atlasBacked) {
    candidate = voiceIndex == 0 ? atlasA : atlasB;
  } else {
    const GenerativeParams& params =
        engine.genreManager().getCompiledGenerativeParams();
    const GenreBehavior behavior = synthBehaviorForVoice(engine, voiceIndex);
    engine.modeManager().generatePattern(
        candidate, engine.bpm(), params, behavior, voiceIndex);
  }

  return stageSynth(
      engine, voiceIndex, target, candidate, atlasBacked, variation);
}

MaterialQueueResult queueDrumGenerationForBar(MiniAcid& engine) {
  const PatternTarget target = currentDrumTarget(engine);
  if (!validBankSlot(target)) return MaterialQueueResult::Failed;

  DrumPatternSet candidate = engine.sceneManager().getCurrentDrumPattern();
  SynthPattern atlasA{};
  SynthPattern atlasB{};
  DrumPatternSet atlasDrums{};
  const uint8_t recipe = engine.genreManager().recipe();
  const uint8_t variation = atlasVariationForLane(
      engine, recipe, kDrumsMask);
  const bool atlasBacked = realizeAtlas(
      engine, variation, atlasA, atlasB, atlasDrums);

  if (atlasBacked) {
    candidate = atlasDrums;
  } else {
    const GenerativeParams& params =
        engine.genreManager().getCompiledGenerativeParams();
    const GenreBehavior behavior = engine.genreManager().getBehavior();
    engine.modeManager().generateDrumPattern(candidate, params, behavior);
  }

  return stageDrums(
      engine, target, candidate, MaterialAction::Generation,
      atlasBacked, variation);
}

MaterialQueueResult queueDrumVoiceGenerationForBar(MiniAcid& engine,
                                                   int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_DRUM_VOICES) {
    return MaterialQueueResult::Failed;
  }
  const PatternTarget target = currentDrumTarget(engine);
  if (!validBankSlot(target)) return MaterialQueueResult::Failed;

  DrumPatternSet candidate = pendingOrCurrentDrums(engine, target);
  SynthPattern atlasA{};
  SynthPattern atlasB{};
  DrumPatternSet atlasDrums{};
  const uint8_t recipe = engine.genreManager().recipe();
  const uint8_t variation = atlasVariationForLane(
      engine, recipe, kDrumsMask);
  const bool atlasBacked = realizeAtlas(
      engine, variation, atlasA, atlasB, atlasDrums);

  if (atlasBacked) {
    candidate.voices[voiceIndex] = atlasDrums.voices[voiceIndex];
  } else {
    const GenerativeParams& params =
        engine.genreManager().getCompiledGenerativeParams();
    const GenreBehavior behavior = engine.genreManager().getBehavior();
    engine.modeManager().generateDrumVoice(
        candidate.voices[voiceIndex], voiceIndex, params, behavior);
  }

  return stageDrums(
      engine, target, candidate, MaterialAction::Generation,
      atlasBacked, variation);
}

MaterialQueueResult queueDrumChaosForBar(MiniAcid& engine) {
  const PatternTarget target = currentDrumTarget(engine);
  if (!validBankSlot(target)) return MaterialQueueResult::Failed;

  DrumPatternSet candidate = engine.sceneManager().getCurrentDrumPattern();
  const GenerativeParams& params =
      engine.genreManager().getCompiledGenerativeParams();
  uint32_t rng = mix32(
      engine.modeManager().generationSeed() ^
      (g_stageSerial.fetch_add(1, std::memory_order_relaxed) + 1) ^
      0xC4A05EEDu);
  auto next = [&rng]() -> uint32_t {
    rng = rng * 1664525u + 1013904223u;
    return rng;
  };

  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    GenreBehavior behavior = engine.genreManager().getBehavior();
    behavior.stepMask = static_cast<uint16_t>(next() & 0xFFFFu);
    behavior.motifLength = static_cast<uint8_t>(1 + (next() % 8u));
    behavior.useMotif = (next() & 1u) != 0;
    behavior.avoidClusters = (next() & 1u) != 0;
    behavior.allowChromatic = true;
    behavior.forceOctaveJump = true;
    engine.modeManager().generateDrumVoice(
        candidate.voices[voice], voice, params, behavior);
  }

  return stageDrums(
      engine, target, candidate, MaterialAction::Chaos, false, 0);
}

bool commitPendingMaterialAtBarStart(SceneManager& scenes) {
  if (!g_pendingValid.load(std::memory_order_acquire)) return false;

  MiniAcid* owner = g_pending.owner;
  if (!owner || &owner->sceneManager() != &scenes) return false;

  // The currently loaded Scene banks represent one pattern page. Never apply a
  // pending address prepared for another page to numerically identical slots.
  if (owner->currentPageIndex() != g_pending.page) {
    g_pendingValid.store(false, std::memory_order_release);
    g_pending = PendingMaterial{};
    g_status.store(
        static_cast<uint8_t>(MaterialCommitStatus::CancelledPageMismatch),
        std::memory_order_release);
    LOG_DEBUG("MaterialCommit", "%s\n",
              "cancelled: page changed before BAR_START");
    return false;
  }

  if (((g_pending.mask & kSynthAMask) != 0 &&
       !validBankSlot(g_pending.synthTarget[0])) ||
      ((g_pending.mask & kSynthBMask) != 0 &&
       !validBankSlot(g_pending.synthTarget[1])) ||
      ((g_pending.mask & kDrumsMask) != 0 &&
       !validBankSlot(g_pending.drumTarget))) {
    g_pendingValid.store(false, std::memory_order_release);
    g_pending = PendingMaterial{};
    g_status.store(
        static_cast<uint8_t>(MaterialCommitStatus::CancelledInvalidTarget),
        std::memory_order_release);
    return false;
  }

  // Publish invalid before touching Scene state. The control thread can only
  // stage while the audio task is paused at an AudioMutationGate boundary, so
  // it cannot race this bounded BAR_START copy section.
  g_pendingValid.store(false, std::memory_order_release);

  Scene& scene = scenes.currentScene();
  if ((g_pending.mask & kSynthAMask) != 0) {
    const PatternTarget target = g_pending.synthTarget[0];
    scene.synthABanks[target.bank].patterns[target.slot] = g_pending.synth[0];
  }
  if ((g_pending.mask & kSynthBMask) != 0) {
    const PatternTarget target = g_pending.synthTarget[1];
    scene.synthBBanks[target.bank].patterns[target.slot] = g_pending.synth[1];
  }
  if ((g_pending.mask & kDrumsMask) != 0) {
    const PatternTarget target = g_pending.drumTarget;
    scene.drumBanks[target.bank].patterns[target.slot] = g_pending.drums;
  }

  LOG_DEBUG("MaterialCommit",
            "committed action=%s mask=0x%02x at BAR_START\n",
            materialActionLabel(g_pending.action),
            static_cast<unsigned>(g_pending.mask));
  g_pending = PendingMaterial{};
  GroovePuterState::markSceneMutated();
  g_commitSerial.fetch_add(1, std::memory_order_release);
  g_status.store(
      static_cast<uint8_t>(MaterialCommitStatus::Committed),
      std::memory_order_release);
  return true;
}
