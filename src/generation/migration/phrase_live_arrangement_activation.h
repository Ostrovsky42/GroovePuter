#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace GroovePuterRhythm::PhraseLiveArrangementDetail {

using QuantizedGenerationDetail::PatternTarget;
using QuantizedGenerationDetail::PendingGeneration;
using QuantizedGenerationDetail::SlotState;

constexpr float kPhraseActivationBpmSentinel = -1.0f;

struct PhraseActivationMetadata {
  PatternTarget selectionTarget{};
  int8_t songSlot = -1;
  int8_t songStart = -1;
  int8_t bars = 0;
  int8_t audibleSongRow = -1;
  bool active = false;
};

static_assert(std::is_trivially_copyable<PhraseActivationMetadata>::value,
              "Phrase pending metadata must remain fixed value state");

// Payload extension only. Publication ownership remains the 0.9.9-C
// g_slotState/g_publishedSlot pair; this array has no queue/state machine of its
// own and is indexed by the already-owned C slot.
inline PhraseActivationMetadata g_phraseActivation[2]{};

inline bool isPhraseActivationSlot(int slot) {
  if (slot < 0 || slot > 1) return false;
  const PendingGeneration& pending = QuantizedGenerationDetail::g_slots[slot];
  return g_phraseActivation[slot].active &&
         pending.bpm == kPhraseActivationBpmSentinel;
}

inline void clearPhraseActivationMetadata(int slot) {
  if (slot < 0 || slot > 1) return;
  g_phraseActivation[slot] = PhraseActivationMetadata{};
}

inline bool exactAudibleTargetStillActive(
    MiniAcid& engine,
    const PhraseActivationMetadata& metadata) {
  SceneManager& scenes = engine.sceneManager();
  return engine.songModeEnabled() &&
         engine.songPlaybackSlot() == metadata.songSlot &&
         engine.currentSongPosition() == metadata.audibleSongRow &&
         scenes.activeSongSlot() == metadata.songSlot &&
         QuantizedGenerationDetail::targetStillActive(
             scenes, metadata.selectionTarget);
}

inline bool copyCurrentAudibleSnapshot(
    MiniAcid& engine,
    const PatternTarget& target,
    PendingGeneration& pending) {
  SceneManager& scenes = engine.sceneManager();
  const Scene& scene = scenes.currentScene();
  if (!QuantizedGenerationDetail::targetValid(target)) return false;

  const int songSlot = engine.songPlaybackSlot();
  const int row = engine.currentSongPosition();
  if (songSlot < 0 || songSlot > 1 || row < 0 || row >= Song::kMaxPositions) {
    return false;
  }

  auto snapshotSynth = [&](SongTrack track, int voice) -> bool {
    const int global = scenes.songPatternAtSlot(songSlot, row, track);
    if (global < 0) {
      pending.synth[voice] = SynthPattern{};
      return true;
    }
    const int page = songPatternPage(global);
    const int bank = songPatternBank(global);
    const int pattern = songPatternIndexInBank(global);
    if (page != target.page ||
        bank != target.synthBank[voice] ||
        pattern != target.synthSlot[voice]) {
      return false;
    }
    pending.synth[voice] =
        voice == 0
            ? scene.synthABanks[bank].patterns[pattern]
            : scene.synthBBanks[bank].patterns[pattern];
    return true;
  };

  if (!snapshotSynth(SongTrack::SynthA, 0) ||
      !snapshotSynth(SongTrack::SynthB, 1)) {
    return false;
  }

  const int drumGlobal = scenes.songPatternAtSlot(
      songSlot, row, SongTrack::Drums);
  if (drumGlobal < 0) {
    pending.drums = DrumPatternSet{};
  } else {
    const int page = songPatternPage(drumGlobal);
    const int bank = songPatternBank(drumGlobal);
    const int pattern = songPatternIndexInBank(drumGlobal);
    if (page != target.page || bank != target.drumBank ||
        pattern != target.drumSlot) {
      return false;
    }
    pending.drums = scene.drumBanks[bank].patterns[pattern];
  }

  pending.genre = scene.genre;
  pending.mode = engine.grooveboxMode();
  pending.bpm = kPhraseActivationBpmSentinel;
  pending.swingPct = scene.feel.swingPct;
  pending.scope = QuantizedGenerationScope::Full;
  return true;
}

