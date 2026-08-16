from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"anchor not found in {path}: {old[:100]!r}")
    p.write_text(text.replace(old, new, 1))


# Shared candidate/activation slot metadata: capture the committed revision and
# add an Armed state. Armed is readable by AudioTask as the old audible overlay
# but BAR_START may claim only Ready, which closes the tiny COMMIT publication
# race without blocking/spinning the audio thread.
replace_once(
    "src/generation/migration/quantized_generation_commit_impl.h",
    """  QuantizedGenerationScope scope = QuantizedGenerationScope::Full;\n  SynthPattern synth[2]{};\n  DrumPatternSet drums{};\n};\n\nenum class SlotState : uint8_t {\n  Empty = 0,\n  Writing,\n  Ready,\n  Reading,\n};\n""",
    """  QuantizedGenerationScope scope = QuantizedGenerationScope::Full;\n  SynthPattern synth[2]{};\n  DrumPatternSet drums{};\n  uint32_t committedRevision = 0;\n};\n\nenum class SlotState : uint8_t {\n  Empty = 0,\n  Writing,\n  Armed,\n  Ready,\n  Reading,\n};\n""",
)

replace_once(
    "src/generation/migration/quantized_generation_commit_impl.h",
    """inline PatternTarget captureTarget(SceneManager& scenes) {\n""",
    """inline PatternTarget captureTarget(const SceneManager& scenes) {\n""",
)
replace_once(
    "src/generation/migration/quantized_generation_commit_impl.h",
    """inline bool targetStillActive(SceneManager& scenes, const PatternTarget& target) {\n""",
    """inline bool targetStillActive(const SceneManager& scenes, const PatternTarget& target) {\n""",
)

old_lease_start = "inline WriteLease acquireWriteLease() {"
old_lease_end = "inline void releaseWriteSlot(int slot) {"
p = Path("src/generation/migration/quantized_generation_commit_impl.h")
s = p.read_text()
a = s.index(old_lease_start)
b = s.index(old_lease_end, a)
new_lease = r'''inline WriteLease acquireWriteLease() {
  // 0.9.9-C policy: one pending audible activation, no hidden queue.
  // A second generation intent while an activation is Armed/Ready is rejected
  // before PREPARE/COMMIT. This deliberately replaces A-stage newest-wins
  // recycling with an explicit Busy policy.
  if (g_publishedSlot.load(std::memory_order_acquire) >= 0) {
    return WriteLease{};
  }

  for (int slot = 0; slot < 2; ++slot) {
    const auto state = static_cast<SlotState>(
        g_slotState[slot].load(std::memory_order_acquire));
    if (state != SlotState::Empty) return WriteLease{};
  }

  for (int slot = 0; slot < 2; ++slot) {
    uint8_t expected = static_cast<uint8_t>(SlotState::Empty);
    if (g_slotState[slot].compare_exchange_strong(
            expected,
            static_cast<uint8_t>(SlotState::Writing),
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      return WriteLease{slot, false};
    }
  }
  return WriteLease{};
}

inline int acquireCompanionActivationSlot(int preparedSlot) {
  for (int slot = 0; slot < 2; ++slot) {
    if (slot == preparedSlot) continue;
    uint8_t expected = static_cast<uint8_t>(SlotState::Empty);
    if (g_slotState[slot].compare_exchange_strong(
            expected,
            static_cast<uint8_t>(SlotState::Writing),
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      return slot;
    }
  }
  return -1;
}

'''
p.write_text(s[:a] + new_lease + s[b:])

# Append C statuses without changing accepted A/B numeric identities.
replace_once(
    "src/generation/migration/quantized_generation_commit_impl.h",
    """  Busy,\n  AttemptUnavailable,\n};\n""",
    """  Busy,\n  AttemptUnavailable,\n  Activated,\n  CancelledRevisionChanged,\n  CancelledExplicit,\n};\n""",
)

