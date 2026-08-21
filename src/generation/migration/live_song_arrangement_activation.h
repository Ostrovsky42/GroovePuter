#ifndef GROOVEPUTER_GENERATION_MIGRATION_LIVE_SONG_ARRANGEMENT_ACTIVATION_H
#define GROOVEPUTER_GENERATION_MIGRATION_LIVE_SONG_ARRANGEMENT_ACTIVATION_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "../../state/undo_receipts.h"

namespace GroovePuterRhythm::LiveSongArrangementDetail {

using QuantizedGenerationDetail::PatternTarget;
using QuantizedGenerationDetail::PendingGeneration;
using QuantizedGenerationDetail::SlotState;
using QuantizedGenerationDetail::WriteLease;

constexpr float kSongActivationBpmSentinel = -2.0f;

enum class SongActivationKind : uint8_t {
  None = 0,
  PersistentMutation,
  PlaybackSlotSwitch,
};

enum class SongLiveStatus : uint8_t {
  NoChange = 0,
  CommittedNow,
  PendingNextRow,
  Busy,
  TargetChanged,
};

struct SongActivationMetadata {
  PatternTarget audibleTarget{};
  uint32_t baseRevision = 0;
  int8_t sourcePlaybackSlot = -1;
  int8_t sourceRow = -1;
  int8_t editedSongSlot = -1;
  int8_t targetPlaybackSlot = -1;
  uint8_t audibleTrackMask = 0;
  SongActivationKind kind = SongActivationKind::None;
  bool beforeReverse = false;
  bool active = false;
};

struct SongMutationLease {
  int slot = -1;
  bool boundaryRequired = false;
  SongLiveStatus status = SongLiveStatus::CommittedNow;

