#ifndef GROOVEPUTER_GENERATION_HARMONY_PITCH_SET_H
#define GROOVEPUTER_GENERATION_HARMONY_PITCH_SET_H

#include <cstdint>

#include "harmonic_types.h"

namespace GroovePuterHarmony {

struct IntervalClassVector {
  uint8_t counts[6]{};
};

// Exactly one byte per subset of a 12-tone pitch-class set.
struct PcVoiceLeadingScratch {
  uint8_t costs[1u << 12]{};
};

struct PcVoiceLeadingStats {
  uint16_t peakStateCount = 0;
  uint32_t statesVisited = 0;
  uint32_t transitionCount = 0;
};

static_assert(sizeof(PcVoiceLeadingScratch) == 4096,
              "PC voice-leading scratch must remain bounded to 4096 bytes");

bool isValidPitchSet12(PitchSet12 set);
uint8_t popcountPitchSet12(PitchSet12 set);

HarmonyStatus transposeSet(PitchSet12 set, int16_t semitones, PitchSet12& out);
HarmonyStatus invertSet(PitchSet12 set, int16_t axis, PitchSet12& out);
HarmonyStatus commonTones(PitchSet12 a, PitchSet12 b, uint8_t& out);
HarmonyStatus setDifference(PitchSet12 a, PitchSet12 b, uint8_t& out);
HarmonyStatus intervalClassVector(PitchSet12 set, IntervalClassVector& out);
HarmonyStatus rootSemitoneDistance(uint8_t rootA, uint8_t rootB, uint8_t& out);
HarmonyStatus rootFifthsDistance(uint8_t rootA, uint8_t rootB, uint8_t& out);

HarmonyStatus pcVoiceLeadingCost(PitchSet12 a,
                                 PitchSet12 b,
                                 PcVoiceLeadingScratch& scratch,
                                 uint8_t& out,
                                 PcVoiceLeadingStats* stats = nullptr);

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_PITCH_SET_H
