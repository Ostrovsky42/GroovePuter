#ifndef GROOVEPUTER_TOOLS_GF2_G4_C0R3_TONAL_PROBE_H
#define GROOVEPUTER_TOOLS_GF2_G4_C0R3_TONAL_PROBE_H

#include "../../src/generation/tonal/tonal_materializer.h"

#include <cstdint>

namespace GroovePuterRhythm {
namespace G4C0R3 {

constexpr uint8_t kMaxCapturedTonalPlans = 3;

struct TonalProbeSnapshot {
  TonalMaterializationPlan plans[kMaxCapturedTonalPlans]{};
  uint8_t callCount = 0;
  bool overflow = false;
};

void resetTonalProbe();
TonalProbeSnapshot tonalProbeSnapshot();

}  // namespace G4C0R3
}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_TOOLS_GF2_G4_C0R3_TONAL_PROBE_H
