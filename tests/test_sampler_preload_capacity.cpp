#include "../src/sampler/ram_sample_store.h"
#include "../src/sampler/sample_loader.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

uint32_t gProbeFrames = 0;
std::size_t gProbeBudget = 0;
std::size_t gDecodeBudget = 0;
int gProbeCalls = 0;
int gDecodeCalls = 0;

class TestRamSampleStore final : public RamSampleStore {
 public:
  void seedSlot(int index, SampleId id, std::size_t bytes, uint32_t access) {
    assert(index >= 0 && index < kMaxSampleSlots);
    assert(bytes > 0 && bytes % sizeof(int16_t) == 0);

    int16_t* pcm = static_cast<int16_t*>(malloc(bytes));
    assert(pcm != nullptr);
    memset(pcm, 0, bytes);

    auto& slot = slots_[index];
    slot.frames = static_cast<uint32_t>(bytes / sizeof(int16_t));
    slot.sampleRate = 22050;
    slot.sizeBytes = bytes;
    slot.kind = SampleSlotKind::Resident;
    slot.data.store(pcm, std::memory_order_relaxed);
    slot.refCount.store(0, std::memory_order_relaxed);
    slot.lastAccess.store(access, std::memory_order_relaxed);
    slot.id.store(id.value, std::memory_order_relaxed);
    slot.ready.store(true, std::memory_order_release);
    currentPoolUsage_ += bytes;
  }

  std::size_t usage() const { return currentPoolUsage_; }
};

void resetStubs(uint32_t frames) {
  gProbeFrames = frames;
  gProbeBudget = 0;
  gDecodeBudget = 0;
  gProbeCalls = 0;
  gDecodeCalls = 0;
}

void testEvictsBeforeDecodeAllocation() {
  TestRamSampleStore store;
  store.setPoolSize(32);
  store.seedSlot(0, {1}, 16, 1);  // oldest: must be evicted first
  store.seedSlot(1, {2}, 12, 2);
  assert(store.usage() == 28);
  assert(store.registerFile({99}, "new.wav"));

  resetStubs(8);  // 16 decoded bytes
  assert(store.preload({99}));

  assert(gProbeCalls == 1);
  assert(gDecodeCalls == 1);
  assert(gProbeBudget == std::numeric_limits<std::size_t>::max());

  // Storage policy is chosen only after allocation-free metadata inspection.
  // The resident branch must then evict the 16-byte oldest slot first,
  // leaving 20 bytes of actual capacity and decode with exactly that budget.
  assert(gDecodeBudget == 20);
  assert(store.usage() == 28);
  assert(store.view({1}).empty());
  assert(!store.view({2}).empty());
  assert(!store.view({99}).empty());
}

void testBusyPoolRejectsBeforeDecodeAllocation() {
  TestRamSampleStore store;
  store.setPoolSize(32);
  store.seedSlot(0, {10}, 16, 1);
  store.seedSlot(1, {11}, 12, 2);
  assert(store.registerFile({100}, "busy.wav"));

  const SampleHandle h0 = store.acquireHandle({10});
  const SampleHandle h1 = store.acquireHandle({11});
  assert(h0.valid());
  assert(h1.valid());

  resetStubs(8);  // needs 16 bytes, only 4 are free and nothing is evictable
  assert(!store.preload({100}));
  assert(gProbeCalls == 1);
  assert(gDecodeCalls == 0);
  assert(store.usage() == 28);
  assert(store.view({100}).empty());

  store.releaseHandle(h0);
  store.releaseHandle(h1);
}

}  // namespace

const char* wavLoadErrorName(WavLoadError) {
  return "preload-capacity-test-stub";
}

bool inspectWavFileBounded(const char*, WavInspectResult& inspected,
                           std::size_t maxDecodedBytes, WavLoadError* error) {
  ++gProbeCalls;
  gProbeBudget = maxDecodedBytes;
  inspected = {};
  inspected.info.sampleRate = 22050;
  inspected.info.channels = 1;
  inspected.info.bitsPerSample = 16;
  inspected.info.numFrames = gProbeFrames;
  inspected.sourceChannels = 1;
  inspected.sourceDataBytes = gProbeFrames * sizeof(int16_t);
  inspected.decodedBytes =
      static_cast<std::size_t>(gProbeFrames) * sizeof(int16_t);
  if (inspected.decodedBytes > maxDecodedBytes) {
    if (error != nullptr) *error = WavLoadError::TooLarge;
    return false;
  }
  if (error != nullptr) *error = WavLoadError::Ok;
  return true;
}

bool decodeWavFileBounded(const char*, const WavInspectResult& inspected,
                          int16_t** outPcm, std::size_t maxDecodedBytes,
                          WavLoadError* error) {
  ++gDecodeCalls;
  gDecodeBudget = maxDecodedBytes;
  if (outPcm == nullptr) {
    if (error != nullptr) *error = WavLoadError::InvalidArgument;
    return false;
  }
  *outPcm = nullptr;
  if (inspected.decodedBytes > maxDecodedBytes) {
    if (error != nullptr) *error = WavLoadError::TooLarge;
    return false;
  }

  *outPcm = static_cast<int16_t*>(malloc(inspected.decodedBytes));
  if (*outPcm == nullptr) {
    if (error != nullptr) *error = WavLoadError::OutOfMemory;
    return false;
  }
  if (error != nullptr) *error = WavLoadError::Ok;
  return true;
}

int main() {
  testEvictsBeforeDecodeAllocation();
  testBusyPoolRejectsBeforeDecodeAllocation();
  return 0;
}