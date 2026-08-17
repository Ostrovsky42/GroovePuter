#ifndef GROOVEPUTER_GENERATION_MIGRATION_QUANTIZED_GENERATION_UNDO_OWNER_IMPL_H
#define GROOVEPUTER_GENERATION_MIGRATION_QUANTIZED_GENERATION_UNDO_OWNER_IMPL_H

#include <cstddef>
#include <type_traits>

#include "../../state/undo_owner.h"

namespace GroovePuterRhythm {
namespace QuantizedGenerationDetail {

struct GenerationUndoPayload {
  PatternTarget target{};
  GenreSettings genre{};
  GrooveboxMode mode = GrooveboxMode::Minimal;
  float bpm = 100.0f;
  uint8_t swingPct = 0;
  QuantizedGenerationScope scope = QuantizedGenerationScope::Full;
  SynthPattern synth[2]{};
  DrumPatternSet drums{};
};

static_assert(std::is_trivially_copyable<GenerationUndoPayload>::value,
              "generation Undo receipt must remain a fixed trivially-copyable value");
static_assert(sizeof(GenerationUndoPayload) <= GroovePuterUndo::kUndoPayloadBytes,
              "generation Undo receipt exceeds the accepted 0.9.8-R2 DRAM budget");

inline GenerationUndoPayload captureGenerationUndo(
    MiniAcid& engine,
    const PatternTarget& target,
    QuantizedGenerationScope scope) {
  const Scene& scene = engine.sceneManager().currentScene();
  GenerationUndoPayload before{};
  before.target = target;
  before.genre = scene.genre;
  before.mode = engine.grooveboxMode();
  before.bpm = engine.bpm();
  before.swingPct = scene.feel.swingPct;
  before.scope = scope;
  before.synth[0] = scene.synthABanks[target.synthBank[0]]
      .patterns[target.synthSlot[0]];
  before.synth[1] = scene.synthBBanks[target.synthBank[1]]
      .patterns[target.synthSlot[1]];
  before.drums = scene.drumBanks[target.drumBank]
      .patterns[target.drumSlot];
  return before;
}

inline void applyPreparedGeneration(
    MiniAcid& engine,
    const PendingGeneration& pending) {
  Scene& scene = engine.sceneManager().currentScene();
  if (pending.scope == QuantizedGenerationScope::Full) {
    scene.synthABanks[pending.target.synthBank[0]]
        .patterns[pending.target.synthSlot[0]] = pending.synth[0];
    scene.synthBBanks[pending.target.synthBank[1]]
        .patterns[pending.target.synthSlot[1]] = pending.synth[1];
    scene.drumBanks[pending.target.drumBank]
        .patterns[pending.target.drumSlot] = pending.drums;
    scene.genre = pending.genre;
    scene.feel.swingPct = pending.swingPct;
    engine.setGrooveboxMode(pending.mode);
    engine.setBpm(pending.bpm);
    return;
  }

  if (pending.scope == QuantizedGenerationScope::Drums) {
    scene.drumBanks[pending.target.drumBank]
        .patterns[pending.target.drumSlot] = pending.drums;
    return;
  }

  const int voice = pending.scope == QuantizedGenerationScope::SynthA ? 0 : 1;
  Bank<SynthPattern>& bank = voice == 0
      ? scene.synthABanks[pending.target.synthBank[0]]
      : scene.synthBBanks[pending.target.synthBank[1]];
  bank.patterns[pending.target.synthSlot[voice]] = pending.synth[voice];
}

inline bool commitPreparedGeneration(
    MiniAcid& engine,
    const PendingGeneration& pending) {
  SceneManager& scenes = engine.sceneManager();
  if (!targetValid(pending.target) ||
      !targetStillActive(scenes, pending.target)) {
    return false;
  }

  const GenerationUndoPayload before = captureGenerationUndo(
      engine, pending.target, pending.scope);
  return GroovePuterUndo::undoOwner().commitPrepared(
      GroovePuterUndo::UndoKind::Generation,
      before,
      [&engine, &pending]() { applyPreparedGeneration(engine, pending); });
}

inline bool validateGenerationUndo(
    MiniAcid& engine,
    const GenerationUndoPayload& before) {
  SceneManager& scenes = engine.sceneManager();
  return targetValid(before.target) && targetStillActive(scenes, before.target);
}

inline void restoreGenerationUndo(
    MiniAcid& engine,
    const GenerationUndoPayload& before) {
  Scene& scene = engine.sceneManager().currentScene();
  if (before.scope == QuantizedGenerationScope::Full) {
    scene.synthABanks[before.target.synthBank[0]]
        .patterns[before.target.synthSlot[0]] = before.synth[0];
    scene.synthBBanks[before.target.synthBank[1]]
        .patterns[before.target.synthSlot[1]] = before.synth[1];
    scene.drumBanks[before.target.drumBank]
        .patterns[before.target.drumSlot] = before.drums;
    scene.genre = before.genre;
    scene.feel.swingPct = before.swingPct;
    engine.setGrooveboxMode(before.mode);
    engine.setBpm(before.bpm);
    return;
  }

  if (before.scope == QuantizedGenerationScope::Drums) {
    scene.drumBanks[before.target.drumBank]
        .patterns[before.target.drumSlot] = before.drums;
    return;
  }

  const int voice = before.scope == QuantizedGenerationScope::SynthA ? 0 : 1;
  Bank<SynthPattern>& bank = voice == 0
      ? scene.synthABanks[before.target.synthBank[0]]
      : scene.synthBBanks[before.target.synthBank[1]];
  bank.patterns[before.target.synthSlot[voice]] = before.synth[voice];
}



inline StrongRhythmMigrationContext drumMigrationContextFor(
    MiniAcid& engine, const Scene& scene) {
  StrongRhythmMigrationContext context{};
  // Preserve the historical DRUMS G identity: this path used the local drum
  // pattern index rather than the Song/global pattern address.
  context.patternAddress = engine.currentDrumPatternIndex();
  context.level = GroovePuterState::currentGenerationLevel();
  context.feelProfile = static_cast<FeelProfileId>(scene.feel.timingProfile);
  float feelAmount = scene.generatorParams.microTimingAmount;
  if (feelAmount < 0.0f) feelAmount = 0.0f;
  if (feelAmount > 1.0f) feelAmount = 1.0f;
  context.feelAmount = static_cast<uint8_t>(feelAmount * 100.0f + 0.5f);
  int root = scene.generatorParams.scaleRoot % 12;
  if (root < 0) root += 12;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = static_cast<uint8_t>(root);
  context.scaleTypeValue = static_cast<ScaleTypeValue>(scene.generatorParams.scale);
  return context;
}

inline bool allocateDrumAttemptFor(const GenreSettings& genre,
                         const StrongRhythmMigrationContext& context,
                         uint32_t& ordinal) {
  ordinal = 0;
  if (selectStrongRhythmRoute(genre) == StrongRhythmRoute::Legacy) return true;
  const auto allocation = GroovePuterState::allocateGenerationAttempt(
      genre.generativeMode, genre.recipe, context.level, context.patternAddress);
  if (!allocation.ok()) return false;
  ordinal = allocation.ordinal;
  return true;
}

inline bool prepareDrumCandidate(MiniAcid& engine,
                       const PatternTarget& target,
                       uint32_t attemptOrdinal,
                       PendingGeneration& candidate) {
  SceneManager& scenes = engine.sceneManager();
  const Scene& scene = scenes.currentScene();
  if (!targetValid(target)) return false;

  candidate.owner = &engine;
  candidate.target = target;
  candidate.genre = scene.genre;
  candidate.mode = engine.grooveboxMode();
  candidate.bpm = engine.bpm();
  candidate.swingPct = scene.feel.swingPct;
  candidate.scope = QuantizedGenerationScope::Drums;
  candidate.synth[0] = scene.synthABanks[target.synthBank[0]].patterns[target.synthSlot[0]];
  candidate.synth[1] = scene.synthBBanks[target.synthBank[1]].patterns[target.synthSlot[1]];
  candidate.drums = scene.drumBanks[target.drumBank].patterns[target.drumSlot];

  GrooveboxModeManager scratchMode(engine);
  scratchMode.setModeLocal(engine.grooveboxMode());
  scratchMode.setFlavorLocal(engine.modeManager().flavor());
  scratchMode.setGenerationSeed(engine.modeManager().generationSeed());
  const GenerativeParams& params = engine.genreManager().getCompiledGenerativeParams();
  const GenreBehavior behavior = engine.genreManager().getBehavior();
  scratchMode.generateDrumPattern(candidate.drums, params, behavior);

  StrongRhythmMigrationContext context = drumMigrationContextFor(engine, scene);
  context.generationAttemptOrdinal = attemptOrdinal;
  (void)migrateStrongRhythmDrums(scene.genre, context, candidate.drums);
  return targetStillActive(scenes, target);
}

inline void exchangeGenerationUndo(MiniAcid& engine,
                         GenerationUndoPayload& retained) {
  Scene& scene = engine.sceneManager().currentScene();
  if (retained.scope == QuantizedGenerationScope::Full) {
    GroovePuterUndo::exchangeFixedValue(
        scene.synthABanks[retained.target.synthBank[0]].patterns[retained.target.synthSlot[0]],
        retained.synth[0]);
    GroovePuterUndo::exchangeFixedValue(
        scene.synthBBanks[retained.target.synthBank[1]].patterns[retained.target.synthSlot[1]],
        retained.synth[1]);
    GroovePuterUndo::exchangeFixedValue(
        scene.drumBanks[retained.target.drumBank].patterns[retained.target.drumSlot],
        retained.drums);
    GroovePuterUndo::exchangeFixedValue(scene.genre, retained.genre);
    GroovePuterUndo::exchangeFixedValue(scene.feel.swingPct, retained.swingPct);
    const GrooveboxMode liveMode = engine.grooveboxMode();
    engine.setGrooveboxMode(retained.mode);
    retained.mode = liveMode;
    const float liveBpm = engine.bpm();
    engine.setBpm(retained.bpm);
    retained.bpm = liveBpm;
    return;
  }
  if (retained.scope == QuantizedGenerationScope::Drums) {
    GroovePuterUndo::exchangeFixedValue(
        scene.drumBanks[retained.target.drumBank].patterns[retained.target.drumSlot],
        retained.drums);
    return;
  }
  const int voice = retained.scope == QuantizedGenerationScope::SynthA ? 0 : 1;
  Bank<SynthPattern>& bank = voice == 0
      ? scene.synthABanks[retained.target.synthBank[0]]
      : scene.synthBBanks[retained.target.synthBank[1]];
  GroovePuterUndo::exchangeFixedValue(
      bank.patterns[retained.target.synthSlot[voice]], retained.synth[voice]);
}

}  // namespace QuantizedGenerationDetail

inline bool commitQuantizedGenerationAtBarStart(SceneManager& scenes) {
  using namespace QuantizedGenerationDetail;
  const int slot = g_publishedSlot.exchange(-1, std::memory_order_acq_rel);
  if (slot < 0 || slot > 1) return false;

  uint8_t expected = static_cast<uint8_t>(SlotState::Ready);
  if (!g_slotState[slot].compare_exchange_strong(
          expected,
          static_cast<uint8_t>(SlotState::Reading),
          std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    return false;
  }

  const PendingGeneration& pending = g_slots[slot];
  MiniAcid* owner = pending.owner;
  if (owner == nullptr || &owner->sceneManager() != &scenes ||
      !targetValid(pending.target) ||
      !targetStillActive(scenes, pending.target)) {
    g_slotState[slot].store(
        static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::CancelledTargetChanged),
        std::memory_order_release);
    return false;
  }

  const bool committed = commitPreparedGeneration(*owner, pending);
  g_slotState[slot].store(
      static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
  if (!committed) {
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::Busy),
        std::memory_order_release);
    return false;
  }

