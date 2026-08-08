#pragma once

#include <cstdint>

#include "../../dsp/mini_drumvoices.h"
#include "../rhythm/rhythm_realizer.h"
#include "../../../scenes.h"

namespace GroovePuterRhythm {
namespace Audition {

struct MaterializeOptions {
  bool bassEnabled = false;
  int8_t bassNote = 36;  // C2, fixed pitch so Stage 3A tests rhythm only.
};

// Stage 3A is intentionally one-bar and role-level. It does not bind
// ChordRhythm/MelodicRhythm, does not map gate classes to synth articulation,
// and does not mutate Scene state. The caller owns where these temporary
// patterns are installed.
bool materializeOneBar(const RhythmPhrasePlan& plan,
                       const MaterializeOptions& options,
                       DrumPatternSet& drums,
                       SynthPattern& synthA,
                       SynthPattern& synthB);

}  // namespace Audition
}  // namespace GroovePuterRhythm
