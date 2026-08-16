#pragma once
#include "sample_store.h"
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <array>
#include <map>
#include <cstdio>

#if defined(ARDUINO)
#include <SD.h>
#endif

// Fixed size pool slots to avoid dynamic allocation and map lookups in audio thread.
static constexpr int kMaxSampleSlots = 64;

// Stream V1 is deliberately bounded. Eight 512-byte pages provide a 4 KiB
// working set: enough for double-buffered research with up to four concurrent
// streamed voices while preserving eight logical SamplerPool voices.
static constexpr std::size_t kSamplerStreamPageBytes = 512;
static constexpr std::size_t kSamplerStreamPageFrames =
    kSamplerStreamPageBytes / sizeof(int16_t);
static constexpr int kSamplerStreamPageCount = 8;
static constexpr std::size_t kSamplerStreamCacheBytes =
    kSamplerStreamPageBytes * kSamplerStreamPageCount;
static constexpr int kSamplerStreamRequestCapacity = 16;
static constexpr int kSamplerStreamIoHandleCount = 4;

// Keep genuinely tiny samples on the old resident fast path. Ordinary musical
// one-shots are streamed so they never require one large contiguous PCM block.
static constexpr std::size_t kSamplerResidentFastPathMaxBytes = 2048;

enum class SampleSlotKind : uint8_t {
  Empty = 0,
  Resident = 1,
  Streamed = 2,
};

struct SampleSlot {
  std::atomic<uint32_t> id{0};      // 0 = empty
  std::atomic<bool> ready{false};   // true after metadata/data is fully published
  std::atomic<const int16_t*> data{nullptr};
  uint32_t frames = 0;
  uint32_t sampleRate = 0;
  std::size_t sizeBytes = 0;        // resident PCM bytes only
  uint64_t dataOffset = 0;          // streamed WAV data chunk offset
  uint32_t sourceDataBytes = 0;
  uint16_t sourceChannels = 0;
  SampleSlotKind kind = SampleSlotKind::Empty;
  std::atomic<uint32_t> refCount{0};
  std::atomic<uint32_t> lastAccess{0};
};

struct SamplerStreamPage {
  std::atomic<bool> ready{false};
  std::atomic<uint32_t> readers{0};
  uint32_t sampleId = 0;
  uint32_t startFrame = 0;
  uint16_t validFrames = 0;
  int16_t* data = nullptr;
  std::atomic<uint32_t> lastAccess{0};
};

struct SamplerStreamRequest {
  SampleHandle handle{};
  uint32_t frame = 0;
};

struct SamplerStreamIoSlot {
  uint32_t sampleId = 0;
  uint32_t lastAccess = 0;
#if defined(ARDUINO)
  File file;
#else
  std::FILE* file = nullptr;
#endif
};

class RamSampleStore : public ISampleStore {
public:
  RamSampleStore();
  ~RamSampleStore() override;

  // --- Audio Thread Interface (Lock-Free) ---
  SampleHandle acquireHandle(SampleId id) override;
  void releaseHandle(SampleHandle h) override;
  SampleView viewHandle(SampleHandle h) const override;
  SampleSourceInfo sourceInfoHandle(SampleHandle h) const override;
  bool readFrameHandle(SampleHandle h, uint32_t frame, int16_t& out) override;
  bool requestFrameHandle(SampleHandle h, uint32_t frame) override;

  // Legacy ID-based API (deprecated)
  void acquire(SampleId id) override;
  void release(SampleId id) override;
  SampleView view(SampleId id) const override;

  // --- Control/Main Thread Interface ---
  bool registerFile(SampleId id, const std::string& path) override;
  bool bindSampleIndex(const SampleIndex* index) override;
  bool preload(SampleId id) override;
  bool beginStreamingCache() override;
  void serviceIo(std::size_t maxPages = 4) override;
  SamplerStreamStats streamStats() const override;
  void noteStreamStarve() override;
  void noteStreamDrop() override;
  void evictLRU() override;
  std::size_t freePoolBytes() const override;
  void setPoolSize(std::size_t bytes) override { maxPoolBytes_ = bytes; }

  bool streamingCacheReady() const { return streamCacheMemory_ != nullptr; }
  std::size_t streamingCacheBytes() const {
    return streamingCacheReady() ? kSamplerStreamCacheBytes : 0;
  }

protected:
  uint32_t nextTime();

  // Slots: accessible by both threads.
  std::array<SampleSlot, kMaxSampleSlots> slots_;

  // Main-thread path ownership. On Cardputer #283 normally binds the stable
  // SampleIndex and leaves filePaths_ empty; the map remains for compatibility.
  std::mutex pathsMutex_;
  std::map<uint32_t, std::string> filePaths_;
  const SampleIndex* sampleIndex_ = nullptr;  // borrowed; session lifetime

  std::size_t currentPoolUsage_;
  std::size_t maxPoolBytes_;
  std::atomic<uint32_t> timeCounter_;

private:
  int findQuiescentSlot_() const;
  bool evictOne_(bool residentOnly);
  void resetSlot_(SampleSlot& slot);
  const char* resolvePathControl_(SampleId id);

  bool preloadResident_(SampleId id, const char* path,
                        const struct WavInspectResult& inspected);
  bool preloadStreamed_(SampleId id, const char* path,
                        const struct WavInspectResult& inspected);

  bool streamPageContains_(const SamplerStreamPage& page,
                           uint32_t sampleId, uint32_t frame) const;
  bool loadStreamPageControl_(SampleHandle handle, uint32_t frame);
  int chooseStreamPageForWrite_();
  void invalidateStreamPages_(uint32_t sampleId);

  int ensureIoSlot_(uint32_t sampleId, const char* path);
  void closeIoSlot_(int index);
  void closeIoForSample_(uint32_t sampleId);
  bool seekIoSlot_(int index, uint64_t absoluteOffset);
  std::size_t readIoSlot_(int index, uint8_t* dst, std::size_t bytes);

  // Audio never takes this mutex. It serializes filesystem/cache mutation
  // between explicit preload/eviction calls and the dedicated refill worker.
  std::mutex streamControlMutex_;

  std::array<SamplerStreamPage, kSamplerStreamPageCount> streamPages_{};
  int16_t* streamCacheMemory_ = nullptr;

  std::array<SamplerStreamRequest, kSamplerStreamRequestCapacity> streamRequests_{};
  std::atomic<uint32_t> streamRequestWrite_{0};
  std::atomic<uint32_t> streamRequestRead_{0};

  std::array<SamplerStreamIoSlot, kSamplerStreamIoHandleCount> streamIo_{};
  uint32_t streamIoClock_ = 0;

  std::atomic<uint32_t> streamCacheHits_{0};
  std::atomic<uint32_t> streamCacheMisses_{0};
  std::atomic<uint32_t> streamPagesLoaded_{0};
  std::atomic<uint32_t> streamRequestDrops_{0};
  std::atomic<uint32_t> streamStarveEpisodes_{0};
  std::atomic<uint32_t> streamVoiceDrops_{0};
  std::atomic<uint32_t> streamMaxReadMicros_{0};

#if defined(ARDUINO)
  uint32_t lastStreamStatsLogMs_ = 0;
#endif
};