  g_status.store(
      static_cast<uint8_t>(QuantizedGenerationStatus::Committed),
      std::memory_order_release);
  g_commitSerial.fetch_add(1, std::memory_order_release);
  return true;
}

inline QuantizedGenerationResult regenerateWithQuantizedCommit(
    MiniAcid& engine,
    const GenreSettings& requestedGenre,
    GrooveboxMode requestedMode,
    bool applyTempo,
    float requestedBpm) {
  using namespace QuantizedGenerationDetail;

  SceneManager& scenes = engine.sceneManager();
  const PatternTarget target = captureTarget(scenes);
  if (!targetValid(target)) return QuantizedGenerationResult::Failed;
  const RealizationLevel requestLevel = GroovePuterState::currentGenerationLevel();

  const WriteLease lease = acquireWriteLease();
  if (lease.slot < 0) {
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::Busy),
        std::memory_order_release);
    return QuantizedGenerationResult::Failed;
  }

  uint32_t attemptOrdinal = 0;
  if (!allocateAttemptFor(requestedGenre, requestLevel, target, attemptOrdinal)) {
    releaseWriteSlot(lease.slot);
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::AttemptUnavailable),
        std::memory_order_release);
    return QuantizedGenerationResult::AttemptUnavailable;
  }

  PendingGeneration& candidate = g_slots[lease.slot];
  if (!preparePlayingCandidate(
          engine,
          requestedGenre,
          requestedMode,
          applyTempo,
          requestedBpm,
          target,
          requestLevel,
          attemptOrdinal,
          lease.hasPreviousPending,
          candidate)) {
    releaseWriteSlot(lease.slot);
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::CancelledTargetChanged),
        std::memory_order_release);
    return QuantizedGenerationResult::Failed;
  }

  if (!engine.isPlaying()) {
    const bool committed = commitPreparedGeneration(engine, candidate);
    releaseWriteSlot(lease.slot);
    if (!committed) {
      g_status.store(
          static_cast<uint8_t>(QuantizedGenerationStatus::Busy),
          std::memory_order_release);
      return QuantizedGenerationResult::Failed;
    }
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::Committed),
        std::memory_order_release);
    g_commitSerial.fetch_add(1, std::memory_order_release);
    return QuantizedGenerationResult::CommittedNow;
  }

  publishWriteSlot(lease.slot);
  engine.genreManager().setPendingCommitHook(&commitQuantizedGenerationAtBarStart);
  return QuantizedGenerationResult::PendingNextBar;
}

