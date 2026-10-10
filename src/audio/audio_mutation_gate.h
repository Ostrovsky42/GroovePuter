#pragma once
#ifndef GROOVEPUTER_AUDIO_MUTATION_GATE_H
#define GROOVEPUTER_AUDIO_MUTATION_GATE_H

#include <atomic>
#include <cstdint>

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <chrono>
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

    const uint32_t waitStart = nowUs_();
    pauseRequested_.store(true, std::memory_order_release);
    while (!audioPaused_.load(std::memory_order_acquire)) {
      yieldCurrentThread_();
    }
    beginHold_(waitStart);
  }

  void unlockControl() {
    if (controlDepth_ == 0) return;
    --controlDepth_;
    if (controlDepth_ != 0) return;

    pauseRequested_.store(false, std::memory_order_release);
    endHold_();
  }

  // 0.9.19 UI frame probe. Control-thread only: how long the UI waited for
  // the renderer to reach a block boundary, and how long it then kept the
  // renderer paused. Totals run and may wrap; maxima reset on take.
  uint32_t controlWaitUsTotal() const { return waitUsTotal_; }
  uint32_t controlWaits() const { return waits_; }
  uint32_t controlHoldUsTotal() const { return holdUsTotal_; }
  uint32_t takeControlWaitMaxUs() {
    const uint32_t m = waitMaxUs_;
    waitMaxUs_ = 0;
    return m;
  }
  uint32_t takeControlHoldMaxUs() {
    const uint32_t m = holdMaxUs_;
    holdMaxUs_ = 0;
    return m;
  }

  // The outermost control mutation may temporarily release audio while it
  // performs storage I/O against an immutable candidate. Its caller must
  // reacquire before publishing any RAM state and before its Scope ends.
  bool openControlIoWindow() {
    if (controlDepth_ != 1 || ioWindowOpen_) return false;
    ioWindowOpen_ = true;
    endHold_();
    pauseRequested_.store(false, std::memory_order_release);
    while (audioTaskActive_.load(std::memory_order_acquire) &&
           audioPaused_.load(std::memory_order_acquire)) {
      yieldCurrentThread_();
    }
    return true;
  }

  bool closeControlIoWindow() {
    if (controlDepth_ != 1 || !ioWindowOpen_) return false;
    if (audioTaskActive_.load(std::memory_order_acquire)) {
      const uint32_t waitStart = nowUs_();
      pauseRequested_.store(true, std::memory_order_release);
      while (!audioPaused_.load(std::memory_order_acquire)) {
        yieldCurrentThread_();
      }
      beginHold_(waitStart);
    }
    ioWindowOpen_ = false;
    return true;
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
  static uint32_t nowUs_() {
#if defined(ARDUINO)
    return micros();
#else
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
#endif
  }

  void beginHold_(uint32_t waitStartUs) {
    lockedAtUs_ = nowUs_();
    const uint32_t waited = lockedAtUs_ - waitStartUs;
    waitUsTotal_ += waited;
    ++waits_;
    if (waited > waitMaxUs_) waitMaxUs_ = waited;
    holding_ = true;
  }

  void endHold_() {
    if (!holding_) return;
    holding_ = false;
    const uint32_t held = nowUs_() - lockedAtUs_;
    holdUsTotal_ += held;
    if (held > holdMaxUs_) holdMaxUs_ = held;
  }

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
  bool ioWindowOpen_ = false;
  bool holding_ = false;
  uint32_t lockedAtUs_ = 0;
  uint32_t waitUsTotal_ = 0;
  uint32_t waits_ = 0;
  uint32_t waitMaxUs_ = 0;
  uint32_t holdUsTotal_ = 0;
  uint32_t holdMaxUs_ = 0;
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

#endif  // GROOVEPUTER_AUDIO_MUTATION_GATE_H
