#ifndef GROOVEPUTER_GENERATION_HARMONY_HARMONIC_SNAPSHOT_H
#define GROOVEPUTER_GENERATION_HARMONY_HARMONIC_SNAPSHOT_H

#include "harmonic_scale.h"

namespace GroovePuterHarmony {

inline bool sameHarmonyEvent(const HarmonyEventKey& a, const HarmonyEventKey& b) {
  return a.materialId == b.materialId &&
         a.publicationGeneration == b.publicationGeneration &&
         a.transportEpoch == b.transportEpoch && a.eventOrdinal == b.eventOrdinal;
}

inline bool validateHarmonySnapshot(const HarmonySnapshot& snapshot) {
  if (snapshot.status != HarmonyStatus::Ready || snapshot.key.materialId == 0 ||
      snapshot.key.publicationGeneration == 0 || snapshot.durationSteps == 0 ||
      snapshot.base.rootPc > 11 ||
      (snapshot.base.bassPc != kNoBassPc && snapshot.base.bassPc > 11)) {
    return false;
  }
  const ChordQualityDefinition* quality = qualityDefinition(snapshot.base.qualityId);
  if (quality == nullptr || (snapshot.base.pitchMask & ~kPitchSet12Mask) != 0 ||
      snapshot.base.pitchMask != transposePitchSet(quality->intervalMask,
                                                    snapshot.base.rootPc)) {
    return false;
  }
  if (snapshot.tonal.tonicPc > 11 || snapshot.tonal.homeTonicPc > 11 ||
      (snapshot.tonal.scaleMask & ~kPitchSet12Mask) != 0 ||
      (snapshot.tonal.homeScaleMask & ~kPitchSet12Mask) != 0) {
    return false;
  }
  const ScaleDefinition* scale = scaleDefinition(snapshot.tonal.scaleId);
  const ScaleDefinition* homeScale = scaleDefinition(snapshot.tonal.homeScaleId);
  if (scale == nullptr || homeScale == nullptr ||
      snapshot.tonal.scaleMask != transposePitchSet(scale->intervalMask,
                                                     snapshot.tonal.tonicPc) ||
      snapshot.tonal.homeScaleMask != transposePitchSet(homeScale->intervalMask,
                                                         snapshot.tonal.homeTonicPc)) {
    return false;
  }
  for (uint8_t i = 0; i < static_cast<uint8_t>(HarmonicRole::Count); ++i) {
    const RolePitchPolicy& role = snapshot.roles[i];
    if (((role.allowed | role.preferred) & ~kPitchSet12Mask) != 0 ||
        (role.preferred & ~role.allowed) != 0) {
      return false;
    }
  }
  return true;
}

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_HARMONIC_SNAPSHOT_H
