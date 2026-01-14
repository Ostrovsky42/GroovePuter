#pragma once
#include <cstdint>
#include <atomic>
#include <cstddef>
#include "../audio/audio_config.h"

// Unique identifier for a sample file (hash of path/name)
struct SampleId { 
  uint32_t value; 
  
  bool operator==(const SampleId& other) const { return value == other.value; }
  bool operator!=(const SampleId& other) const { return value != other.value; }
};

// Metadata for a sample file
struct WavInfo {
  uint32_t sampleRate;
  uint16_t channels;
  uint16_t bitsPerSample;
  uint32_t numFrames;
};

// Lightweight view into audio data for the audio thread.
// POD type - safe to copy.
struct SampleView {
  const int16_t* pcm;   // pointer to data in pool
  uint32_t frames;      // valid length
  uint32_t sampleRate;  // original rate
  
  bool empty() const { return pcm == nullptr || frames == 0; }
};

// Handle returned by acquireHandle - binds voice to a specific slot
struct SampleHandle {
  uint16_t slot = 0xFFFF;  // 0xFFFF = invalid
  SampleId id = {0};
  
  bool valid() const { return slot != 0xFFFF && id.value != 0; }
  static SampleHandle invalid() { return {0xFFFF, {0}}; }
};

// Abstract interface for the Sample Store "Warehouse"
class ISampleStore {
public:
  virtual ~ISampleStore() = default;

  // Main Thread: Request to load a sample into RAM.
  // Returns true if loaded (or already in RAM).
  virtual bool preload(SampleId id) = 0;
  
  // === Handle-based API (preferred for audio thread) ===
  
  // Audio Thread: Acquire a handle to a slot. 
  // Returns valid handle if sample is loaded, invalid otherwise.
  // Increases refCount on the slot.
  virtual SampleHandle acquireHandle(SampleId id) = 0;
  
  // Audio Thread: Release the handle.
  // Decreases refCount on the slot.
  virtual void releaseHandle(SampleHandle h) = 0;

  // Audio Thread: Get direct view of data by handle.
  // O(1), no search, guaranteed not to block.
  virtual SampleView viewHandle(SampleHandle h) const = 0;
  
  // === Legacy ID-based API (deprecated, kept for compatibility) ===
  
  // Audio Thread: Acquire by ID (searches slots)
  virtual void acquire(SampleId id) = 0;
  
  // Audio Thread: Release by ID (searches slots)
  virtual void release(SampleId id) = 0;

  // Audio Thread: Get view by ID (searches slots)
  virtual SampleView view(SampleId id) const = 0;
  
  // Main Thread: Unload samples from RAM that have refCount == 0
  virtual void evictLRU() = 0;
  
  // Debug/Stats
  virtual std::size_t freePoolBytes() const = 0;
  virtual void setPoolSize(std::size_t bytes) = 0;
};
