#ifndef GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_HARMONIC_POLICY_H
#define GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_HARMONIC_POLICY_H

#include <cstdint>

namespace GroovePuterRhythm {

// Append-only. Zero-initialized selection preserves the accepted F08 clock.
enum class PhraseHarmonicPolicyId : uint8_t {
  HalfBar = 0,
  Static,
  Slow,
  Prolong,
  Syncopated,
  Count,
};

}  // namespace GroovePuterRhythm
#endif  // GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_HARMONIC_POLICY_H
