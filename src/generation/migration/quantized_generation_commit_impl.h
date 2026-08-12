#ifndef GROOVEPUTER_QUANTIZED_GENERATION_COMMIT_IMPL_H
#define GROOVEPUTER_QUANTIZED_GENERATION_COMMIT_IMPL_H

#include <atomic>
#include <cstdint>

#include "../../dsp/atlas_runtime.h"
#include "../../dsp/miniacid_engine.h"
#include "../../dsp/mode_manager.h"
#include "../../state/generation_request_state.h"
#include "../../state/scene_revision.h"
#include "strong_rhythm_live_bridge.h"
#include "strong_rhythm_migration.h"

namespace GroovePuterRhythm {

enum class QuantizedGenerationResult : uint8_t {
  Failed = 0,
  CommittedNow,
  PendingNextBar,
  AttemptUnavailable,
};

enum class QuantizedGenerationStatus : uint8_t {
  Idle = 0,
  PendingNextBar,
  Committed,
  CancelledTargetChanged,
  Busy,
  AttemptUnavailable,
};

enum class QuantizedGenerationScope : uint8_t {
  Full = 0,
  SynthA,
  SynthB,
};

namespace QuantizedGenerationDetail {

struct PatternTarget {
  int page = -1;
  int synthBank[2] = {-1, -1};
  int synthSlot[2] = {-1, -1};
  int drumBank = -1;
  int drumSlot = -1;
};

struct PendingGeneration {
  MiniAcid* owner = nullptr;
  PatternTarget target{};
  GenreSettings genre{};
  GrooveboxMode mode = GrooveboxMode::Minimal;
  float bpm = 100.0f;
  uint8_t swingPct = 0;
  QuantizedGenerationScope scope = QuantizedGenerationScope::Full;
  SynthPattern synth[2]{};
  DrumPatternSet drums{};
};

enum class SlotState : uint8_t {
  Empty = 0,
  Writing,
  Ready,
  Reading,
};

struct WriteLease {
  int slot = -1;
  bool hasPreviousPending = false;
};

// Double-buffer publication keeps the AudioTask completely out of control-side
// generation. The UI writes only a slot it exclusively owns, then atomically
// publishes its index. BAR_START atomically claims one immutable Ready slot and
// never waits for the writer. The second slot is fixed DRAM, not heap state.
inline PendingGeneration g_slots[2]{};
inline std::atomic<uint8_t> g_slotState[2]{
    static_cast<uint8_t>(SlotState::Empty),
    static_cast<uint8_t>(SlotState::Empty),
};
inline std::atomic<int8_t> g_publishedSlot{-1};
inline std::atomic<uint8_t> g_status{
    static_cast<uint8_t>(QuantizedGenerationStatus::Idle)};
inline std::atomic<uint32_t> g_commitSerial{0};

inline PatternTarget captureTarget(SceneManager& scenes) {
  PatternTarget target{};
  target.page = scenes.currentPageIndex();
  target.synthBank[0] = scenes.getCurrentBankIndex(1);
  target.synthBank[1] = scenes.getCurrentBankIndex(2);
  target.synthSlot[0] = scenes.getCurrentSynthPatternIndex(0);
  target.synthSlot[1] = scenes.getCurrentSynthPatternIndex(1);
  target.drumBank = scenes.getCurrentBankIndex(0);
  target.drumSlot = scenes.getCurrentDrumPatternIndex();
  return target;
}

inline bool validBankSlot(int bank, int slot) {
  return bank >= 0 && bank < kBankCount &&
         slot >= 0 && slot < Bank<SynthPattern>::kPatterns;
}

inline bool targetValid(const PatternTarget& target) {
  return target.page >= 0 && target.page < kMaxPages &&
         validBankSlot(target.synthBank[0], target.synthSlot[0]) &&
         validBankSlot(target.synthBank[1], target.synthSlot[1]) &&
         validBankSlot(target.drumBank, target.drumSlot);
}

inline bool sameTarget(const PatternTarget& lhs, const PatternTarget& rhs) {
  return lhs.page == rhs.page &&
         lhs.synthBank[0] == rhs.synthBank[0] &&
         lhs.synthBank[1] == rhs.synthBank[1] &&
         lhs.synthSlot[0] == rhs.synthSlot[0] &&
         lhs.synthSlot[1] == rhs.synthSlot[1] &&
         lhs.drumBank == rhs.drumBank &&
         lhs.drumSlot == rhs.drumSlot;
}

inline int patternAddressFor(const PatternTarget& target) {
  return songPatternFromPageBankIndex(
      target.page, target.drumBank, target.drumSlot);
}

inline int synthPatternAddressFor(const PatternTarget& target, int voice) {
  if (voice < 0 || voice > 1) return -1;
  return songPatternFromPageBankIndex(
      target.page, target.synthBank[voice], target.synthSlot[voice]);
}

inline bool targetStillActive(SceneManager& scenes, const PatternTarget& target) {
  return sameTarget(captureTarget(scenes), target);
}

inline void retireReadySlot(int slot) {
  if (slot < 0 || slot > 1) return;
  uint8_t expected = static_cast<uint8_t>(SlotState::Ready);
  g_slotState[slot].compare_exchange_strong(
      expected,
      static_cast<uint8_t>(SlotState::Empty),
      std::memory_order_acq_rel,
      std::memory_order_acquire);
}

inline void clearPublished(QuantizedGenerationStatus status) {
  const int old = g_publishedSlot.exchange(-1, std::memory_order_acq_rel);
  retireReadySlot(old);
  g_status.store(static_cast<uint8_t>(status), std::memory_order_release);
}

inline WriteLease acquireWriteLease() {
  // Newest intent wins. First try to claim the currently published transaction
  // back from BAR_START. If BAR_START already claimed it for Reading, fail fast
  // rather than touching Scene while the audio thread is publishing material.
  const int old = g_publishedSlot.exchange(-1, std::memory_order_acq_rel);
  if (old >= 0 && old <= 1) {
    uint8_t expected = static_cast<uint8_t>(SlotState::Ready);
    if (g_slotState[old].compare_exchange_strong(
            expected,
            static_cast<uint8_t>(SlotState::Writing),
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      return WriteLease{old, true};
    }
    return WriteLease{};
  }

  // A non-published Ready/Reading/Writing slot means BAR_START and the control
  // side are crossing exactly now. The boundary copy is intentionally tiny;
  // reject this keypress as Busy instead of introducing a data race or spin in
  // AudioTask. A subsequent keypress can stage normally.
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

inline void releaseWriteSlot(int slot) {
  if (slot < 0 || slot > 1) return;
  g_slotState[slot].store(
      static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
}

inline void publishWriteSlot(int slot) {
  g_slotState[slot].store(
      static_cast<uint8_t>(SlotState::Ready), std::memory_order_release);
  const int old = g_publishedSlot.exchange(
      static_cast<int8_t>(slot), std::memory_order_acq_rel);
  if (old >= 0 && old != slot) retireReadySlot(old);
  g_status.store(
      static_cast<uint8_t>(QuantizedGenerationStatus::PendingNextBar),
      std::memory_order_release);
}

inline StrongRhythmMigrationContext migrationContextFor(
    const Scene& scene,
    const PatternTarget& target) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = static_cast<int16_t>(patternAddressFor(target));
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
  context.scaleTypeValue =
      static_cast<ScaleTypeValue>(scene.generatorParams.scale);
  return context;
}

inline bool allocateAttemptFor(
    const GenreSettings& genre,
    RealizationLevel level,
    const PatternTarget& target,
    uint32_t& attemptOrdinal) {
  attemptOrdinal = 0;
  if (selectStrongRhythmRoute(genre) == StrongRhythmRoute::Legacy) return true;
  const auto allocation = GroovePuterState::allocateGenerationAttempt(
      genre.generativeMode, genre.recipe, level, patternAddressFor(target));
  if (!allocation.ok()) return false;
  attemptOrdinal = allocation.ordinal;
  return true;
}

inline bool allocateSynthAttemptFor(
    const GenreSettings& genre,
    RealizationLevel level,
    const PatternTarget& target,
    int voice,
    uint32_t& attemptOrdinal) {
  attemptOrdinal = 0;
  if (selectStrongRhythmRoute(genre) == StrongRhythmRoute::Legacy) return true;
  const auto allocation = GroovePuterState::allocateGenerationAttempt(
      genre.generativeMode, genre.recipe, level,
      synthPatternAddressFor(target, voice));
  if (!allocation.ok()) return false;
  attemptOrdinal = allocation.ordinal;
  return true;
}

inline void applyLegacyRoleSplit(
    GenerativeMode mode,
    GenreBehavior& bassBehavior,
    GenreBehavior& leadBehavior) {
  if (mode != GenerativeMode::Reggae) return;

  bassBehavior.stepMask = 0x1111;
  bassBehavior.motifLength = 2;
  bassBehavior.avoidClusters = true;
  bassBehavior.forceOctaveJump = false;

  leadBehavior.stepMask = 0xAAAA;
  leadBehavior.motifLength = 4;
  leadBehavior.avoidClusters = false;
  leadBehavior.forceOctaveJump = false;
}

inline bool preparePlayingCandidate(
    MiniAcid& engine,
    const GenreSettings& requestedGenre,
    GrooveboxMode requestedMode,
    bool applyTempo,
    float requestedBpm,
    const PatternTarget& target,
    RealizationLevel requestLevel,
    uint32_t generationAttemptOrdinal,
    bool usePreviousPending,
    PendingGeneration& candidate) {
  SceneManager& scenes = engine.sceneManager();
  const Scene& scene = scenes.currentScene();
  if (!targetValid(target)) return false;

  const bool reusePriorMaterial = usePreviousPending &&
      candidate.owner == &engine &&
      sameTarget(candidate.target, target);

  candidate.owner = &engine;
  candidate.target = target;
  candidate.genre = requestedGenre;
  candidate.mode = requestedMode;
  candidate.bpm = applyTempo && requestedBpm > 0.0f
      ? requestedBpm
      : engine.bpm();
  candidate.swingPct = scene.feel.swingPct;
  candidate.scope = QuantizedGenerationScope::Full;

  if (!reusePriorMaterial) {
    // Copy the exact captured target, not a second lookup through mutable current
    // indices. If Song/page ownership moves while preparation is running, the
    // final targetStillActive() gate rejects publication.
    candidate.synth[0] = scene.synthABanks[target.synthBank[0]]
        .patterns[target.synthSlot[0]];
    candidate.synth[1] = scene.synthBBanks[target.synthBank[1]]
        .patterns[target.synthSlot[1]];
    candidate.drums = scene.drumBanks[target.drumBank]
        .patterns[target.drumSlot];
  }

  AtlasRuntimeMetadata atlasMetadata{};
  const bool atlasBacked = AtlasRuntime::applyRecipe(
      requestedGenre.recipe,
      0,
      candidate.synth[0],
      candidate.synth[1],
      candidate.drums,
      &atlasMetadata);

  if (atlasBacked) {
    candidate.swingPct = atlasMetadata.swingPercent;
    if (applyTempo && atlasMetadata.bpm > 0) {
      candidate.bpm = static_cast<float>(atlasMetadata.bpm);
    }
  } else {
    const GenerativeParams genreParams =
        GenreCatalog::compiledGenerativeParams(requestedGenre);
    const GenreBehavior behavior = GenreCatalog::behavior(requestedGenre);
    GenreBehavior bassBehavior = behavior;
    GenreBehavior leadBehavior = behavior;
    applyLegacyRoleSplit(
        static_cast<GenerativeMode>(requestedGenre.generativeMode),
        bassBehavior,
        leadBehavior);

    // Use a private generator state so requested Genre mode affects the same
    // deterministic RNG domain without touching the live ModeManager observed
    // by AudioTask. Engine access from generation is read-only here.
    GrooveboxModeManager scratchMode(engine);
    scratchMode.setModeLocal(requestedMode);
    scratchMode.setFlavorLocal(engine.modeManager().flavor());
    scratchMode.setGenerationSeed(engine.modeManager().generationSeed());
    scratchMode.generatePattern(
        candidate.synth[0], candidate.bpm, genreParams, bassBehavior, 0);
    scratchMode.generatePattern(
        candidate.synth[1], candidate.bpm, genreParams, leadBehavior, 1);
    scratchMode.generateDrumPattern(candidate.drums, genreParams, behavior);
  }

  StrongRhythmMigrationContext context = migrationContextFor(scene, target);
  context.level = requestLevel;
  context.generationAttemptOrdinal = generationAttemptOrdinal;
  (void)migrateStrongRhythmMaterial(
      requestedGenre,
      context,
      candidate.drums,
      candidate.synth[0],
      candidate.synth[1]);

  // The audio transport may advance Song/page ownership while generation runs.
  // Never publish a candidate for a target that is no longer the exact active
  // page/bank/slot tuple the user generated from.
  return targetStillActive(scenes, target);
}

inline bool prepareSynthCandidate(
    MiniAcid& engine,
    const GenreSettings& requestedGenre,
    const PatternTarget& target,
    int voice,
    RealizationLevel requestLevel,
    uint32_t generationAttemptOrdinal,
    PendingGeneration& candidate) {
  SceneManager& scenes = engine.sceneManager();
  const Scene& scene = scenes.currentScene();
  if (!targetValid(target) || voice < 0 || voice > 1) return false;

  candidate.owner = &engine;
  candidate.target = target;
  candidate.genre = requestedGenre;
  candidate.mode = engine.grooveboxMode();
  candidate.bpm = engine.bpm();
  candidate.swingPct = scene.feel.swingPct;
  candidate.scope = voice == 0
      ? QuantizedGenerationScope::SynthA
      : QuantizedGenerationScope::SynthB;
  candidate.synth[0] = scene.synthABanks[target.synthBank[0]]
      .patterns[target.synthSlot[0]];
  candidate.synth[1] = scene.synthBBanks[target.synthBank[1]]
      .patterns[target.synthSlot[1]];
  candidate.drums = scene.drumBanks[target.drumBank]
      .patterns[target.drumSlot];

  StrongRhythmMigrationContext context = migrationContextFor(scene, target);
  context.patternAddress = static_cast<int16_t>(
      synthPatternAddressFor(target, voice));
  context.level = requestLevel;
  context.generationAttemptOrdinal = generationAttemptOrdinal;
  const StrongRhythmMigrationResult migration = migrateStrongRhythmSynths(
      requestedGenre,
      context,
      candidate.drums,
      candidate.synth[0],
      candidate.synth[1]);
  return migration.status == StrongRhythmMigrationStatus::Applied &&
         targetStillActive(scenes, target);
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
    owner->setGrooveboxMode(pending.mode);
    owner->setBpm(pending.bpm);
  } else {
    const int voice = pending.scope == QuantizedGenerationScope::SynthA ? 0 : 1;
    Bank<SynthPattern>& bank = voice == 0
        ? scene.synthABanks[pending.target.synthBank[0]]
        : scene.synthBBanks[pending.target.synthBank[1]];
    bank.patterns[pending.target.synthSlot[voice]] = pending.synth[voice];
  }

  g_slotState[slot].store(
      static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
  g_status.store(
      static_cast<uint8_t>(QuantizedGenerationStatus::Committed),
      std::memory_order_release);
  g_commitSerial.fetch_add(1, std::memory_order_release);
  GroovePuterState::markSceneMutated();
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
  Scene& scene = scenes.currentScene();
  const PatternTarget target = captureTarget(scenes);
  if (!targetValid(target)) return QuantizedGenerationResult::Failed;
  const RealizationLevel requestLevel = GroovePuterState::currentGenerationLevel();

  // STOP remains the immediate-edit path. Allocate the request ordinal before
  // the first live mutation; a failed allocation therefore leaves Scene/DSP
  // untouched. The migration is performed directly with the assigned ordinal so
  // it cannot be allocated a second time by the generic live bridge.
  if (!engine.isPlaying()) {
    uint32_t attemptOrdinal = 0;
    if (!allocateAttemptFor(requestedGenre, requestLevel, target, attemptOrdinal)) {
      g_status.store(
          static_cast<uint8_t>(QuantizedGenerationStatus::AttemptUnavailable),
          std::memory_order_release);
      return QuantizedGenerationResult::AttemptUnavailable;
    }

    clearPublished(QuantizedGenerationStatus::Idle);
    scene.genre = requestedGenre;
    engine.setGrooveboxMode(requestedMode);
    if (applyTempo && requestedBpm > 0.0f) engine.setBpm(requestedBpm);

    engine.regeneratePatternsWithGenre();
    StrongRhythmMigrationContext context = migrationContextFor(scene, target);
    context.level = requestLevel;
    context.generationAttemptOrdinal = attemptOrdinal;
    (void)migrateStrongRhythmMaterial(
        scene.genre,
        context,
        scene.drumBanks[target.drumBank].patterns[target.drumSlot],
        scene.synthABanks[target.synthBank[0]].patterns[target.synthSlot[0]],
        scene.synthBBanks[target.synthBank[1]].patterns[target.synthSlot[1]]);

    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::Committed),
        std::memory_order_release);
    g_commitSerial.fetch_add(1, std::memory_order_release);
    return QuantizedGenerationResult::CommittedNow;
  }

  // PLAY preparation is deliberately lock-free with respect to AudioTask. It
  // only reads live state and writes a slot exclusively leased to this caller.
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
  const RealizationLevel requestLevel =
      GroovePuterState::currentGenerationLevel();

  const WriteLease lease = acquireWriteLease();
  if (lease.slot < 0) {
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::Busy),
        std::memory_order_release);
    return QuantizedGenerationResult::Failed;
  }

