#pragma once

#ifndef GROOVEPUTER_STATE_UNDO_OWNER_H
#define GROOVEPUTER_STATE_UNDO_OWNER_H

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "bounded_undo_slot.h"
#include "scene_revision.h"

namespace GroovePuterUndo {

// Smallest measured capacity that fits the currently characterized Tier-1
// receipts, including one-row A+B+Drums materialization (1428 B).
constexpr std::size_t kUndoPayloadBytes = 1536;

enum class UndoResult : uint8_t {
  NothingToUndo = 0,
  KindMismatch,
  TargetUnavailable,
  ContextUnavailable,
  Expired,
  Restored,
};

template <typename T>
inline void exchangeFixedValue(T& live, T& retained) {
  static_assert(std::is_trivially_copyable<T>::value,
                "history exchange requires fixed trivially-copyable values");
  auto* lhs = reinterpret_cast<uint8_t*>(&live);
  auto* rhs = reinterpret_cast<uint8_t*>(&retained);
  for (std::size_t i = 0; i < sizeof(T); ++i) {
    const uint8_t value = lhs[i];
    lhs[i] = rhs[i];
    rhs[i] = value;
  }
}

class UndoOwner {
 public:
  static constexpr std::size_t payloadCapacity() { return kUndoPayloadBytes; }
  static constexpr std::size_t lifecyclePayloadCapacity() {
    return BoundedUndoSlot<kUndoPayloadBytes>::lifecyclePayloadCapacity();
  }

  bool hasUndo() const { return slot_.hasUndo(); }
  UndoKind kind() const { return slot_.kind(); }
  uint16_t payloadSize() const { return slot_.payloadSize(); }
  uint32_t committedRevision() const { return committed_revision_; }
  bool nextIsRedo() const { return slot_.nextIsRedo(); }
  bool hasLifecycle() const { return slot_.hasLifecycle(); }

  bool readLifecycle(UndoLifecycleMetadata& lifecycle) const {
    return slot_.readLifecycle(lifecycle);
  }

  bool retainsPatternBacking(int globalPattern, uint8_t trackMask) const {
    if (globalPattern < 0 || trackMask == 0) return false;
    UndoLifecycleMetadata lifecycle{};
    if (!slot_.readLifecycle(lifecycle)) return false;
    for (int i = 0; i < lifecycle.count; ++i) {
      const UndoRetainedResource& resource = lifecycle.resources[i];
      if (resource.kind == UndoRetainedResourceKind::PatternBacking &&
          resource.resourceId == globalPattern &&
          (resource.mask & trackMask) != 0) {
        return true;
      }
    }
    return false;
  }

  // Persistence can ask the current receipt to remove runtime-only retained
  // bytes from a detached persistence view without consuming Undo/Redo state.
  void sanitizeForPersistence(void* persistenceView) const {
    UndoLifecycleMetadata lifecycle{};
    if (!slot_.readLifecycle(lifecycle) ||
        lifecycle.sanitizeForPersistence == nullptr) {
      return;
    }
    lifecycle.sanitizeForPersistence(
        lifecycle.context, lifecycle, persistenceView);
  }

  void clear() {
    UndoLifecycleMetadata retired{};
    const bool hadLifecycle = slot_.readLifecycle(retired);
    slot_.clear();
    committed_revision_ = 0;
    if (hadLifecycle) releaseLifecycle(retired);
  }

  // COMMIT entry point for already-prepared persistent edits.
  // Admission happens before apply. If this supersedes a backing-aware receipt,
  // its now-unowned resources are reclaimed only after the new persistent state
  // has been installed, so shared/new owners can protect overlapping backing.
  template <typename Payload, typename ApplyFn>
  bool commitPrepared(UndoKind kind, const Payload& before, ApplyFn&& apply) {
    static_assert(std::is_trivially_copyable<Payload>::value,
                  "Undo payloads must remain trivially copyable fixed values");

    if (kind == UndoKind::None || sizeof(Payload) > kUndoPayloadBytes) {
      return false;
    }

    const GroovePuterState::SceneRevisionState revision_before =
        GroovePuterState::sceneRevisionSnapshot();
    UndoLifecycleMetadata retired{};
    const bool hadLifecycle = slot_.readLifecycle(retired);

    if (!slot_.publish(kind, before, revision_before)) return false;

    std::forward<ApplyFn>(apply)();
    if (hadLifecycle) releaseLifecycle(retired);
    GroovePuterState::markSceneMutated();
    committed_revision_ =
        GroovePuterState::sceneRevisionSnapshot().currentRevision;
    return true;
  }

