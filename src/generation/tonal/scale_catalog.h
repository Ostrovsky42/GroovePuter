#ifndef GROOVEPUTER_GENERATION_TONAL_SCALE_CATALOG_H
#define GROOVEPUTER_GENERATION_TONAL_SCALE_CATALOG_H

#include <cstdint>

namespace GroovePuterRhythm {

// Numeric ABI of the existing global ScaleType enum in scenes.h. Keep this
// lightweight catalog Scene-independent; source regressions pin the enum order.
using ScaleTypeValue = uint8_t;
constexpr ScaleTypeValue kScaleMinor = 0;
constexpr ScaleTypeValue kScaleMajor = 1;
constexpr ScaleTypeValue kScaleDorian = 2;
constexpr ScaleTypeValue kScalePhrygian = 3;
constexpr ScaleTypeValue kScaleLydian = 4;
constexpr ScaleTypeValue kScaleMixolydian = 5;
constexpr ScaleTypeValue kScaleLocrian = 6;
constexpr ScaleTypeValue kScalePentatonicMajor = 7;
constexpr ScaleTypeValue kScalePentatonicMinor = 8;
constexpr ScaleTypeValue kScaleChromatic = 9;
constexpr ScaleTypeValue kDefaultScaleTypeValue = kScaleDorian;
constexpr uint8_t kScaleTypeCount = 10;

struct ScaleDefinitionView {
  const int8_t* intervals = nullptr;
  uint8_t count = 0;
};

// Single generation-side owner of the global ScaleType interval data.
// TonalProjector and legacy AdvancedPatternGenerator consume this catalog.
// PerformanceKeyboard deliberately remains a separate live-input compatibility
// context for this Stage 15 release and is not a generated-pitch owner.
inline constexpr int8_t kScaleIntervalsMinor[] = {0, 2, 3, 5, 7, 8, 10};
inline constexpr int8_t kScaleIntervalsMajor[] = {0, 2, 4, 5, 7, 9, 11};
inline constexpr int8_t kScaleIntervalsDorian[] = {0, 2, 3, 5, 7, 9, 10};
inline constexpr int8_t kScaleIntervalsPhrygian[] = {0, 1, 3, 5, 7, 8, 10};
inline constexpr int8_t kScaleIntervalsLydian[] = {0, 2, 4, 6, 7, 9, 11};
inline constexpr int8_t kScaleIntervalsMixolydian[] = {0, 2, 4, 5, 7, 9, 10};
inline constexpr int8_t kScaleIntervalsLocrian[] = {0, 1, 3, 5, 6, 8, 10};
inline constexpr int8_t kScaleIntervalsPentatonicMajor[] = {0, 2, 4, 7, 9};
inline constexpr int8_t kScaleIntervalsPentatonicMinor[] = {0, 3, 5, 7, 10};
inline constexpr int8_t kScaleIntervalsChromatic[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

constexpr ScaleDefinitionView scaleDefinitionFor(ScaleTypeValue value) {
  switch (value) {
    case kScaleMinor: return {kScaleIntervalsMinor, 7};
    case kScaleMajor: return {kScaleIntervalsMajor, 7};
    case kScaleDorian: return {kScaleIntervalsDorian, 7};
    case kScalePhrygian: return {kScaleIntervalsPhrygian, 7};
    case kScaleLydian: return {kScaleIntervalsLydian, 7};
    case kScaleMixolydian: return {kScaleIntervalsMixolydian, 7};
    case kScaleLocrian: return {kScaleIntervalsLocrian, 7};
    case kScalePentatonicMajor: return {kScaleIntervalsPentatonicMajor, 5};
    case kScalePentatonicMinor: return {kScaleIntervalsPentatonicMinor, 5};
    case kScaleChromatic: return {kScaleIntervalsChromatic, 12};
    default: return {};
  }
}

constexpr bool isCatalogScaleTypeValue(ScaleTypeValue value) {
  return value < kScaleTypeCount;
}

constexpr int floorDivScaleDegree(int value, int divisor) {
  int quotient = value / divisor;
  const int remainder = value % divisor;
  return remainder < 0 ? quotient - 1 : quotient;
}

constexpr int scaleDegreeToSemitone(ScaleTypeValue scaleTypeValue, int degree) {
  const ScaleDefinitionView scale = scaleDefinitionFor(scaleTypeValue);
  if (scale.intervals == nullptr || scale.count == 0) return 0;
  const int octave = floorDivScaleDegree(degree, scale.count);
  const int index = degree - octave * scale.count;
  return octave * 12 + scale.intervals[index];
}

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_TONAL_SCALE_CATALOG_H
