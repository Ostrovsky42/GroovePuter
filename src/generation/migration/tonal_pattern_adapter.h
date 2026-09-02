#ifndef GROOVEPUTER_GENERATION_MIGRATION_TONAL_PATTERN_ADAPTER_H
#define GROOVEPUTER_GENERATION_MIGRATION_TONAL_PATTERN_ADAPTER_H

#include <cstdint>

#include "../../../scenes.h"
#include "../tonal/tonal_materializer.h"

namespace GroovePuterRhythm {

enum class TonalPatternAdaptStatus : uint8_t {
  Ok = 0,
  InvalidPlan,
  Count,
};

TonalPatternAdaptStatus adaptTonalPlanToSynthPattern(
    const SynthPattern& compatibilitySource,
    const TonalMaterializationPlan& tonalPlan,
    StepMask accentOnsets,
    StepMask slideIntoOnsets,
    SynthPattern& destination,
    const uint8_t* sourceOrder = nullptr,
    uint8_t sourceOrderCount = 0);

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_MIGRATION_TONAL_PATTERN_ADAPTER_H
