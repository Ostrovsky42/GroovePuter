#pragma once

#include <atomic>
#include <cstdint>

#include "../../dsp/miniacid_engine.h"
#include "../../state/scene_revision.h"
#include "strong_rhythm_live_bridge.h"

namespace GroovePuterRhythm {

enum class QuantizedGenerationResult : uint8_t {
  Failed = 0,
  CommittedNow,
  PendingNextBar,
};

enum class QuantizedGenerationStatus : uint8_t {
  Idle = 0,
  PendingNextBar,
  Committed,
  CancelledTargetChanged,
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
  SynthPattern synth[2]{};
  DrumPatternSet drums{};
};

inline PendingGeneration g_pending{};
inline std::atomic<bool> g_pendingValid{false};
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

inline bool targetStillActive(SceneManager& scenes, const PatternTarget& target) {
  return sameTarget(captureTarget(scenes), target);
}

inline void clearPending(QuantizedGenerationStatus status) {
  g_pendingValid.store(false, std::memory_order_release);
  g_status.store(static_cast<uint8_t>(status), std::memory_order_release);
}

inline bool compatiblePending(MiniAcid& engine, const PatternTarget& target) {
  return g_pendingValid.load(std::memory_order_acquire) &&
         g_pending.owner == &engine &&
         sameTarget(g_pending.target, target);
}

}  // namespace QuantizedGenerationDetail

inline bool commitQuantizedGenerationAtBarStart(SceneManager& scenes) {
  using namespace QuantizedGenerationDetail;
  if (!g_pendingValid.load(std::memory_order_acquire)) return false;

  MiniAcid* owner = g_pending.owner;
  if (owner == nullptr || &owner->sceneManager() != &scenes ||
      !targetValid(g_pending.target) ||
      !targetStillActive(scenes, g_pending.target)) {
    clearPending(QuantizedGenerationStatus::CancelledTargetChanged);
    return false;
  }

  Scene& scene = scenes.currentScene();
  scene.synthABanks[g_pending.target.synthBank[0]]
      .patterns[g_pending.target.synthSlot[0]] = g_pending.synth[0];
  scene.synthBBanks[g_pending.target.synthBank[1]]
      .patterns[g_pending.target.synthSlot[1]] = g_pending.synth[1];
  scene.drumBanks[g_pending.target.drumBank]
      .patterns[g_pending.target.drumSlot] = g_pending.drums;
  scene.genre = g_pending.genre;
  scene.feel.swingPct = g_pending.swingPct;

  owner->setGrooveboxMode(g_pending.mode);
  owner->setBpm(g_pending.bpm);

  g_pendingValid.store(false, std::memory_order_release);
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

  if (!engine.isPlaying()) {
    clearPending(QuantizedGenerationStatus::Idle);
    scene.genre = requestedGenre;
    engine.setGrooveboxMode(requestedMode);
    if (applyTempo && requestedBpm > 0.0f) engine.setBpm(requestedBpm);
    regenerateWithStrongRhythmMigration(engine);
    g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::Committed),
        std::memory_order_release);
    g_commitSerial.fetch_add(1, std::memory_order_release);
    return QuantizedGenerationResult::CommittedNow;
  }

  const PatternTarget target = captureTarget(scenes);
  if (!targetValid(target)) return QuantizedGenerationResult::Failed;

  const SynthPattern activeSynthA = scenes.getCurrentSynthPattern(0);
  const SynthPattern activeSynthB = scenes.getCurrentSynthPattern(1);
  const DrumPatternSet activeDrums = scenes.getCurrentDrumPattern();
  const GenreSettings activeGenre = scene.genre;
  const GrooveboxMode activeMode = engine.grooveboxMode();
  const float activeBpm = engine.bpm();
  const uint8_t activeSwingPct = scene.feel.swingPct;

  SynthPattern generationBaseA = activeSynthA;
  SynthPattern generationBaseB = activeSynthB;
  DrumPatternSet generationBaseDrums = activeDrums;
  if (compatiblePending(engine, target)) {
    generationBaseA = g_pending.synth[0];
    generationBaseB = g_pending.synth[1];
    generationBaseDrums = g_pending.drums;
  }

  g_pendingValid.store(false, std::memory_order_release);

  scene.genre = requestedGenre;
  engine.setGrooveboxMode(requestedMode);
  if (applyTempo && requestedBpm > 0.0f) engine.setBpm(requestedBpm);
  scenes.editCurrentSynthPattern(0) = generationBaseA;
  scenes.editCurrentSynthPattern(1) = generationBaseB;
  scenes.editCurrentDrumPattern() = generationBaseDrums;

  regenerateWithStrongRhythmMigration(engine);

  PendingGeneration candidate{};
  candidate.owner = &engine;
  candidate.target = target;
  candidate.genre = scene.genre;
  candidate.mode = engine.grooveboxMode();
  candidate.bpm = engine.bpm();
  candidate.swingPct = scene.feel.swingPct;
  candidate.synth[0] = scenes.getCurrentSynthPattern(0);
  candidate.synth[1] = scenes.getCurrentSynthPattern(1);
  candidate.drums = scenes.getCurrentDrumPattern();

  scenes.editCurrentSynthPattern(0) = activeSynthA;
  scenes.editCurrentSynthPattern(1) = activeSynthB;
  scenes.editCurrentDrumPattern() = activeDrums;
  scene.genre = activeGenre;
  scene.feel.swingPct = activeSwingPct;
  engine.setGrooveboxMode(activeMode);
  engine.setBpm(activeBpm);

  g_pending = candidate;
  g_pendingValid.store(true, std::memory_order_release);
  g_status.store(
      static_cast<uint8_t>(QuantizedGenerationStatus::PendingNextBar),
      std::memory_order_release);
  engine.genreManager().setPendingCommitHook(&commitQuantizedGenerationAtBarStart);
  return QuantizedGenerationResult::PendingNextBar;
}

inline bool hasPendingQuantizedGeneration(const MiniAcid& engine) {
  return QuantizedGenerationDetail::g_pendingValid.load(std::memory_order_acquire) &&
         QuantizedGenerationDetail::g_pending.owner == &engine;
}

inline QuantizedGenerationStatus quantizedGenerationStatus() {
  return static_cast<QuantizedGenerationStatus>(
      QuantizedGenerationDetail::g_status.load(std::memory_order_acquire));
}

inline uint32_t quantizedGenerationCommitSerial() {
  return QuantizedGenerationDetail::g_commitSerial.load(std::memory_order_acquire);
}

}  // namespace GroovePuterRhythm
