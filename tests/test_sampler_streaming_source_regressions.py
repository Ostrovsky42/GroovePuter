#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

store_h = (ROOT / "src/sampler/ram_sample_store.h").read_text()
store_cpp = (ROOT / "src/sampler/ram_sample_store.cpp").read_text()
voice_h = (ROOT / "src/sampler/sampler_voice.h").read_text()
voice_cpp = (ROOT / "src/sampler/sampler_voice.cpp").read_text()
boot = (ROOT / "sampler_boot_registry.ino").read_text()
browser = (ROOT / "src/ui/pages/sampler_page.cpp").read_text()
pool = (ROOT / "src/sampler/sampler_pool.h").read_text()
track = (ROOT / "src/sampler/drum_sampler_track.h").read_text()
encoder = (ROOT / "miniacid_encoder8.cpp").read_text()


def section(text: str, start: str, end: str) -> str:
    begin = text.find(start)
    assert begin >= 0, f"missing section start: {start}"
    finish = text.find(end, begin + len(start))
    assert finish >= 0, f"missing section end: {end}"
    return text[begin:finish]


# Fixed, bounded V1 cache. Logical polyphony/pad count stay intact.
assert "kSamplerStreamPageBytes = 512" in store_h
assert "kSamplerStreamPageFrames =" in store_h
assert "kSamplerStreamPageCount = 8" in store_h
assert "kSamplerStreamRequestCapacity = 16" in store_h
assert "kSamplerStreamIoHandleCount = 4" in store_h
assert "kSamplerResidentFastPathMaxBytes = 2048" in store_h
assert "kMaxVoices = 8" in pool
assert "kNumPads = 16" in track

# Audio-facing streamed reads/requests may touch fixed atomics/pages only.
read_frame = section(
    store_cpp,
    "bool RamSampleStore::readFrameHandle",
    "bool RamSampleStore::requestFrameHandle",
)
request_frame = section(
    store_cpp,
    "bool RamSampleStore::requestFrameHandle",
    "void RamSampleStore::acquire",
)
for forbidden in (
    "SD.open",
    "fopen(",
    "fread(",
    "fseek(",
    "malloc(",
    "free(",
    "new ",
    "delete ",
    "lock_guard",
    "streamControlMutex_",
):
    assert forbidden not in read_frame, f"audio read contains {forbidden}"
    assert forbidden not in request_frame, f"audio request contains {forbidden}"

# Filesystem work belongs to the control-side page loader/service.
control_io = section(
    store_cpp,
    "int RamSampleStore::ensureIoSlot_",
    "SamplerStreamStats RamSampleStore::streamStats",
)
assert "SD.open" in control_io or "std::fopen" in control_io
assert "loadStreamPageControl_" in control_io
assert "void RamSampleStore::serviceIo" in control_io

# The worker and explicit control-plane preload/eviction serialize page/File
# mutation without changing the lock-free audio contract.
assert "std::mutex streamControlMutex_" in store_h
preload = section(
    store_cpp,
    "bool RamSampleStore::preload(SampleId id)",
    "bool RamSampleStore::preloadResident_",
)
service_io = section(
    store_cpp,
    "void RamSampleStore::serviceIo",
    "SamplerStreamStats RamSampleStore::streamStats",
)
evict_lru = section(
    store_cpp,
    "void RamSampleStore::evictLRU()",
    "std::size_t RamSampleStore::freePoolBytes",
)
assert "streamControlMutex_" in preload
assert "streamControlMutex_" in service_io
assert "streamControlMutex_" in evict_lru

# Request generation is page-deduplicated and bounded.
assert "pendingPageStart == pageStart" in request_frame
assert "write - read >= static_cast<uint32_t>(kSamplerStreamRequestCapacity)" in request_frame

# Voice layer retains the resident pointer fast path and adds cache-only stream use.
assert "const int16_t* pcm_" in voice_h
assert "bool streamed_" in voice_h
assert "store.readFrameHandle" in voice_h
assert "store.requestFrameHandle" in voice_h
assert "kStreamDropFrames" in voice_h
assert "source.storage == SampleStorageKind::Streamed" in voice_cpp

# Prefetch follows 256-frame page boundaries and only runs once per entered
# page. Two pages is the fixed V1 horizon; page/cache tuning remains a later
# hardware gate, not part of the scheduling fix.
assert "kStreamPageFrames = 256" in voice_h
assert "kStreamPrefetchPages = 2" in voice_h
assert "lastStreamPageStart_" in voice_h
assert "requestStreamWindow_(store, frame)" in voice_h
assert "kLookAheadFrames" not in voice_h

# Streaming cache and the 4096-byte worker stack are reserved before catalog/UI
# fragmentation. ESP-IDF dynamic task stack size is specified in bytes.
cache_pos = boot.find("beginStreamingCache")
worker_call_pos = boot.find("if (!startSamplerIoTask())")
scan_pos = boot.find("index.scanDirectory")
assert cache_pos >= 0 and worker_call_pos >= 0 and scan_pos >= 0
assert cache_pos < worker_call_pos < scan_pos
assert 'logSamplerRegistryHeap("before-stream-cache")' in boot
assert 'logSamplerRegistryHeap("after-stream-cache")' in boot
assert 'logSamplerRegistryHeap("after-stream-worker")' in boot
assert "kSamplerIoTaskStackBytes = 4096" in boot
assert "kSamplerIoTaskPriority = 1" in boot
assert "kSamplerIoTaskCore = 0" in boot
assert '"SamplerIoTask"' in boot
assert "xTaskCreatePinnedToCore" in boot
assert "g_sampleStore.serviceIo(1)" in boot
assert "vTaskDelay(pdMS_TO_TICKS(1))" in boot

# The Cardputer UI/input/Encoder8 poll must never synchronously perform sampler
# SD refills again. This is the hardware regression that caused system-wide lag.
encoder_update = section(
    encoder,
    "void Encoder8Miniacid::update()",
    "void Encoder8Miniacid::setInitialColors()",
)
assert "serviceIo" not in encoder_update
assert "sampleStore" not in encoder_update
assert "if (!sensor_initialized_) return" in encoder_update

# The existing nested folder browser and stable selection path must survive.
for required in (
    "SAMPLE BROWSER",
    "indexedSubdirectories",
    "filesInDirectory",
    "browserGoParent",
    "activateSampleBrowserSelection",
    "calculateStableRef(candidate.fullPath)",
):
    assert required in browser, f"folder-browser regression: {required}"

print("sampler streaming source regressions: PASS")
