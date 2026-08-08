#include "src/audio/audio_control_snapshot.h"

#include <atomic>
#include <cassert>
#include <cstdint>
#include <thread>

namespace {

struct ProbeSnapshot {
  uint32_t revision = 0;
  uint32_t a = 0;
  uint32_t b = 0;
  uint32_t checksum = 0;
};

ProbeSnapshot makeSnapshot(uint32_t revision) {
  ProbeSnapshot value;
  value.revision = revision;
  value.a = revision * 3u + 7u;
  value.b = revision ^ 0xA5A55A5Au;
  value.checksum = value.revision ^ value.a ^ value.b ^ 0x13579BDFu;
  return value;
}

void assertCoherent(const ProbeSnapshot& value) {
  assert(value.a == value.revision * 3u + 7u);
  assert(value.b == (value.revision ^ 0xA5A55A5Au));
  assert(value.checksum ==
         (value.revision ^ value.a ^ value.b ^ 0x13579BDFu));
}

struct BoundaryProbe {
  int applyCount = 0;
  int captureCount = 0;
};

void applyProbe(void* context) {
  auto* probe = static_cast<BoundaryProbe*>(context);
  assert(probe != nullptr);
  ++probe->applyCount;
}

void captureProbe(void* context) {
  auto* probe = static_cast<BoundaryProbe*>(context);
  assert(probe != nullptr);
  ++probe->captureCount;
}

}  // namespace

int main() {
  using GroovePuterAudio::AudioControlBoundaryRegistry;
  using GroovePuterAudio::AudioControlSnapshotBuffer;

  AudioControlSnapshotBuffer<ProbeSnapshot> buffer;
  buffer.initialize(makeSnapshot(0));
  assertCoherent(buffer.read());

  buffer.publish(makeSnapshot(1));
  ProbeSnapshot once = buffer.read();
  assert(once.revision == 1);
  assertCoherent(once);

  // Stress the double-buffer handshake. The reader must never observe fields
  // from two different publications even when writer and reader run together.
  constexpr uint32_t kIterations = 50000;
  std::atomic<bool> writerDone{false};
  std::thread writer([&]() {
    for (uint32_t revision = 2; revision <= kIterations; ++revision) {
      buffer.publish(makeSnapshot(revision));
    }
    writerDone.store(true, std::memory_order_release);
  });

  uint32_t lastSeen = 0;
  while (!writerDone.load(std::memory_order_acquire)) {
    const ProbeSnapshot value = buffer.read();
    assertCoherent(value);
    if (value.revision > lastSeen) lastSeen = value.revision;
  }
  writer.join();

  const ProbeSnapshot finalValue = buffer.read();
  assert(finalValue.revision == kIterations);
  assertCoherent(finalValue);
  assert(finalValue.revision >= lastSeen);

  auto& registry = AudioControlBoundaryRegistry::instance();
  registry.setAudioTaskActive(false);

  BoundaryProbe probe;
  assert(registry.registerClient(&probe, &applyProbe, &captureProbe));
  assert(registry.clientCount() >= 1);
  assert(!registry.shouldQueueRoutineControls());

  // Starting the audio task captures the pre-existing control state once.
  registry.setAudioTaskActive(true);
  assert(probe.captureCount == 1);
  assert(registry.shouldQueueRoutineControls());

  registry.applyPendingAtAudioBoundary();
  assert(probe.applyCount == 1);

  registry.setStructuralMutationActive(true);
  assert(!registry.shouldQueueRoutineControls());
  registry.captureAllAfterStructuralMutation();
  assert(probe.captureCount == 2);

  registry.setStructuralMutationActive(false);
  assert(registry.shouldQueueRoutineControls());

  registry.setAudioTaskActive(false);
  assert(!registry.shouldQueueRoutineControls());
  registry.unregisterClient(&probe);

  return 0;
}
