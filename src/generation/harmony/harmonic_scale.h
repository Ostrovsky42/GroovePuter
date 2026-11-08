#ifndef GROOVEPUTER_GENERATION_HARMONY_HARMONIC_SCALE_H
#define GROOVEPUTER_GENERATION_HARMONY_HARMONIC_SCALE_H

#include "harmonic_quality.h"

namespace GroovePuterHarmony {

constexpr uint16_t kBootstrapScaleCount = 10;

inline const ScaleDefinition* scaleDefinition(ScaleId id) {
  return id < kBootstrapScaleCount ? &kBootstrapScaleDefinitions[id] : nullptr;
}

bool legacyScaleToId(uint8_t legacyScaleType, ScaleId& out);

inline bool legacyScaleToId(uint8_t legacyScaleType, ScaleId& out) {
  static constexpr ScaleId kLegacyScaleMap[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  if (legacyScaleType >= sizeof(kLegacyScaleMap) / sizeof(kLegacyScaleMap[0])) return false;
  out = kLegacyScaleMap[legacyScaleType];
  return true;
}

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_HARMONIC_SCALE_H
