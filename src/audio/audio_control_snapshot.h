#pragma once
#ifndef GROOVEPUTER_AUDIO_CONTROL_SNAPSHOT_H
#define GROOVEPUTER_AUDIO_CONTROL_SNAPSHOT_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace GroovePuterAudio {

// Single-control-writer / single-audio-reader double buffer.
//
// The control thread writes a complete immutable value into the inactive slot
// and publishes it with one release-store. The audio thread announces the slot
// it is copying so a rapid second control publish never overwrites a slot while
// it is being read. The control side may spin only for the duration of one small
// POD copy; the audio side never waits for the control thread.
template <typename T>
class AudioControlSnapshotBuffer {
 public:
  static_assert(std::is_trivially_copyable<T>::value,
                "audio control snapshots must be trivially copyable");

  AudioControlSnapshotBuffer() = default;

  void initialize(const T& value) {
    slots_[0] = value;
    slots_[1] = value;
    activeSlot_.store(0, std::memory_order_release);
    readerSlot_.store(kNoReader, std::memory_order_release);
  }

  void publish(const T& value) {
    const uint8_t current = activeSlot_.load(std::memory_order_acquire);
    const uint8_t inactive = static_cast<uint8_t>(1u - current);

    // Only the non-audio control thread can wait here. The reader copies a
    // small POD and clears readerSlot_ immediately afterwards.
    while (readerSlot_.load(std::memory_order_acquire) == inactive) {
      std::atomic_signal_fence(std::memory_order_seq_cst);
    }

    slots_[inactive] = value;
    activeSlot_.store(inactive, std::memory_order_release);
  }

  T read() const {
    for (;;) {
      const uint8_t slot = activeSlot_.load(std::memory_order_acquire);
      readerSlot_.store(slot, std::memory_order_release);

      // If the writer published between our first load and announcement, do
      // not copy from the old slot; retry against the newly active snapshot.
      if (activeSlot_.load(std::memory_order_acquire) != slot) {
        readerSlot_.store(kNoReader, std::memory_order_release);
        continue;
      }

      const T value = slots_[slot];
      readerSlot_.store(kNoReader, std::memory_order_release);
      return value;
    }
  }

 private:
  static constexpr uint8_t kNoReader = 0xFFu;

  T slots_[2]{};
  std::atomic<uint8_t> activeSlot_{0};
  mutable std::atomic<uint8_t> readerSlot_{kNoReader};
};

// Fixed-capacity boundary registry. Clients register before the audio task is
// active and stay alive until it is stopped. No allocation occurs here.
class AudioControlBoundaryRegistry {
 public:
  using Callback = void (*)(void* context);

  static AudioControlBoundaryRegistry& instance() {
    static AudioControlBoundaryRegistry registry;
    return registry;
  }

  bool registerClient(void* context,
                      Callback applyPending,
                      Callback captureStructuralState) {
    if (!context || !applyPending || !captureStructuralState) return false;
    if (audioTaskActive_.load(std::memory_order_acquire)) return false;

    for (size_t i = 0; i < clientCount_; ++i) {
      if (clients_[i].context == context) return true;
    }
    if (clientCount_ >= kMaxClients) return false;

    clients_[clientCount_++] =
        Client{context, applyPending, captureStructuralState};
    return true;
  }

  void unregisterClient(void* context) {
    if (!context) return;
    // Runtime destruction while the audio task is live would invalidate a
    // callback target. Embedded SwappableSynthVoice instances live for the
    // whole audio-task lifetime; host tests stop the registry before teardown.
    if (audioTaskActive_.load(std::memory_order_acquire)) return;

    for (size_t i = 0; i < clientCount_; ++i) {
      if (clients_[i].context != context) continue;
      for (size_t j = i + 1; j < clientCount_; ++j) {
        clients_[j - 1] = clients_[j];
      }
      clients_[clientCount_ - 1] = Client{};
      --clientCount_;
      return;
    }
  }

  void setAudioTaskActive(bool active) {
    if (active) {
      // Capture parameters changed during boot before routine controls start
      // queueing snapshots.
      captureAllAfterStructuralMutation();
    }
    audioTaskActive_.store(active, std::memory_order_release);
    if (!active) {
      structuralMutationActive_.store(false, std::memory_order_release);
    }
  }

  bool audioTaskActive() const {
    return audioTaskActive_.load(std::memory_order_acquire);
  }

  void setStructuralMutationActive(bool active) {
    structuralMutationActive_.store(active, std::memory_order_release);
  }

  bool structuralMutationActive() const {
    return structuralMutationActive_.load(std::memory_order_acquire);
  }

  bool shouldQueueRoutineControls() const {
    return audioTaskActive() && !structuralMutationActive();
  }

  // Audio task only. Called exactly once at a renderer block boundary.
  void applyPendingAtAudioBoundary() {
    if (!audioTaskActive_.load(std::memory_order_acquire)) return;
    for (size_t i = 0; i < clientCount_; ++i) {
      clients_[i].applyPending(clients_[i].context);
    }
  }

  // Control thread only, while the renderer is stopped at a boundary (or
  // before it has started). This makes direct structural setters authoritative
  // again and prevents a stale queued snapshot from overwriting them later.
  void captureAllAfterStructuralMutation() {
    for (size_t i = 0; i < clientCount_; ++i) {
      clients_[i].captureStructuralState(clients_[i].context);
    }
  }

  size_t clientCount() const { return clientCount_; }

 private:
  struct Client {
    void* context = nullptr;
    Callback applyPending = nullptr;
    Callback captureStructuralState = nullptr;
  };

  static constexpr size_t kMaxClients = 4;

  AudioControlBoundaryRegistry() = default;

  Client clients_[kMaxClients]{};
  size_t clientCount_ = 0;
  std::atomic<bool> audioTaskActive_{false};
  std::atomic<bool> structuralMutationActive_{false};
};

}  // namespace GroovePuterAudio

#endif  // GROOVEPUTER_AUDIO_CONTROL_SNAPSHOT_H