  bool ok() const {
    return status == SongLiveStatus::CommittedNow ||
           status == SongLiveStatus::PendingNextRow;
  }
};

static_assert(std::is_trivially_copyable<SongActivationMetadata>::value,
              "D3 Song activation metadata must remain fixed value state");
static_assert(std::is_trivially_copyable<SongMutationLease>::value,
              "D3 Song mutation lease must remain fixed value state");

// Payload extension only. The publication state machine remains exactly the
// accepted 0.9.9-C g_slots/g_slotState/g_publishedSlot owner.
inline SongActivationMetadata g_songActivation[2]{};

inline bool activatePendingSongArrangementAtBarStart(SceneManager& scenes);

inline bool sameSongPosition(const SongPosition& lhs,
                             const SongPosition& rhs) {
  for (int track = 0; track < SongPosition::kTrackCount; ++track) {
    if (lhs.patterns[track] != rhs.patterns[track]) return false;
  }
  return true;
}

inline uint8_t audibleTrackMaskFor(const MiniAcid& engine,
                                   int playbackSlot,
                                   int row) {
  uint8_t mask = 0;
  const SceneManager& scenes = engine.sceneManager();
  if (scenes.songPatternAtSlot(playbackSlot, row, SongTrack::SynthA) >= 0) {
    mask |= 1u << 0;
  }
  if (scenes.songPatternAtSlot(playbackSlot, row, SongTrack::SynthB) >= 0) {
    mask |= 1u << 1;
  }
  if (scenes.songPatternAtSlot(playbackSlot, row, SongTrack::Drums) >= 0) {
    mask |= 1u << 2;
  }
  return mask;
}

inline bool isSongActivationSlot(int slot) {
  if (slot < 0 || slot > 1) return false;
  return g_songActivation[slot].active &&
         QuantizedGenerationDetail::g_slots[slot].bpm ==
             kSongActivationBpmSentinel;
}

inline void clearSongActivationMetadata(int slot) {
  if (slot < 0 || slot > 1) return;
  g_songActivation[slot] = SongActivationMetadata{};
}

inline const SongActivationMetadata* pendingSongMetadata(
    const MiniAcid& engine,
    int* slotOut = nullptr) {
  const int slot = QuantizedGenerationDetail::g_publishedSlot.load(
      std::memory_order_acquire);
  if (!isSongActivationSlot(slot)) return nullptr;
  const auto state = static_cast<SlotState>(
      QuantizedGenerationDetail::g_slotState[slot].load(
          std::memory_order_acquire));
  if (state != SlotState::Armed && state != SlotState::Ready) return nullptr;
  const PendingGeneration& pending = QuantizedGenerationDetail::g_slots[slot];
  const SongActivationMetadata& metadata = g_songActivation[slot];
  if (pending.owner != &engine ||
      engine.songPlaybackSlot() != metadata.sourcePlaybackSlot ||
      engine.currentSongPosition() != metadata.sourceRow) {
    return nullptr;
  }
  if (slotOut != nullptr) *slotOut = slot;
  return &metadata;
}

inline const SynthPattern* pendingAudibleSongSynthPattern(
    const MiniAcid& engine,
    int voice) {
  if (voice < 0 || voice > 1) return nullptr;
  int slot = -1;
  if (pendingSongMetadata(engine, &slot) == nullptr) return nullptr;
  return &QuantizedGenerationDetail::g_slots[slot].synth[voice];
}

inline const DrumPatternSet* pendingAudibleSongDrumPatternSet(
    const MiniAcid& engine) {
  int slot = -1;
  if (pendingSongMetadata(engine, &slot) == nullptr) return nullptr;
  return &QuantizedGenerationDetail::g_slots[slot].drums;
}

inline int pendingAudibleSongPatternIndex(const MiniAcid& engine,
                                          SongTrack track) {
  int slot = -1;
  const SongActivationMetadata* metadata = pendingSongMetadata(engine, &slot);
  if (metadata == nullptr) return -1;
  const PendingGeneration& pending = QuantizedGenerationDetail::g_slots[slot];
  switch (track) {
    case SongTrack::SynthA:
      return (metadata->audibleTrackMask & (1u << 0)) != 0
          ? pending.target.synthSlot[0] : -1;
    case SongTrack::SynthB:
      return (metadata->audibleTrackMask & (1u << 1)) != 0
          ? pending.target.synthSlot[1] : -1;
    case SongTrack::Drums:
      return (metadata->audibleTrackMask & (1u << 2)) != 0
          ? pending.target.drumSlot : -1;
    case SongTrack::Voice:
    default:
      return -1;
  }
}

inline bool audibleSongReverse(const MiniAcid& engine,
                               bool committedReverse) {
  const SongActivationMetadata* metadata = pendingSongMetadata(engine);
  return metadata == nullptr ? committedReverse : metadata->beforeReverse;
}

inline bool mutationNeedsBoundary(const MiniAcid& engine,
                                  int editedSlot,
                                  const Song& before,
                                  const Song& after) {
  if (!engine.isPlaying() || !engine.songModeEnabled() ||
      editedSlot != engine.songPlaybackSlot()) {
    return false;
  }
  const int row = engine.currentSongPosition();
  if (row < 0 || row >= Song::kMaxPositions) return false;
  return before.reverse != after.reverse ||
         !sameSongPosition(before.positions[row], after.positions[row]);
}

inline bool persistentSongMutationConflictsWithPending(
    const MiniAcid& engine) {
  return engine.isPlaying() &&
         QuantizedGenerationDetail::g_publishedSlot.load(
             std::memory_order_acquire) >= 0;
}

inline SongMutationLease prepareSongMutationActivation(
    MiniAcid& engine,
    int editedSlot,
    const Song& before,
    const Song& after) {
  SongMutationLease result{};
  const bool boundaryRequired =
      mutationNeedsBoundary(engine, editedSlot, before, after);
  result.boundaryRequired = boundaryRequired;

  if (!boundaryRequired) {
    if (persistentSongMutationConflictsWithPending(engine)) {
      result.status = SongLiveStatus::Busy;
    } else {
      result.status = SongLiveStatus::CommittedNow;
    }
    return result;
  }

  const WriteLease lease = QuantizedGenerationDetail::acquireWriteLease();
  if (lease.slot < 0) {
    QuantizedGenerationDetail::g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::Busy),
        std::memory_order_release);
    result.status = SongLiveStatus::Busy;
    return result;
  }

  const PatternTarget target =
      QuantizedGenerationDetail::captureTarget(engine.sceneManager());
  PendingGeneration& pending = QuantizedGenerationDetail::g_slots[lease.slot];
  if (!PhraseLiveArrangementDetail::copyCurrentAudibleSnapshot(
          engine, target, pending)) {
    QuantizedGenerationDetail::releaseWriteSlot(lease.slot);
    result.status = SongLiveStatus::TargetChanged;
    return result;
  }

  SongActivationMetadata metadata{};
  metadata.audibleTarget = target;
  metadata.baseRevision =
      GroovePuterState::sceneRevisionSnapshot().currentRevision;
  metadata.sourcePlaybackSlot =
      static_cast<int8_t>(engine.songPlaybackSlot());
  metadata.sourceRow = static_cast<int8_t>(engine.currentSongPosition());
  metadata.editedSongSlot = static_cast<int8_t>(editedSlot);
  metadata.targetPlaybackSlot = metadata.sourcePlaybackSlot;
  metadata.audibleTrackMask = audibleTrackMaskFor(
      engine, metadata.sourcePlaybackSlot, metadata.sourceRow);
  metadata.kind = SongActivationKind::PersistentMutation;
  metadata.beforeReverse = before.reverse;
  metadata.active = true;

  pending.bpm = kSongActivationBpmSentinel;
  pending.committedRevision = 0;
  g_songActivation[lease.slot] = metadata;
  QuantizedGenerationDetail::armActivationSlot(lease.slot);

  result.slot = lease.slot;
  result.status = SongLiveStatus::PendingNextRow;
  return result;
}

