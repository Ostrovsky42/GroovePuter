#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <vector>

#include "src/generation/harmony/pitch_set.h"

using namespace GroovePuterHarmony;

namespace {
uint8_t circularDistance(uint8_t a, uint8_t b) {
  const uint8_t raw = a > b ? a - b : b - a;
  return std::min<uint8_t>(raw, 12 - raw);
}

uint8_t bruteForce(PitchSet12 a, PitchSet12 b) {
  std::array<uint8_t, 12> left{};
  std::array<uint8_t, 12> right{};
  uint8_t leftCount = 0;
  uint8_t rightCount = 0;
  for (uint8_t pc = 0; pc < 12; ++pc) {
    if (a & (1u << pc)) left[leftCount++] = pc;
    if (b & (1u << pc)) right[rightCount++] = pc;
  }
  const auto* small = &left;
  const auto* large = &right;
  uint8_t smallCount = leftCount;
  uint8_t largeCount = rightCount;
  if (leftCount > rightCount) {
    small = &right;
    large = &left;
    smallCount = rightCount;
    largeCount = leftCount;
  }
  uint8_t best = 255;
  std::array<bool, 12> used{};
  std::function<void(uint8_t, uint8_t)> visit = [&](uint8_t index, uint8_t cost) {
    if (index == smallCount) {
      best = std::min(best, cost);
      return;
    }
    for (uint8_t j = 0; j < largeCount; ++j) {
      if (used[j]) continue;
      used[j] = true;
      visit(index + 1, static_cast<uint8_t>(
          cost + circularDistance((*small)[index], (*large)[j])));
      used[j] = false;
    }
  };
  visit(0, 0);
  if (smallCount != largeCount) {
    best = static_cast<uint8_t>(best + 3 * (largeCount - smallCount));
  }
  return best;
}

uint8_t cardinality(PitchSet12 mask) { return popcountPitchSet12(mask); }
}  // namespace

int main() {
  PcVoiceLeadingScratch scratch{};
  PcVoiceLeadingStats stats{};
  uint8_t cost = 255;
  constexpr PitchSet12 cMajor = (1u << 0) | (1u << 4) | (1u << 7);
  constexpr PitchSet12 cMinor = (1u << 0) | (1u << 3) | (1u << 7);
  constexpr PitchSet12 eMinor = (1u << 4) | (1u << 7) | (1u << 11);
  constexpr PitchSet12 aMinor = (1u << 9) | (1u << 0) | (1u << 4);
  constexpr PitchSet12 cMajor7 = cMajor | (1u << 11);
  assert(pcVoiceLeadingCost(cMajor, cMinor, scratch, cost) == HarmonyStatus::Ready);
  assert(cost == 1);
  assert(pcVoiceLeadingCost(cMajor, eMinor, scratch, cost) == HarmonyStatus::Ready);
  assert(cost == 1);
  assert(pcVoiceLeadingCost(cMajor, aMinor, scratch, cost) == HarmonyStatus::Ready);
  assert(cost == 2);
  assert(pcVoiceLeadingCost(0, cMajor, scratch, cost) == HarmonyStatus::Ready);
  assert(cost == 9);
  assert(pcVoiceLeadingCost(cMajor, cMajor7, scratch, cost) == HarmonyStatus::Ready);
  assert(cost == 3);

  // Independent nearest-note assignment reuses target C for both C and C#:
  // it reports 1; injective matching plus one unmatched-tone penalty is 3.
  constexpr PitchSet12 adjacent = (1u << 0) | (1u << 1);
  constexpr PitchSet12 singleC = 1u << 0;
  assert(pcVoiceLeadingCost(adjacent, singleC, scratch, cost) == HarmonyStatus::Ready);
  assert(cost == 3);
  assert(pcVoiceLeadingCost(0x1000, cMajor, scratch, cost) == HarmonyStatus::Invalid);
  assert(cost == 3);

  std::vector<PitchSet12> oracleMasks;
  for (PitchSet12 mask = 0; mask <= kPitchSet12Mask; ++mask) {
    if (cardinality(mask) <= 4) oracleMasks.push_back(mask);
  }
  auto compareWithOracle = [&](PitchSet12 a, PitchSet12 b) {
    uint8_t observed = 255;
    PcVoiceLeadingStats measured{};
    assert(pcVoiceLeadingCost(a, b, scratch, observed, &measured) ==
           HarmonyStatus::Ready);
    assert(observed == bruteForce(a, b));
    assert(measured.peakStateCount <= 16);
  };
  for (PitchSet12 a : oracleMasks) {
    for (PitchSet12 b : oracleMasks) compareWithOracle(a, b);
  }

  for (PitchSet12 a : {cMajor, cMinor, eMinor, aMinor, cMajor7}) {
    for (PitchSet12 b : {cMajor, cMinor, eMinor, aMinor, cMajor7}) {
      uint8_t original = 0;
      assert(pcVoiceLeadingCost(a, b, scratch, original) == HarmonyStatus::Ready);
      for (int shift = 0; shift < 12; ++shift) {
        PitchSet12 ta = 0;
        PitchSet12 tb = 0;
        assert(transposeSet(a, shift, ta) == HarmonyStatus::Ready);
        assert(transposeSet(b, shift, tb) == HarmonyStatus::Ready);
        uint8_t transposed = 255;
        assert(pcVoiceLeadingCost(ta, tb, scratch, transposed) ==
               HarmonyStatus::Ready);
        assert(transposed == original);
      }
    }
  }

  constexpr PitchSet12 sixA =
      (1u << 0) | (1u << 2) | (1u << 4) | (1u << 5) | (1u << 7) | (1u << 9);
  constexpr PitchSet12 sixB =
      (1u << 1) | (1u << 3) | (1u << 5) | (1u << 6) | (1u << 8) | (1u << 10);
  assert(pcVoiceLeadingCost(sixA, sixB, scratch, cost, &stats) ==
         HarmonyStatus::Ready);
  assert(cost == bruteForce(sixA, sixB));
  assert(stats.peakStateCount == 64);
  assert(stats.statesVisited <= 12u * 64u);
  assert(stats.transitionCount > 0);
  std::cout << "HGS exact PC voice leading: PASS\n";
  return 0;
}
