#include "ram_sample_store.h"
#include "sample_index.h"
#include "sample_loader.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>

#if defined(ESP32) || defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

#if defined(ARDUINO)
#include <Arduino.h>
#include <SD.h>
#endif

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

uint32_t streamClockMicros() {
#if defined(ARDUINO)
  return micros();
#else
  using namespace std::chrono;
  return static_cast<uint32_t>(
      duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
#endif
}

int16_t readLe16(const uint8_t* p) {
  const uint16_t value = static_cast<uint16_t>(p[0]) |
                         (static_cast<uint16_t>(p[1]) << 8);
  return static_cast<int16_t>(value);
}

}  // namespace

RamSampleStore::RamSampleStore()
    : currentPoolUsage_(0), maxPoolBytes_(256 * 1024), timeCounter_(0) {
  for (auto& slot : slots_) resetSlot_(slot);
}

RamSampleStore::~RamSampleStore() {
  for (auto& slot : slots_) {
    slot.ready.store(false, std::memory_order_release);
    int16_t* ptr = const_cast<int16_t*>(
        slot.data.exchange(nullptr, std::memory_order_acq_rel));
    if (ptr) std::free(ptr);
    resetSlot_(slot);
  }

  for (int i = 0; i < kSamplerStreamIoHandleCount; ++i) {
    closeIoSlot_(i);
  }

  if (streamCacheMemory_) {
#if defined(ESP32) || defined(ESP_PLATFORM)
    heap_caps_free(streamCacheMemory_);
#else
    std::free(streamCacheMemory_);
#endif
    streamCacheMemory_ = nullptr;
  }
}

void RamSampleStore::resetSlot_(SampleSlot& slot) {
  slot.id.store(0, std::memory_order_relaxed);
  slot.ready.store(false, std::memory_order_relaxed);
  slot.data.store(nullptr, std::memory_order_relaxed);
  slot.frames = 0;
  slot.sampleRate = 0;
  slot.sizeBytes = 0;
  slot.dataOffset = 0;
  slot.sourceDataBytes = 0;
  slot.sourceChannels = 0;
  slot.kind = SampleSlotKind::Empty;
  slot.refCount.store(0, std::memory_order_relaxed);
  slot.lastAccess.store(0, std::memory_order_relaxed);
}

uint32_t RamSampleStore::nextTime() {
  return timeCounter_.fetch_add(1, std::memory_order_relaxed);
}

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
  if (slot.id.load(std::memory_order_acquire) != h.id.value ||
      !slot.ready.load(std::memory_order_acquire)) {
    return {nullptr, 0, 0};
  }

  const int16_t* p = slot.data.load(std::memory_order_acquire);
  return {p, slot.frames, slot.sampleRate};
}

SampleSourceInfo RamSampleStore::sourceInfoHandle(SampleHandle h) const {
  if (!h.valid() || h.slot >= kMaxSampleSlots) return {};
  const auto& slot = slots_[h.slot];
  if (slot.id.load(std::memory_order_acquire) != h.id.value ||
      !slot.ready.load(std::memory_order_acquire)) {
    return {};
  }

  SampleStorageKind storage = SampleStorageKind::None;
  if (slot.kind == SampleSlotKind::Resident) {
    storage = SampleStorageKind::Resident;
  } else if (slot.kind == SampleSlotKind::Streamed) {
    storage = SampleStorageKind::Streamed;
  }
  return {slot.frames, slot.sampleRate, storage};
}

bool RamSampleStore::streamPageContains_(const SamplerStreamPage& page,
                                         uint32_t sampleId,
                                         uint32_t frame) const {
  if (page.sampleId != sampleId || page.validFrames == 0) return false;
  return frame >= page.startFrame &&
         frame < page.startFrame + static_cast<uint32_t>(page.validFrames);
}