# Replace B1's combined persistent/runtime apply + BAR_START persistent COMMIT
# with the C split: persistent COMMIT now, old audible overlay until BAR_START,
# runtime activation only at the boundary.
p = Path("src/generation/migration/quantized_generation_undo_owner_impl.h")
s = p.read_text()
a = s.index("inline void applyPreparedGeneration(")
b = s.index("inline bool validateGenerationUndo(", a)
new_owner_core = r'''inline void applyPreparedGenerationPersistent(
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
  if (pending.owner != &engine ||
      !targetValid(pending.target) ||
      !targetStillActive(engine.sceneManager(), pending.target)) {
    return nullptr;
  }
  return &pending;
}

inline const SynthPattern* pendingAudibleSynthPattern(
    const MiniAcid& engine, int voice) {
  const PendingGeneration* pending = pendingAudibleActivation(engine);
  if (pending == nullptr || voice < 0 || voice > 1) return nullptr;
  const bool included = pending->scope == QuantizedGenerationScope::Full ||
      (voice == 0 && pending->scope == QuantizedGenerationScope::SynthA) ||
      (voice == 1 && pending->scope == QuantizedGenerationScope::SynthB);
  return included ? &pending->synth[voice] : nullptr;
}

inline const DrumPatternSet* pendingAudibleDrumPatternSet(
    const MiniAcid& engine) {
  const PendingGeneration* pending = pendingAudibleActivation(engine);
  if (pending == nullptr ||
      pending->scope != QuantizedGenerationScope::Full) return nullptr;
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

'''
p.write_text(s[:a] + new_owner_core + s[b:])

# Replace BAR_START function with activation-only work. It leaves Armed slots in
# place if COMMIT is crossing the boundary, so that request safely activates at
# the following BAR_START instead of exposing half-published state.
p = Path("src/generation/migration/quantized_generation_undo_owner_impl.h")
s = p.read_text()
a = s.index("inline bool commitQuantizedGenerationAtBarStart(SceneManager& scenes) {")
b = s.index("inline QuantizedGenerationResult regenerateWithQuantizedCommit(", a)
bar_fn = r'''inline bool commitQuantizedGenerationAtBarStart(SceneManager& scenes) {
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

  const uint32_t currentRevision =
      GroovePuterState::sceneRevisionSnapshot().currentRevision;
  if (pending.committedRevision == 0 ||
      currentRevision != pending.committedRevision) {
    g_slotState[slot].store(
        static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
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

'''
p.write_text(s[:a] + bar_fn + s[b:])

# Replace the tail of full-generation flow after PREPARE. Persistent COMMIT is
# now immediate under PLAY; a companion fixed slot carries only the OLD audible
# snapshot until BAR_START.
p = Path("src/generation/migration/quantized_generation_undo_owner_impl.h")
s = p.read_text()
old = r'''  if (!engine.isPlaying()) {
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
}'''
new = r'''  GenerationUndoPayload before = captureGenerationUndo(
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
}'''
if s.count(old) != 1:
    raise SystemExit(f"full generation flow anchor count={s.count(old)}")
s = s.replace(old, new, 1)
p.write_text(s)

# Replace the analogous synth-only tail (the remaining identical B1 tail).
p = Path("src/generation/migration/quantized_generation_undo_owner_impl.h")
s = p.read_text()
old2 = r'''  if (!engine.isPlaying()) {
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
}'''
new2 = r'''  GenerationUndoPayload before = captureGenerationUndo(
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
}'''
if s.count(old2) != 1:
    raise SystemExit(f"synth generation flow anchor count={s.count(old2)}")
s = s.replace(old2, new2, 1)
p.write_text(s)

# Generation Undo must cancel the matching scheduled audible activation, but
# only after the persistent restore has succeeded.
replace_once(
    "src/generation/migration/quantized_generation_undo_owner_impl.h",
    r'''inline GroovePuterUndo::UndoResult undoLastQuantizedGeneration(MiniAcid& engine) {
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
''',
    r'''inline GroovePuterUndo::UndoResult undoLastQuantizedGeneration(MiniAcid& engine) {
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
''',
)

# MiniAcid sees the old audible overlay while Scene already contains committed
# truth. Include C's public inline accessors only in the .cpp to avoid widening
# the engine header dependency graph.
replace_once(
    "src/dsp/miniacid_engine.cpp",
    """#include \"../input/musical_event_queue.h\"\n\n#include \"../sampler/sample_index.h\"\n""",
    """#include \"../input/musical_event_queue.h\"\n#include \"../generation/migration/quantized_generation_commit.h\"\n\n#include \"../sampler/sample_index.h\"\n""",
)

