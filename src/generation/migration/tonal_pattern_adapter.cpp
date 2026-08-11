#include "tonal_pattern_adapter.h"

namespace GroovePuterRhythm {

TonalPatternAdaptStatus adaptTonalPlanToSynthPattern(
    const SynthPattern& compatibilitySource,
    const TonalMaterializationPlan& tonalPlan,
    StepMask accentOnsets,
    StepMask slideIntoOnsets,
    SynthPattern& destination,
    const uint8_t* sourceOrder,
    uint8_t sourceOrderCount) {
  (void)compatibilitySource;
  (void)tonalPlan;
  (void)accentOnsets;
  (void)slideIntoOnsets;
  (void)destination;
  (void)sourceOrder;
  (void)sourceOrderCount;
  return TonalPatternAdaptStatus::InvalidPlan;
}

}  // namespace GroovePuterRhythm
