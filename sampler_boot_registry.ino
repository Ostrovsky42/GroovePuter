#include "src/platform/cardputer_sd.h"
#include "src/sampler/sample_scene_persistence.h"

#if defined(ESP32) || defined(ESP_PLATFORM) || defined(ARDUINO)
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace {

#if defined(ESP32) || defined(ESP_PLATFORM) || defined(ARDUINO)
// SD.open() enters a substantially deeper Arduino/FS call chain than idle
// serviceIo(). Hardware tracing measured only 1300 bytes free before the first
// refill; loadStreamPageControl_ then consumes about 624 bytes before SD.open,
// overflowing the former 3072-byte task stack on transport START.
constexpr uint32_t kSamplerIoTaskStackBytes = 4096;
constexpr UBaseType_t kSamplerIoTaskPriority = 1;
constexpr BaseType_t kSamplerIoTaskCore = 0;
TaskHandle_t g_samplerIoTaskHandle = nullptr;
#endif

void logSamplerRegistryHeap(const char* phase) {
#if defined(ESP32) || defined(ESP_PLATFORM) || defined(ARDUINO)
  const std::size_t freeInternal =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const std::size_t largestInternal =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  Serial.printf(
      "[SAMPLER-REGISTRY] heap phase=%s free8=%u largest8=%u\n",
      phase ? phase : "?", static_cast<unsigned>(freeInternal),
      static_cast<unsigned>(largestInternal));
#else
  (void)phase;
#endif
}

#if defined(ESP32) || defined(ESP_PLATFORM) || defined(ARDUINO)
void samplerIoTask(void*) {
  while (true) {
    // One bounded page per turn keeps SD work out of the UI/input loop while
    // yielding between reads to the higher-priority USB dispatcher and peers.
    g_sampleStore.serviceIo(1);
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

bool startSamplerIoTask() {
  if (g_samplerIoTaskHandle != nullptr) return true;

  const uint32_t freeBefore =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const uint32_t largestBefore =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  const BaseType_t created = xTaskCreatePinnedToCore(
      samplerIoTask,
      "SamplerIoTask",
      kSamplerIoTaskStackBytes,
      nullptr,
      kSamplerIoTaskPriority,
      &g_samplerIoTaskHandle,
      kSamplerIoTaskCore);
  if (created != pdPASS || g_samplerIoTaskHandle == nullptr) {
    g_samplerIoTaskHandle = nullptr;
    Serial.printf(
        "[SAMPLER-IO] task create failed result=%d freeInt=%u largest=%u\n",
        static_cast<int>(created), static_cast<unsigned>(freeBefore),
        static_cast<unsigned>(largestBefore));
    return false;
  }

  Serial.printf(
      "[SAMPLER-IO] task ready stackBytes=%u core=%d priority=%u freeInt=%u largest=%u\n",
      static_cast<unsigned>(kSamplerIoTaskStackBytes),
      static_cast<int>(kSamplerIoTaskCore),
      static_cast<unsigned>(kSamplerIoTaskPriority),
      static_cast<unsigned>(
          heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
      static_cast<unsigned>(heap_caps_get_largest_free_block(
          MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
  return true;
}
#else
bool startSamplerIoTask() { return true; }
#endif

void prepareSamplerRegistryAfterSdMount() {
  // Reserve the fixed streaming page cache before catalog/UI allocations
  // fragment internal RAM. This is control-side setup; no PCM file is loaded.
  logSamplerRegistryHeap("before-stream-cache");
  if (!g_sampleStore.beginStreamingCache()) {
    Serial.println(
        "[SAMPLER-STREAM] WARN fixed cache unavailable; ordinary one-shots will reject");
  }
  logSamplerRegistryHeap("after-stream-cache");

  // Reserve the refill-worker stack before the 172-file catalog and UI fragment
  // DRAM. The worker has no page requests yet, so starting it before the scan
  // cannot touch the filesystem until a streamed voice actually asks for data.
  if (!startSamplerIoTask()) {
    Serial.println(
        "[SAMPLER-IO] WARN refill worker unavailable; streamed playback will starve");
  }
  logSamplerRegistryHeap("after-stream-worker");

  // C establishes the registry before Scene restore. D keeps that lifecycle
  // and publishes the same SampleIndex as the persistence identity authority.
  // No PCM is loaded here.
  g_miniAcidInstance.sampleStore = &g_sampleStore;

  logSamplerRegistryHeap("before-scan");

  auto& index = g_miniAcidInstance.sampleIndex;
  index.scanDirectory("/sd/samples");
  if (index.getFiles().empty()) {
    index.scanDirectory("/samples");
  }

  logSamplerRegistryHeap("after-scan");

  GroovePuterSampler::setScenePersistenceSampleIndex(&index);
  const SampleRegistryBindResult bind = index.bindToStore(g_sampleStore);

  logSamplerRegistryHeap("after-bind");

  Serial.printf(
      "[SAMPLER-REGISTRY] ready discovered=%u registered=%u stableReject=%u legacyReject=%u storeReject=%u\n",
      static_cast<unsigned>(bind.discovered),
      static_cast<unsigned>(bind.registered),
      static_cast<unsigned>(bind.rejectedStable),
      static_cast<unsigned>(bind.rejectedLegacy),
      static_cast<unsigned>(bind.rejectedStore));

  if (bind.rejectedStable != 0 || bind.rejectedStore != 0 ||
      bind.registered != bind.discovered) {
    Serial.println(
        "[SAMPLER-REGISTRY] WARN stable/runtime sample ownership rejected");
  }
  if (bind.rejectedLegacy != 0) {
    Serial.println(
        "[SAMPLER-REGISTRY] NOTE ambiguous legacy IDs require stable Scene refs");
  }
}

__attribute__((constructor)) void installSamplerBootRegistryHook() {
  GroovePuterPlatform::setCardputerSdReadyHook(
      prepareSamplerRegistryAfterSdMount);
}

}  // namespace
