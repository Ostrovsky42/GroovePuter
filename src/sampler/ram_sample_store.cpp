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

// External WAV loader. The store supplies its current pool budget so metadata
// admission can reject oversized decoded PCM before allocation or data read.
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

  // 3. Parse metadata and load only if the final decoded mono PCM fits the
  // whole sampler pool. Oversized files fail before PCM allocation/data read.
  WavInfo info{};
  int16_t* pcm = nullptr;
  if (!loadWavFileBounded(path.c_str(), info, &pcm, maxPoolBytes_)) {
    printf("Preload: loadWavFile failed for %s\n", path.c_str());
    return false;
  }

  const std::size_t size = info.numFrames * sizeof(int16_t);
  printf("Preload: Loaded %u frames (%u bytes). Pool usage: %u/%u\n",
         info.numFrames, static_cast<unsigned>(size),
         static_cast<unsigned>(currentPoolUsage_),
         static_cast<unsigned>(maxPoolBytes_));

  // Defensive invariant: bounded loader must never return an allocation that
  // is larger than the store's pool admission budget.
  if (size > maxPoolBytes_) {
    printf("Preload: Sample is larger than the entire pool\n");
    free(pcm);
    return false;
  }

  // 4. Evict until enough capacity exists. Abort when eviction makes no
  // progress all candidates are then referenced by active audio voices.
  while (currentPoolUsage_ + size > maxPoolBytes_) {
    const std::size_t usageBefore = currentPoolUsage_;
    evictLRU();
    if (currentPoolUsage_ >= usageBefore) {
      printf("Preload: Pool is busy; no evictable sample slots\n");
      free(pcm);
      return false;
    }
  }

  // 5. Find a fully quiescent empty slot. A withdrawn slot may temporarily
  // retain a rollback reference from an acquisition that lost a race.
  int slotIdx = -1;
  for (int i = 0; i < kMaxSampleSlots; ++i) {
    auto& slot = slots_[i];
    if (slot.id.load(std::memory_order_acquire) == 0 &&
        \ÛÝœ™XYK›ØY
ÝŽ›Y[[ÜžWÛÜ™\—ØXÜ]Z\™JH	‰‚ˆÛÝœ™YÛÝ[›ØY
ÝŽ›Y[[ÜžWÛÜ™\—ØXÜ]Z\™JHOH	‰‚ˆÛÝ™]K›ØY
ÝŽ›Y[[ÜžWÛÜ™\—ØXÜ]Z\™JHOH[ŠHÂˆÛÝYHNÂˆœ™XZÎÂˆBˆB‚ˆYˆ
ÛÝY
HÂˆš[Š”™[ØYˆ›È]ZY\ØÙ[œ™YHÛÝ×ˆŠNÂˆœ™YJÛJNÂˆ™]\›ˆ˜[ÙNÂˆB‚ˆËÈ‹ˆš[[™X›\ÚHÛÝˆ›Û‹X]ÛZXÈY]Y]H\È›ÝXÝYžHBˆËÈš[˜[™XYH™[X\ÙH[™X]Ú[™ÈXÜ]Z\™H[ˆ™XY\œË‚ˆ]]ÉˆÛÝHÛÝ×ÖÜÛÝYNÂˆÛÝ™œ˜[Y\ÈH[™›Ë›[Qœ˜[Y\ÎÂˆÛÝœØ[\T˜]HH[™›ËœØ[\T˜]NÂˆÛÝœÚ^™Pž]\ÈHÚ^™NÂˆÛÝ™]KœÝÜ™JÛKÝŽ›Y[[ÜžWÛÜ™\—Ü™[^Y
NÂˆÛÝ›\ÝXØÙ\ÜËœÝÜ™J™^[YJ
KÝŽ›Y[[ÜžWÛÜ™\—Ü™[^Y
NÂˆÛÝœ™YÛÝ[œÝÜ™JÝŽ›Y[[ÜžWÛÜ™\—Ü™[^Y
NÂˆÛÝšYœÝÜ™JY˜[YKÝŽ›Y[[ÜžWÛÜ™\—Ü™[^Y
NÂˆÛÝœ™XYKœÝÜ™JYKÝŽ›Y[[ÜžWÛÜ™\—Ü™[X\ÙJNÂ‚ˆÝ\œ™[ÛÛ\ØYÙWÈ
ÏHÚ^™NÂˆ™]\›ˆYNÂŸB‚›ÚY˜[TØ[\TÝÜ™NŽ™]šXÝ•J
HÂˆ[Ø[™Y]RYHLNÂˆZ[Ì—ÝÛ\Ý[YHHÝŽ›[Y\šX×Û[Z]ÏZ[Ì—ÝŽŽ›X^

NÂ‚ˆ›Üˆ
[HHÈHÓX^Ø[\TÛÝÎÈ
ÊÚJHÂˆ]]ÉˆÛÝHÛÝ×ÖÚWNÂˆÛÛœÝZ[Ì—ÝYHÛÝšY›ØY
ÝŽ›Y[[ÜžWÛÜ™\—ØXÜ]Z\™JNÂˆYˆ
YOH	‰ˆÛÝœ™XYK›ØY
ÝŽ›Y[[ÜžWÛÜ™\—ØXÜ]Z\™JH	‰‚ˆÛÝœ™YÛÝ[›ØY
ÝŽ›Y[[ÜžWÛÜ™\—ØXÜ]Z\™JHOH
HÂˆÛÛœÝZ[Ì—ÝXØÙ\ÜÈHÛÝ›\ÝXØÙ\ÜË›ØY
ÝŽ›Y[[ÜžWÛÜ™\—Ü™[^Y
NÂˆYˆ
XØÙ\ÜÈÛ\Ý[YJHÂˆÛ\Ý[YHHXØÙ\ÜÎÂˆØ[™Y]RYHNÂˆBˆBˆB‚ˆYˆ
Ø[™Y]RY
H™]\›ŽÂ‚ˆ]]ÉˆÛÝHÛÝ×ÖØØ[™Y]RYNÂ‚ˆËÈÚ]˜]ÈX›XØ][Ûˆš\œÝˆ[žHXÜ]Z\Ú][Ûˆ]ØœÙ\™YHÛ™XYBˆËÈ˜[YH]\Ý[˜Ü™[Y[[™[ˆ™]˜[Y]H]™Y›Ü™H™]\›š[™ÈH[™K‚ˆ›ÛÛ^XÝY™XYHHYNÂˆYˆ
\ÛÝœ™XYK˜ÛÛ\\™WÙ^Ú[™ÙWÜÝ›Û™Êˆ^XÝY™XYK˜[ÙKˆÝŽ›Y[[ÜžWÛÜ™\—ØXÜWÜ™[ˆÝŽ›Y[[ÜžWÛÜ™\—ØXÜ]Z\™JJHÂˆ™]\›ŽÂˆB‚ˆYˆ
ÛÝœ™YÛÝ[›ØY
ÝŽ›Y[[ÜžWÛÜ™\—ØXÜ]Z\™JHOH
HÂˆÛÝœ™XYKœÝÜ™JYKÝŽ›Y[[ÜžWÛÜ™\—Ü™[X\ÙJNÂˆ™]\›ŽÂˆB‚ˆÛÝšYœÝÜ™JÝŽ›Y[[ÜžWÛÜ™\—Ü™[X\ÙJNÂˆ[M—Ý
ˆˆHÛÛœÝØØ\Ý[M—Ý
ŠˆÛÝ™]K™^Ú[™ÙJ[‹ÝŽ›Y[[ÜžWÛÜ™\—ØXÜWÜ™[
JNÂˆYˆ
ŠHœ™YJŠNÂ‚ˆYˆ
ÛÝœÚ^™Pž]\ÈHÝ\œ™[ÛÛ\ØYÙWÊHÂˆÝ\œ™[ÛÛ\ØYÙWÈOHÛÝœÚ^™Pž]\ÎÂˆH[ÙHÂˆÝ\œ™[ÛÛ\ØYÙWÈHÂˆBˆÛÝ™œ˜[Y\ÈHÂˆÛÝœØ[\T˜]HHÂˆÛÝœÚ^™Pž]\ÈHÂŸB‚œÝŽœÚ^™WÝ˜[TØ[\TÝÜ™NŽ™œ™YTÛÛž]\Ê
HÛÛœÝÂˆYˆ
Ý\œ™[ÛÛ\ØYÙWÈˆX^ÛÛž]\×ÊH™]\›ˆÂˆ™]\›ˆX^ÛÛž]\×ÈHÝ\œ™[ÛÛ\ØYÙWÎÂŸB