inline bool armPhraseActivation(
    MiniAcid& engine,
    int slot,
    const PatternTarget& selectionTarget,
    int songSlot,
    int songStart,
    int bars,
    int audibleSongRow) {
  if (slot < 0 || slot > 1 || songSlot < 0 || songSlot > 1 ||
      songStart < 0 || songStart >= Song::kMaxPositions ||
      bars < 1 || bars > 8 || audibleSongRow < 0 ||
      audibleSongRow >= Song::kMaxPositions) {
    return false;
  }

  PendingGeneration& pending = QuantizedGenerationDetail::g_slots[slot];
  pending = PendingGeneration{};
  pending.owner = &engine;
  pending.target = selectionTarget;
  pending.committedRevision = 0;
  if (!copyCurrentAudibleSnapshot(engine, selectionTarget, pending)) {
    pending = PendingGeneration{};
    return false;
  }

  PhraseActivationMetadata metadata{};
  metadata.selectionTarget = selectionTarget;
  metadata.songSlot = static_cast<int8_t>(songSlot);
  metadata.songStart = static_cast<int8_t>(songStart);
  metadata.bars = static_cast<int8_t>(bars);
  metadata.audibleSongRow = static_cast<int8_t>(audibleSongRow);
  metadata.active = true;
  g_phraseActivation[slot] = metadata;

  QuantizedGenerationDetail::armActivationSlot(slot);
  return true;
}

inline void completePhraseActivation(int slot, uint32_t committedRevision) {
  if (!isPhraseActivationSlot(slot)) return;
  QuantizedGenerationDetail::g_slots[slot].committedRevision =
      committedRevision;
  QuantizedGenerationDetail::g_slotState[slot].store(
      static_cast<uint8_t>(SlotState::Ready), std::memory_order_release);
  QuantizedGenerationDetail::g_status.store(
      static_cast<uint8_t>(QuantizedGenerationStatus::PendingNextBar),
      std::memory_order_release);
}

inline void abortPhraseActivation(
    int slot,
    QuantizedGenerationStatus status =
        QuantizedGenerationStatus::CancelledExplicit) {
  QuantizedGenerationDetail::abortArmedActivation(slot, status);
  clearPhraseActivationMetadata(slot);
}

inline bool hasPendingPhraseActivationForRevision(
    const MiniAcid& engine,
    uint32_t revision) {
  const int slot = QuantizedGenerationDetail::g_publishedSlot.load(
      std::memory_order_acquire);
  if (!isPhraseActivationSlot(slot)) return false;
  const auto state = static_cast<SlotState>(
      QuantizedGenerationDetail::g_slotState[slot].load(
          std::memory_order_acquire));
  if (state != SlotState::Armed && state != SlotState::Ready) return false;
  const PendingGeneration& pending = QuantizedGenerationDetail::g_slots[slot];
  return pending.owner == &engine &&
         pending.committedRevision == revision;
}

inline bool cancelPendingPhraseActivationForRevision(
    MiniAcid& engine,
    uint32_t revision) {
  const int slot = QuantizedGenerationDetail::g_publishedSlot.load(
      std::memory_order_acquire);
  if (!isPhraseActivationSlot(slot)) return false;
  PendingGeneration& pending = QuantizedGenerationDetail::g_slots[slot];
  if (pending.owner != &engine || pending.committedRevision != revision) {
    return false;
  }
  abortPhraseActivation(
      slot, QuantizedGenerationStatus::CancelledExplicit);
  return true;
}