bool RamSampleStore::readFrameHandle(SampleHandle h, uint32_t frame,
                                     int16_t& out) {
  if (!h.valid() || h.slot >= kMaxSampleSlots) return false;
  auto& slot = slots_[h.slot];
  if (slot.id.load(std::memory_order_acquire) != h.id.value ||
      !slot.ready.load(std::memory_order_acquire) || frame >= slot.frames) {
    return false;
  }

  if (slot.kind == SampleSlotKind::Resident) {
    const int16_t* pcm = slot.data.load(std::memory_order_acquire);
    if (!pcm) return false;
    out = pcm[frame];
    return true;
  }

  if (slot.kind != SampleSlotKind::Streamed || !streamingCacheReady()) {
    return false;
  }

  for (auto& page : streamPages_) {
    if (!page.ready.load(std::memory_order_acquire)) continue;

    page.readers.fetch_add(1, std::memory_order_acq_rel);
    if (!page.ready.load(std::memory_order_acquire)) {
      page.readers.fetch_sub(1, std::memory_order_acq_rel);
      continue;
    }

    const bool match = streamPageContains_(page, h.id.value, frame);
    if (match) {
      out = page.data[frame - page.startFrame];
      page.lastAccess.store(nextTime(), std::memory_order_relaxed);
    }
    page.readers.fetch_sub(1, std::memory_order_acq_rel);

    if (match) {
      streamCacheHits_.fetch_add(1, std::memory_order_relaxed);
      return true;
    }
  }

  streamCacheMisses_.fetch_add(1, std::memory_order_relaxed);
  return false;
}

bool RamSampleStore::requestFrameHandle(SampleHandle h, uint32_t frame) {
  if (!h.valid() || h.slot >= kMaxSampleSlots) return false;
  const auto& slot = slots_[h.slot];
  if (slot.id.load(std::memory_order_acquire) != h.id.value ||
      !slot.ready.load(std::memory_order_acquire) ||
      slot.kind != SampleSlotKind::Streamed || frame >= slot.frames) {
    return false;
  }

  const uint32_t pageStart =
      (frame / static_cast<uint32_t>(kSamplerStreamPageFrames)) *
      static_cast<uint32_t>(kSamplerStreamPageFrames);

  // Avoid producing work when the requested page is already published.
  for (const auto& page : streamPages_) {
    if (page.ready.load(std::memory_order_acquire) &&
        page.sampleId == h.id.value && page.startFrame == pageStart) {
      return true;
    }
  }

  const uint32_t write = streamRequestWrite_.load(std::memory_order_relaxed);
  const uint32_t read = streamRequestRead_.load(std::memory_order_acquire);

  // Deduplicate at page granularity. A voice calls this every rendered frame
  // for lookahead, but at most one pending request per sample/page is queued.
  for (uint32_t cursor = read; cursor < write; ++cursor) {
    const auto& pending =
        streamRequests_[cursor % kSamplerStreamRequestCapacity];
    if (!pending.handle.valid() ||
        pending.handle.slot != h.slot || pending.handle.id != h.id) {
      continue;
    }
    const uint32_t pendingPageStart =
        (pending.frame / static_cast<uint32_t>(kSamplerStreamPageFrames)) *
        static_cast<uint32_t>(kSamplerStreamPageFrames);
    if (pendingPageStart == pageStart) return true;
  }

  if (write - read >= static_cast<uint32_t>(kSamplerStreamRequestCapacity)) {
    streamRequestDrops_.fetch_add(1, std::memory_order_relaxed);
    return false;
  }

  auto& request = streamRequests_[write % kSamplerStreamRequestCapacity];
  request.handle = h;
  request.frame = frame;
  streamRequestWrite_.store(write + 1, std::memory_order_release);
  return true;
}

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
      return {p, slot.frames, slot.sampleRate};
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

  printf("Sample registry: refusing conflicting ID %u: %s != %s\n",
         static_cast<unsigned>(id.value),
         it->second.c_str(), path.c_str());
  return false;
}

bool RamSampleStore::bindSampleIndex(const SampleIndex* index) {
  if (index == nullptr) return false;
  sampleIndex_ = index;
  return true;
}

