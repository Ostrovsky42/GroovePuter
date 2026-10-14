#ifndef GROOVEPUTER_GENERATION_HARMONY_HARMONIC_ROLE_POLICY_H
#define GROOVEPUTER_GENERATION_HARMONY_HARMONIC_ROLE_POLICY_H

#include "harmonic_types.h"

namespace GroovePuterHarmony {

// Computes the bounded pitch vocabulary a live/generated role may use for one
// resolved chord. `forbidden` is a hard capability mask and is subtracted from
// both sets; callers own any genre-specific decisions that produced it.
HarmonyStatus rolePitchPolicy(const HarmonicState& chord,
                             const TonalContext& tonal,
                             HarmonicRole role,
                             PitchSet12 forbidden,
                             RolePitchPolicy& out);

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_HARMONIC_ROLE_POLICY_H
