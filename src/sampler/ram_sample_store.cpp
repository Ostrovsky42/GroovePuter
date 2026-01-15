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

// External WAV loader
bool loadWavFile(const char* path, WavInfo& info, int16_t** outPcm);

RamSampleStore::RamSampleStore() : currentPoolUsage_(0), maxPoolBytes_(256 * 1024), timeCounter_(0) {
  for (auto& slot : slots_) {
    slot.id.store(0);
    slot.ready.store(false);
    slot.data.store(nullptr);
    slot.refCount.store(0);
    slot.lastAccess.store(0);
  }
}

uint32_t RamSampleStore::nextTime() {
  return timeCounter_.fetch_add(1, std::memory_order_relaxed);
}

// === Handle-based API (preferred) ===

SampleHandle RamSampleStore::acquireHandle(SampleId id) {
  for (uint16_t i = 0; i < kMaxSampleSlots; ++i) {
    auto& slot = slots_[i];
    if (slot.id.load(std::memory_order_relaxed) == id.value &&
        slot.ready.load(std::memory_order_acquire)) {
      slot.refCount.fetch_add(1, std::memory_order_relaxed);
      slot.lastAccess.store(nextTime(), std::memory_order_relaxed);
      return {i, id};
    }
  }
  return SampleHandle::invalid();
}

void RamSampleStore::releaseHandle(SampleHandle h) {
  if (!h.valid() || h.slot >= kMaxSampleSlots) return;
  auto& slot = slots_[h.slot];
  // Verify ID still matches (defensive)
  if (slot.id.load(std::memory_order_relaxed) == h.id.value) {
    slot.refCount.fetch_sub(1, std::memory_order_relaxed);
  }
}

SampleView RamSampleStore::viewHandle(SampleHandle h) const {
  if (!h.valid() || h.slot >= kMaxSampleSlots) return {nullptr, 0, 0};
  const auto& slot = slots_[h.slot];
  // O(1) direct access, no search
  if (slot.id.load(std::memory_order_relaxed) == h.id.value &&
      slot.ready.load(std::memory_order_acquire)) {
    const int16_t* p = slot.data.load(std::memory_order_relaxed);
    if (p) return {p, slot.frames, slot.sampleRate};
  }
  return {nullptr, 0, 0};
}

// === Legacy ID-based API ===

void RamSampleStore::acquire(SampleId id) {
  for (auto& slot : slots_) {
    if (slot.id.load(std::memory_order_relaxed) == id.value) {
      slot.refCount.fetch_add(1, std::memory_order_relaxed);
      slot.lastAccess.store(nextTime(), std::memory_order_relaxed);
      return;
    }
  }
}

void RamSampleStore::release(SampleId id) {
  for (auto& slot : slots_) {
    if (slot.id.load(std::memory_order_relaxed) == id.value) {
      slot.refCount.fetch_sub(1, std::memory_order_relaxed);
      return;
    }
  }
}

SampleView RamSampleStore::view(SampleId id) const {
  for (const auto& slot : slots_) {
    if (slot.id.load(std::memory_order_relaxed) == id.value &&
        slot.ready.load(std::memory_order_acquire)) {
      const int16_t* p = slot.data.load(std::memory_order_relaxed);
      if (p) {
        return {p, slot.frames, slot.sampleRate};
      }
    }
  }
  return {nullptr, 0, 0};
}

void RamSampleStore::registerFile(SampleId id, const std::string& path) {
  std::lock_guard<std::mutex> lk(pathsMutex_);
  filePaths_[id.value] = path;
}

bool RamSampleStore::preload(SampleId id) {
  // 1. Check if already loaded
  for (auto& slot : slots_) {
    if (slot.id.load(std::memory_order_acquire) == id.value) {
      slot.lastAccess.store(nextTime(), std::memory_order_relaxed);
      return true;
    }
  }

  // 2. Find path
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

  // 3. Load from disk
  WavInfo info;
  int16_t* pcm = nullptr;
  if (!loadWavFile(path.c_str(), info, &pcm)) {
    printf("Preload: loadWavFile failed for %s\n", path.c_str());
    return false;
  }
  
  std::size_t size = info.numFrames * sizeof(int16_t);
  printf("Preload: Loaded %u frames (%u bytes). Pool usage: %u/%u\n", 
         info.numFrames, (unsigned)size, (unsigned)currentPoolUsage_, (unsigned)maxPoolBytes_);
  
  // 4. Find free slot or evict
  int slotIdx = -1;
  while (currentPoolUsage_ + size > maxPoolBytes_) {
    printf("Preload: Evicting LRU to make space...\n");
    evictLRU();
    // Safety break if eviction didn't help (e.g. all locked?)
    if (freePoolBytes() < size && currentPoolUsage_ == 0) break; // Should not happen if evict works
  }
  
  if (currentPoolUsage_ + size > maxPoolBytes_) {
      printf("Preload: Pool full! Needed %u, have %u free. Max: %u\n", 
             (unsigned)size, (unsigned)freePoolBytes(), (unsigned)maxPoolBytes_);
      free(pcm);
      return false;
  }

  // Find empty slot
  for (int i = 0; i < kMaxSampleSlots; ++i) {
    if (slots_[i].id.load(std::memory_order_relaxed) == 0) {
      slotIdx = i;
      break;
    }
  }

  if (slotIdx < 0) {
    printf("Preload: No free slots!\n");
    if (pcm) free(pcm);
    return false;
  }

  // 5. Fill slot
  slots_[slotIdx].frames = info.numFrames;
  slots_[slotIdx].sampleRate = info.sampleRate;
  slots_[slotIdx].sizeBytes = size;
  slots_[slotIdx].data.store(pcm, std::memory_order_relaxed);
  slots_[slotIdx].lastAccess.store(nextTime(), std::memory_order_relaxed);
  slots_[slotIdx].refCount.store(0, std::memory_order_relaxed);
  
  // Publish ID
  slots_[slotIdx].id.store(id.value, std::memory_order_relaxed);
  
  // Publish ready LAST with release semantics
  slots_[slotIdx].ready.store(true, std::memory_order_release);
  
  currentPoolUsage_ += size;
  return true;
}

void RamSampleStore::evictLRU() {
  int candidateIdx = -1;
  uint32_t oldestTime = std::numeric_limits<uint32_t>::max();

  for (int i = 0; i < kMaxSampleSlots; ++i) {
    uint32_t tid = slots_[i].id.load(std::memory_order_relaxed);
    if (tid != 0 && slots_[i].refCount.load(std::memory_order_relaxed) == 0) {
      uint32_t t = slots_[i].lastAccess.load(std::memory_order_relaxed);
      if (t < oldestTime) {
         oldestTime = t;
         candidateIdx = i;
      }
    }
  }

  if (candidateIdx >= 0) {
    // Clear ready first to stop new acquisitions
    slots_[candidateIdx].ready.store(false, std::memory_order_release);
    slots_[candidateIdx].id.store(0, std::memory_order_relaxed);
    int16_t* ptr = (int16_t*)slots_[candidateIdx].data.exchange(nullptr, std::memory_order_acquire);
    if (ptr) free(ptr); 
    
    currentPoolUsage_ -= slots_[candidateIdx].sizeBytes;
    slots_[candidateIdx].sizeBytes = 0;
  }
}

std::size_t RamSampleStore::freePoolBytes() const {
  if (currentPoolUsage_ > maxPoolBytes_) return 0;
  return maxPoolBytes_ - currentPoolUsage_;
}
