#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "scene_revision.h"

namespace GroovePuterUndo {

enum class UndoKind : uint8_t {
  None = 0,
  Pattern,
  Song,
  Phrase,
  Generation,
};

// Fixed-capacity one-level storage primitive for 0.9.8 Undo receipts.
//
// This type deliberately does not know Scene, UI pages, AudioGuard, filesystem,
// or MIDI. It only retains a trivially-copyable before-state payload together
// with the exact Scene revision state that existed before the mutation.
//
// No global instance is created by this header. R2 must choose a measured
// payload capacity and an authoritative owner before any fixed DRAM is reserved.
template <std::size_t PayloadBytes>
class BoundedUndoSlot {
 public:
  static_assert(PayloadBytes > 0, "Undo payload capacity must be non-zero");
  static_assert(PayloadBytes <= UINT16_MAX,
                "Undo payload size field is intentionally 16-bit");

  static constexpr std::size_t capacity() { return PayloadBytes; }

  bool hasUndo() const { return kind_ != UndoKind::None; }
  UndoKind kind() const { return kind_; }
  uint16_t payloadSize() const { return payload_size_; }
  bool nextIsRedo() const { return next_is_redo_; }
  void setNextIsRedo(bool redo) { next_is_redo_ = redo; }

  const GroovePuterState::SceneRevisionState& revisionBefore() const {
    return revision_before_;
  }

  template <typename Payload>
  bool publish(UndoKind kind,
               const Payload& before,
               const GroovePuterState::SceneRevisionState& revision_before) {
    static_assert(std::is_trivially_copyable<Payload>::value,
                  "Undo payloads must be trivially copyable fixed values");

    // Admission happens before touching the retained receipt. A failed/no-op
    // caller can therefore leave the previous valid Undo available.
    if (kind == UndoKind::None || sizeof(Payload) > PayloadBytes) return false;

    std::memcpy(payload_.data(), &before, sizeof(Payload));
    revision_before_ = revision_before;
    payload_size_ = static_cast<uint16_t>(sizeof(Payload));
    kind_ = kind;
    next_is_redo_ = false;
    return true;
  }

  template <typename Payload>
  bool read(UndoKind expected_kind, Payload& before) const {
    static_assert(std::is_trivially_copyable<Payload>::value,
                  "Undo payloads must be trivially copyable fixed values");

    if (kind_ != expected_kind || payload_size_ != sizeof(Payload)) return false;
    std::memcpy(&before, payload_.data(), sizeof(Payload));
    return true;
  }

  void clear() {
    kind_ = UndoKind::None;
    payload_size_ = 0;
    revision_before_ = GroovePuterState::SceneRevisionState{};
    next_is_redo_ = false;
  }

 private:
  std::array<uint8_t, PayloadBytes> payload_{};
  GroovePuterState::SceneRevisionState revision_before_{};
  uint16_t payload_size_{0};
  UndoKind kind_{UndoKind::None};
  bool next_is_redo_{false};
};

}  // namespace GroovePuterUndo
