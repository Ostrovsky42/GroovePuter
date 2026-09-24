#include "../src/audio/audio_mutation_gate.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

int main() {
  AudioMutationGate gate;
  std::atomic<bool> running{true};
  std::atomic<uint32_t> blocks{0};

  gate.setAudioTaskActive(true);
  std::thread audio([&] {
    while (running.load(std::memory_order_acquire)) {
      gate.waitAtAudioBoundary();
      blocks.fetch_add(1, std::memory_order_relaxed);
      std::this_thread::yield();
    }
  });

  while (blocks.load(std::memory_order_acquire) < 10) {
    std::this_thread::yield();
  }

  gate.lockControl();
  assert(gate.pauseRequested());
  assert(gate.audioPaused());
  const uint32_t pausedAt = blocks.load(std::memory_order_acquire);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  assert(blocks.load(std::memory_order_acquire) == pausedAt);

  // Nested guards must not release the outer mutation window.
  gate.lockControl();
  gate.unlockControl();
  assert(gate.pauseRequested());
  assert(gate.audioPaused());

  gate.unlockControl();
  while (blocks.load(std::memory_order_acquire) == pausedAt) {
    std::this_thread::yield();
  }

  // A control-side storage transaction may let rendering continue while it
  // performs I/O, then reacquire exclusion for bounded RAM publication.
  gate.lockControl();
  const uint32_t beforeIo = blocks.load(std::memory_order_acquire);
  assert(gate.openControlIoWindow());
  while (blocks.load(std::memory_order_acquire) == beforeIo) {
    std::this_thread::yield();
  }
  assert(gate.closeControlIoWindow());
  const uint32_t afterIo = blocks.load(std::memory_order_acquire);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  assert(blocks.load(std::memory_order_acquire) == afterIo);
  gate.unlockControl();

  running.store(false, std::memory_order_release);
  gate.setAudioTaskActive(false);
  audio.join();
  return 0;
}
