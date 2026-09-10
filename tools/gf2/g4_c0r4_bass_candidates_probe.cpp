// Research-only observation of the production bass family candidate table.
//
// The table is intentionally not copied here. Include the production owner in
// this translation unit, rename its public symbols to avoid colliding with the
// normally linked production object, then query its private candidatesFor()
// directly in the same translation unit.
#define isValidBassRhythmId g4C0R4ProbeIsValidBassRhythmId
#define isBassRhythmCompatibleWithFamily \
  g4C0R4ProbeIsBassRhythmCompatibleWithFamily
#define realizeBassRhythm g4C0R4ProbeRealizeBassRhythm
#define bassRhythmName g4C0R4ProbeBassRhythmName
#include "../../src/generation/roles/bass_rhythm.cpp"
#undef bassRhythmName
#undef realizeBassRhythm
#undef isBassRhythmCompatibleWithFamily
#undef isValidBassRhythmId

#include "g4_c0r4_bass_candidates_probe.h"

namespace GroovePuterRhythm {
namespace G4C0R4 {

uint16_t nativeBassCandidateMask(RhythmFamily family) {
  const BassCandidates candidates = candidatesFor(family);
  uint16_t result = 0;
  for (uint8_t index = 0; index < candidates.count; ++index) {
    const uint8_t ordinal = static_cast<uint8_t>(candidates.values[index]);
    if (ordinal < 16u) {
      result = static_cast<uint16_t>(result | (uint16_t{1} << ordinal));
    }
  }
  return result;
}

}  // namespace G4C0R4
}  // namespace GroovePuterRhythm