inline QuantizedGenerationResult regenerateSynthWithQuantizedCommit(
    MiniAcid& engine,
    int voice) {
  using namespace QuantizedGenerationDetail;

  if (voice < 0 || voice > 1) return QuantizedGenerationResult::Failed;
  SceneManager& scenes = engine.sceneManager();
  const PatternTarget target = captureTarget(scenes);
  if (!targetValid(target)) return QuantizedGenerationResult::Failed;

  GenreSettings genre = scenes.currentScene().genre;
  genre.morphTarget = 0;
  genre.morphAmount = 0;
  const RealizationLevel requestLevel = GroovePuterState::currentGenerationLevel();

  const WriteLease lease = acquireWriteLease();
  if (lease.slot < 0) {
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::Busy),
        std::memory_order_release);
    return QuantizedGenerationResult::Failed;
  }

  uint32_t attemptOrdinal = 0;
  if (!allocateAttemptFor(genre, requestLevel, target, attemptOrdinal)) {
    releaseWriteSlot(lease.slot);
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::AttemptUnavailable),
        std::memory_order_release);
    return QuantizedGenerationResult::AttemptUnavailable;
  }

  PendingGeneration& candidate = g_slots[lease.slot];
  if (!prepareSynthCandidate(
          engine,
          genre,
          target,
          voice,
          requestLevel,
          attemptOrdinal,
          candidate)) {
    releaseWriteSlot(lease.slot);
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::CancelledTargetChanged),
        std::memory_order_release);
    return QuantizedGenerationResult::Failed;
  }

  if (!engine.isPlaying()) {
    const bool committed = commitPreparedGeneration(engine, candidate);
    releaseWriteSlot(lease.slot);
    if (!committed) {
      g_status.store(
          static_cast<uint8_t>(QuantizedGenerationStatus::Busy),
          std::memory_order_release);
      return QuantizedGenerationResult::Failed;
    }
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::Committed),
        std::memory_order_release);
    g_commitSerial.fetch_add(1, std::memory_order_release);
    return QuantizedGenerationResult::CommittedNow;
  }

  publishWriteSlot(lease.slot);
  engine.genreManager().setPendingCommitHook(&commitQuantizedGenerationAtBarStart);
  return QuantizedGenerationResult::PendingNextBar;
}