inline bool songMutationTargetStillCommitSafe(
    const MiniAcid& engine,
    const SongMutationLease& lease) {
  if (!lease.boundaryRequired || lease.slot < 0) return true;
  if (!isSongActivationSlot(lease.slot)) return false;
  const SongActivationMetadata& metadata = g_songActivation[lease.slot];
  return engine.isPlaying() && engine.songModeEnabled() &&
         engine.songPlaybackSlot() == metadata.sourcePlaybackSlot &&
         engine.currentSongPosition() == metadata.sourceRow &&
         GroovePuterState::sceneRevisionSnapshot().currentRevision ==
             metadata.baseRevision &&
         QuantizedGenerationDetail::targetStillActive(
             engine.sceneManager(), metadata.audibleTarget);
}

inline void completeSongMutationActivation(int slot,
                                           uint32_t committedRevision) {
  if (!isSongActivationSlot(slot)) return;
  PendingGeneration& pending = QuantizedGenerationDetail::g_slots[slot];
  pending.committedRevision = committedRevision;
  QuantizedGenerationDetail::g_slotState[slot].store(
      static_cast<uint8_t>(SlotState::Ready), std::memory_order_release);
  QuantizedGenerationDetail::g_status.store(
      static_cast<uint8_t>(QuantizedGenerationStatus::PendingNextBar),
      std::memory_order_release);
  if (pending.owner != nullptr) {
    pending.owner->genreManager().setPendingCommitHook(
        &activatePendingSongArrangementAtBarStart);
  }
}

inline void abortSongMutationActivation(
    const SongMutationLease& lease,
    QuantizedGenerationStatus status =
        QuantizedGenerationStatus::CancelledExplicit) {
  if (!lease.boundaryRequired || lease.slot < 0) return;
  QuantizedGenerationDetail::abortArmedActivation(lease.slot, status);
  clearSongActivationMetadata(lease.slot);
}

