#include "harmonic_role_policy.h"

#include "harmonic_quality.h"
#include "harmonic_scale.h"

namespace GroovePuterHarmony {
namespace {

bool validContext(const TonalContext& tonal) {
  if (tonal.tonicPc > 11 || tonal.homeTonicPc > 11) return false;
  const ScaleDefinition* scale = scaleDefinition(tonal.scaleId);
  const ScaleDefinition* home = scaleDefinition(tonal.homeScaleId);
  return scale != nullptr && home != nullptr &&
         tonal.scaleMask == transposePitchSet(scale->intervalMask, tonal.tonicPc) &&
         tonal.homeScaleMask ==
             transposePitchSet(home->intervalMask, tonal.homeTonicPc);
}

bool validChord(const HarmonicState& chord) {
  if (chord.rootPc > 11 ||
      (chord.bassPc != kNoBassPc && chord.bassPc > 11)) return false;
  const ChordQualityDefinition* quality = qualityDefinition(chord.qualityId);
  return quality != nullptr &&
         chord.pitchMask == transposePitchSet(quality->intervalMask,
                                              chord.rootPc);
}

}  // namespace

HarmonyStatus rolePitchPolicy(const HarmonicState& chord,
                             const TonalContext& tonal,
                             HarmonicRole role,
                             PitchSet12 forbidden,
                             RolePitchPolicy& out) {
  if (!validChord(chord) || !validContext(tonal) ||
      static_cast<uint8_t>(role) >= static_cast<uint8_t>(HarmonicRole::Count) ||
      (forbidden & ~kPitchSet12Mask) != 0) {
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