# Runtime-only mode activation entry point.
replace_once(
    "src/dsp/miniacid_engine.h",
    """  void setGrooveboxMode(GrooveboxMode mode);\n  GrooveboxMode grooveboxMode() const;\n""",
    """  void setGrooveboxMode(GrooveboxMode mode);\n  void activateCommittedGrooveboxModeRuntime(GrooveboxMode mode);\n  GrooveboxMode grooveboxMode() const;\n""",
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    r'''void MiniAcid::setGrooveboxMode(GrooveboxMode mode) {
  sceneManager_.setMode(mode);
  modeManager_.setModeLocal(mode);
  syncModeToVoices();
}

void MiniAcid::syncModeToVoices() {''',
    r'''void MiniAcid::setGrooveboxMode(GrooveboxMode mode) {
  sceneManager_.setMode(mode);
  modeManager_.setModeLocal(mode);
  syncModeToVoices();
}

void MiniAcid::activateCommittedGrooveboxModeRuntime(GrooveboxMode mode) {
  // SceneManager already contains the committed mode. ACTIVATE only publishes
  // the matching DSP/runtime state and therefore owns no persistence revision.
  modeManager_.setModeLocal(mode);
  syncModeToVoices();
}

void MiniAcid::syncModeToVoices() {''',
)

# Overlay all material read by AudioTask, including direct drum-set timing and
# groove-recipe/swing reads that would otherwise leak committed truth mid-bar.
replace_once(
    "src/dsp/miniacid_engine.cpp",
    r'''const SynthPattern& MiniAcid::activeSynthPattern(int synthIndex) const {
  int idx = clamp303Voice(synthIndex);
  SongTrack track = idx == 0 ? SongTrack::SynthA : SongTrack::SynthB;
  int pat = songPatternIndexForTrack(track);
  if (pat < 0) return kEmptySynthPattern;
  return sceneManager_.getSynthPattern(idx, pat);
}

const DrumPattern& MiniAcid::activeDrumPattern(int drumVoiceIndex) const {
  int idx = clampDrumVoice(drumVoiceIndex);
  int pat = songPatternIndexForTrack(SongTrack::Drums);
  const DrumPatternSet& set = pat >= 0 ? sceneManager_.getDrumPatternSet(pat)
                                       : kEmptyDrumPatternSet;
  return set.voices[idx];
}''',
    r'''const SynthPattern& MiniAcid::activeSynthPattern(int synthIndex) const {
  int idx = clamp303Voice(synthIndex);
  SongTrack track = idx == 0 ? SongTrack::SynthA : SongTrack::SynthB;
  int pat = songPatternIndexForTrack(track);
  if (pat < 0) return kEmptySynthPattern;
  if (const SynthPattern* pending =
          GroovePuterRhythm::QuantizedGenerationDetail::pendingAudibleSynthPattern(
              *this, idx)) {
    return *pending;
  }
  return sceneManager_.getSynthPattern(idx, pat);
}

const DrumPattern& MiniAcid::activeDrumPattern(int drumVoiceIndex) const {
  int idx = clampDrumVoice(drumVoiceIndex);
  int pat = songPatternIndexForTrack(SongTrack::Drums);
  if (const DrumPatternSet* pending =
          GroovePuterRhythm::QuantizedGenerationDetail::pendingAudibleDrumPatternSet(
              *this)) {
    return pending->voices[idx];
  }
  const DrumPatternSet& set = pat >= 0 ? sceneManager_.getDrumPatternSet(pat)
                                       : kEmptyDrumPatternSet;
  return set.voices[idx];
}''',
)

replace_once(
    "src/dsp/miniacid_engine.cpp",
    r'''  // Timing constants
  int swingPct = sceneManager_.currentScene().feel.swingPct;
''',
    r'''  // Timing constants. During C pending activation the Scene already holds
  // committed truth, while the old swing remains the audible truth until BAR_START.
  int swingPct = GroovePuterRhythm::QuantizedGenerationDetail::audibleGenerationSwingPct(
      *this, sceneManager_.currentScene().feel.swingPct);
''',
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    r'''    // Drums
    const DrumPatternSet& dSet = sceneManager_.getCurrentDrumPattern();
''',
    r'''    // Drums
    const DrumPatternSet* pendingDrums =
        GroovePuterRhythm::QuantizedGenerationDetail::pendingAudibleDrumPatternSet(*this);
    const DrumPatternSet& dSet = pendingDrums
        ? *pendingDrums
        : sceneManager_.getCurrentDrumPattern();
''',
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    r'''  const auto recipe = genreManager_.getGrooveRecipe();
''',
    r'''  const GenreSettings* audibleGenre =
      GroovePuterRhythm::QuantizedGenerationDetail::pendingAudibleGenreSettings(*this);
  const auto recipe = audibleGenre
      ? GenreCatalog::grooveRecipe(*audibleGenre)
      : genreManager_.getGrooveRecipe();
''',
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    r'''  const DrumPatternSet& currentDrumPatternSet = sceneManager_.getCurrentDrumPattern();
''',
    r'''  const DrumPatternSet* pendingDrums =
      GroovePuterRhythm::QuantizedGenerationDetail::pendingAudibleDrumPatternSet(*this);
  const DrumPatternSet& currentDrumPatternSet = pendingDrums
      ? *pendingDrums
      : sceneManager_.getCurrentDrumPattern();
''',
)

