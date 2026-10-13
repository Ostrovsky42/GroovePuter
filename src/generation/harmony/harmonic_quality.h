#ifndef GROOVEPUTER_GENERATION_HARMONY_HARMONIC_QUALITY_H
#define GROOVEPUTER_GENERATION_HARMONY_HARMONIC_QUALITY_H

#include "harmonic_types.h"
#include "generated/harmonic_vocabulary.h"

namespace GroovePuterRhythm {
enum class ChordQuality : uint8_t;
}

namespace GroovePuterHarmony {

constexpr QualityId kQualityMajor = 0;
constexpr QualityId kQualityMinor = 1;
constexpr QualityId kQualityDominant7 = 2;
constexpr QualityId kQualityDiminished = 3;
constexpr QualityId kQualityAugmented = 4;
constexpr QualityId kQualitySus4 = 5;
constexpr QualityId kQualityMinor7 = 6;
constexpr QualityId kQualityMajor7 = 7;
constexpr QualityId kQualityMinor9 = 8;
constexpr QualityId kQualityMajor9 = 9;
constexpr uint16_t kBootstrapQualityCount = 10;

inline const ChordQualityDefinition* qualityDefinition(QualityId id) {
  return id < kBootstrapQualityCount ? &kBootstrapQualityDefinitions[id] : nullptr;
}

bool legacyQualityToId(GroovePuterRhythm::ChordQuality legacy, QualityId& out);

constexpr PitchSet12 transposePitchSet(PitchSet12 set, int offset) {
  offset %= 12;
  if (offset < 0) offset += 12;
  set &= kPitchSet12Mask;
  return offset == 0
             ? set
             : static_cast<PitchSet12>(((set << offset) | (set >> (12 - offset))) &
                                       kPitchSet12Mask);
}

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_HARMONIC_QUALITY_H
