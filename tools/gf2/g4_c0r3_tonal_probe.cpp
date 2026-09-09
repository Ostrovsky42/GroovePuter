#include "g4_c0r3_tonal_probe.h"

#include "../../src/generation/migration/tonal_pattern_adapter.h"

namespace GroovePuterRhythm {

// The production tonal_pattern_adapter.cpp is compiled for the C0R3 research
// binary with its public symbol renamed to this function. The wrapper below
// therefore sees the exact TonalMaterializationPlan handed to the real adapter
// without changing production source or reconstructing semantics from Pattern.
TonalPatternAdaptStatus g4C0R3RealAdaptTonalPlanToSynthPattern(
    const SynthPattern& compatibilitySource,
    const TonalMaterializationPlan& tonalPlan,
    StepMask accentOnsets,
    StepMask slideIntoOnsets,
    SynthPattern& destination,
    const uint8_t* sourceOrder,
    uint8_t sourceOrderCount);

namespace G4C0R3 {
namespace {
TonalProbeSnapshot g_snapshot{};
}

void resetTonalProbe() {
  g_snapshot = TonalProbeSnapshot{};
}

TonalProbeSnapshot tonalProbeSnapshot() {
  return g_snapshot;
}

void capture(const TonalMaterializationPlan& plan) {
  if (g_snapshot.callCount >= kMaxCapturedTonalPlans) {
    g_snapshot.overflow = true;
    return;
  }
  g_snapshot.plans[g_snapshot.callCount++] = plan;
}

}  // namespace G4C0R3

TonalPatternAdaptStatus adaptTonalPlanToSynthPattern(
    const SynthPattern& compatibilitySource,
    const TonalMaterializationPlan& tonalPlan,
    StepMask accentOnsets,
    StepMask slideIntoOnsets,
    SynthPattern& destination,
    const uint8_t* sourceOrder,
    uint8_t sourceOrderCount) {
  G4C0R3::capture(tonalPlan);
  return g4C0R3RealAdaptTonalPlanToSynthPattern(
      compatibilitySource,
      tonalPlan,
      accentOnsets,
      slideIntoOnsets,
      destination,
      sourceOrder,
      sourceOrderCount);
}

}  // namespace GroovePuterRhythm
