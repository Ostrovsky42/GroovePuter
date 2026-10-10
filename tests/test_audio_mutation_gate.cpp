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

  assert(gate.controlWaits() == 0 && gate.controlHoldUsTotal() == 0);
  gate.lockControl();
  assert(gate.pauseRequested());
  assert(gate.audioPaused());
  assert(gate.controlWaits() == 1);
  const uint32_t pausedAt = blocks.load(std::memory_order_acquire);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  assert(blocks.load(std::memory_order_acquire) == pausedAt);

  // Nested guards must not release the outer mutation window.
  gate.lockControl();
  gate.unlockControl();
  assert(gate.pauseRequested());
  assert(gate.audioPaused());

  assert(gate.controlWaits() == 1);  // nested lock waits for nothing
  gate.unlockControl();
  // 0.9.19 probe: the outer hold covered the 2 ms sleep above.
  assert(gate.controlHoldUsTotal() >= 2000);
  const uint32_t firstHoldMax = gate.takeControlHoldMaxUs();
  assert(firstHoldMax >= 2000 && gate.takeControlHoldMaxUs() == 0);
  while (blocks.load(std::memory_order_acquire) == pausedAt) {
    std::this_thread::yield();
  }

  // A control-side storage transaction may let rendering continue while it
  // performs I/O, then reacquire exclusion for bounded RAM publication.
  gate.lockControl();
  const uint32_t beforeIo = blocks.load(std::memory_order_acquire);
  assert(gate.openControlIoWindow());
  const uint32_t holdBeforeIo = gate.controlHoldUsTotal();
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  while (blocks.load(std::memory_order_acquire) == beforeIo) {
    std::this_thread::yield();
  }
  // Audio runs during the I/O window: that time is not a hold.
  assert(gate.controlHoldUsTotal() == holdBeforeIo);
  assert(gate.closeControlIoWindow());
  assert(gate.controlWaits() == 3);  // lock + reacquire after the window
  const uint32_t afterIo = blocks.load(std::memory_order_acquire);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  assert(blocks.load(std::memory_order_acquire) == afterIo);
  gate.unlockControl();

  running.store(false, std::memory_order_release);
  gate.setAudioTaskActive(false);
  audio.join();
  return 0;
}
