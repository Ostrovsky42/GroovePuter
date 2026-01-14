#pragma once
#include "sample_store.h"
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <array>
#include <map>

// Fixed size pool slots to avoid dynamic allocation and map lookups in audio thread
static constexpr int kMaxSampleSlots = 64;
// default: 256KB, can be changed via setPoolSize

struct SampleSlot {
  std::atomic<uint32_t> id{0};      // 0 = empty
  std::atomic<bool> ready{false};   // true after data is fully published
  std::atomic<const int16_t*> data{nullptr};
  uint32_t frames = 0;
  uint32_t sampleRate = 0;
  std::size_t sizeBytes = 0;
  std::atomic<uint32_t> refCount{0};
  std::atomic<uint32_t> lastAccess{0};
};

class RamSampleStore : public ISampleStore {
public:
  RamSampleStore();
  
  // --- Audio Thread Interface (Lock-Free) ---
  // Handle-based API (preferred)
  SampleHandle acquireHandle(SampleId id) override;
  void releaseHandle(SampleHandle h) override;
  SampleView viewHandle(SampleHandle h) const override;
  
  // Legacy ID-based API (deprecated)
  void acquire(SampleId id) override;
  void release(SampleId id) override;
  SampleView view(SampleId id) const override;

  // --- Main Thread Interface ---
  bool preload(SampleId id) override;
  void evictLRU() override;
  std::size_t freePoolBytes() const override;
  void setPoolSize(std::size_t bytes) { maxPoolBytes_ = bytes; }

  // Helpers
  void registerFile(SampleId id, const std::string& path);

protected:
  uint32_t nextTime();

  // Slots: accessible by both threads
  std::array<SampleSlot, kMaxSampleSlots> slots_;
  
  // Main thread only state
  std::mutex pathsMutex_;
  std::map<uint32_t, std::string> filePaths_; // ID -> Path
  std::size_t currentPoolUsage_;
  std::size_t maxPoolBytes_;
  std::atomic<uint32_t> timeCounter_;
};
