#include "src/platform/cardputer_sd.h"

namespace {

void prepareSamplerRegistryAfterSdMount() {
  // C establishes control-side identity/registry before MiniAcid::init() is
  // allowed to restore Scene sampler pads. No PCM is loaded here.
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

  if (!bind.clean()) {
    Serial.println(
        "[SAMPLER-REGISTRY] WARN ambiguous/conflicting sample identity rejected");
  }
}

__attribute__((constructor)) void installSamplerBootRegistryHook() {
  GroovePuterPlatform::setCardputerSdReadyHook(
      prepareSamplerRegistryAfterSdMount);
}

}  // namespace
