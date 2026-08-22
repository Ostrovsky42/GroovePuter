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

  bool hasUndo() const { return slot_.hasUndo(); }
  UndoKind kind() const { return slot_.kind(); }
  uint16_t payloadSize() const { return slot_.payloadSize(); }
  uint32_t committedRevision() const { return committed_revision_; }
  bool nextIsRedo() const { return slot_.nextIsRedo(); }

  void clear() {
    slot_.clear();
    committed_revision_ = 0;
  }

  // COMMIT entry point for already-prepared persistent edits.
  //
  // PREPARE must have completed all failure/no-op checks before this call. The
  // apply callback is therefore the bounded, synchronous, infallible commit
  // phase: it must not perform generation, filesystem/JSON work, waiting, or
  // unbounded allocation. One call represents one logical Scene mutation.
  //
  // Admission occurs before apply, so an oversized/invalid receipt leaves the
  // previous valid Undo and Scene revision untouched.
  template <typename Payload, typename ApplyFn>
  bool commitPrepared(UndoKind kind, const Payload& before, ApplyFn&& apply) {
    static_assert(std::is_trivially_copyable<Payload>::value,
                  "Undo payloads must remain trivially copyable fixed values");

    if (kind == UndoKind::None || sizeof(Payload) > kUndoPayloadBytes) {
      return false;
    }

    const GroovePuterState::SceneRevisionState revision_before =
        GroovePuterState::sceneRevisionSnapshot();

    // Publishing cannot fail after the identical admission check above. It is
    // done before the infallible commit callback so a successful persistent
    // write can never exist without its retained before-state receipt.
    if (!slot_.publish(kind, before, revision_before)) return false;

    std::forward<ApplyFn>(apply)();
    GroovePuterState::markSceneMutated();
    committed_revision_ =
        GroovePuterState::sceneRevisionSnapshot().currentRevision;
    return true;
  }

  // Receipt inspection stays typed and owned by the child that understands the
  // payload. Do not add scalar-returning peek helpers: values such as synth 0
  // are valid identities, not validity flags.
  template <typename Payload>
  bool read(UndoKind expected_kind, Payload& before) const {
    return slot_.read(expected_kind, before);
  }

  // User Undo restores the last committed persistent edit, not runtime
  // activation state. Validation must be read-only. restore() is the bounded,
  // synchronous, infallible restore phase after validation succeeds.
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
    // Save does not change currentRevision. If the committed edit itself became
    // the persisted baseline after this receipt was published, Undo restores
    // the older data but must remain dirty relative to that newer saved state.
    if (current.persistedRevision == committed_revision_) {
      restored_revision.persistedRevision = current.persistedRevision;
    }
    GroovePuterState::restoreSceneRevision(restored_revision);
    clear();
    return UndoResult::Restored;
  }

  // One-step history toggle. The retained target payload is exchanged with the
  // live state in-place, then the same fixed slot stores the reverse transition.
  // No second resident payload and no second full-size stack object are created.
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
      // Wrong owner context is not an Undo operation and must not consume the
      // retained pair. Let the application-level Ctrl+Z fallback report
      // UNDO/REDO: NOT HERE; navigation remains an explicit Esc action.
      return UndoResult::ContextUnavailable;
    }

    const bool was_redo = slot_.nextIsRedo();
    const GroovePuterState::SceneRevisionState target_revision =
        slot_.revisionBefore();
    std::forward<ExchangeFn>(exchange)(retained);

    GroovePuterState::SceneRevisionState restored_revision = target_revision;
    if (current.persistedRevision == committed_revision_) {
      restored_revision.persistedRevision = current.persistedRevision;
    }
    GroovePuterState::restoreSceneRevision(restored_revision);

    // Same admitted payload size/kind, so publication cannot fail here. The
    // retained value now contains the state we just left and current is the
    // revision to restore on the inverse keypress.
    if (!slot_.publish(expected_kind, retained, current)) {
      clear();
      return UndoResult::Expired;
    }
    slot_.setNextIsRedo(!was_redo);
    committed_revision_ = restored_revision.currentRevision;
    return UndoResult::Restored;
  }

 private:
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
