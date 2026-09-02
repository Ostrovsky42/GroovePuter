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

bool taggedSemitone(const BassPitchBehaviorPlan& plan, uint8_t ordinal) {
  return (plan.semitoneOffsetOrdinals &
          static_cast<uint16_t>(1u << ordinal)) != 0;
}

BassPitchBehaviorRequest requestFor(StepMask onsets) {
  BassPitchBehaviorRequest request{};
  request.rhythmPlan.id = BassRhythmId::SyncopatedHook;
  request.rhythmPlan.onsets = onsets;
  request.archetypeId = 401;
  request.generation.projectSeed = 0x31415926u;
  request.generation.phraseOrdinal = 11;
  request.barOrdinal = 2;
  request.minDegreeOffset = -7;
  request.maxDegreeOffset = 7;
  request.maxLeapDegrees = 7;
  return request;
}

void enableAllPolicy(BassPitchBehaviorRequest& request) {
  request.policy.allowedContours = kAllBassPitchContours;
  request.policy.preferredContours = 0;
  request.policy.allowedArticulations = kAllBassArticulationStyles;
  request.policy.preferredArticulations = 0;
}

void assertSame(const BassPitchBehaviorResult& a,
                const BassPitchBehaviorResult& b) {
  assert(a.status == b.status);
  assert(a.plan.contour == b.plan.contour);
  assert(a.plan.articulation == b.plan.articulation);
  assert(a.plan.onsets == b.plan.onsets);
  assert(a.plan.continuations == b.plan.continuations);
  assert(a.plan.accentOnsets == b.plan.accentOnsets);
  assert(a.plan.slideIntoOnsets == b.plan.slideIntoOnsets);
  assert(a.plan.onsetCount == b.plan.onsetCount);
  assert(a.plan.semitoneOffsetOrdinals == b.plan.semitoneOffsetOrdinals);
  assert(std::memcmp(a.plan.onsetSteps, b.plan.onsetSteps,
                     sizeof(a.plan.onsetSteps)) == 0);
  assert(std::memcmp(a.plan.tonalOffsets, b.plan.tonalOffsets,
                     sizeof(a.plan.tonalOffsets)) == 0);
}

void assertDegreeBounds(const BassPitchBehaviorPlan& plan,
                        int8_t minimum,
                        int8_t maximum,
                        uint8_t maximumLeap) {
  for (uint8_t index = 0; index < plan.onsetCount; ++index) {
    if (taggedSemitone(plan, index)) continue;
    assert(plan.tonalOffsets[index] >= minimum);
    assert(plan.tonalOffsets[index] <= maximum);
    if (index == 0 || taggedSemitone(plan, index - 1u)) continue;
    int difference = static_cast<int>(plan.tonalOffsets[index]) -
                     static_cast<int>(plan.tonalOffsets[index - 1u]);
    if (difference < 0) difference = -difference;
    assert(difference <= maximumLeap);
  }
}

}  // namespace