inline QuantizedGenerationResult regenerateDrumsWithQuantizedCommit(MiniAcid& engine) {
  using namespace QuantizedGenerationDetail;
  SceneManager& scenes = engine.sceneManager();
  const PatternTarget target = captureTarget(scenes);
  if (!targetValid(target)) return QuantizedGenerationResult::Failed;

  const WriteLease lease = acquireWriteLease();
  if (lease.slot < 0) {
    g_status.store(static_cast<uint8_t>(QuantizedGenerationStatus::Busy),
         std::memory_order_release);
    return QuantizedGenerationResult::Failed;
  }

  const StrongRhythmMigrationContext context =
      drumMigrationContextFor(engine, scenes.currentScene());
  uint32_t attemptOrdinal = 0;
  if (!allocateDrumAttemptFor(scenes.currentScene().genre, context, attemptOrdinal)) {
    releaseWriteSlot(lease.slot);
    g_status.store(static_cast<uint8_t>(QuantizedGenerationStatus::AttemptUnavailable),
         std::memory_order_release);
    return QuantizedGenerationResult::AttemptUnavailable;
  }

  PendingGeneration& candidate = g_slots[lease.slot];
  if (!prepareDrumCandidate(engine, target, attemptOrdinal, candidate) ||
      !commitPreparedGeneration(engine, candidate)) {
    releaseWriteSlot(lease.slot);
    g_status.store(static_cast<uint8_t>(QuantizedGenerationStatus::CancelledTargetChanged),
         std::memory_order_release);
    return QuantizedGenerationResult::Failed;
  }
  releaseWriteSlot(lease.slot);
  g_status.store(static_cast<uint8_t>(QuantizedGenerationStatus::Committed),
       std::memory_order_release);
  g_commitSerial.fetch_add(1, std::memory_order_release);
  return QuantizedGenerationResult::CommittedNow;
}

