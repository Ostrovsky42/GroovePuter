#include "ram_sample_store.h"
#include "sample_index.h"
#include <atomic>
#include <mutex>
#include <array>
#include <map>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <algorithm>

// Metadata-only probe and bounded WAV loader. The store first inspects decoded
// size, reclaims evictable pool capacity, then permits allocation/data read.
bool inspectWavFileBounded(const char* path, WavInfo& info,
                           std::size_t maxDecodedBytes);
bool loadWavFileBounded(const char* path, WavInfo& info, int16_t** outPcm,
                        std::size_t maxDecodedBytes);

namespace {

bool tryAcquireSlot(SampleSlot& slot, uint32_t expectedId) {
  if (expectedId == 0) return false;
  if (slot.id.load(std::memory_order_acquire) != expectedId) return false;
  if (!slot.ready.load(std::memory_order_acquire)) return false;

  slot.refCount.fetch_add(1, std::memory_order_acq_rel);

  // Eviction first withdraws ready, then checks refCount. Revalidate after
  // taking the reference so a concurrent withdrawal cannot publish a handle.
  if (slot.ready.load(std::memory_order_acquire) &&
      slot.id.load(std::memory_order_acquire) == expectedId) {
    return true;
  }

  slot.refCount.fetch_sub(1, std::memory_order_acq_rel);
  return false;
}

void releaseSlotReference(SampleSlot& slot) {
  uint32_t count = slot.refCount.load(std::memory_order_acquire);
  while (count > 0) {
    if (slot.refCount.compare_exchange_weak(
            count, count - 1,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      return;
    }
  }
}

}  // namespace

RamSampleStore::RamSampleStore()
    : currentPoolUsage_(0), maxPoolBytes_(256 * 1024), timeCounter_(0) {
  for (auto& slot : slots_) {
    slot.id.store(0, std::memory_order_relaxed);
    slot.ready.store(false, std::memory_order_relaxed);
    slot.data.store(nullptr, std::memory_order_relaxed);
    slot.refCount.store(0, std::memory_order_relaxed);
    slot.lastAccess.store(0, std::memory_order_relaxed);
  }
}

uint32_t RamSampleStore::nextTime() {
  return timeCounter_.fetch_add(1, std::memory_order_relaxed);
}

// === Handle-based API (preferred) ===

SampleHandle RamSampleStore::acquireHandle(SampleId id) {
  for (uint16_t i = 0; i < kMaxSampleSlots; ++i) {
    auto& slot = slots_[i];
    if (tryAcquireSlot(slot, id.value)) {
      slot.lastAccess.store(nextTime(), std::memory_order_relaxed);
      return {i, id};
    }
  }
  return SampleHandle::invalid();
}

void RamSampleStore::releaseHandle(SampleHandle h) {
  if (!h.valid() || h.slot >= kMaxSampleSlots) return;
  auto& slot = slots_[h.slot];
  if (slot.id.load(std::memory_order_acquire) == h.id.value) {
    releaseSlotReference(slot);
  }
}

SampleView RamSampleStore::viewHandle(SampleHandle h) const {
  if (!h.valid() || h.slot >= kMaxSampleSlots) return {nullptr, 0, 0};
  const auto& slot = slots_[h.slot];
  if (slot.id.load(std::memory_order_acquire) == h.id.value &&
      slot.ready.load(std::memory_order_acquire)) {
    const int16_t* p = slot.data.load(std::memory_order_acquire);
    if (p) return {p, slot.frames, slot.sampleRate};
  }
  return {nullptr, 0, 0};
}

// === Legacy ID-based API ===

void RamSampleStore::acquire(SampleId id) {
  for (auto& slot : slots_) {
    if (tryAcquireSlot(slot, id.value)) {
      slot.lastAccess.store(nextTime(), std::memory_order_relaxed);
      return;
    }
  }
}

void RamSampleStore::release(SampleId id) {
  for (auto& slot : slots_) {
    if (slot.id.load(std::memory_order_acquire) == id.value) {
      releaseSlotReference(slot);
      return;
    }
  }
}

SampleView RamSampleStore::view(SampleId id) const {
  for (const auto& slot : slots_) {
    if (slot.id.load(std::memory_order_acquire) == id.value &&
        slot.ready.load(std::memory_order_acquire)) {
      const int16_t* p = slot.data.load(std::memory_order_acquire);
      if (p) return {p, slot.frames, slot.sampleRate};
    }
  }
  return {nullptr, 0, 0};
}

bool RamSampleStore::registerFile(SampleId id, const std::string& path) {
  if (id.value == 0 || path.empty()) return false;

  std::lock_guard<std::mutex> lk(pathsMutex_);
  const auto it = filePaths_.find(id.value);
  if (it == filePaths_.end()) {
    filePaths_.emplace(id.value, path);
    return true;
  }

  if (it->second == path) return true;

  // A runtime ID may never silently change physical ownership. This protects
  // legacy basename hashes from last-write-wins corruption while 0.9.3-D
  // migrates persisted sampler ownership to stable SampleRef.
  printf("Sample registry: refusing conflicting ID %u: %s != %s\n",
         static_cast<unsigned>(id.value),
         it->second.c_str(), path.c_str());
  return false;
}

bool RamSampleStore::preload(SampleId id) {
  // 1. Check if already loaded and fully published.
  for (auto& slot : slots_) {
    if (slot.id.load(std::memory_order_acquire) == id.value &&
        slot.ready.load(std::memory_order_acquire)) {
      slot.lastAccess.store(nextTime(), std::memory_order_relaxed);
      return true;
    }
  }

  // 2. Find path.
  std::string path;
  {
    std::lock_guard<std::mutex> lk(pathsMutex_);
    auto it = filePaths_.find(id.value);
    if (it == filePaths_.end()) {
      printf("Preload: ID %u not found in registry\n", id.value);
      return false;
    }
    path = it->second;
  }

  printf("Preload: Loading %s ...\n", path.c_str());

  // 3. Metadata-only admission. No PCM allocation or data-chunk read is
  // allowed before the store knows the decoded mono size.
  WavInfo inspectedInfo{};
  if (!inspectWavFileBounded(path.c_str(), inspectedInfo, maxPoolBytes_)) {
    printf("Preload: WAV inspection failed for %s\n", path.c_str());
    return false;
  }

  const std::size_t requiredSize =
      static_cast<std::size_t>(inspectedInfo.numFrames) * sizeof(int16_t);
  if (requiredSize > maxPoolBytes_) {
    printf("Preload: Sample is larger than the entire pool\n");
    return false;
  }

  // 4. Reclaim pool capacity before any PCM allocation. If every eviction
  // candidate is referenced by active audio, fail without allocating the new
  // sample. This removes the old-resident + new-sample transient pool peak.
  while (currentPoolUsage_ + requiredSize > maxPoolBytes_) {
    const std::size_t usageBefore = currentPoolUsage_;
    evictLRU();
    if (currentPoolUsage_ >= usageBefore) {
      printf("Preload: Pool is busy; no evictable sample slots\n");
      return false;
    }
  }

  auto findQuiescentSlot = [&]() -> int {
    for (int i = 0; i < kMaxSampleSlots; ++i) {
      auto& slot = slots_[i];
      if (slot.id.load(std::memory_order_acquire) == 0 &&
          !slot.ready.load(std::memory_order_acquire) &&
          slot.refCount.load(std::memory_order_acquire) == 0 &&
          slot.data.load(std::memory_order_acquire) == nullptr) {
        return i;
      }
    }
    return -1;
  };

  // Slot pressure is also an admission condition. Free one slot before decode
  // rather than allocating PCM and discovering the 64-slot table is full.
  int slotIdx = findQuiescentSlot();
  if (slotIdx < 0) {
    const std::size_t usageBefore = currentPoolUsage_;
    evictLRU();
    if (currentPoolUsage_ >= usageBefore) {
      printf("Preload: No evictable slot available\n");
      return false;
    }
    slotIdx = findQuiescentSlot();
    if (slotIdx < 0) {
      printf("Preload: No quiescent free slots\n");
      return false;
    }
  }

  // 5. Decode only after admission/eviction. Pass the actual remaining pool
  // capacity, not the entire configured pool, so a file that changes between
  // probe and decode still fails before allocation if it no longer fits.
  WavInfo info{};
  int16_t* pcm = nullptr;
  const std::size_t decodeBudget = freePoolBytes();
  if (!loadWavFileBounded(path.c_str(), info, &pcm, decodeBudget)) {
    printf("Preload: loadWavFile failed for %s\n", path.c_str());
    return false;
  }

  const std::size_t size =
      static_cast<std::size_t>(info.numFrames) * sizeof(int16_t);
  printf("Preload: Loaded %u frames (%u bytes). Pool usage: %u/%u\n",
         info.numFrames, static_cast<unsigned>(size),
         static_cast<unsigned>(currentPoolUsage_),
         static_cast<unsigned>(maxPoolBytes_));

  // Defensive invariants for a file changed between inspect and decode.
  if (size > decodeBudget || currentPoolUsage_ + size > maxPoolBytes_) {
    printf("Preload: Decoded sample no longer fits admitted pool capacity\n");
    free(pcm);
    return false;
  }

  // 6. Fill and publish the pre-admitted slot. Non-atomic metadata is
  // protected by the final ready release and matching acquire in readers.
  auto& slot = slots_[slotIdx];
  slot.frames = info.numFrames;
  slot.sampleRate = info.sampleRate;
  slot.sizeBytes = size;
  slot.data.store(pcm, std::memory_order_relaxed);
  slot.lastAccess.store(nextTime(), std::memory_order_relaxed);
  slot.refCount.store(0, std::memory_order_relaxed);
  slot.id.store(id.value, std::memory_order_relaxed);
  slot.ready.store(true, std::memory_order_release);

  currentPoolUsage_ += size;
  return true;
}

void RamSampleStore::evictLRU() {
  int candidateIdx = -1;
  uint32_t oldestTime = std::numeric_limits<uint32_t>::max();

  for (int i = 0; i < kMaxSampleSlots; ++i) {
    auto& slot = slots_[i];
    const uint32_t id = slot.id.load(std::memory_order_acquire);
    if (id != 0 && slot.ready.load(std::memory_order_acquire) &&
        slot.refCount.load(std::memory_order_acquire) == 0) {
      const uint32_t access = slot.lastAccess.load(std::memory_order_relaxed);
      if (access < oldestTime) {
        oldestTime = access;
        candidateIdx = i;
      }
    }
  }

  if (candidateIdx < 0) return;

  auto& slot = slots_[candidateIdx];

  // Withdraw publication first. Any acquisition that observed the old ready
  // value must increment and then revalidate it before returning a handle.
  bool expectedReady = true;
  if (!slot.ready.compare_exchange_strong(
          expectedReady, false,
          std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    return;
  }

  if (slot.refCount.load(std::memory_order_acquire) != 0) {
    slot.ready.store(true, std::memory_order_release);
    return;
  }

  slot.id.store(0, std::memory_order_release);
  int16_t* ptr = const_cast<int16_t*>(
      slot.data.exchange(nullptr, std::memory_order_acq_rel));
  if (ptr) free(ptr);

  if (slot.sizeBytes <= currentPoolUsage_) {
    currentPoolUsage_ -= slot.sizeBytes;
  } else {
    currentPoolUsage_ = 0;
  }
  slot.frames = 0;
  slot.sampleRate = 0;
  slot.sizeBytes = 0;
}

std::size_t RamSampleStore::freePoolBytes() const {
  if (currentPoolUsage_ > maxPoolBytes_) return 0;
  return maxPoolBytes_ - currentPoolUsage_;
}