# Lifecycle cancellation: pending is runtime-only and never survives reset,
# STOP, a successful project load, or New. Cancellation does not roll back Scene.
replace_once(
    "src/dsp/miniacid_engine.cpp",
    r'''void MiniAcid::reset() {
  LOG_PRINTLN("    - MiniAcid::reset: Start");
''',
    r'''void MiniAcid::reset() {
  GroovePuterRhythm::QuantizedGenerationDetail::cancelPendingGenerationActivation(*this);
  LOG_PRINTLN("    - MiniAcid::reset: Start");
''',
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    r'''void MiniAcid::stop() {
  LOG_PRINTLN("[DSP] STOP command received");
''',
    r'''void MiniAcid::stop() {
  GroovePuterRhythm::QuantizedGenerationDetail::cancelPendingGenerationActivation(*this);
  LOG_PRINTLN("[DSP] STOP command received");
''',
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    r'''  if (!loaded) {
    Serial.printf("[LoadScene] FAILED - reverting to: %s\n", previousName.c_str());
''',
    r'''  if (!loaded) {
    Serial.printf("[LoadScene] FAILED - reverting to: %s\n", previousName.c_str());
''',
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    r'''  Serial.println("[LoadScene] Applying scene state...");
  applySceneStateFromManager();
''',
    r'''  GroovePuterRhythm::QuantizedGenerationDetail::cancelPendingGenerationActivation(*this);
  Serial.println("[LoadScene] Applying scene state...");
  applySceneStateFromManager();
''',
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    r'''bool MiniAcid::createNewSceneWithName(const std::string& name) {
  if (!sceneStorage_) return false;
''',
    r'''bool MiniAcid::createNewSceneWithName(const std::string& name) {
  if (!sceneStorage_) return false;
  GroovePuterRhythm::QuantizedGenerationDetail::cancelPendingGenerationActivation(*this);
''',
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    r'''void MiniAcid::loadSceneFromStorage() {
  lastSceneLoadRecoveredAutosave_ = false;
''',
    r'''void MiniAcid::loadSceneFromStorage() {
  GroovePuterRhythm::QuantizedGenerationDetail::cancelPendingGenerationActivation(*this);
  lastSceneLoadRecoveredAutosave_ = false;
''',
)

# Save must serialize committed BPM, not overwrite it with the old audible BPM
# while a full activation is pending.
replace_once(
    "src/dsp/miniacid_engine.cpp",
    r'''void MiniAcid::syncSceneStateToManager() {
  sceneManager_.setBpm(bpmValue);
''',
    r'''void MiniAcid::syncSceneStateToManager() {
  if (!GroovePuterRhythm::QuantizedGenerationDetail::hasPendingFullGenerationActivation(*this)) {
    sceneManager_.setBpm(bpmValue);
  }
''',
)

# Update the Genre hook comment: the hook is retained to keep the transport
# surface narrow, but C changes its ownership from COMMIT to ACTIVATE.
replace_once(
    "src/dsp/genre_manager.h",
    r'''    // Compatibility bridge for MiniAcid's existing BAR_START hook. The old
    // pending-recipe owner was removed; quantized material generation installs
    // a bounded commit callback instead. Returning false prevents the legacy
    // audio-thread regeneration path from running after the callback commits.
''',
    r'''    // Compatibility bridge for MiniAcid's existing BAR_START hook. The old
    // pending-recipe owner was removed; 0.9.9-C installs a bounded ACTIVATE
    // callback here. Persistent COMMIT has already completed on the control
    // side. Returning false prevents the legacy audio-thread regeneration path.
''',
)