inline GroovePuterUndo::UndoResult toggleLastQuantizedGeneration(MiniAcid& engine) {
  using namespace QuantizedGenerationDetail;
  return GroovePuterUndo::undoOwner().togglePrepared<GenerationUndoPayload>(
      GroovePuterUndo::UndoKind::Generation,
      [&engine](const GenerationUndoPayload& retained) {
        return validateGenerationUndo(engine, retained);
      },
      [&engine](GenerationUndoPayload& retained) {
        exchangeGenerationUndo(engine, retained);
      });
}

inline GroovePuterUndo::UndoResult undoLastQuantizedGeneration(MiniAcid& engine) {
  using namespace QuantizedGenerationDetail;
  return GroovePuterUndo::undoOwner().undoPrepared<GenerationUndoPayload>(
      GroovePuterUndo::UndoKind::Generation,
      [&engine](const GenerationUndoPayload& before) {
        return validateGenerationUndo(engine, before);
      },
      [&engine](const GenerationUndoPayload& before) {
        restoreGenerationUndo(engine, before);
      });
}

inline std::size_t quantizedGenerationUndoPayloadSize() {
  return sizeof(QuantizedGenerationDetail::GenerationUndoPayload);
}

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_MIGRATION_QUANTIZED_GENERATION_UNDO_OWNER_IMPL_H