int main() {
  // Fail-safe default never silently enables Stage 15C vocabulary.
  {
    for (uint32_t seed = 0; seed < 256; ++seed) {
      BassPitchBehaviorRequest request = requestFor(mask({0, 3, 7, 10, 14}));
      request.generation.projectSeed = seed;
      request.generation.phraseOrdinal = static_cast<uint16_t>(seed);
      const auto result = realizeBassPitchBehavior(request);
      assert(result.status == BassPitchBehaviorStatus::Ok);
      assert(result.plan.contour == BassPitchContourId::RootAnchor);
      assert(result.plan.articulation == BassArticulationStyleId::Plain);
      assert(result.plan.onsets == request.rhythmPlan.onsets);
      assert(result.plan.continuations == request.rhythmPlan.continuations);
      assert(result.plan.semitoneOffsetOrdinals == 0);
      for (uint8_t i = 0; i < result.plan.onsetCount; ++i)
        assert(result.plan.tonalOffsets[i] == 0);
    }
  }

  {
    BassPitchBehaviorRequest request = requestFor(0);
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::ValidButEmpty);
    assert(result.plan.contour == BassPitchContourId::RootAnchor);
    assert(result.plan.articulation == BassArticulationStyleId::Plain);
    assert(result.plan.semitoneOffsetOrdinals == 0);
  }

  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 3, 7, 10, 14}));
    enableAllPolicy(request);
    assertSame(realizeBassPitchBehavior(request),
               realizeBassPitchBehavior(request));
  }

  // Preferred policy, not RhythmFamily, drives AUTO.
  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 4, 8, 12}));
    enableAllPolicy(request);
    request.policy.preferredContours =
        bassPitchContourBit(BassPitchContourId::RootFifth);
    request.policy.preferredArticulations =
        bassArticulationStyleBit(BassArticulationStyleId::AccentPulse);
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assert(result.plan.contour == BassPitchContourId::RootFifth);
    assert(result.plan.articulation == BassArticulationStyleId::AccentPulse);
  }

  // Explicit vocabulary outside the conservative default is invalid.
  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 4, 8}));
    request.requestedContour = BassPitchContourId::RootFifth;
    assert(realizeBassPitchBehavior(request).status ==
           BassPitchBehaviorStatus::InvalidRequest);
  }

  // Compatibility fallbacks are mandatory in every policy.
  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 4, 8}));
    request.policy.allowedContours =
        bassPitchContourBit(BassPitchContourId::RootFifth);
    assert(realizeBassPitchBehavior(request).status ==
           BassPitchBehaviorStatus::InvalidRequest);
  }

  // Preferred masks must be subsets of allowed masks.
  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 4, 8}));
    request.policy.preferredArticulations =
        bassArticulationStyleBit(BassArticulationStyleId::Dynamic);
    assert(realizeBassPitchBehavior(request).status ==
           BassPitchBehaviorStatus::InvalidRequest);
  }

  // Root/fifth is tagged chromatic intent: +7 semitones, independent of scale
  // cardinality. The bit index is onset ordinal, not grid step.
  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 4, 8, 12}));
    enableAllPolicy(request);
    request.requestedContour = BassPitchContourId::RootFifth;
    request.requestedArticulation = BassArticulationStyleId::Plain;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assert(result.plan.tonalOffsets[0] == 0);
    assert(result.plan.tonalOffsets[1] == 7);
    assert(result.plan.tonalOffsets[2] == 0);
    assert(result.plan.tonalOffsets[3] == 7);
    assert(!taggedSemitone(result.plan, 0));
    assert(taggedSemitone(result.plan, 1));
    assert(!taggedSemitone(result.plan, 2));
    assert(taggedSemitone(result.plan, 3));
  }

  // Root/octave is +12 semitones, also tagged explicitly.
  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 6, 12}));
    enableAllPolicy(request);
    request.requestedContour = BassPitchContourId::RootOctave;
    request.requestedArticulation = BassArticulationStyleId::Plain;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assert(result.plan.tonalOffsets[0] == 0);
    assert(result.plan.tonalOffsets[1] == 12);
    assert(result.plan.tonalOffsets[2] == 0);
    assert(taggedSemitone(result.plan, 1));
  }

  // Neighbor/approach vocabulary remains scale-degree intent.
  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 3, 6, 9, 12}));
    enableAllPolicy(request);
    request.requestedContour = BassPitchContourId::NeighborReturn;
    request.requestedArticulation = BassArticulationStyleId::Plain;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assert(result.plan.semitoneOffsetOrdinals == 0);
    assert(result.plan.tonalOffsets[0] == 0);
    assert(result.plan.tonalOffsets[1] == 1);
    assert(result.plan.tonalOffsets[2] == 0);
    assert(result.plan.tonalOffsets[3] == -1);
    assert(result.plan.tonalOffsets[4] == 0);
  }

  {
    BassPitchBehaviorRequest request = requestFor(mask({2, 7, 12}));
    enableAllPolicy(request);
    request.requestedContour = BassPitchContourId::StepApproach;
    request.requestedArticulation = BassArticulationStyleId::Plain;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assert(result.plan.semitoneOffsetOrdinals == 0);
    assert(result.plan.tonalOffsets[0] == -2);
    assert(result.plan.tonalOffsets[1] == -1);
    assert(result.plan.tonalOffsets[2] == 0);
  }

  // Composite vocabulary can mix units without ambiguity.
  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 4, 8, 12}));
    enableAllPolicy(request);
    request.requestedContour = BassPitchContourId::RootFifthNeighbor;
    request.requestedArticulation = BassArticulationStyleId::Plain;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assert(result.plan.tonalOffsets[0] == 0);
    assert(result.plan.tonalOffsets[1] == 7);
    assert(result.plan.tonalOffsets[2] == 1);
    assert(result.plan.tonalOffsets[3] == 0);
    assert(taggedSemitone(result.plan, 1));
    assert(!taggedSemitone(result.plan, 2));
  }

  // Timing topology remains immutable and articulation masks remain onset-only.
  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 4, 8, 12}));
    enableAllPolicy(request);
    request.rhythmPlan.continuations = mask({1, 2, 5});
    request.requestedContour = BassPitchContourId::PedalTurn;
    request.requestedArticulation = BassArticulationStyleId::AccentPulse;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assert(result.plan.onsets == request.rhythmPlan.onsets);
    assert(result.plan.continuations == request.rhythmPlan.continuations);
    assert((result.plan.accentOnsets & ~result.plan.onsets) == 0);
    assert((result.plan.slideIntoOnsets & ~result.plan.onsets) == 0);
  }

  // Existing legato-connected scale-degree topology may carry slide intent.
  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 2, 4}));
    enableAllPolicy(request);
    request.rhythmPlan.continuations = mask({1, 3});
    request.requestedContour = BassPitchContourId::NeighborReturn;
    request.requestedArticulation = BassArticulationStyleId::LegatoApproach;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assert((result.plan.slideIntoOnsets & stepBit(2)) != 0);
    assert((result.plan.slideIntoOnsets & stepBit(4)) != 0);
    assert(result.plan.continuations == request.rhythmPlan.continuations);
  }

  // A real timing gap is never repaired just to realize a slide.
  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 2, 4}));
    enableAllPolicy(request);
    request.requestedContour = BassPitchContourId::NeighborReturn;
    request.requestedArticulation = BassArticulationStyleId::LegatoApproach;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assert(result.plan.slideIntoOnsets == 0);
    assert(result.plan.continuations == 0);
  }

  // Mixed units are never compared as if they shared maxLeapDegrees, and they
  // do not create a slide simply because their raw integers are close.
  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 2, 4}));
    enableAllPolicy(request);
    request.rhythmPlan.continuations = mask({1, 3});
    request.requestedContour = BassPitchContourId::RootFifth;
    request.requestedArticulation = BassArticulationStyleId::LegatoApproach;
    request.maxLeapDegrees = 1;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assert(result.plan.tonalOffsets[1] == 7);
    assert(taggedSemitone(result.plan, 1));
    assert(result.plan.slideIntoOnsets == 0);
  }

  // Degree bounds apply only to untagged degree values.
  {
    BassPitchBehaviorRequest request = requestFor(mask({0, 4, 8, 12}));
    enableAllPolicy(request);
    request.requestedContour = BassPitchContourId::LeapReturn;
    request.requestedArticulation = BassArticulationStyleId::Plain;
    request.minDegreeOffset = -2;
    request.maxDegreeOffset = 2;
    request.maxLeapDegrees = 1;
    const auto result = realizeBassPitchBehavior(request);
    assert(result.status == BassPitchBehaviorStatus::Ok);
    assert(result.plan.semitoneOffsetOrdinals == 0);
    assertDegreeBounds(result.plan, -2, 2, 1);
  }

  {
    BassPitchBehaviorRequest request = requestFor(mask({0}));
    request.rhythmPlan.continuations = stepBit(6);
    assert(realizeBassPitchBehavior(request).status ==
           BassPitchBehaviorStatus::InvalidRequest);
  }

  // Diversity is explicit opt-in; default behavior above remains invariant.
  {
    bool sawDifferent = false;
    BassPitchBehaviorResult first{};
    for (uint16_t ordinal = 0; ordinal < 256; ++ordinal) {
      BassPitchBehaviorRequest request =
          requestFor(mask({0, 3, 7, 10, 14}));
      enableAllPolicy(request);
      request.generation.phraseOrdinal = ordinal;
      const auto result = realizeBassPitchBehavior(request);
      assert(result.status == BassPitchBehaviorStatus::Ok);
      assert(result.plan.onsets == request.rhythmPlan.onsets);
      assert(result.plan.continuations == request.rhythmPlan.continuations);
      assert((result.plan.accentOnsets & ~result.plan.onsets) == 0);
      assert((result.plan.slideIntoOnsets & ~result.plan.onsets) == 0);
      assertDegreeBounds(result.plan, -7, 7, 7);
      if (ordinal == 0) {
        first = result;
      } else if (result.plan.contour != first.plan.contour ||
                 result.plan.articulation != first.plan.articulation ||
                 result.plan.accentOnsets != first.plan.accentOnsets ||
                 result.plan.slideIntoOnsets != first.plan.slideIntoOnsets ||
                 result.plan.semitoneOffsetOrdinals !=
                     first.plan.semitoneOffsetOrdinals ||
                 std::memcmp(result.plan.tonalOffsets,
                             first.plan.tonalOffsets,
                             sizeof(result.plan.tonalOffsets)) != 0) {
        sawDifferent = true;
      }
    }
    assert(sawDifferent);
  }

  return 0;
}
