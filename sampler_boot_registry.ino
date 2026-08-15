#include "src/platform/cardputer_sd.h"
#include "src/sampler/sample_scene_persistence.h"

#if defined(ESP32) || defined(ESP_PLATFORM) || defined(ARDUINO)
#include <esp_heap_caps.h>
#endif

namespace {

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

void prepareSamplerRegistryAfterSdMount() {
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