# Fallback G: same generator and compact Undo receipt, but PLAY joins the same
# activation owner. The old Pattern is armed as audible overlay before COMMIT;
# no note-off/selector is forced mid-bar. Compact Undo cancels matching pending.
p = Path("src/ui/pages/pattern_edit_page.cpp")
s = p.read_text()
old_compact_undo = r'''    if (owner.kind() == UndoKind::Generation &&
        owner.payloadSize() == sizeof(SynthPatternUndoPayload)) {
      const UndoResult result =
          owner.undoPrepared<SynthPatternUndoPayload>(
              UndoKind::Generation,
              [&](const SynthPatternUndoPayload& receipt) {
                return GroovePuterUndo::synthPatternUndoTargetAvailable(
                    mini_acid_.sceneManager(), receipt);
              },
              [&](const SynthPatternUndoPayload& receipt) {
                const auto restore = [&]() {
                  GroovePuterUndo::restoreSynthPatternUndo(
                      mini_acid_.sceneManager(), receipt);
                };
                if (audio_guard_) audio_guard_(restore);
                else restore();
              });
'''
new_compact_undo = r'''    if (owner.kind() == UndoKind::Generation &&
        owner.payloadSize() == sizeof(SynthPatternUndoPayload)) {
      const uint32_t committedRevision = owner.committedRevision();
      const UndoResult result =
          owner.undoPrepared<SynthPatternUndoPayload>(
              UndoKind::Generation,
              [&](const SynthPatternUndoPayload& receipt) {
                return GroovePuterUndo::synthPatternUndoTargetAvailable(
                    mini_acid_.sceneManager(), receipt);
              },
              [&](const SynthPatternUndoPayload& receipt) {
                const auto restore = [&]() {
                  GroovePuterUndo::restoreSynthPatternUndo(
                      mini_acid_.sceneManager(), receipt);
                };
                if (audio_guard_) audio_guard_(restore);
                else restore();
              });
      if (result == UndoResult::Restored) {
        GroovePuterRhythm::QuantizedGenerationDetail::
            cancelPendingGenerationActivationForRevision(
                mini_acid_, committedRevision);
      }
'''
if s.count(old_compact_undo) != 1:
    raise SystemExit(f"compact undo anchor count={s.count(old_compact_undo)}")
s = s.replace(old_compact_undo, new_compact_undo, 1)
p.write_text(s)

p = Path("src/ui/pages/pattern_edit_page.cpp")
s = p.read_text()
a = s.index("  // B2 closes the R3 generation handoff")
b = s.index("\n  if (keyF)", a)
new_fallback = r'''  // C keeps B2's legacy/fallback musical generator but joins the one
  // bounded activation contract. STOP commits and is audible immediately.
  // PLAY arms the old Pattern as audible truth, commits the new Pattern now,
  // then releases that overlay only at BAR_START.
  if (keyG) {
    using GroovePuterUndo::SynthPatternUndoPayload;
    using GroovePuterUndo::UndoKind;

    SceneManager& manager = mini_acid_.sceneManager();
    SynthPatternUndoPayload before{};
    if (!GroovePuterUndo::captureCurrentSynthPatternUndo(
            manager, voice_index_, before)) {
      return true;
    }

    SynthPattern after = before.before;
    preparePatternEditorGeneration(mini_acid_, voice_index_, after);
    if (GroovePuterUndo::PatternEdit::samePattern(before.before, after)) {
      return true;
    }
    if (!GroovePuterUndo::synthPatternUndoTargetAvailable(manager, before)) {
      return true;
    }

    SynthPatternUndoPayload prepared = before;
    prepared.before = after;
    int activationSlot = -1;
    if (mini_acid_.isPlaying()) {
      const auto target = GroovePuterRhythm::QuantizedGenerationDetail::
          captureGenerationActivationTarget(manager);
      activationSlot = GroovePuterRhythm::QuantizedGenerationDetail::
          armCompactSynthActivation(
              mini_acid_, target, voice_index_, before.before);
      if (activationSlot < 0) {
        UI::showToast("GEN BUSY", 800);
        return true;
      }
    }

    const bool committed = GroovePuterUndo::undoOwner().commitPrepared(
        UndoKind::Generation, before, [&]() {
          GroovePuterUndo::restoreSynthPatternUndo(manager, prepared);
        });
    if (!committed) {
      if (activationSlot >= 0) {
        GroovePuterRhythm::QuantizedGenerationDetail::abortArmedActivation(
            activationSlot,
            GroovePuterRhythm::QuantizedGenerationStatus::Busy);
      }
      return true;
    }

    if (activationSlot >= 0) {
      GroovePuterRhythm::QuantizedGenerationDetail::completeArmedActivation(
          activationSlot,
          GroovePuterUndo::undoOwner().committedRevision());
      UI::showToast("GEN -> NEXT BAR", 1000);
    }
    return true;
  }
'''
p.write_text(s[:a] + new_fallback + s[b:])

print("0.9.9-C production patch applied")
