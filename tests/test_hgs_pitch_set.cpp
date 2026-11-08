#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/harmony/pitch_set.h"

using namespace GroovePuterHarmony;

int main() {
  for (uint16_t mask = 0; mask <= kPitchSet12Mask; ++mask) {
    assert(isValidPitchSet12(mask));
    PitchSet12 transformed = 0xffff;
    assert(transposeSet(mask, 12, transformed) == HarmonyStatus::Ready);
    assert(transformed == mask);
    for (int a = -12; a <= 12; ++a) {
      PitchSet12 inverse = 0;
      assert(invertSet(mask, a, inverse) == HarmonyStatus::Ready);
      PitchSet12 twice = 0;
      assert(invertSet(inverse, a, twice) == HarmonyStatus::Ready);
      assert(twice == mask);
      for (int b = -12; b <= 12; ++b) {
        PitchSet12 first = 0;
        PitchSet12 composed = 0;
        assert(transposeSet(mask, b, first) == HarmonyStatus::Ready);
        assert(transposeSet(first, a, composed) == HarmonyStatus::Ready);
        PitchSet12 direct = 0;
        assert(transposeSet(mask, a + b, direct) == HarmonyStatus::Ready);
        assert(composed == direct);
      }
    }

    IntervalClassVector icv{};
    assert(intervalClassVector(mask, icv) == HarmonyStatus::Ready);
    uint16_t sum = 0;
    for (uint8_t count : icv.counts) sum += count;
    const uint8_t cardinality = popcountPitchSet12(mask);
    assert(sum == cardinality * (cardinality - 1) / 2);
  }

  PitchSet12 sentinel = 0x123;
  assert(transposeSet(0x1000, 1, sentinel) == HarmonyStatus::Invalid);
  assert(sentinel == 0x123);
  assert(invertSet(0x8000, -1, sentinel) == HarmonyStatus::Invalid);
  assert(sentinel == 0x123);
  uint8_t scalar = 91;
  assert(commonTones(0x1000, 0, scalar) == HarmonyStatus::Invalid);
  assert(scalar == 91);
  assert(setDifference(0, 0x1000, scalar) == HarmonyStatus::Invalid);
  assert(scalar == 91);
  IntervalClassVector unchanged{{9, 8, 7, 6, 5, 4}};
  assert(intervalClassVector(0x1000, unchanged) == HarmonyStatus::Invalid);
  assert(unchanged.counts[0] == 9 && unchanged.counts[5] == 4);
  uint8_t common = 0;
  uint8_t difference = 0;
  assert(commonTones(0x91, 0x89, common) == HarmonyStatus::Ready);
  assert(common == 2);
  assert(setDifference(0x91, 0x89, difference) == HarmonyStatus::Ready);
  assert(difference == 2);

  constexpr PitchSet12 samples[] = {0, 1, 0x91, 0x89, 0x555, 0xabc};
  for (PitchSet12 a : samples) {
    for (PitchSet12 b : samples) {
      uint8_t commonBefore = 0;
      uint8_t differenceBefore = 0;
      assert(commonTones(a, b, commonBefore) == HarmonyStatus::Ready);
      assert(setDifference(a, b, differenceBefore) == HarmonyStatus::Ready);
      IntervalClassVector icvBefore{};
      assert(intervalClassVector(a, icvBefore) == HarmonyStatus::Ready);
      for (int shift = 0; shift < 12; ++shift) {
        PitchSet12 ta = 0;
        PitchSet12 tb = 0;
        assert(transposeSet(a, shift, ta) == HarmonyStatus::Ready);
        assert(transposeSet(b, shift, tb) == HarmonyStatus::Ready);
        uint8_t commonAfter = 255;
        uint8_t differenceAfter = 255;
        assert(commonTones(ta, tb, commonAfter) == HarmonyStatus::Ready);
        assert(setDifference(ta, tb, differenceAfter) == HarmonyStatus::Ready);
        assert(commonAfter == commonBefore);
        assert(differenceAfter == differenceBefore);
        IntervalClassVector icvAfter{};
        assert(intervalClassVector(ta, icvAfter) == HarmonyStatus::Ready);
        for (uint8_t index = 0; index < 6; ++index) {
          assert(icvAfter.counts[index] == icvBefore.counts[index]);
        }
      }
    }
  }

  for (uint8_t a = 0; a < 12; ++a) {
    for (uint8_t b = 0; b < 12; ++b) {
      uint8_t semitone = 255;
      uint8_t fifth = 255;
      assert(rootSemitoneDistance(a, b, semitone) == HarmonyStatus::Ready);
      assert(rootFifthsDistance(a, b, fifth) == HarmonyStatus::Ready);
      const uint8_t raw = a > b ? a - b : b - a;
      assert(semitone == (raw < 12 - raw ? raw : 12 - raw));
      int fifthSteps = (7 * (static_cast<int>(b) - a)) % 12;
      if (fifthSteps < 0) fifthSteps += 12;
      assert(fifth == (fifthSteps < 12 - fifthSteps
                           ? fifthSteps
                           : 12 - fifthSteps));
    }
  }

  uint8_t invalidDistance = 37;
  assert(rootSemitoneDistance(12, 0, invalidDistance) == HarmonyStatus::Invalid);
  assert(invalidDistance == 37);
  assert(rootFifthsDistance(0, 12, invalidDistance) == HarmonyStatus::Invalid);
  assert(invalidDistance == 37);
  std::cout << "HGS pitch-set algebra: PASS\n";
  return 0;
}