  // Same canonical owner and same one-slot capacity, but with bounded resource
  // retention metadata stored in the reserved tail of the existing slot.
  template <typename Payload, typename ApplyFn>
  bool commitPreparedWithLifecycle(
      UndoKind kind,
      const Payload& before,
      const UndoLifecycleMetadata& lifecycle,
      ApplyFn&& apply) {
    static_assert(std::is_trivially_copyable<Payload>::value,
                  "Undo payloads must remain trivially copyable fixed values");

    if (kind == UndoKind::None ||
        sizeof(Payload) > lifecyclePayloadCapacity() ||
        lifecycle.count > kUndoRetainedResourceCapacity) {
      return false;
    }

    const GroovePuterState::SceneRevisionState revision_before =
        GroovePuterState::sceneRevisionSnapshot();
    UndoLifecycleMetadata retired{};
    const bool hadLifecycle = slot_.readLifecycle(retired);

    if (!slot_.publishWithLifecycle(
            kind, before, revision_before, lifecycle)) {
      return false;
    }

    std::forward<ApplyFn>(apply)();
    if (hadLifecycle) releaseLifecycle(retired);
    GroovePuterState::markSceneMutated();
    committed_revision_ =
        GroovePuterState::sceneRevisionSnapshot().currentRevision;
    return true;
  }

  template <typename Payload>
  bool read(UndoKind expected_kind, Payload& before) const {
    return slot_.read(expected_kind, before);
  }

  template <typename Payload, typename ValidateFn, typename RestoreFn>
  UndoResult undoPrepared(UndoKind expected_kind,
                          ValidateFn&& validate,
                          RestoreFn&& restore) {
    if (!slot_.hasUndo()) return UndoResult::NothingToUndo;
    if (slot_.kind() != expected_kind) return UndoResult::KindMismatch;

    const GroovePuterState::SceneRevisionState current =
        GroovePuterState::sceneRevisionSnapshot();
    if (committed_revision_ == 0 || current.currentRevision != committed_revision_) {
      clear();
      return UndoResult::Expired;
    }

    Payload before{};
    if (!slot_.read(expected_kind, before)) return UndoResult::KindMismatch;
    if (!std::forward<ValidateFn>(validate)(before)) {
      return UndoResult::TargetUnavailable;
    }

    const GroovePuterState::SceneRevisionState revision_before =
        slot_.revisionBefore();
    std::forward<RestoreFn>(restore)(before);

    GroovePuterState::SceneRevisionState restored_revision = revision_before;
    if (current.persistedRevision == committed_revision_) {
      restored_revision.persistedRevision = current.persistedRevision;
    }
    GroovePuterState::restoreSceneRevision(restored_revision);
    clear();
    return UndoResult::Restored;
  }

  // One-step exchange preserves the exact lifecycle metadata together with the
  // reverse transition. Redo therefore keeps physical backing reserved without
  // a second resident material snapshot.
  template <typename Payload, typename ValidateFn, typename ExchangeFn>
  UndoResult togglePrepared(UndoKind expected_kind,
                            ValidateFn&& validate,
                            ExchangeFn&& exchange) {
    static_assert(std::is_trivially_copyable<Payload>::value,
                  "Undo payloads must remain trivially copyable fixed values");
    if (!slot_.hasUndo()) return UndoResult::NothingToUndo;
    if (slot_.kind() != expected_kind) return UndoResult::KindMismatch;

    const GroovePuterState::SceneRevisionState current =
        GroovePuterState::sceneRevisionSnapshot();
    if (committed_revision_ == 0 || current.currentRevision != committed_revision_) {
      clear();
      return UndoResult::Expired;
    }

    Payload retained{};
    if (!slot_.read(expected_kind, retained)) return UndoResult::KindMismatch;
    if (!std::forward<ValidateFn>(validate)(retained)) {
      return UndoResult::ContextUnavailable;
    }

    UndoLifecycleMetadata lifecycle{};
    const bool hasLifecycle = slot_.readLifecycle(lifecycle);
    const bool was_redo = slot_.nextIsRedo();
    const GroovePuterState::SceneRevisionState target_revision =
        slot_.revisionBefore();
    std::forward<ExchangeFn>(exchange)(retained);

    GroovePuterState::SceneRevisionState restored_revision = target_revision;
    if (current.persistedRevision == committed_revision_) {
      restored_revision.persistedRevision = current.persistedRevision;
    }
    GroovePuterState::restoreSceneRevision(restored_revision);

    const bool published = hasLifecycle
        ? slot_.publishWithLifecycle(
              expected_kind, retained, current, lifecycle)
        : slot_.publish(expected_kind, retained, current);
    if (!published) {
      clear();
      return UndoResult::Expired;
    }
    slot_.setNextIsRedo(!was_redo);
    committed_revision_ = restored_revision.currentRevision;
    return UndoResult::Restored;
  }

 private:
  static void releaseLifecycle(const UndoLifecycleMetadata& lifecycle) {
    if (lifecycle.cleanup != nullptr) {
      lifecycle.cleanup(lifecycle.context, lifecycle);
    }
  }

  BoundedUndoSlot<kUndoPayloadBytes> slot_{};
  uint32_t committed_revision_{0};
};

inline UndoOwner& undoOwner() {
  static UndoOwner owner{};
  return owner;
}

static_assert(sizeof(UndoOwner) <= 1552,
              "authoritative one-level Undo owner exceeded the measured R2 DRAM budget");

}  // namespace GroovePuterUndo

#endif  // GROOVEPUTER_STATE_UNDO_OWNER_H