inline SongLiveStatus requestSongPlaybackSwitch(MiniAcid& engine,
                                                int targetSlot) {
  if (targetSlot < 0) targetSlot = 0;
  if (targetSlot > 1) targetSlot = 1;
  if (engine.songPlaybackSlot() == targetSlot) return SongLiveStatus::NoChange;

  if (!engine.isPlaying() || !engine.songModeEnabled()) {
    engine.setSongPlaybackSlot(targetSlot);
    return SongLiveStatus::CommittedNow;
  }

  const WriteLease lease = QuantizedGenerationDetail::acquireWriteLease();
  if (lease.slot < 0) {
    QuantizedGenerationDetail::g_status.store(
        static_cast<uint8_t>(QuantizedGenerationStatus::Busy),
        std::memory_order_release);
    return SongLiveStatus::Busy;
  }

  const PatternTarget target =
      QuantizedGenerationDetail::captureTarget(engine.sceneManager());
  PendingGeneration& pending = QuantizedGenerationDetail::g_slots[lease.slot];
  if (!PhraseLiveArrangementDetail::copyCurrentAudibleSnapshot(
          engine, target, pending)) {
    QuantizedGenerationDetail::releaseWriteSlot(lease.slot);
    return SongLiveStatus::TargetChanged;
  }

  SongActivationMetadata metadata{};
  metadata.audibleTarget = target;
  metadata.baseRevision =
      GroovePuterState::sceneRevisionSnapshot().currentRevision;
  metadata.sourcePlaybackSlot =
      static_cast<int8_t>(engine.songPlaybackSlot());
  metadata.sourceRow = static_cast<int8_t>(engine.currentSongPosition());
  metadata.editedSongSlot = -1;
  metadata.targetPlaybackSlot = static_cast<int8_t>(targetSlot);
  metadata.audibleTrackMask = audibleTrackMaskFor(
      engine, metadata.sourcePlaybackSlot, metadata.sourceRow);
  metadata.kind = SongActivationKind::PlaybackSlotSwitch;
  metadata.beforeReverse = engine.sceneManager().isSongReverseAtSlot(
      metadata.sourcePlaybackSlot);
  metadata.active = true;

  pending.bpm = kSongActivationBpmSentinel;
  pending.committedRevision = metadata.baseRevision;
  g_songActivation[lease.slot] = metadata;

  // Runtime-only switch has no persistent COMMIT phase. Install the shared
  // BAR_START hook before publication, then publish directly as Ready so there
  // is no visible Armed window that could miss the exact row boundary.
  engine.genreManager().setPendingCommitHook(
      &activatePendingSongArrangementAtBarStart);
  QuantizedGenerationDetail::publishWriteSlot(lease.slot);
  return SongLiveStatus::PendingNextRow;
}

inline bool hasPendingSongActivationForRevision(const MiniAcid& engine,
                                                uint32_t revision) {
  int slot = -1;
  const SongActivationMetadata* metadata = pendingSongMetadata(engine, &slot);
  if (metadata == nullptr) return false;
  return QuantizedGenerationDetail::g_slots[slot].committedRevision == revision;
}

inline bool cancelPendingSongActivationForRevision(MiniAcid& engine,
                                                   uint32_t revision) {
  int slot = -1;
  if (pendingSongMetadata(engine, &slot) == nullptr) return false;
  PendingGeneration& pending = QuantizedGenerationDetail::g_slots[slot];
  if (pending.committedRevision != revision) return false;
  QuantizedGenerationDetail::abortArmedActivation(
      slot, QuantizedGenerationStatus::CancelledExplicit);
  clearSongActivationMetadata(slot);
  return true;
}

inline bool songUndoWouldAffectAudibleTruth(
    const MiniAcid& engine,
    const GroovePuterUndo::SongUndoPayload& receipt) {
  if (!engine.isPlaying() || !engine.songModeEnabled() ||
      receipt.songSlot != engine.songPlaybackSlot()) {
    return false;
  }
  const int row = engine.currentSongPosition();
  if (row < 0 || row >= Song::kMaxPositions) return false;
  const Song& committed =
      engine.sceneManager().currentScene().songs[receipt.songSlot];
  return committed.reverse != receipt.before.reverse ||
         !sameSongPosition(committed.positions[row], receipt.before.positions[row]);
}

inline bool songRowBoundaryDue(const MiniAcid& engine) {
  const int bars = engine.cycleBarCount();
  if (bars <= 1) return true;
  return engine.cycleBarIndex() >= bars - 1;
}

