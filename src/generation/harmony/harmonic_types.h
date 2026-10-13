#ifndef GROOVEPUTER_GENERATION_HARMONY_HARMONIC_TYPES_H
#define GROOVEPUTER_GENERATION_HARMONY_HARMONIC_TYPES_H

#include <cstdint>
#include <type_traits>

namespace GroovePuterHarmony {

using PitchSet12 = uint16_t;
using QualityId = uint16_t;
using ScaleId = uint16_t;
using RelationTags = uint16_t;

constexpr PitchSet12 kPitchSet12Mask = 0x0fffu;
constexpr uint8_t kNoBassPc = 255;

enum class SectionIntent : uint8_t { Take, Keep, Develop, Break, Return };
enum class HarmonyStatus : uint8_t {
  NoHarmony = 0,
  Ready = 1,
  Unsupported,
  Stale,
  Forbidden,
  Invalid,
  CapacityExceeded,
  ReturnUnreachable,
};
// Provenance of the owner that determined the final harmonic meaning of a
// generated material event. Unknown is intentionally the zero/default value.
enum class HarmonicAuthority : uint8_t { Unknown = 0, Generic, Idiom, Graph };
enum class HarmonicRole : uint8_t { Bass, Chord, Lead, Count };
enum class QualityFamily : uint8_t { MajorMinor, Dominant, Diminished, Augmented, Suspended };
enum class ScaleFamily : uint8_t { Major, Minor, Modes, Pentatonic, Chromatic };

struct ChordQualityDefinition {
  QualityId id = 0;
  PitchSet12 intervalMask = 0;
  uint8_t cardinality = 0;
  QualityFamily family = QualityFamily::MajorMinor;
  PitchSet12 triadicSkeleton = 0;
  uint8_t complexityRank = 0;
};

struct ScaleDefinition {
  ScaleId id = 0;
  PitchSet12 intervalMask = 0;
  uint8_t cardinality = 0;
  ScaleFamily family = ScaleFamily::Modes;
  uint8_t modeId = 0;
  uint8_t parentId = 255;
};

struct HarmonicState {
  QualityId qualityId = 0;
  PitchSet12 pitchMask = 0;
  uint8_t rootPc = 0;
  uint8_t bassPc = kNoBassPc;
};

struct TonalContext {
  ScaleId scaleId = 0;
  ScaleId homeScaleId = 0;
  PitchSet12 scaleMask = 0;
  PitchSet12 homeScaleMask = 0;
  uint8_t tonicPc = 0;
  uint8_t homeTonicPc = 0;
};

struct RolePitchPolicy {
  PitchSet12 allowed = 0;
  PitchSet12 preferred = 0;
};

struct HarmonyEventKey {
  uint32_t materialId = 0;
  uint32_t publicationGeneration = 0;
  uint32_t transportEpoch = 0;
  uint32_t eventOrdinal = 0;
};

struct HarmonySnapshot {
  HarmonyEventKey key{};
  uint64_t onsetStep = 0;
  uint16_t durationSteps = 0;
  RolePitchPolicy roles[static_cast<uint8_t>(HarmonicRole::Count)]{};
  HarmonicState base{};
  TonalContext tonal{};
  HarmonyStatus status = HarmonyStatus::NoHarmony;
};

static_assert(std::is_trivially_copyable<HarmonySnapshot>::value,
              "HarmonySnapshot must remain a bounded value type");
static_assert(sizeof(HarmonyEventKey) == 16, "HarmonyEventKey layout budget changed");
static_assert(sizeof(HarmonySnapshot) <= 64, "HarmonySnapshot exceeded its bounded budget");

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_HARMONIC_TYPES_H
