#include <cassert>
#include <cstdint>

#include "src/generation/harmony/harmonic_role_policy.h"
#include "src/generation/harmony/harmonic_quality.h"
#include "src/generation/harmony/harmonic_scale.h"
#include "src/generation/harmony/pitch_set.h"

using namespace GroovePuterHarmony;

int main() {
  constexpr ScaleId kMinorScale = 0;  // Explicit bootstrap registry ID.
  const ScaleDefinition* cMinor = scaleDefinition(kMinorScale);
  const ChordQualityDefinition* g7 = qualityDefinition(kQualityDominant7);
  assert(cMinor != nullptr && g7 != nullptr);
  const TonalContext tonal{
      kMinorScale, kMinorScale, cMinor->intervalMask, cMinor->intervalMask, 0, 0};
  const HarmonicState chord{
      kQualityDominant7, transposePitchSet(g7->intervalMask, 7), 7, kNoBassPc};

  RolePitchPolicy bass{};
  assert(rolePitchPolicy(chord, tonal, HarmonicRole::Bass, 0, bass) ==
         HarmonyStatus::Ready);
  const PitchSet12 rootAndFifth = static_cast<PitchSet12>(
      (1u << 7) | (1u << 2));
  assert(bass.allowed == chord.pitchMask);
  assert(bass.preferred == rootAndFifth);

  RolePitchPolicy lead{};
  assert(rolePitchPolicy(chord, tonal, HarmonicRole::Lead, 0, lead) ==
         HarmonyStatus::Ready);
  assert((lead.allowed & (1u << 11)) != 0);  // B is the chord's major seventh.
  assert((lead.preferred & (1u << 11)) != 0);
  assert(lead.allowed == static_cast<PitchSet12>(tonal.scaleMask | chord.pitchMask));

  RolePitchPolicy chordRole{};
  const PitchSet12 forbidRoot = static_cast<PitchSet12>(1u << 7);
  assert(rolePitchPolicy(chord, tonal, HarmonicRole::Chord, forbidRoot,
                         chordRole) == HarmonyStatus::Ready);
  assert((chordRole.allowed & forbidRoot) == 0);
  assert((chordRole.preferred & forbidRoot) == 0);
  assert((chordRole.preferred & ~chordRole.allowed) == 0);

  RolePitchPolicy unchanged{};
  unchanged.allowed = 0x123;
  unchanged.preferred = 0x045;
  assert(rolePitchPolicy(chord, tonal, HarmonicRole::Count, 0, unchanged) ==
         HarmonyStatus::Invalid);
  assert(unchanged.allowed == 0x123 && unchanged.preferred == 0x045);
}
