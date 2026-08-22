#ifndef GROOVEPUTER_GENERATION_MIGRATION_QUANTIZED_GENERATION_UNDO_OWNER_IMPL_H
#define GROOVEPUTER_GENERATION_MIGRATION_QUANTIZED_GENERATION_UNDO_OWNER_IMPL_H

#include <cstddef>
#include <type_traits>

#include "../../state/undo_owner.h"

namespace GroovePuterRhythm {

bool commitQuantizedGenerationAtBarStart(SceneManager& scenes);

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

inline void applyPreparedGenerationPersistent(
    MiniAcid& engine,
    const PendingGeneration& pending) {
  SceneManager& scenes = engine.sceneManager();
  Scene& scene = scenes.currentScene();
  if (pending.scope == QuantizedGenerationScope::Full) {
    scene.synthABanks[pending.target.synthBank[0]]
        .patterns[pending.target.synthSlot[0]] = pending.synth[0];
    scene.synthBBanks[pending.target.synthBank[1]]
        .patterns[pending.target.synthSlot[1]] = pending.synth[1];
    scene.drumBanks[pending.target.drumBank]
        .patterns[pending.target.drumSlot] = pending.drums;
    scene.genre = pending.genre;
    scene.feel.swingPct = pending.swingPct;
    // Persistent truth is complete at COMMIT. Runtime mode/BPM stay on the
    // old audible truth until ACTIVATE when transport is playing.
    scenes.setMode(pending.mode);
    scenes.setBpm(pending.bpm);
    return;
  }

  const int voice = pending.scope == QuantizedGenerationScope::SynthA ? 0 : 1;
  Bank<SynthPattern>& bank = voice == 0
      ? scene.synthABanks[pending.target.synthBank[0]]
      : scene.synthBBanks[pending.target.synthBank[1]];
  bank.patterns[pending.target.synthSlot[voice]] = pending.synth[voice];
}

inline void activatePreparedGenerationRuntime(
    MiniAcid& engine,
    const PendingGeneration& pending) {
  if (pending.scope != QuantizedGenerationScope::Full) return;
  engine.activateCommittedGrooveboxModeRuntime(pending.mode);
  engine.setBpm(pending.bpm);
}

inline bool commitPreparedGeneration(
    MiniAcid& engine,
    const PendingGeneration& pending,
    const GenerationUndoPayload& before) {
  SceneManager& scenes = engine.sceneManager();
  if (!targetValid(pending.target) ||
      !targetStillActive(scenes, pending.target)) {
    return false;
  }

  return GroovePuterUndo::undoOwner().commitPrepared(
      GroovePuterUndo::UndoKind::Generation,
      before,
      [&engine, &pending]() {
        applyPreparedGenerationPersistent(engine, pending);
      });
}

inline bool commitPreparedGeneration(
    MiniAcid& engine,
    const PendingGeneration& pending,
    GenerationUndoPayload* capturedBefore = nullptr) {
  const GenerationUndoPayload before = captureGenerationUndo(
      engine, pending.target, pending.scope);
  if (!commitPreparedGeneration(engine, pending, before)) return false;
  if (capturedBefore != nullptr) *capturedBefore = before;
  return true;
}

inline void fillAudibleActivationSnapshot(
    PendingGeneration& activation,
    MiniAcid& engine,
    const PatternTarget& target,
    QuantizedGenerationScope scope,
    const GenerationUndoPayload& before,
    GrooveboxMode activateMode,
    float activateBpm) {
  activation = PendingGeneration{};
  activation.owner = &engine;
  activation.target = target;
  activation.scope = scope;
  activation.genre = before.genre;
  activation.swingPct = before.swingPct;
  activation.synth[0] = before.synth[0];
  activation.synth[1] = before.synth[1];
  activation.drums = before.drums;
  // These two values are the NEW committed runtime controls to publish only
  // when the old audible overlay is released at BAR_START.
  activation.mode = activateMode;
  activation.bpm = activateBpm;
  activation.committedRevision = 0;
}

inline void armActivationSlot(int slot) {
  g_slotState[slot].store(
      static_cast<uint8_t>(SlotState::Armed), std::memory_order_release);
  g_publishedSlot.store(static_cast<int8_t>(slot), std::memory_order_release);
}

inline void completeArmedActivation(int slot, uint32_t committedRevision) {
  if (slot < 0 || slot > 1) return;
  g_slots[slot].committedRevision = committedRevision;
  g_slotState[slot].store(
      static_cast<uint8_t>(SlotState::Ready), std::memory_order_release);
  g_status.store(
      static_cast<uint8_t>(QuantizedGenerationStatus::PendingNextBar),
      std::memory_order_release);
  if (g_slots[slot].owner != nullptr) {
    g_slots[slot].owner->genreManager().setPendingCommitHook(
        &commitQuantizedGenerationAtBarStart);
  }
}

inline void abortArmedActivation(int slot,
                                 QuantizedGenerationStatus status) {
  if (slot < 0 || slot > 1) return;
  int8_t expectedSlot = static_cast<int8_t>(slot);
  g_publishedSlot.compare_exchange_strong(
      expectedSlot, -1, std::memory_order_acq_rel, std::memory_order_acquire);
  g_slotState[slot].store(
      static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
  g_status.store(static_cast<uint8_t>(status), std::memory_order_release);
}

inline const PendingGeneration* pendingAudibleActivation(
    const MiniAcid& engine) {
  const int slot = g_publishedSlot.load(std::memory_order_acquire);
  if (slot < 0 || slot > 1) return nullptr;
  const SlotState state = static_cast<SlotState>(
      g_slotState[slot].load(std::memory_order_acquire));
  if (state != SlotState::Armed && state != SlotState::Ready) return nullptr;
  const PendingGeneration& pending = g_slots[slot];
  if (pending.owner != &engine || !targetValid(pending.target)) return nullptr;
  // Global audible truth stays on the old snapshot until BAR_START even
  // if a selector changes. Material accessors validate target identity
  // separately so old Pattern bytes are never redirected.
  return &pending;
}

inline const SynthPattern* pendingAudibleSynthPattern(
    const MiniAcid& engine, int voice) {
  const PendingGeneration* pending = pendingAudibleActivation(engine);
  if (pending == nullptr || voice < 0 || voice > 1) return nullptr;
  const bool included = pending->scope == QuantizedGenerationScope::Full ||
      (voice == 0 && pending->scope == QuantizedGenerationScope::SynthA) ||
      (voice == 1 && pending->scope == QuantizedGenerationScope::SynthB);
  if (!included ||
      !targetStillActive(engine.sceneManager(), pending->target)) return nullptr;
  return &pending->synth[voice];
}

inline const DrumPatternSet* pendingAudibleDrumPatternSet(
    const MiniAcid& engine) {
  const PendingGeneration* pending = pendingAudibleActivation(engine);
  if (pending == nullptr ||
      pending->scope != QuantizedGenerationScope::Full ||
      !targetStillActive(engine.sceneManager(), pending->target)) return nullptr;
  return &pending->drums;
}

inline const GenreSettings* pendingAudibleGenreSettings(
    const MiniAcid& engine) {
  const PendingGeneration* pending = pendingAudibleActivation(engine);
  if (pending == nullptr ||
      pending->scope != QuantizedGenerationScope::Full) return nullptr;
  return &pending->genre;
}

inline uint8_t audibleGenerationSwingPct(const MiniAcid& engine,
                                         uint8_t committedSwingPct) {
  const PendingGeneration* pending = pendingAudibleActivation(engine);
  if (pending == nullptr ||
      pending->scope != QuantizedGenerationScope::Full) {
    return committedSwingPct;
  }
  return pending->swingPct;
}

inline bool hasPendingFullGenerationActivation(const MiniAcid& engine) {
  const PendingGeneration* pending = pendingAudibleActivation(engine);
  return pending != nullptr &&
         pending->scope == QuantizedGenerationScope::Full;
}

inline void synchronizeCommittedGenerationRuntime(MiniAcid& engine) {
  // Removing pending never rolls persistent Scene truth back. If normal
  // ACTIVATE is skipped, runtime controls converge to current committed
  // mode/BPM so the next transport start cannot use stale controls.
  engine.activateCommittedGrooveboxModeRuntime(engine.sceneManager().getMode());
  engine.setBpm(engine.sceneManager().getBpm());
}

inline bool cancelPendingGenerationActivationForRevision(
    MiniAcid& engine, uint32_t committedRevision) {
  const int slot = g_publishedSlot.load(std::memory_order_acquire);
  if (slot < 0 || slot > 1) return false;
  const PendingGeneration& pending = g_slots[slot];
  if (pending.owner != &engine ||
      pending.committedRevision != committedRevision) return false;

  uint8_t expectedState = static_cast<uint8_t>(SlotState::Ready);
  if (!g_slotState[slot].compare_exchange_strong(
          expectedState,
          static_cast<uint8_t>(SlotState::Empty),
          std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    return false;
  }
  int8_t expectedSlot = static_cast<int8_t>(slot);
  g_publishedSlot.compare_exchange_strong(
      expectedSlot, -1, std::memory_order_acq_rel, std::memory_order_acquire);
  g_status.store(
      static_cast<uint8_t>(QuantizedGenerationStatus::CancelledExplicit),
      std::memory_order_release);
  return true;
}

inline bool cancelPendingGenerationActivation(MiniAcid& engine) {
  const int slot = g_publishedSlot.load(std::memory_order_acquire);
  if (slot < 0 || slot > 1) return false;
  PendingGeneration& pending = g_slots[slot];
  if (pending.owner != &engine) return false;

  uint8_t state = g_slotState[slot].load(std::memory_order_acquire);
  if (state != static_cast<uint8_t>(SlotState::Armed) &&
      state != static_cast<uint8_t>(SlotState::Ready)) {
    return false;
  }
  if (!g_slotState[slot].compare_exchange_strong(
          state,
          static_cast<uint8_t>(SlotState::Empty),
          std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    return false;
  }
  int8_t expectedSlot = static_cast<int8_t>(slot);
  g_publishedSlot.compare_exchange_strong(
      expectedSlot, -1, std::memory_order_acq_rel, std::memory_order_acquire);
  g_status.store(
      static_cast<uint8_t>(QuantizedGenerationStatus::CancelledExplicit),
      std::memory_order_release);
  return true;
}

inline int armCompactSynthActivation(
    MiniAcid& engine,
    const PatternTarget& target,
    int voice,
    const SynthPattern& audibleBefore) {
  if (voice < 0 || voice > 1 || !targetValid(target)) return -1;
  const WriteLease lease = acquireWriteLease();
  if (lease.slot < 0) {
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::Busy),
        std::memory_order_release);
    return -1;
  }

  PendingGeneration& activation = g_slots[lease.slot];
  activation = PendingGeneration{};
  activation.owner = &engine;
  activation.target = target;
  activation.scope = voice == 0
      ? QuantizedGenerationScope::SynthA
      : QuantizedGenerationScope::SynthB;
  activation.synth[voice] = audibleBefore;
  activation.genre = engine.sceneManager().currentScene().genre;
  activation.swingPct = engine.sceneManager().currentScene().feel.swingPct;
  activation.mode = engine.grooveboxMode();
  activation.bpm = engine.bpm();
  armActivationSlot(lease.slot);
  return lease.slot;
}

inline PatternTarget captureGenerationActivationTarget(
    const SceneManager& scenes) {
  return captureTarget(scenes);
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
    engine.sceneManager().setBpm(before.bpm);
    engine.setBpm(before.bpm);
    return;
  }

  const int voice = before.scope == QuantizedGenerationScope::SynthA ? 0 : 1;
  Bank<SynthPattern>& bank = voice == 0
      ? scene.synthABanks[before.target.synthBank[0]]
      : scene.synthBBanks[before.target.synthBank[1]];
  bank.patterns[before.target.synthSlot[voice]] = before.synth[voice];
}

inline void exchangeGenerationUndo(MiniAcid& engine,
                                   GenerationUndoPayload& retained) {
  SceneManager& scenes = engine.sceneManager();
  Scene& scene = scenes.currentScene();
  if (retained.scope == QuantizedGenerationScope::Full) {
    GroovePuterUndo::exchangeFixedValue(
        scene.synthABanks[retained.target.synthBank[0]]
            .patterns[retained.target.synthSlot[0]],
        retained.synth[0]);
    GroovePuterUndo::exchangeFixedValue(
        scene.synthBBanks[retained.target.synthBank[1]]
            .patterns[retained.target.synthSlot[1]],
        retained.synth[1]);
    GroovePuterUndo::exchangeFixedValue(
        scene.drumBanks[retained.target.drumBank]
            .patterns[retained.target.drumSlot],
        retained.drums);
    GroovePuterUndo::exchangeFixedValue(scene.genre, retained.genre);
    GroovePuterUndo::exchangeFixedValue(scene.feel.swingPct, retained.swingPct);

    const GrooveboxMode committedMode = scenes.getMode();
    scenes.setMode(retained.mode);
    retained.mode = committedMode;
    const float committedBpm = scenes.getBpm();
    scenes.setBpm(retained.bpm);
    retained.bpm = committedBpm;

    // During PLAY the old runtime truth is already the correct side while an
    // Undo-before-boundary cancels pending ACTIVATE. Never republish runtime
    // controls mid-bar. STOP can converge runtime immediately.
    if (!engine.isPlaying()) {
      engine.activateCommittedGrooveboxModeRuntime(scenes.getMode());
      engine.setBpm(scenes.getBpm());
    }
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
  const int slot = g_publishedSlot.load(std::memory_order_acquire);
  if (slot < 0 || slot > 1) return false;

  uint8_t expected = static_cast<uint8_t>(SlotState::Ready);
  if (!g_slotState[slot].compare_exchange_strong(
          expected,
          static_cast<uint8_t>(SlotState::Reading),
          std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    // Armed means COMMIT is crossing this exact BAR_START. Never wait/spin;
    // keep the old audible overlay and claim it on the next boundary.
    return false;
  }

  int8_t expectedSlot = static_cast<int8_t>(slot);
  if (!g_publishedSlot.compare_exchange_strong(
          expectedSlot, -1,
          std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    g_slotState[slot].store(
        static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
    return false;
  }

  const PendingGeneration& pending = g_slots[slot];
  MiniAcid* owner = pending.owner;
  if (owner == nullptr) {
    g_slotState[slot].store(
        static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::CancelledTargetChanged),
        std::memory_order_release);
    return false;
  }
  if (&owner->sceneManager() != &scenes ||
      !targetValid(pending.target) ||
      !targetStillActive(scenes, pending.target)) {
    g_slotState[slot].store(
        static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
    synchronizeCommittedGenerationRuntime(*owner);
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::CancelledTargetChanged),
        std::memory_order_release);
    return false;
  }

  const uint32_t currentRevision =
      GroovePuterState::sceneRevisionSnapshot().currentRevision;
  if (pending.committedRevision == 0 ||
      currentRevision != pending.committedRevision) {
    g_slotState[slot].store(
        static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
    synchronizeCommittedGenerationRuntime(*owner);
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::CancelledRevisionChanged),
        std::memory_order_release);
    return false;
  }

  // ACTIVATE is runtime-only: release the old audible overlay and synchronize
  // deferred mode/BPM. No Scene write, revision, Undo publication, allocation,
  // filesystem access or generation occurs at BAR_START.
  activatePreparedGenerationRuntime(*owner, pending);
  g_slotState[slot].store(
      static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
  g_status.store(
      static_cast<uint8_t>(QuantizedGenerationStatus::Activated),
      std::memory_order_release);
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

  GenerationUndoPayload before = captureGenerationUndo(
      engine, candidate.target, candidate.scope);

  if (!engine.isPlaying()) {
    const bool committed = commitPreparedGeneration(engine, candidate, before);
    if (committed) activatePreparedGenerationRuntime(engine, candidate);
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

  const int activationSlot = acquireCompanionActivationSlot(lease.slot);
  if (activationSlot < 0) {
    releaseWriteSlot(lease.slot);
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::Busy),
        std::memory_order_release);
    return QuantizedGenerationResult::Failed;
  }
  fillAudibleActivationSnapshot(
      g_slots[activationSlot], engine, candidate.target, candidate.scope,
      before, candidate.mode, candidate.bpm);
  armActivationSlot(activationSlot);

  const bool committed = commitPreparedGeneration(engine, candidate, before);
  releaseWriteSlot(lease.slot);
  if (!committed) {
    abortArmedActivation(activationSlot, QuantizedGenerationStatus::Busy);
    return QuantizedGenerationResult::Failed;
  }

  const uint32_t committedRevision =
      GroovePuterUndo::undoOwner().committedRevision();
  completeArmedActivation(activationSlot, committedRevision);
  g_commitSerial.fetch_add(1, std::memory_order_release);
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

  GenerationUndoPayload before = captureGenerationUndo(
      engine, candidate.target, candidate.scope);

  if (!engine.isPlaying()) {
    const bool committed = commitPreparedGeneration(engine, candidate, before);
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

  const int activationSlot = acquireCompanionActivationSlot(lease.slot);
  if (activationSlot < 0) {
    releaseWriteSlot(lease.slot);
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::Busy),
        std::memory_order_release);
    return QuantizedGenerationResult::Failed;
  }
  fillAudibleActivationSnapshot(
      g_slots[activationSlot], engine, candidate.target, candidate.scope,
      before, candidate.mode, candidate.bpm);
  armActivationSlot(activationSlot);

  const bool committed = commitPreparedGeneration(engine, candidate, before);
  releaseWriteSlot(lease.slot);
  if (!committed) {
    abortArmedActivation(activationSlot, QuantizedGenerationStatus::Busy);
    return QuantizedGenerationResult::Failed;
  }

  const uint32_t committedRevision =
      GroovePuterUndo::undoOwner().committedRevision();
  completeArmedActivation(activationSlot, committedRevision);
  g_commitSerial.fetch_add(1, std::memory_order_release);
  return QuantizedGenerationResult::PendingNextBar;
}

inline GroovePuterUndo::UndoResult undoLastQuantizedGeneration(MiniAcid& engine) {
  using namespace QuantizedGenerationDetail;
  const uint32_t committedRevision =
      GroovePuterUndo::undoOwner().committedRevision();
  const GroovePuterUndo::UndoResult result =
      GroovePuterUndo::undoOwner().undoPrepared<GenerationUndoPayload>(
          GroovePuterUndo::UndoKind::Generation,
          [&engine](const GenerationUndoPayload& before) {
            return validateGenerationUndo(engine, before);
          },
          [&engine](const GenerationUndoPayload& before) {
            restoreGenerationUndo(engine, before);
          });
  if (result == GroovePuterUndo::UndoResult::Restored) {
    cancelPendingGenerationActivationForRevision(engine, committedRevision);
  }
  return result;
}

inline GroovePuterUndo::UndoResult toggleLastQuantizedGeneration(MiniAcid& engine) {
  using namespace QuantizedGenerationDetail;
  auto& owner = GroovePuterUndo::undoOwner();
  if (!owner.hasUndo() || owner.kind() != GroovePuterUndo::UndoKind::Generation) {
    return GroovePuterUndo::UndoResult::NothingToUndo;
  }

  const bool redo = owner.nextIsRedo();
  const uint32_t committedRevision = owner.committedRevision();
  if (engine.isPlaying()) {
    // Undo is safe only while the matching old audible snapshot is still
    // authoritative. Redo during PLAY would require a fresh ACTIVATE snapshot;
    // keep the retained pair and require STOP instead of leaking mid-bar truth.
    const PendingGeneration* pending = pendingAudibleActivation(engine);
    const bool matchingPending = pending != nullptr &&
        pending->committedRevision == committedRevision;
    if (redo || !matchingPending) {
      return GroovePuterUndo::UndoResult::ContextUnavailable;
    }
  }

  const GroovePuterUndo::UndoResult result =
      owner.togglePrepared<GenerationUndoPayload>(
          GroovePuterUndo::UndoKind::Generation,
          [&engine](const GenerationUndoPayload& retained) {
            return validateGenerationUndo(engine, retained);
          },
          [&engine](GenerationUndoPayload& retained) {
            exchangeGenerationUndo(engine, retained);
          });
  if (result == GroovePuterUndo::UndoResult::Restored &&
      engine.isPlaying() && !redo) {
    cancelPendingGenerationActivationForRevision(engine, committedRevision);
  }
  return result;
}

inline std::size_t quantizedGenerationUndoPayloadSize() {
  return sizeof(QuantizedGenerationDetail::GenerationUndoPayload);
}

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_MIGRATION_QUANTIZED_GENERATION_UNDO_OWNER_IMPL_H
