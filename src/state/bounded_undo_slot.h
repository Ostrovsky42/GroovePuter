#pragma once

#ifndef GROOVEPUTER_STATE_BOUNDED_UNDO_SLOT_H
#define GROOVEPUTER_STATE_BOUNDED_UNDO_SLOT_H

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

enum class UndoRetainedResourceKind : uint8_t {
  None = 0,
  PatternBacking = 1,
};

struct UndoRetainedResource {
  int16_t resourceId{-1};
  uint8_t mask{0};
  UndoRetainedResourceKind kind{UndoRetainedResourceKind::None};
};

constexpr uint8_t kUndoRetainedResourceCapacity = 20;
constexpr std::size_t kUndoLifecycleTailBytes = 112;

struct UndoLifecycleMetadata;
using UndoLifecycleCleanupFn = void (*)(void*, const UndoLifecycleMetadata&);
using UndoLifecycleSanitizeFn =
    void (*)(void*, const UndoLifecycleMetadata&, void*);

struct UndoLifecycleMetadata {
  void* context{nullptr};
  UndoLifecycleCleanupFn cleanup{nullptr};
  UndoLifecycleSanitizeFn sanitizeForPersistence{nullptr};
  UndoRetainedResource resources[kUndoRetainedResourceCapacity]{};
  uint8_t count{0};
  uint8_t pageIndex{0xFFu};
  uint8_t reserved[2]{0, 0};
};

static_assert(sizeof(UndoRetainedResource) == 4,
              "P1b retained resource budget changed");
static_assert(sizeof(UndoLifecycleMetadata) <= kUndoLifecycleTailBytes,
              "P1b Undo lifecycle metadata exceeds reserved tail");
static_assert(std::is_trivially_copyable<UndoLifecycleMetadata>::value,
              "P1b Undo lifecycle metadata must remain fixed value state");

template <std::size_t PayloadBytes>
class BoundedUndoSlot {
 public:
  static_assert(PayloadBytes > kUndoLifecycleTailBytes,
                "Undo payload must leave room for lifecycle metadata");
  static_assert(PayloadBytes < kLifecycleFlag,
                "Undo payload size must fit below lifecycle flag");

  static constexpr std::size_t capacity() { return PayloadBytes; }
  static constexpr std::size_t lifecyclePayloadCapacity() {
    return PayloadBytes - kUndoLifecycleTailBytes;
  }

  bool hasUndo() const { return kind_ != UndoKind::None; }
  UndoKind kind() const { return kind_; }
  uint16_t payloadSize() const {
    return static_cast<uint16_t>(payload_size_ & kPayloadSizeMask);
  }
  bool hasLifecycle() const { return (payload_size_ & kLifecycleFlag) != 0; }
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
    if (kind == UndoKind::None || sizeof(Payload) > PayloadBytes) return false;

    std::memcpy(payload_.data(), &before, sizeof(Payload));
    revision_before_ = revision_before;
    payload_size_ = static_cast<uint16_t>(sizeof(Payload));
    kind_ = kind;
    next_is_redo_ = false;
    return true;
  }

  template <typename Payload>
  bool publishWithLifecycle(
      UndoKind kind,
      const Payload& before,
      const GroovePuterState::SceneRevisionState& revision_before,
      const UndoLifecycleMetadata& lifecycle) {
    static_assert(std::is_trivially_copyable<Payload>::value,
                  "Undo payloads must be trivially copyable fixed values");
    if (kind == UndoKind::None ||
        sizeof(Payload) > lifecyclePayloadCapacity() ||
        lifecycle.count > kUndoRetainedResourceCapacity) {
      return false;
    }

    std::memcpy(payload_.data(), &before, sizeof(Payload));
    std::memcpy(lifecycleStorage(), &lifecycle, sizeof(lifecycle));
    revision_before_ = revision_before;
    payload_size_ = static_cast<uint16_t>(sizeof(Payload)) | kLifecycleFlag;
    kind_ = kind;
    next_is_redo_ = false;
    return true;
  }

  template <typename Payload>
  bool read(UndoKind expected_kind, Payload& before) const {
    static_assert(std::is_trivially_copyable<Payload>::value,
                  "Undo payloads must be trivially copyable fixed values");
    if (kind_ != expected_kind || payloadSize() != sizeof(Payload)) return false;
    std::memcpy(&before, payload_.data(), sizeof(Payload));
    return true;
  }

  bool readLifecycle(UndoLifecycleMetadata& lifecycle) const {
    if (!hasUndo() || !hasLifecycle()) return false;
    std::memcpy(&lifecycle, lifecycleStorage(), sizeof(lifecycle));
    return lifecycle.count <= kUndoRetainedResourceCapacity;
  }

  void clear() {
    kind_ = UndoKind::None;
    payload_size_ = 0;
    revision_before_ = GroovePuterState::SceneRevisionState{};
    next_is_redo_ = false;
  }

 private:
  static constexpr uint16_t kLifecycleFlag = 0x8000u;
  static constexpr uint16_t kPayloadSizeMask = 0x7FFFu;

  uint8_t* lifecycleStorage() {
    return payload_.data() + PayloadBytes - kUndoLifecycleTailBytes;
  }
  const uint8_t* lifecycleStorage() const {
    return payload_.data() + PayloadBytes - kUndoLifecycleTailBytes;
  }

  std::array<uint8_t, PayloadBytes> payload_{};
  GroovePuterState::SceneRevisionState revision_before_{};
  uint16_t payload_size_{0};
  UndoKind kind_{UndoKind::None};
  bool next_is_redo_{false};
};

}  // namespace GroovePuterUndo

#endif  // GROOVEPUTER_STATE_BOUNDED_UNDO_SLOT_H
