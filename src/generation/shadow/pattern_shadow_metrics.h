#pragma once

#include <cstdint>

#include "../materialization/pattern_materializer.h"

namespace GroovePuterRhythm {

enum ShadowTargetFlags : uint8_t {
  ShadowDrums = 1u << 0,
  ShadowSynthA = 1u << 1,
  ShadowSynthB = 1u << 2,
};

constexpr uint8_t kAllShadowTargets =
    ShadowDrums | ShadowSynthA | ShadowSynthB;

struct ShadowPatternMetrics {
  uint16_t legacyOnsets = 0;
  uint16_t vocabularyOnsets = 0;
  uint16_t sharedOnsets = 0;
  uint16_t differingSlots = 0;
  uint16_t legacyAccents = 0;
  uint16_t vocabularyAccents = 0;
  uint16_t sharedAccents = 0;
};

// Pure observer: compares a materialized vocabulary candidate with the already
// produced legacy output. It never mutates either side and intentionally
// ignores velocity/timing/FX so Stage 4 measures rhythmic topology first.
ShadowPatternMetrics compareShadowPatterns(
    const DrumPatternSet& legacyDrums,
    const SynthPattern& legacySynthA,
    const SynthPattern& legacySynthB,
    const MaterializedPatterns& vocabulary,
    uint8_t targetFlags = kAllShadowTargets);

}  // namespace GroovePuterRhythm