inline bool activatePendingPhraseArrangementAtBarStart(
    SceneManager& scenes) {
  const int slot = QuantizedGenerationDetail::g_publishedSlot.load(
      std::memory_order_acquire);
  if (!isPhraseActivationSlot(slot)) return false;

  uint8_t expected = static_cast<uint8_t>(SlotState::Ready);
  if (!QuantizedGenerationDetail::g_slotState[slot].compare_exchange_strong(
          expected,
          static_cast<uint8_t>(SlotState::Reading),
          std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    return false;
  }

  int8_t published = static_cast<int8_t>(slot);
  if (!QuantizedGenerationDetail::g_publishedSlot.compare_exchange_strong(
          published, -1,
          std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    QuantizedGenerationDetail::g_slotState[slot].store(
        static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
    clearPhraseActivationMetadata(slot);
    return false;
  }

  PendingGeneration& pending = QuantizedGenerationDetail::g_slots[slot];
  const PhraseActivationMetadata metadata = g_phraseActivation[slot];
  MiniAcid* owner = pending.owner;
  QuantizedGenerationStatus finalStatus =
      QuantizedGenerationStatus::CancelledTargetChanged;
  bool activated = false;

  if (owner != nullptr && &owner->sceneManager() == &scenes &&
      pending.committedRevision != 0) {
    const uint32_t currentRevision =
        GroovePuterUndo::undoOwner().committedRevision();
    if (currentRevision != pending.committedRevision) {
      finalStatus = QuantizedGenerationStatus::CancelledRevisionChanged;
    } else if (exactAudibleTargetStillActive(*owner, metadata)) {
      // Same 96-PPQN BAR_START used by 0.9.9-C. This is runtime-only: the
      // persistent Song/Patterns were already committed on the control path.
      owner->setSongPosition(metadata.songStart);
      finalStatus = QuantizedGenerationStatus::Activated;
      activated = true;
    }
  }

  pending = PendingGeneration{};
  clearPhraseActivationMetadata(slot);
  QuantizedGenerationDetail::g_slotState[slot].store(
      static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
  QuantizedGenerationDetail::g_status.store(
      static_cast<uint8_t>(finalStatus), std::memory_order_release);
  return activated;
}

inline bool settlePendingPhraseArrangementOnStop(MiniAcid& engine) {
  const int slot = QuantizedGenerationDetail::g_publishedSlot.load(
      std::memory_order_acquire);
  if (!isPhraseActivationSlot(slot)) return false;

  auto state = static_cast<SlotState>(
      QuantizedGenerationDetail::g_slotState[slot].load(
          std::memory_order_acquire));
  if (state != SlotState::Ready && state != SlotState::Armed) return false;

  uint8_t expected = static_cast<uint8_t>(state);
  if (!QuantizedGenerationDetail::g_slotState[slot].compare_exchange_strong(
          expected,
          static_cast<uint8_t>(SlotState::Reading),
          std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    return false;
  }

  int8_t published = static_cast<int8_t>(slot);
  if (!QuantizedGenerationDetail::g_publishedSlot.compare_exchange_strong(
          published, -1,
          std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    QuantizedGenerationDetail::g_slotState[slot].store(
        static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
    clearPhraseActivationMetadata(slot);
    return true;
  }

  PendingGeneration& pending = QuantizedGenerationDetail::g_slots[slot];
  const PhraseActivationMetadata metadata = g_phraseActivation[slot];
  const uint32_t currentRevision =
      GroovePuterUndo::undoOwner().committedRevision();
  const bool settle = state == SlotState::Ready &&
      pending.owner == &engine && pending.committedRevision != 0 &&
      currentRevision == pending.committedRevision &&
      QuantizedGenerationDetail::targetStillActive(
          engine.sceneManager(), metadata.selectionTarget) &&
      engine.sceneManager().activeSongSlot() == metadata.songSlot;

  pending = PendingGeneration{};
  clearPhraseActivationMetadata(slot);
  QuantizedGenerationDetail::g_slotState[slot].store(
      static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);

  if (settle) {
    engine.setSongMode(true);
    engine.setSongPlaybackSlot(metadata.songSlot);
    engine.setSongPosition(metadata.songStart);
    QuantizedGenerationDetail::g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::Activated),
        std::memory_order_release);
  } else {
    QuantizedGenerationDetail::g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::CancelledExplicit),
        std::memory_order_release);
  }
  return true;
}

inline std::size_t pendingPhraseActivationMetadataBytes() {
  return sizeof(g_phraseActivation);
}

}  // namespace GroovePuterRhythm::PhraseLiveArrangementDetail
