#pragma once
#ifndef GROOVEPUTER_GENERATED_PATTERN_HARMONY_ORIGIN_H
#define GROOVEPUTER_GENERATED_PATTERN_HARMONY_ORIGIN_H

#include <cstdint>

#include "material_identity.h"
#include "material_version.h"
#include "../generation/harmony/harmonic_types.h"
#include "../generation/roles/chord_progression.h"
#include "../generation/roles/harmonic_rhythm.h"
#include "../generation/tonal/scale_catalog.h"

namespace GroovePuterMaterial {

// Session evidence from the one-bar generator. The exact Pattern version is
// checked on every read, so later edits cannot inherit its old harmony.
struct GeneratedPatternHarmonyOrigin {
  MaterialAddress address{};
  MaterialVersionToken version{};
  uint32_t sourceId = 0; // session-local identity; legacy Pattern slots may have no MaterialId
  uint32_t publicationGeneration = 0;
  GroovePuterRhythm::StepMask onsets = 0;
  GroovePuterRhythm::HarmonicEvent events[GroovePuterRhythm::kMaxHarmonicEvents]{};
  uint8_t rootPitchClass = 0;
  GroovePuterRhythm::ScaleTypeValue scaleTypeValue =
      GroovePuterRhythm::kDefaultScaleTypeValue;
  uint8_t eventCount = 0;
  GroovePuterHarmony::HarmonicAuthority authority =
      GroovePuterHarmony::HarmonicAuthority::Unknown;

  bool valid() const {
    uint8_t onsetCount = 0;
    for (uint8_t step = 0; step < 16; ++step)
      if ((onsets & GroovePuterRhythm::stepBit(step)) != 0) ++onsetCount;
    return address.voice == 0 && sourceId != 0 && version.valid() &&
           publicationGeneration != 0 &&
           rootPitchClass < 12 &&
           GroovePuterRhythm::isCatalogScaleTypeValue(scaleTypeValue) &&
           eventCount > 0 && eventCount <= GroovePuterRhythm::kMaxHarmonicEvents &&
           onsetCount == eventCount;
  }
};

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_GENERATED_PATTERN_HARMONY_ORIGIN_H