  uint32_t attemptOrdinal = 0;
  if (!allocateSynthAttemptFor(
          genre, requestLevel, target, voice, attemptOrdinal)) {
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
    Scene& scene = scenes.currentScene();
    Bank<SynthPattern>& bank = voice == 0
        ? scene.synthABanks[target.synthBank[0]]
        : scene.synthBBanks[target.synthBank[1]];
    bank.patterns[target.synthSlot[voice]] = candidate.synth[voice];
    releaseWriteSlot(lease.slot);
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::Committed),
        std::memory_order_release);
    g_commitSerial.fetch_add(1, std::memory_order_release);
    return QuantizedGenerationResult::CommittedNow;
  }

  publishWriteSlot(lease.slot);
  engine.genreManager().setPendingCommitHook(
      &commitQuantizedGenerationAtBarStart);
  return QuantizedGenerationResult::PendingNextBar;
}

inline bool hasPendingQuantizedGeneration(const MiniAcid& engine) {
  (void)engine;
  return QuantizedGenerationDetail::g_publishedSlot.load(
      std::memory_order_acquire) >= 0;
}

inline QuantizedGenerationStatus quantizedGenerationStatus() {
  return static_cast<QuantizedGenerationStatus>(
      QuantizedGenerationDetail::g_status.load(std::memory_order_acquire));
}

inline uint32_t quantizedGenerationCommitSerial() {
  return QuantizedGenerationDetail::g_commitSerial.load(std::memory_order_acquire);
}

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_QUANTIZED_GENERATION_COMMIT_IMPL_H
