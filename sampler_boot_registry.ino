#include "src/platform/cardputer_sd.h"

namespace {

void prepareSamplerRegistryAfterSdMount() {
  // C establishes the registry before Scene restore. D keeps that lifecycle
  // and publishes the same SampleIndex as the persistence identity authority.
  // No PCM is loaded here.
  g_miniAcidInstance.sampleStore = &g_sampleStore;

  auto& index = g_miniAcidInstance.sampleIndex;
  index.scanDirectory("/sd/samples");
  if (index.getFiles().empty()) {
    index.scanDirectory("/samples");
  }

  const SampleRegistryBindResult bind = index.bindToStore(g_sampleStore);
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