inline bool activatePendingSongArrangementAtBarStart(SceneManager& scenes) {
  const int slot = QuantizedGenerationDetail::g_publishedSlot.load(
      std::memory_order_acquire);
  if (!isSongActivationSlot(slot)) return false;

  PendingGeneration& pending = QuantizedGenerationDetail::g_slots[slot];
  SongActivationMetadata metadata = g_songActivation[slot];
  MiniAcid* owner = pending.owner;
  if (owner == nullptr || &owner->sceneManager() != &scenes ||
      !songRowBoundaryDue(*owner)) {
    return false;
  }

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
    clearSongActivationMetadata(slot);
    return false;
  }

  QuantizedGenerationStatus finalStatus =
      QuantizedGenerationStatus::CancelledTargetChanged;
  const uint32_t currentRevision =
      GroovePuterState::sceneRevisionSnapshot().currentRevision;

  if (pending.committedRevision != 0 &&
      currentRevision == pending.committedRevision &&
      owner->isPlaying() && owner->songModeEnabled() &&
      owner->songPlaybackSlot() == metadata.sourcePlaybackSlot &&
      owner->currentSongPosition() == metadata.sourceRow) {
    if (metadata.kind == SongActivationKind::PlaybackSlotSwitch) {
      owner->setSongPlaybackSlot(metadata.targetPlaybackSlot);
    }
    // PersistentMutation needs no Scene write here: committed Song truth is
    // already authoritative. Clearing the old audible overlay before the normal
    // row advance is the ACTIVATE operation; committed reverse is then observed
    // by that same boundary.
    finalStatus = QuantizedGenerationStatus::Activated;
  } else if (currentRevision != pending.committedRevision) {
    finalStatus = QuantizedGenerationStatus::CancelledRevisionChanged;
  }

  pending = PendingGeneration{};
  clearSongActivationMetadata(slot);
  QuantizedGenerationDetail::g_slotState[slot].store(
      static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
  QuantizedGenerationDetail::g_status.store(
      static_cast<uint8_t>(finalStatus), std::memory_order_release);
  return false;  // Never request generation from GenreManager's shared hook.
}

inline bool settlePendingSongArrangementOnStop(MiniAcid& engine) {
  const int slot = QuantizedGenerationDetail::g_publishedSlot.load(
      std::memory_order_acquire);
  if (!isSongActivationSlot(slot)) return false;
  const auto state = static_cast<SlotState>(
      QuantizedGenerationDetail::g_slotState[slot].load(
          std::memory_order_acquire));
  if (state != SlotState::Armed && state != SlotState::Ready) return false;

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
    clearSongActivationMetadata(slot);
    return true;
  }

  PendingGeneration& pending = QuantizedGenerationDetail::g_slots[slot];
  const SongActivationMetadata metadata = g_songActivation[slot];
  const uint32_t currentRevision =
      GroovePuterState::sceneRevisionSnapshot().currentRevision;
  const bool committed = state == SlotState::Ready &&
      pending.owner == &engine && pending.committedRevision != 0 &&
      currentRevision == pending.committedRevision;

  if (committed) {
    if (metadata.kind == SongActivationKind::PlaybackSlotSwitch) {
      engine.setSongPlaybackSlot(metadata.targetPlaybackSlot);
    } else if (metadata.kind == SongActivationKind::PersistentMutation) {
      engine.setSongPosition(metadata.sourceRow);
    }
  }

  pending = PendingGeneration{};
  clearSongActivationMetadata(slot);
  QuantizedGenerationDetail::g_slotState[slot].store(
      static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
  QuantizedGenerationDetail::g_status.store(
      static_cast<uint8_t>(committed
          ? QuantizedGenerationStatus::Activated
          : QuantizedGenerationStatus::CancelledExplicit),
      std::memory_order_release);
  return true;
}

inline std::size_t pendingSongActivationMetadataBytes() {
  return sizeof(g_songActivation);
}

}  // namespace GroovePuterRhythm::LiveSongArrangementDetail

#endif  // GROOVEPUTER_GENERATION_MIGRATION_LIVE_SONG_ARRANGEMENT_ACTIVATION_H
