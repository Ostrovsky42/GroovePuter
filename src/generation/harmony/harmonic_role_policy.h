#ifndef GROOVEPUTER_GENERATION_HARMONY_HARMONIC_ROLE_POLICY_H
#define GROOVEPUTER_GENERATION_HARMONY_HARMONIC_ROLE_POLICY_H

#include "harmonic_types.h"
#include "harmonic_quality.h"
#include "harmonic_scale.h"

namespace GroovePuterHarmony {

// Computes the bounded pitch vocabulary a live/generated role may use for one
// resolved chord. `forbidden` is a hard capability mask and is subtracted from
// both sets; callers own any genre-specific decisions that produced it.
inline HarmonyStatus rolePitchPolicy(const HarmonicState& chord,
                                     const TonalContext& tonal,
                                     HarmonicRole role,
                                     PitchSet12 forbidden,
                                     RolePitchPolicy& out) {
  if (chord.rootPc > 11 ||
      (chord.bassPc != kNoBassPc && chord.bassPc > 11) ||
      tonal.tonicPc > 11 || tonal.homeTonicPc > 11 ||
      static_cast<uint8_t>(role) >= static_cast<uint8_t>(HarmonicRole::Count) ||
      (forbidden & ~kPitchSet12Mask) != 0) {
    return HarmonyStatus::Invalid;
  }
  const ChordQualityDefinition* quality = qualityDefinition(chord.qualityId);
  const ScaleDefinition* scale = scaleDefinition(tonal.scaleId);
  const ScaleDefinition* home = scaleDefinition(tonal.homeScaleId);
  if (quality == nullptr || scale == nullptr || home == nullptr ||
      chord.pitchMask != transposePitchSet(quality->intervalMask, chord.rootPc) ||
      tonal.scaleMask != transposePitchSet(scale->intervalMask, tonal.tonicPc) ||
      tonal.homeScaleMask !=
          transposePitchSet(home->intervalMask, tonal.homeTonicPc)) {
    return HarmonyStatus::Invalid;
  }

  PitchSet12 allowed = chord.pitchMask;
  PitchSet12 preferred = chord.pitchMask;
  switch (role) {
    case HarmonicRole::Bass:
      preferred = static_cast<PitchSet12>(
          (1u << chord.rootPc) | (1u << ((chord.rootPc + 7u) % 12u)));
      break;
    case HarmonicRole::Chord:
      break;
    case HarmonicRole::Lead:
      allowed = static_cast<PitchSet12>(tonal.scaleMask | chord.pitchMask);
      break;
    case HarmonicRole::Count:
      return HarmonyStatus::Invalid;
  }
  allowed = static_cast<PitchSet12>(allowed & ~forbidden & kPitchSet12Mask);
  preferred = static_cast<PitchSet12>(preferred & allowed);
  const RolePitchPolicy result{allowed, preferred};
  out = result;
  return HarmonyStatus::Ready;
}

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_HARMONIC_ROLE_POLICY_H
