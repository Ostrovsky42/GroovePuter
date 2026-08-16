#pragma once
#include <cstdint>
#include <atomic>
#include <cstddef>
#include <string>
#include "../audio/audio_config.h"

// Compact runtime identifier used by the lock-free audio path.
// Persisted/stable file identity is handled separately by SampleRef.
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

// Lightweight view into resident audio data for the audio thread.
// POD type - safe to copy.
struct SampleView {
  const int16_t* pcm;   // pointer to resident data; nullptr for streamed data
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

enum class SampleStorageKind : uint8_t {
  None = 0,
  Resident = 1,
  Streamed = 2,
};

// Metadata required by SamplerVoice without exposing filesystem or cache
// ownership. For resident sources, pcm is still obtained through viewHandle().
struct SampleSourceInfo {
  uint32_t frames = 0;
  uint32_t sampleRate = 0;
  SampleStorageKind storage = SampleStorageKind::None;

  bool valid() const {
    return frames != 0 && sampleRate != 0 && storage != SampleStorageKind::None;
  }
};

struct SamplerStreamStats {
  uint32_t cacheHits = 0;
  uint32_t cacheMisses = 0;
  uint32_t pagesLoaded = 0;
  uint32_t requestDrops = 0;
  uint32_t starveEpisodes = 0;
  uint32_t voiceDrops = 0;
  uint32_t maxReadMicros = 0;
  uint32_t activeStreams = 0;
};

class SampleIndex;

// Abstract interface for the Sample Store "Warehouse"
class ISampleStore {
public:
  virtual ~ISampleStore() = default;

  // Control Thread: Register one validated runtime ID -> physical path binding.
  // Implementations must fail closed when an existing ID is rebound to a
  // different path. Default false keeps lightweight test stores source-compatible.
  virtual bool registerFile(SampleId id, const std::string& path) {
    (void)id;
    (void)path;
    return false;
  }

  // Control Thread: Borrow the session-stable sample index as the path
  // resolver. Stores that support this avoid copying every catalog path into
  // a second registry. The index must outlive the store binding.
  virtual bool bindSampleIndex(const SampleIndex* index) {
    (void)index;
    return false;
  }

  // Main Thread: Request to prepare a sample for playback.
  // Implementations may choose resident PCM or a bounded streamed source.
  virtual bool preload(SampleId id) = 0;

  // === Handle-based API (preferred for audio thread) ===

  // Audio Thread: Acquire a handle to a slot.
  // Returns valid handle if sample is prepared, invalid otherwise.
  // Increases refCount on the slot.
  virtual SampleHandle acquireHandle(SampleId id) = 0;

  // Audio Thread: Release the handle.
  // Decreases refCount on the slot.
  virtual void releaseHandle(SampleHandle h) = 0;

  // Audio Thread: Get direct view of resident data by handle.
  // O(1), no search, guaranteed not to block. Streamed sources return pcm=null.
  virtual SampleView viewHandle(SampleHandle h) const = 0;

  // Audio Thread: Get immutable playback metadata for a prepared source.
  // Default implementation keeps existing resident-only test stores compatible.
  virtual SampleSourceInfo sourceInfoHandle(SampleHandle h) const {
    const SampleView view = viewHandle(h);
    if (view.empty()) return {};
    return {view.frames, view.sampleRate, SampleStorageKind::Resident};
  }

  // Audio Thread: Read exactly one source frame. Default implementation is the
  // resident path. Stream-capable stores override this with lock-free cache reads.
  virtual bool readFrameHandle(SampleHandle h, uint32_t frame, int16_t& out) {
    const SampleView view = viewHandle(h);
    if (view.empty() || frame >= view.frames) return false;
    out = view.pcm[frame];
    return true;
  }

  // Audio Thread: Hint/request that a source frame should be cache-resident.
  // Must never perform filesystem I/O, allocate, free, or block.
  virtual bool requestFrameHandle(SampleHandle h, uint32_t frame) {
    (void)h;
    (void)frame;
    return false;
  }

  // Control Thread: Reserve the fixed streaming cache while internal heap is
  // still contiguous. Resident-only stores may treat this as a no-op success.
  virtual bool beginStreamingCache() { return true; }

  // Control Thread: Service queued streaming page requests. Filesystem I/O is
  // owned here, never by the audio callback.
  virtual void serviceIo(std::size_t maxPages = 4) { (void)maxPages; }

  virtual SamplerStreamStats streamStats() const { return {}; }
  virtual void noteStreamStarve() {}
  virtual void noteStreamDrop() {}

  // === Legacy ID-based API (deprecated, kept for compatibility) ===

  // Audio Thread: Acquire by ID (searches slots)
  virtual void acquire(SampleId id) = 0;

  // Audio Thread: Release by ID (searches slots)
  virtual void release(SampleId id) = 0;

  // Audio Thread: Get resident view by ID (searches slots)
  virtual SampleView view(SampleId id) const = 0;

  // Main Thread: Unload samples from RAM that have refCount == 0
  virtual void evictLRU() = 0;

  // Debug/Stats for resident PCM budget
  virtual std::size_t freePoolBytes() const = 0;
  virtual void setPoolSize(std::size_t bytes) = 0;
};
