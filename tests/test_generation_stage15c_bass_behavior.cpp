#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "src/generation/roles/bass_pitch_behavior.h"

using namespace GroovePuterRhythm;

namespace {

StepMask mask(std::initializer_list<uint8_t> steps) {
  StepMask result = 0;
  for (uint8_t step : steps)
    result = static_cast<StepMask>(result | stepBit(step));
  return result;
}

BassPitchBehaviorRequest requestFor(StepMask onsets) {
  BassPitchBehaviorRequest request{};
  request.rhythmPlan.id = BassRhythmId::SyncopatedHook;
  request.rhythmPlan.onsets = onsets;
  request.family = RhythmFamily::Funk16;
  request.archetypeId = 401;
  request.generation.projectSeed = 0x31415926u;
  request.generation.phraseOrdinal = 11;
  request.barOrdinal = 2;
  request.minDegreeOffset = -7;
  request.maxDegreeOffset = 7;
  request.maxLeapDegrees = 7;
  return request;
}

void assertSame(const BassPitchBehaviorResult& a,
                const BassPitchBehaviorResult& b) {
  assert(std::memcmp(&a, &b, sizeof(a)) == 0);
}

void assertBounds(const BassPitchBehaviorPlan& plan,
                  int8_t minimum,
                  int8_t maximum,
                  uint8_t maximumLeap) {
  for (uint8_t index = 0; index < plan.onsetCount; ++index) {
    const int value = plan.degreeOffsets[index];
    assert(value >= minimum);
    assert(value <= maximum);
    if (index == 0) continue;
    int difference = static_cast<int>(plan.degreeOffsets[index]) -
                     static_cast<int>(plan.degreeOffsets[index - 1]);
    if (difference < 0) difference = -difference;
    assert(difference <= maximumLeap);
  }
}

}  // namespace

int main() {
  {
    BassPitchBehaviorRequest request = requestFor(0);
    request.requestedContour = BassPitchContourId::RootOctave;
    request.requestedArticulation = BassArticulationStyleId::Dynamic;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::ValidButEmpty);
    assert(result.plan.onsetCount == 0);
    assert(result.plan.onsets == 0);
    assert(result.plan.continuations == 0);
    assert(result.plan.accentOnsets == 0);
    assert(result.plan.slideIntoOnsets == 0);
  }

  {
    const BassPitchBehaviorRequest request = requestFor(mask({0, 3, 7, 10, 14}));
    assertSame(realizeBassPitchBehavior(request),
               realizeBassPitchBehavior(request));
  }

  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 4, 8, 12}));
    request.requestedContour = BassPitchContourId::RootFifth;
    request.requestedArticulation = BassArticulationStyleId::Plain;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assert(result.plan.degreeOffsets[0] == 0);
    assert(result.plan.degreeOffsets[1] == 4);
    assert(result.plan.degreeOffsets[2] == 0);
    assert(result.plan.degreeOffsets[3] == 4);
    assert(result.plan.onsets == request.rhythmPlan.onsets);
    assert(result.plan.accentOnsets == 0);
    assert(result.plan.slideIntoOnsets == 0);
  }

  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 3, 6, 9, 12}));
    request.requestedContour = BassPitchContourId::NeighborReturn;
    request.requestedArticulation = BassArticulationStyleId::Plain;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assert(result.plan.degreeOffsets[0] == 0);
    assert(result.plan.degreeOffsets[1] == 1);
    assert(result.plan.degreeOffsets[2] == 0);
    assert(result.plan.degreeOffsets[3] == -1);
    assert(result.plan.degreeOffsets[4] == 0);
  }

  {
    BassPitchBehaviorRequest request = requestFor(mask({2, 7, 12}));
    request.requestedContour = BassPitchContourId::StepApproach;
    request.requestedArticulation = BassArticulationStyleId::Plain;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assert(result.plan.degreeOffsets[0] == -2);
    assert(result.plan.degreeOffsets[1] == -1);
    assert(result.plan.degreeOffsets[2] == 0);
  }

  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 4, 8, 12}));
    request.rhythmPlan.continuations = mask({1, 2, 5});
    request.requestedContour = BassPitchContourId::PedalTurn;
    request.requestedArticulation = BassArticulationStyleId::AccentPulse;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assert(result.plan.onsets == request.rhythmPlan.onsets);
    assert(result.plan.continuations == request.rhythmPlan.continuations);
    assert((result.plan.accentOnsets & ~result.plan.onsets) == 0);
    assert((result.plan.slideIntoOnsets & ~result.plan.onsets) == 0);
    assert((result.plan.accentOnsets & stepBit(0)) != 0);
  }

  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 2, 4}));
    request.requestedContour = BassPitchContourId::NeighborReturn;
    request.requestedArticulation = BassArticulationStyleId::LegatoApproach;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assert((result.plan.accentOnsets & stepBit(0)) != 0);
    assert((result.plan.slideIntoOnsets & stepBit(0)) == 0);
    assert((result.plan.slideIntoOnsets & stepBit(2)) != 0);
    assert((result.plan.slideIntoOnsets & stepBit(4)) != 0);
  }

  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 4, 8, 12}));
    request.requestedContour = BassPitchContourId::RootOctave;
    request.requestedArticulation = BassArticulationStyleId::Dynamic;
    request.minDegreeOffset = -2;
    request.maxDegreeOffset = 3;
    request.maxLeapDegrees = 2;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assertBounds(result.plan, -2, 3, 2);
    assert((result.plan.accentOnsets & ~result.plan.onsets) == 0);
    assert((result.plan.slideIntoOnsets & ~result.plan.onsets) == 0);
  }

  {
    BassPitchBehaviorRequest request = requestFor(mask({0}));
    request.rhythmPlan.continuations = stepBit(6);
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::InvalidRequest);
  }

  {
    bool sawDifferentBehavior = false;
    BassPitchBehaviorResult first{};
    for (uint16_t ordinal = 0; ordinal < 128; ++ordinal) {
      BassPitchBehaviorRequest request = requestFor(mask({0, 3, 7, 10, 14}));
      request.generation.phraseOrdinal = ordinal;
      const auto result = realizeBassPitchBehavior(request);
      assert(result.status == BassPitchBehaviorStatus::Ok);
      assert(result.plan.onsets == request.rhythmPlan.onsets);
      assert(result.plan.continuations == request.rhythmPlan.continuations);
      assert((result.plan.accentOnsets & ~result.plan.onsets) == 0);
      assert((result.plan.slideIntoOnsets & ~result.plan.onsets) == 0);
      assertBounds(result.plan, -7, 7, 7);
      if (ordinal == 0) {
        first = result;
      } else if (result.plan.contour != first.plan.contour ||
                 result.plan.articulation != first.plan.articulation ||
                 result.plan.accentOnsets != first.plan.accentOnsets ||
                 result.plan.slideIntoOnsets != first.plan.slideIntoOnsets ||
                 std::memcmp(result.plan.degreeOffsets,
                             first.plan.degreeOffsets,
                             sizeof(result.plan.degreeOffsets)) != 0) {
        sawDifferentBehavior = true;
      }
    }
    assert(sawDifferentBehavior);
  }

  return 0;
}
