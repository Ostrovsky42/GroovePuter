#pragma once

#include <atomic>
#include <cstdint>

#if !defined(ARDUINO)
#include <thread>
#endif

// Coordinates control-plane mutations with the real-time renderer without
// holding a mutex while a DSP block is being generated. The control thread
// requests a pause, and the audio thread acknowledges it only at a block
// boundary. Existing UI mutation lambdas can then run without racing DSP.
class AudioMutationGate {
public:
  void setAudioTaskActive(bool active) {
    audioTaskActive_.store(active, std::memory_order_release);
    if (!active) {
      pauseRequested_.store(false, std::memory_order_release);
      audioPaused_.store(false, std::memory_order_release);
    }
  }

  void lockControl() {
    // The UI may enter a top-level guarded event and then call an existing
    // withAudioGuard() lambda. Only the outermost level owns the pause.
    if (controlDepth_++ > 0) return;
    if (!audioTaskActive_.load(std::memory_order_acquire)) return;

    pauseRequested_.store(true, std::memory_order_release);
    while (!audioPaused_.load(std::memory_order_acquire)) {
      yieldCurrentThread_();
    }
  }

  void unlockControl() {
    if (controlDepth_ == 0) return;
    --controlDepth_;
    if (controlDepth_ != 0) return;

    pauseRequested_.store(false, std::memory_order_release);
  }

  void waitAtAudioBoundary() {
    if (!pauseRequested_.load(std::memory_order_acquire)) return;

    audioPaused_.store(true, std::memory_order_release);
    while (pauseRequested_.load(std::memory_order_acquire)) {
      yieldCurrentThread_();
    }
    audioPaused_.store(false, std::memory_order_release);
  }

  bool pauseRequested() const {
    return pauseRequested_.load(std::memory_order_acquire);
  }

  bool audioPaused() const {
    return audioPaused_.load(std::memory_order_acquire);
  }

private:
  static void yieldCurrentThread_() {
#if defined(ARDUINO)
    delay(1);
#else
    std::this_thread::yield();
#endif
  }

  std::atomic<bool> audioTaskActive_{false};
  std::atomic<bool> pauseRequested_{false};
  std::atomic<bool> audioPaused_{false};

  // Accessed only by the single control/UI thread.
  uint32_t controlDepth_ = 0;
};

class AudioMutationScope {
public:
  explicit AudioMutationScope(AudioMutationGate& gate) : gate_(gate) {
    gate_.lockControl();
  }

  ~AudioMutationScope() {
    gate_.unlockControl();
  }

  AudioMutationScope(const AudioMutationScope&) = delete;
  AudioMutationScope& operator=(const AudioMutationScope&) = delete;

private:
  AudioMutationGate& gate_;
};
