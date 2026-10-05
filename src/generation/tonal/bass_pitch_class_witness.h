#ifndef GROOVEPUTER_GENERATION_TONAL_BASS_PITCH_CLASS_WITNESS_H
#define GROOVEPUTER_GENERATION_TONAL_BASS_PITCH_CLASS_WITNESS_H

#include <cstdint>
#include <type_traits>

#include "../rhythm/rhythm_types.h"
#include "tonal_materializer.h"

namespace GroovePuterRhythm {

// D1-B1: exact pitch class (0..11) of every bass attack of one bar, packed as
// 16 nibbles (one per step) in two 32-bit words (8 bytes, 4-byte aligned).
//
// Semantics: nibble(step) is meaningful iff the bar's BassRhythmPlan.onsets
// contains that step; every other nibble carries no meaning. There is no
// sentinel pitch class -- the rhythm plan is the validity mask. Continuation
// steps are NOT attacks. This is owner-derived origin evidence, never a
// statement about any later Pattern.
struct BassPitchClassWitness {
  uint32_t words[2]{0, 0};

  // Fails closed (returns false, leaves the witness unchanged) on step >= 16
  // or pitchClass >= 12.
  bool setPitchClass(uint8_t step, uint8_t pitchClass) {
    if (step >= kStepsPerBar || pitchClass >= 12) return false;
    const uint8_t word = static_cast<uint8_t>(step >> 3);
    const uint8_t shift = static_cast<uint8_t>((step & 7u) * 4u);
    words[word] = (words[word] & ~(0xFu << shift)) |
                  (static_cast<uint32_t>(pitchClass) << shift);
    return true;
  }

  // Returns 0xFF for step >= 16 (never a valid pitch class).
  uint8_t pitchClassAt(uint8_t step) const {
    if (step >= kStepsPerBar) return 0xFF;
    const uint8_t word = static_cast<uint8_t>(step >> 3);
    const uint8_t shift = static_cast<uint8_t>((step & 7u) * 4u);
    return static_cast<uint8_t>((words[word] >> shift) & 0xFu);
  }

  friend bool operator==(const BassPitchClassWitness& a,
                         const BassPitchClassWitness& b) {
    return a.words[0] == b.words[0] && a.words[1] == b.words[1];
  }
  friend bool operator!=(const BassPitchClassWitness& a,
                         const BassPitchClassWitness& b) {
    return !(a == b);
  }
};

static_assert(kStepsPerBar == 16, "witness packs exactly 16 nibbles");
static_assert(sizeof(BassPitchClassWitness) == 8,
              "BassPitchClassWitness must stay 8 bytes");
static_assert(std::is_trivially_copyable<BassPitchClassWitness>::value,
              "BassPitchClassWitness must stay fixed-capacity");

// Builds the witness from the exact tonal materialization plan that constructs
// Synth A (onsetSteps[]/midiNotes[]). Fails closed -- returning false and an
// unchanged output -- if the plan is malformed or its attack steps are not
// exactly `expectedOnsets` (the BassRhythmPlan onsets), so a witness can never
// silently disagree with the rhythm evidence it is paired with.
inline bool makeBassPitchClassWitness(const TonalMaterializationPlan& plan,
                                      StepMask expectedOnsets,
                                      BassPitchClassWitness& out) {
  if (plan.onsetCount > kStepsPerBar) return false;
  BassPitchClassWitness witness{};
  StepMask seen = 0;
  for (uint8_t i = 0; i < plan.onsetCount; ++i) {
    const uint8_t step = plan.onsetSteps[i];
    if (step >= kStepsPerBar || (seen & stepBit(step)) != 0) return false;
    seen = static_cast<StepMask>(seen | stepBit(step));
    if (!witness.setPitchClass(step, static_cast<uint8_t>(plan.midiNotes[i] % 12))) {
      return false;
    }
  }
  if (seen != expectedOnsets) return false;
  out = witness;
  return true;
}

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_TONAL_BASS_PITCH_CLASS_WITNESS_H
