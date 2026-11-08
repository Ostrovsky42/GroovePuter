#ifndef GROOVEPUTER_GENERATION_HARMONY_HARMONIC_SOURCE_H
#define GROOVEPUTER_GENERATION_HARMONY_HARMONIC_SOURCE_H

#include <cstdint>
#include <type_traits>

#include "harmonic_types.h"
#include "../roles/chord_progression.h"

namespace GroovePuterHarmony {

enum class HarmonicStrategy : uint8_t { Legacy = 0, Graph = 1 };
constexpr uint8_t kMaxPreparedGraphEvents = 32;

struct MaterializerCapabilityView {
  uint32_t qualityMask = 0;
  uint32_t scaleMask = 0;
  uint16_t rootMaskByHomeScale[10]{};
  uint8_t supportsSlashBass = 0;
};

struct PreparedGraphHarmony {
  uint32_t materialId = 0;
  uint32_t publicationGeneration = 0;
  uint16_t modelVersion = 0;
  HarmonicAuthority authority = HarmonicAuthority::Unknown;
  uint8_t eventCount = 0;
  ResolvedHarmonicEvent events[kMaxPreparedGraphEvents]{};
};
static_assert(sizeof(PreparedGraphHarmony) <= 672,
              "prepared graph harmony exceeded its bounded memory budget");
static_assert(std::is_trivially_copyable<PreparedGraphHarmony>::value,
              "prepared graph harmony must remain a fixed value object");

struct HarmonicSourceView {
  HarmonicStrategy strategy = HarmonicStrategy::Legacy;
  const GroovePuterRhythm::ChordProgressionSource* legacySource = nullptr;
  const PreparedGraphHarmony* graphSource = nullptr;
};

MaterializerCapabilityView materializerCapabilityView();
HarmonyStatus projectHarmonicState(
    const HarmonicState& state, const TonalContext& context,
    GroovePuterRhythm::HarmonicEvent& out);
HarmonyStatus harmonicEventAt(const HarmonicSourceView& source,
                              uint32_t globalHarmonicOrdinal,
                              GroovePuterRhythm::HarmonicEvent& out);

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_HARMONIC_SOURCE_H
