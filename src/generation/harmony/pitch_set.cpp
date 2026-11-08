#include "pitch_set.h"

#include <cstring>

namespace GroovePuterHarmony {
namespace {

int16_t normalizedOffset(int16_t value) {
  int16_t result = static_cast<int16_t>(value % 12);
  if (result < 0) result += 12;
  return result;
}

uint8_t pitchDistance(uint8_t a, uint8_t b) {
  const uint8_t raw = a > b ? static_cast<uint8_t>(a - b)
                            : static_cast<uint8_t>(b - a);
  return raw < 12 - raw ? raw : static_cast<uint8_t>(12 - raw);
}

uint8_t bitCount12(PitchSet12 set) {
  uint8_t count = 0;
  while (set != 0) {
    count = static_cast<uint8_t>(count + (set & 1u));
    set >>= 1;
  }
  return count;
}

}  // namespace

bool isValidPitchSet12(PitchSet12 set) {
  return (set & static_cast<PitchSet12>(~kPitchSet12Mask)) == 0;
}

uint8_t popcountPitchSet12(PitchSet12 set) {
  return bitCount12(static_cast<PitchSet12>(set & kPitchSet12Mask));
}

HarmonyStatus transposeSet(PitchSet12 set, int16_t semitones, PitchSet12& out) {
  if (!isValidPitchSet12(set)) return HarmonyStatus::Invalid;
  const int16_t offset = normalizedOffset(semitones);
  const PitchSet12 result =
      offset == 0
          ? set
          : static_cast<PitchSet12>(((set << offset) | (set >> (12 - offset))) &
                                    kPitchSet12Mask);
  out = result;
  return HarmonyStatus::Ready;
}

HarmonyStatus invertSet(PitchSet12 set, int16_t axis, PitchSet12& out) {
  if (!isValidPitchSet12(set)) return HarmonyStatus::Invalid;
  const uint8_t normalizedAxis = static_cast<uint8_t>(normalizedOffset(axis));
  PitchSet12 result = 0;
  for (uint8_t pitch = 0; pitch < 12; ++pitch) {
    if ((set & (1u << pitch)) == 0) continue;
    const uint8_t inverted = static_cast<uint8_t>((normalizedAxis + 12 - pitch) % 12);
    result = static_cast<PitchSet12>(result | (1u << inverted));
  }
  out = result;
  return HarmonyStatus::Ready;
}

HarmonyStatus commonTones(PitchSet12 a, PitchSet12 b, uint8_t& out) {
  if (!isValidPitchSet12(a) || !isValidPitchSet12(b)) return HarmonyStatus::Invalid;
  out = bitCount12(static_cast<PitchSet12>(a & b));
  return HarmonyStatus::Ready;
}

HarmonyStatus setDifference(PitchSet12 a, PitchSet12 b, uint8_t& out) {
  if (!isValidPitchSet12(a) || !isValidPitchSet12(b)) return HarmonyStatus::Invalid;
  out = bitCount12(static_cast<PitchSet12>(a ^ b));
  return HarmonyStatus::Ready;
}

HarmonyStatus intervalClassVector(PitchSet12 set, IntervalClassVector& out) {
  if (!isValidPitchSet12(set)) return HarmonyStatus::Invalid;
  IntervalClassVector result{};
  for (uint8_t first = 0; first < 12; ++first) {
    if ((set & (1u << first)) == 0) continue;
    for (uint8_t second = static_cast<uint8_t>(first + 1); second < 12; ++second) {
      if ((set & (1u << second)) == 0) continue;
      const uint8_t raw = static_cast<uint8_t>(second - first);
      const uint8_t intervalClass =
          raw < 12 - raw ? raw : static_cast<uint8_t>(12 - raw);
      ++result.counts[intervalClass - 1];
    }
  }
  out = result;
  return HarmonyStatus::Ready;
}

HarmonyStatus rootSemitoneDistance(uint8_t rootA, uint8_t rootB, uint8_t& out) {
  if (rootA >= 12 || rootB >= 12) return HarmonyStatus::Invalid;
  out = pitchDistance(rootA, rootB);
  return HarmonyStatus::Ready;
}

HarmonyStatus rootFifthsDistance(uint8_t rootA, uint8_t rootB, uint8_t& out) {
  if (rootA >= 12 || rootB >= 12) return HarmonyStatus::Invalid;
  int16_t fifthSteps = static_cast<int16_t>(7 * (static_cast<int16_t>(rootB) - rootA));
  fifthSteps = normalizedOffset(fifthSteps);
  const uint8_t steps = static_cast<uint8_t>(fifthSteps);
  out = steps < 12 - steps ? steps : static_cast<uint8_t>(12 - steps);
  return HarmonyStatus::Ready;
}

HarmonyStatus pcVoiceLeadingCost(PitchSet12 a,
                                 PitchSet12 b,
                                 PcVoiceLeadingScratch& scratch,
                                 uint8_t& out,
                                 PcVoiceLeadingStats* stats) {
  if (!isValidPitchSet12(a) || !isValidPitchSet12(b)) return HarmonyStatus::Invalid;

  uint8_t pitchesA[12]{};
  uint8_t pitchesB[12]{};
  uint8_t countA = 0;
  uint8_t countB = 0;
  for (uint8_t pitch = 0; pitch < 12; ++pitch) {
    if (a & (1u << pitch)) pitchesA[countA++] = pitch;
    if (b & (1u << pitch)) pitchesB[countB++] = pitch;
  }

  const uint8_t* smaller = pitchesA;
  const uint8_t* larger = pitchesB;
  uint8_t smallerCount = countA;
  uint8_t largerCount = countB;
  if (countA > countB) {
    smaller = pitchesB;
    larger = pitchesA;
    smallerCount = countB;
    largerCount = countA;
  }

  std::memset(scratch.costs, 0xff, sizeof(scratch.costs));
  scratch.costs[0] = 0;
  const uint16_t stateCount = static_cast<uint16_t>(1u << smallerCount);
  PcVoiceLeadingStats measured{};
  measured.peakStateCount = stateCount;

  // Descending subset traversal prevents one larger-side pitch from being
  // assigned to more than one smaller-side pitch during this pass.
  for (uint8_t largeIndex = 0; largeIndex < largerCount; ++largeIndex) {
    for (int32_t state = static_cast<int32_t>(stateCount) - 1; state >= 0; --state) {
      ++measured.statesVisited;
      const uint8_t prior = scratch.costs[state];
      if (prior == 0xff) continue;
      for (uint8_t smallIndex = 0; smallIndex < smallerCount; ++smallIndex) {
        const uint16_t bit = static_cast<uint16_t>(1u << smallIndex);
        if ((state & bit) != 0) continue;
        ++measured.transitionCount;
        const uint16_t next = static_cast<uint16_t>(state | bit);
        const uint8_t candidate = static_cast<uint8_t>(
            prior + pitchDistance(smaller[smallIndex], larger[largeIndex]));
        if (candidate < scratch.costs[next]) scratch.costs[next] = candidate;
      }
    }
  }

  const uint8_t matchedCost = scratch.costs[stateCount - 1];
  if (matchedCost == 0xff) return HarmonyStatus::Invalid;
  const uint8_t unmatchedCount = countA > countB
                                     ? static_cast<uint8_t>(countA - countB)
                                     : static_cast<uint8_t>(countB - countA);
  const uint8_t result = static_cast<uint8_t>(matchedCost + 3 * unmatchedCount);
  out = result;
  if (stats != nullptr) *stats = measured;
  return HarmonyStatus::Ready;
}

}  // namespace GroovePuterHarmony