const char* RamSampleStore::resolvePathControl_(SampleId id) {
  if (sampleIndex_ != nullptr) {
    const SampleFileInfo* file = sampleIndex_->resolveRuntimeFile(id);
    if (file != nullptr && !file->fullPath.empty()) return file->fullPath.c_str();
  }

  std::lock_guard<std::mutex> lk(pathsMutex_);
  const auto it = filePaths_.find(id.value);
  return it == filePaths_.end() ? nullptr : it->second.c_str();
}

int RamSampleStore::findQuiescentSlot_() const {
  for (int i = 0; i < kMaxSampleSlots; ++i) {
    const auto& slot = slots_[i];
    if (slot.id.load(std::memory_order_acquire) == 0 &&
        !slot.ready.load(std::memory_order_acquire) &&
        slot.refCount.load(std::memory_order_acquire) == 0 &&
        slot.data.load(std::memory_order_acquire) == nullptr) {
      return i;
    }
  }
  return -1;
}

bool RamSampleStore::beginStreamingCache() {
  if (streamingCacheReady()) return true;

#if defined(ESP32) || defined(ESP_PLATFORM)
  streamCacheMemory_ = static_cast<int16_t*>(heap_caps_malloc(
      kSamplerStreamCacheBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
#else
  streamCacheMemory_ = static_cast<int16_t*>(std::malloc(kSamplerStreamCacheBytes));
#endif

  if (!streamCacheMemory_) {
    printf("[SAMPLER-STREAM] fixed cache allocation failed (%u bytes)\n",
           static_cast<unsigned>(kSamplerStreamCacheBytes));
    return false;
  }

  std::memset(streamCacheMemory_, 0, kSamplerStreamCacheBytes);
  for (int i = 0; i < kSamplerStreamPageCount; ++i) {
    auto& page = streamPages_[i];
    page.ready.store(false, std::memory_order_relaxed);
    page.readers.store(0, std::memory_order_relaxed);
    page.sampleId = 0;
    page.startFrame = 0;
    page.validFrames = 0;
    page.data = streamCacheMemory_ + i * kSamplerStreamPageFrames;
    page.lastAccess.store(0, std::memory_order_relaxed);
  }

  streamRequestWrite_.store(0, std::memory_order_relaxed);
  streamRequestRead_.store(0, std::memory_order_relaxed);
  printf("[SAMPLER-STREAM] fixed cache ready pages=%d pageBytes=%u total=%u\n",
         kSamplerStreamPageCount,
         static_cast<unsigned>(kSamplerStreamPageBytes),
         static_cast<unsigned>(kSamplerStreamCacheBytes));
  return true;
}

bool RamSampleStore::preload(SampleId id) {
  std::lock_guard<std::mutex> streamLock(streamControlMutex_);

  for (auto& slot : slots_) {
    if (slot.id.load(std::memory_order_acquire) == id.value &&
        slot.ready.load(std::memory_order_acquire)) {
      slot.lastAccess.store(nextTime(), std::memory_order_relaxed);
      return true;
    }
  }

  const char* path = resolvePathControl_(id);
  if (!path || *path == '\0') {
    printf("Preload: ID %u not found in registry\n", id.value);
    return false;
  }

  WavInspectResult inspected{};
  WavLoadError inspectError = WavLoadError::Ok;
  if (!inspectWavFileBounded(path, inspected,
                             std::numeric_limits<std::size_t>::max(),
                             &inspectError)) {
    printf("Preload: WAV inspection failed for %s: %s\n", path,
           wavLoadErrorName(inspectError));
    return false;
  }

  const bool resident =
      inspected.decodedBytes <= kSamplerResidentFastPathMaxBytes;
  printf("[SAMPLER-ROUTE] id=%u decodedBytes=%u sourceBytes=%u route=%s path=%s\n",
         static_cast<unsigned>(id.value),
         static_cast<unsigned>(inspected.decodedBytes),
         static_cast<unsigned>(inspected.sourceDataBytes),
         resident ? "RESIDENT" : "STREAMED", path);

  if (resident) {
    return preloadResident_(id, path, inspected);
  }
  return preloadStreamed_(id, path, inspected);
}

bool RamSampleStore::preloadResident_(SampleId id, const char* path,
                                      const WavInspectResult& inspected) {
  const std::size_t requiredSize = inspected.decodedBytes;
  if (requiredSize > maxPoolBytes_) return false;

  while (currentPoolUsage_ + requiredSize > maxPoolBytes_) {
    if (!evictOne_(true)) {
      printf("Preload: resident pool is busy; no evictable resident sample\n");
      return false;
    }
  }

  int slotIdx = findQuiescentSlot_();
  if (slotIdx < 0) {
    if (!evictOne_(false)) {
      printf("Preload: no evictable sample slot\n");
      return false;
    }
    slotIdx = findQuiescentSlot_();
  }
  if (slotIdx < 0) return false;

  int16_t* pcm = nullptr;
  const std::size_t decodeBudget = freePoolBytes();
  WavLoadError decodeError = WavLoadError::Ok;
  if (!decodeWavFileBounded(path, inspected, &pcm, decodeBudget,
                            &decodeError)) {
    printf("Preload: WAV decode failed for %s: %s\n", path,
           wavLoadErrorName(decodeError));
    return false;
  }

  const std::size_t size = inspected.decodedBytes;
  if (size > decodeBudget || currentPoolUsage_ + size > maxPoolBytes_) {
    std::free(pcm);
    return false;
  }

  auto& slot = slots_[slotIdx];
  slot.ready.store(false, std::memory_order_relaxed);
  slot.frames = inspected.info.numFrames;
  slot.sampleRate = inspected.info.sampleRate;
  slot.sizeBytes = size;
  slot.dataOffset = 0;
  slot.sourceDataBytes = 0;
  slot.sourceChannels = 1;
  slot.kind = SampleSlotKind::Resident;
  slot.data.store(pcm, std::memory_order_relaxed);
  slot.lastAccess.store(nextTime(), std::memory_order_relaxed);
  slot.refCount.store(0, std::memory_order_relaxed);
  slot.id.store(id.value, std::memory_order_relaxed);
  slot.ready.store(true, std::memory_order_release);

  currentPoolUsage_ += size;
  printf("[SAMPLER] resident id=%u frames=%u bytes=%u pool=%u/%u\n",
         static_cast<unsigned>(id.value),
         static_cast<unsigned>(slot.frames),
         static_cast<unsigned>(size),
         static_cast<unsigned>(currentPoolUsage_),
         static_cast<unsigned>(maxPoolBytes_));
  return true;
}

bool RamSampleStore::preloadStreamed_(SampleId id, const char* path,
                                      const WavInspectResult& inspected) {
  if (!streamingCacheReady()) {
    printf("[SAMPLER-STREAM] cache unavailable; cannot prepare %s\n", path);
    return false;
  }

  int slotIdx = findQuiescentSlot_();
  if (slotIdx < 0) {
    if (!evictOne_(false)) {
      printf("[SAMPLER-STREAM] no evictable descriptor slot\n");
      return false;
    }
    slotIdx = findQuiescentSlot_();
  }
  if (slotIdx < 0) return false;

  auto& slot = slots_[slotIdx];
  slot.ready.store(false, std::memory_order_relaxed);
  slot.frames = inspected.info.numFrames;
  slot.sampleRate = inspected.info.sampleRate;
  slot.sizeBytes = 0;
  slot.dataOffset = inspected.dataOffset;
  slot.sourceDataBytes = inspected.sourceDataBytes;
  slot.sourceChannels = inspected.sourceChannels;
  slot.kind = SampleSlotKind::Streamed;
  slot.data.store(nullptr, std::memory_order_relaxed);
  slot.lastAccess.store(nextTime(), std::memory_order_relaxed);
  slot.refCount.store(0, std::memory_order_relaxed);
  slot.id.store(id.value, std::memory_order_relaxed);

  invalidateStreamPages_(id.value);
  const SampleHandle handle{static_cast<uint16_t>(slotIdx), id};
  if (!loadStreamPageControl_(handle, 0)) {
    closeIoForSample_(id.value);
    resetSlot_(slot);
    printf("[SAMPLER-STREAM] initial page failed for %s\n", path);
    return false;
  }

  // Assignment owns only the descriptor and the fixed shared page cache. The
  // SD file handle used to prewarm page 0 must not become per-assignment state.
  closeIoForSample_(id.value);

  slot.ready.store(true, std::memory_order_release);
  printf("[SAMPLER-STREAM] prepared id=%u frames=%u sr=%u channels=%u\n",
         static_cast<unsigned>(id.value),
         static_cast<unsigned>(slot.frames),
         static_cast<unsigned>(slot.sampleRate),
         static_cast<unsigned>(slot.sourceChannels));
  return true;
}

int RamSampleStore::chooseStreamPageForWrite_() {
  if (!streamingCacheReady()) return -1;

  for (int i = 0; i < kSamplerStreamPageCount; ++i) {
    auto& page = streamPages_[i];
    if (!page.ready.load(std::memory_order_acquire) &&
        page.readers.load(std::memory_order_acquire) == 0) {
      return i;
    }
  }

  for (int attempt = 0; attempt < kSamplerStreamPageCount; ++attempt) {
    int candidate = -1;
    uint32_t oldest = std::numeric_limits<uint32_t>::max();
    for (int i = 0; i < kSamplerStreamPageCount; ++i) {
      auto& page = streamPages_[i];
      if (!page.ready.load(std::memory_order_acquire) ||
          page.readers.load(std::memory_order_acquire) != 0) {
        continue;
      }
      const uint32_t age = page.lastAccess.load(std::memory_order_relaxed);
      if (age < oldest) {
        oldest = age;
        candidate = i;
      }
    }
    if (candidate < 0) return -1;

    auto& page = streamPages_[candidate];
    bool expected = true;
    if (!page.ready.compare_exchange_strong(
            expected, false, std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      continue;
    }
    if (page.readers.load(std::memory_order_acquire) == 0) return candidate;
    page.ready.store(true, std::memory_order_release);
  }

  return -1;
}

void RamSampleStore::invalidateStreamPages_(uint32_t sampleId) {
  if (sampleId == 0) return;
  for (auto& page : streamPages_) {
    if (!page.ready.load(std::memory_order_acquire) ||
        page.sampleId != sampleId ||
        page.readers.load(std::memory_order_acquire) != 0) {
      continue;
    }
    bool expected = true;
    if (!page.ready.compare_exchange_strong(
            expected, false, std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      continue;
    }
    if (page.readers.load(std::memory_order_acquire) == 0) {
      page.sampleId = 0;
      page.startFrame = 0;
      page.validFrames = 0;
    } else {
      page.ready.store(true, std::memory_order_release);
    }
  }
}

int RamSampleStore::ensureIoSlot_(uint32_t sampleId, const char* path) {
  if (sampleId == 0 || !path || *path == '\0') return -1;

  for (int i = 0; i < kSamplerStreamIoHandleCount; ++i) {
    auto& io = streamIo_[i];
    if (io.sampleId != sampleId) continue;
#if defined(ARDUINO)
    if (io.file) {
#else
    if (io.file != nullptr) {
#endif
      io.lastAccess = ++streamIoClock_;
      return i;
    }
  }

  int target = -1;
  uint32_t oldest = std::numeric_limits<uint32_t>::max();
  for (int i = 0; i < kSamplerStreamIoHandleCount; ++i) {
    auto& io = streamIo_[i];
#if defined(ARDUINO)
    const bool open = static_cast<bool>(io.file);
#else
    const bool open = io.file != nullptr;
#endif
    if (!open) {
      target = i;
      break;
    }
    if (io.lastAccess < oldest) {
      oldest = io.lastAccess;
      target = i;
    }
  }
  if (target < 0) return -1;

  closeIoSlot_(target);
  auto& io = streamIo_[target];
#if defined(ARDUINO)
  io.file = SD.open(path, FILE_READ);
  if (!io.file) return -1;
#else
  io.file = std::fopen(path, "rb");
  if (!io.file) return -1;
#endif
  io.sampleId = sampleId;
  io.lastAccess = ++streamIoClock_;
  printf("[SAMPLER-STREAM-IO] open id=%u slot=%d\n",
         static_cast<unsigned>(sampleId), target);
  return target;
}

void RamSampleStore::closeIoSlot_(int index) {
  if (index < 0 || index >= kSamplerStreamIoHandleCount) return;
  auto& io = streamIo_[index];
  const uint32_t oldSampleId = io.sampleId;
#if defined(ARDUINO)
  const bool wasOpen = static_cast<bool>(io.file);
  if (io.file) io.file.close();
  io.file = File();
#else
  const bool wasOpen = io.file != nullptr;
  if (io.file) std::fclose(io.file);
  io.file = nullptr;
#endif
  io.sampleId = 0;
  io.lastAccess = 0;
  if (wasOpen) {
    printf("[SAMPLER-STREAM-IO] close id=%u slot=%d\n",
           static_cast<unsigned>(oldSampleId), index);
  }
}

void RamSampleStore::closeIoForSample_(uint32_t sampleId) {
  for (int i = 0; i < kSamplerStreamIoHandleCount; ++i) {
    if (streamIo_[i].sampleId == sampleId) closeIoSlot_(i);
  }
}

bool RamSampleStore::seekIoSlot_(int index, uint64_t absoluteOffset) {
  if (index < 0 || index >= kSamplerStreamIoHandleCount) return false;
#if defined(ARDUINO)
  if (absoluteOffset > std::numeric_limits<uint32_t>::max()) return false;
  return streamIo_[index].file.seek(static_cast<uint32_t>(absoluteOffset));
#else
  if (absoluteOffset > static_cast<uint64_t>(LONG_MAX)) return false;
  return std::fseek(streamIo_[index].file,
                    static_cast<long>(absoluteOffset), SEEK_SET) == 0;
#endif
}

std::size_t RamSampleStore::readIoSlot_(int index, uint8_t* dst,
                                        std::size_t bytes) {
  if (index < 0 || index >= kSamplerStreamIoHandleCount || !dst || bytes == 0) {
    return 0;
  }
#if defined(ARDUINO)
  return streamIo_[index].file.read(dst, bytes);
#else
  return std::fread(dst, 1, bytes, streamIo_[index].file);
#endif
}

bool RamSampleStore::loadStreamPageControl_(SampleHandle handle,
                                            uint32_t frame) {
  if (!streamingCacheReady() || !handle.valid() ||
      handle.slot >= kMaxSampleSlots) {
    return false;
  }

  auto& slot = slots_[handle.slot];
  if (slot.id.load(std::memory_order_acquire) != handle.id.value ||
      slot.kind != SampleSlotKind::Streamed || frame >= slot.frames ||
      (slot.sourceChannels != 1 && slot.sourceChannels != 2)) {
    return false;
  }

  const uint32_t pageStart =
      (frame / static_cast<uint32_t>(kSamplerStreamPageFrames)) *
      static_cast<uint32_t>(kSamplerStreamPageFrames);

  for (const auto& page : streamPages_) {
    if (page.ready.load(std::memory_order_acquire) &&
        page.sampleId == handle.id.value && page.startFrame == pageStart) {
      return true;
    }
  }

  const int pageIndex = chooseStreamPageForWrite_();
  if (pageIndex < 0) return false;
  auto& page = streamPages_[pageIndex];
  page.ready.store(false, std::memory_order_relaxed);
  page.sampleId = 0;
  page.startFrame = 0;
  page.validFrames = 0;

  const char* path = resolvePathControl_(handle.id);
  if (!path) return false;
  const int ioIndex = ensureIoSlot_(handle.id.value, path);
  if (ioIndex < 0) return false;

  const uint32_t framesToRead = std::min<uint32_t>(
      static_cast<uint32_t>(kSamplerStreamPageFrames), slot.frames - pageStart);
  const uint32_t sourceBytesPerFrame =
      static_cast<uint32_t>(slot.sourceChannels) * 2u;
  const uint64_t absoluteOffset = slot.dataOffset +
      static_cast<uint64_t>(pageStart) * sourceBytesPerFrame;

  const uint32_t startedAt = streamClockMicros();
  if (!seekIoSlot_(ioIndex, absoluteOffset)) return false;

  uint8_t scratch[512];
  uint32_t writtenFrames = 0;
  while (writtenFrames < framesToRead) {
    const uint32_t maxChunkFrames =
        static_cast<uint32_t>(sizeof(scratch)) / sourceBytesPerFrame;
    const uint32_t chunkFrames = std::min<uint32_t>(
        maxChunkFrames, framesToRead - writtenFrames);
    const std::size_t sourceBytes =
        static_cast<std::size_t>(chunkFrames) * sourceBytesPerFrame;
    if (readIoSlot_(ioIndex, scratch, sourceBytes) != sourceBytes) {
      return false;
    }

    for (uint32_t i = 0; i < chunkFrames; ++i) {
      const uint8_t* src = scratch + i * sourceBytesPerFrame;
      int32_t mono = readLe16(src);
      if (slot.sourceChannels == 2) {
        mono += readLe16(src + 2);
        mono /= 2;
      }
      page.data[writtenFrames + i] = static_cast<int16_t>(mono);
    }
    writtenFrames += chunkFrames;
  }

  const uint32_t elapsed = streamClockMicros() - startedAt;
  uint32_t previousMax = streamMaxReadMicros_.load(std::memory_order_relaxed);
  while (elapsed > previousMax &&
         !streamMaxReadMicros_.compare_exchange_weak(
             previousMax, elapsed, std::memory_order_relaxed,
             std::memory_order_relaxed)) {
  }

  page.sampleId = handle.id.value;
  page.startFrame = pageStart;
  page.validFrames = static_cast<uint16_t>(writtenFrames);
  page.lastAccess.store(nextTime(), std::memory_order_relaxed);
  page.ready.store(true, std::memory_order_release);
  streamPagesLoaded_.fetch_add(1, std::memory_order_relaxed);
  return true;
}

void RamSampleStore::serviceIo(std::size_t maxPages) {
  if (!streamingCacheReady() || maxPages == 0) return;
  std::lock_guard<std::mutex> streamLock(streamControlMutex_);

  std::size_t processed = 0;
  while (processed < maxPages) {
    const uint32_t read = streamRequestRead_.load(std::memory_order_relaxed);
    const uint32_t write = streamRequestWrite_.load(std::memory_order_acquire);
    if (read == write) break;

    const SamplerStreamRequest request =
        streamRequests_[read % kSamplerStreamRequestCapacity];
    streamRequestRead_.store(read + 1, std::memory_order_release);
    ++processed;

    if (!request.handle.valid() || request.handle.slot >= kMaxSampleSlots) continue;
    const auto& slot = slots_[request.handle.slot];
    if (slot.id.load(std::memory_order_acquire) != request.handle.id.value ||
        !slot.ready.load(std::memory_order_acquire) ||
        slot.kind != SampleSlotKind::Streamed) {
      continue;
    }
    loadStreamPageControl_(request.handle, request.frame);
  }

  // File handles are part of the active-stream working set, not assignment
  // state. Audio releases only the refCount; the control-side worker owns all
  // File destruction and reaps a handle as soon as no voice references it.
  for (int i = 0; i < kSamplerStreamIoHandleCount; ++i) {
    const uint32_t sampleId = streamIo_[i].sampleId;
    if (sampleId == 0) continue;

    bool active = false;
    for (const auto& slot : slots_) {
      if (slot.id.load(std::memory_order_acquire) == sampleId &&
          slot.ready.load(std::memory_order_acquire) &&
          slot.refCount.load(std::memory_order_acquire) != 0) {
        active = true;
        break;
      }
    }
    if (!active) closeIoSlot_(i);
  }

#if defined(ARDUINO)
  const uint32_t now = millis();
  if (now - lastStreamStatsLogMs_ >= 2000) {
    lastStreamStatsLogMs_ = now;
    const SamplerStreamStats stats = streamStats();
    if (stats.pagesLoaded != 0 || stats.starveEpisodes != 0 ||
        stats.activeStreams != 0) {
      Serial.printf(
          "[SAMPLER-STREAM] active=%u hit=%u miss=%u pages=%u reqDrop=%u starve=%u voiceDrop=%u sdMaxUs=%u\n",
          static_cast<unsigned>(stats.activeStreams),
          static_cast<unsigned>(stats.cacheHits),
          static_cast<unsigned>(stats.cacheMisses),
          static_cast<unsigned>(stats.pagesLoaded),
          static_cast<unsigned>(stats.requestDrops),
          static_cast<unsigned>(stats.starveEpisodes),
          static_cast<unsigned>(stats.voiceDrops),
          static_cast<unsigned>(stats.maxReadMicros));
    }
  }
#endif
}

SamplerStreamStats RamSampleStore::streamStats() const {
  SamplerStreamStats stats{};
  stats.cacheHits = streamCacheHits_.load(std::memory_order_relaxed);
  stats.cacheMisses = streamCacheMisses_.load(std::memory_order_relaxed);
  stats.pagesLoaded = streamPagesLoaded_.load(std::memory_order_relaxed);
  stats.requestDrops = streamRequestDrops_.load(std::memory_order_relaxed);
  stats.starveEpisodes = streamStarveEpisodes_.load(std::memory_order_relaxed);
  stats.voiceDrops = streamVoiceDrops_.load(std::memory_order_relaxed);
  stats.maxReadMicros = streamMaxReadMicros_.load(std::memory_order_relaxed);

  for (const auto& slot : slots_) {
    if (slot.ready.load(std::memory_order_acquire) &&
        slot.kind == SampleSlotKind::Streamed &&
        slot.refCount.load(std::memory_order_acquire) != 0) {
      ++stats.activeStreams;
    }
  }
  return stats;
}

void RamSampleStore::noteStreamStarve() {
  streamStarveEpisodes_.fetch_add(1, std::memory_order_relaxed);
}

void RamSampleStore::noteStreamDrop() {
  streamVoiceDrops_.fetch_add(1, std::memory_order_relaxed);
}

bool RamSampleStore::evictOne_(bool residentOnly) {
  int candidateIdx = -1;
  uint32_t oldestTime = std::numeric_limits<uint32_t>::max();

  for (int i = 0; i < kMaxSampleSlots; ++i) {
    auto& slot = slots_[i];
    if (slot.id.load(std::memory_order_acquire) == 0 ||
        !slot.ready.load(std::memory_order_acquire) ||
        slot.refCount.load(std::memory_order_acquire) != 0) {
      continue;
    }
    if (residentOnly && slot.kind != SampleSlotKind::Resident) continue;

    const uint32_t access = slot.lastAccess.load(std::memory_order_relaxed);
    if (access < oldestTime) {
      oldestTime = access;
      candidateIdx = i;
    }
  }

  if (candidateIdx < 0) return false;
  auto& slot = slots_[candidateIdx];

  bool expectedReady = true;
  if (!slot.ready.compare_exchange_strong(
          expectedReady, false,
          std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    return false;
  }

  if (slot.refCount.load(std::memory_order_acquire) != 0) {
    slot.ready.store(true, std::memory_order_release);
    return false;
  }

  const uint32_t oldId = slot.id.load(std::memory_order_acquire);
  const SampleSlotKind oldKind = slot.kind;
  const std::size_t oldSize = slot.sizeBytes;

  slot.id.store(0, std::memory_order_release);
  int16_t* ptr = const_cast<int16_t*>(
      slot.data.exchange(nullptr, std::memory_order_acq_rel));
  if (ptr) std::free(ptr);

  if (oldSize <= currentPoolUsage_) {
    currentPoolUsage_ -= oldSize;
  } else {
    currentPoolUsage_ = 0;
  }

  if (oldKind == SampleSlotKind::Streamed) {
    invalidateStreamPages_(oldId);
    closeIoForSample_(oldId);
  }

  resetSlot_(slot);
  return true;
}

void RamSampleStore::evictLRU() {
  std::lock_guard<std::mutex> streamLock(streamControlMutex_);
  (void)evictOne_(false);
}

std::size_t RamSampleStore::freePoolBytes() const {
  if (currentPoolUsage_ > maxPoolBytes_) return 0;
  return maxPoolBytes_ - currentPoolUsage_;
}
