#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "src/generation/roles/melodic_pitch_intent.h"

using namespace GroovePuterRhythm;

namespace {

StepMask mask(std::initializer_list<uint8_t> steps) {
  StepMask result = 0;
  for (uint8_t step : steps)
    result = static_cast<StepMask>(result | stepBit(step));
  return result;
}

bool validTopology(StepMask onsets, StepMask continuations) {
  if ((onsets & continuations) != 0) return false;
  bool active = false;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    const StepMask bit = stepBit(step);
    if ((onsets & bit) != 0) {
      active = true;
    } else if ((continuations & bit) != 0) {
      if (!active) return false;
    } else {
      active = false;
    }
  }
  return true;
}

uint8_t firstSetStep(StepMask value) {
  for (uint8_t step = 0; step < kStepsPerBar; ++step)
    if ((value & stepBit(step)) != 0) return step;
  return kStepsPerBar;
}

MelodicPitchIntentRequest requestFor(StepMask onsets) {
  MelodicPitchIntentRequest request{};
  request.rhythmPlan.rhythmId = MelodicRhythmId::SyncopatedMotif;
  request.rhythmPlan.motif.shape = MotifShapeId::SourceOrder;
  request.rhythmPlan.onsets = onsets;
  request.archetypeId = 401;
  request.generation.projectSeed = 0x12345678u;
  request.generation.phraseOrdinal = 9;
  request.barOrdinal = 3;
  request.minDegreeOffset = -7;
  request.maxDegreeOffset = 7;
  request.maxLeapDegrees = 4;
  request.maxOnsets = kStepsPerBar;
  return request;
}

void enableAllPolicy(MelodicPitchIntentRequest& request) {
  request.policy.allowedRhythmOperations = kAllMelodicRhythmOperations;
  request.policy.preferredRhythmOperations = 0;
  request.policy.allowedContours = kAllMelodicContours;
  request.policy.preferredContours = 0;
  request.policy.allowedMotifOperations = kAllMelodicMotifOperations;
  request.policy.preferredMotifOperations = 0;
}

void isolatePitch(MelodicPitchIntentRequest& request) {
  request.requestedRhythmOperation = MelodicRhythmOperationId::Preserve;
}

void assertSame(const MelodicPitchIntentResult& a,
                const MelodicPitchIntentResult& b) {
  assert(a.status == b.status);
  assert(a.plan.rhythmOperation == b.plan.rhythmOperation);
  assert(a.plan.contour == b.plan.contour);
  assert(a.plan.operation == b.plan.operation);
  assert(a.plan.onsets == b.plan.onsets);
  assert(a.plan.continuations == b.plan.continuations);
  assert(a.plan.onsetCount == b.plan.onsetCount);
  assert(std::memcmp(a.plan.onsetSteps, b.plan.onsetSteps,
                     sizeof(a.plan.onsetSteps)) == 0);
  assert(std::memcmp(a.plan.degreeOffsets, b.plan.degreeOffsets,
                     sizeof(a.plan.degreeOffsets)) == 0);
}

void assertBounds(const MelodicPitchIntentPlan& plan,
                  int8_t minimum,
                  int8_t maximum,
                  uint8_t maximumLeap) {
  for (uint8_t index = 0; index < plan.onsetCount; ++index) {
    assert(plan.degreeOffsets[index] >= minimum);
    assert(plan.degreeOffsets[index] <= maximum);
    if (index == 0) continue;
    int difference = static_cast<int>(plan.degreeOffsets[index]) -
                     static_cast<int>(plan.degreeOffsets[index - 1]);
    if (difference < 0) difference = -difference;
    assert(difference <= maximumLeap);
  }
}

}  // namespace

int main() {
  // Conservative default is invariant for every seed: no rhythmic or tonal
  // behavior is enabled until the composition layer explicitly opts in.
  {
    for (uint32_t seed = 0; seed < 256; ++seed) {
      MelodicPitchIntentRequest request = requestFor(mask({1, 5, 10, 14}));
      request.generation.projectSeed = seed;
      request.generation.phraseOrdinal = static_cast<uint16_t>(seed);
      const auto result = realizeMelodicPitchIntent(request);
      assert(result.status == MelodicPitchIntentStatus::Ok);
      assert(result.plan.rhythmOperation == MelodicRhythmOperationId::Preserve);
      assert(result.plan.contour == MelodicContourId::Static);
      assert(result.plan.operation == MelodicMotifOperationId::None);
      assert(result.plan.onsets == request.rhythmPlan.onsets);
      assert(result.plan.continuations == request.rhythmPlan.continuations);
    }
  }

  {
    MelodicPitchIntentRequest request = requestFor(0);
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::ValidButEmpty);
    assert(result.plan.rhythmOperation == MelodicRhythmOperationId::Preserve);
    assert(result.plan.contour == MelodicContourId::Static);
    assert(result.plan.operation == MelodicMotifOperationId::None);
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({1, 5, 10, 14}));
    enableAllPolicy(request);
    assertSame(realizeMelodicPitchIntent(request),
               realizeMelodicPitchIntent(request));
  }

  // Preferred policy, not a hidden family lookup, drives AUTO.
  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4, 8}));
    enableAllPolicy(request);
    request.policy.preferredRhythmOperations =
        melodicRhythmOperationBit(MelodicRhythmOperationId::ControlledRest);
    request.policy.preferredContours =
        melodicContourBit(MelodicContourId::StepUp);
    request.policy.preferredMotifOperations =
        melodicMotifOperationBit(MelodicMotifOperationId::TerminalReturn);
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.rhythmOperation ==
           MelodicRhythmOperationId::ControlledRest);
    assert(result.plan.contour == MelodicContourId::StepUp);
    assert(result.plan.operation == MelodicMotifOperationId::TerminalReturn);
  }

  // Explicit selection outside allowed vocabulary is rejected.
  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4, 8}));
    request.requestedContour = MelodicContourId::StepUp;
    assert(realizeMelodicPitchIntent(request).status ==
           MelodicPitchIntentStatus::InvalidRequest);
  }

  // Compatibility fallbacks are mandatory in every valid policy.
  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4, 8}));
    request.policy.allowedRhythmOperations =
        melodicRhythmOperationBit(MelodicRhythmOperationId::ControlledRest);
    assert(realizeMelodicPitchIntent(request).status ==
           MelodicPitchIntentStatus::InvalidRequest);
  }

  // Preferred masks must be subsets of allowed masks.
  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4, 8}));
    request.policy.preferredContours =
        melodicContourBit(MelodicContourId::StepUp);
    assert(realizeMelodicPitchIntent(request).status ==
           MelodicPitchIntentStatus::InvalidRequest);
  }

  // Stage 14 onset blocking and continuation legality remain distinct.
  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4}));
    request.rhythmPlan.continuations = mask({1, 2, 5, 6});
    request.allowedOnsetSteps = mask({0, 4});
    request.allowedContinuationSteps = mask({1, 2, 5, 6});
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.onsets == request.rhythmPlan.onsets);
    assert(result.plan.continuations == request.rhythmPlan.continuations);
  }

  // Controlled rest removes a complete note chain.
  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4, 10}));
    enableAllPolicy(request);
    request.rhythmPlan.continuations = mask({5, 6, 11});
    request.requestedRhythmOperation =
        MelodicRhythmOperationId::ControlledRest;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.rhythmOperation ==
           MelodicRhythmOperationId::ControlledRest);
    assert(result.plan.onsetCount == 2);
    assert((result.plan.onsets & stepBit(0)) != 0);
    assert(validTopology(result.plan.onsets, result.plan.continuations));
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({8}));
    enableAllPolicy(request);
    request.requestedRhythmOperation =
        MelodicRhythmOperationId::ControlledRest;
    request.allowEmptyBar = true;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::ValidButEmpty);
    assert(result.plan.onsets == 0);
    assert(result.plan.continuations == 0);
  }

  // Shift moves a complete onset+continuation chain and keeps topology valid.
  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4, 10}));
    enableAllPolicy(request);
    request.rhythmPlan.continuations = mask({5, 6});
    request.requestedRhythmOperation =
        MelodicRhythmOperationId::ShiftInteriorLater;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.rhythmOperation ==
           MelodicRhythmOperationId::ShiftInteriorLater);
    assert(result.plan.onsetCount == 3);
    assert(result.plan.onsets != request.rhythmPlan.onsets);
    assert(validTopology(result.plan.onsets, result.plan.continuations));
  }

  // Impossible shift degrades to Preserve rather than escaping legality.
  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4}));
    enableAllPolicy(request);
    request.requestedRhythmOperation =
        MelodicRhythmOperationId::ShiftInteriorLater;
    request.allowedOnsetSteps = request.rhythmPlan.onsets;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.rhythmOperation == MelodicRhythmOperationId::Preserve);
    assert(result.plan.onsets == request.rhythmPlan.onsets);
  }

  // TerminalEcho must add strictly to the right of the current terminal onset.
  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 8}));
    enableAllPolicy(request);
    request.requestedRhythmOperation = MelodicRhythmOperationId::TerminalEcho;
    request.maxOnsets = 3;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.rhythmOperation == MelodicRhythmOperationId::TerminalEcho);
    const StepMask added = static_cast<StepMask>(
        result.plan.onsets & static_cast<StepMask>(~request.rhythmPlan.onsets));
    assert(added != 0);
    assert(firstSetStep(added) > 8);
  }

  // Terminal at step 15 cannot echo and must preserve.
  {
    MelodicPitchIntentRequest request = requestFor(mask({4, 15}));
    enableAllPolicy(request);
    request.requestedRhythmOperation = MelodicRhythmOperationId::TerminalEcho;
    request.maxOnsets = 3;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.rhythmOperation == MelodicRhythmOperationId::Preserve);
    assert(result.plan.onsets == request.rhythmPlan.onsets);
  }

  // A legal slot on the left is not a substitute for an unavailable right-side
  // echo position.
  {
    MelodicPitchIntentRequest request = requestFor(mask({4, 12}));
    enableAllPolicy(request);
    request.requestedRhythmOperation = MelodicRhythmOperationId::TerminalEcho;
    request.maxOnsets = 3;
    request.allowedOnsetSteps = mask({4, 10, 12});
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.rhythmOperation == MelodicRhythmOperationId::Preserve);
    assert(result.plan.onsets == request.rhythmPlan.onsets);
  }

  // Terminal echo cannot land on an occupied continuation.
  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 8}));
    enableAllPolicy(request);
    request.rhythmPlan.continuations = mask({9, 10, 11, 12, 13, 14, 15});
    request.requestedRhythmOperation = MelodicRhythmOperationId::TerminalEcho;
    request.maxOnsets = 3;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.rhythmOperation == MelodicRhythmOperationId::Preserve);
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4, 8, 12}));
    enableAllPolicy(request);
    isolatePitch(request);
    request.requestedContour = MelodicContourId::StepUp;
    request.requestedOperation = MelodicMotifOperationId::None;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.onsetCount == 4);
    assert(result.plan.degreeOffsets[0] == 0);
    assert(result.plan.degreeOffsets[1] == 1);
    assert(result.plan.degreeOffsets[2] == 2);
    assert(result.plan.degreeOffsets[3] == 3);
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4, 8, 12}));
    enableAllPolicy(request);
    isolatePitch(request);
    request.minDegreeOffset = -2;
    request.maxDegreeOffset = 2;
    request.maxLeapDegrees = 1;
    request.requestedContour = MelodicContourId::LeapReturn;
    request.requestedOperation = MelodicMotifOperationId::ChangeTerminal;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assertBounds(result.plan, -2, 2, 1);
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({0}));
    request.rhythmPlan.continuations = stepBit(5);
    assert(realizeMelodicPitchIntent(request).status ==
           MelodicPitchIntentStatus::InvalidRequest);
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4, 8}));
    request.maxOnsets = 2;
    assert(realizeMelodicPitchIntent(request).status ==
           MelodicPitchIntentStatus::InvalidRequest);
  }

  // Diversity is opt-in: the test explicitly enables the wider vocabulary.
  {
    bool sawDifferent = false;
    MelodicPitchIntentResult first{};
    for (uint16_t ordinal = 0; ordinal < 256; ++ordinal) {
      MelodicPitchIntentRequest request = requestFor(mask({1, 5, 10}));
      enableAllPolicy(request);
      request.maxOnsets = 4;
      request.generation.phraseOrdinal = ordinal;
      const auto result = realizeMelodicPitchIntent(request);
      assert(result.status == MelodicPitchIntentStatus::Ok);
      assert(result.plan.onsetCount <= request.maxOnsets);
      assert(validTopology(result.plan.onsets, result.plan.continuations));
      assertBounds(result.plan, -7, 7, 4);
      if (ordinal == 0) {
        first = result;
      } else if (result.plan.rhythmOperation != first.plan.rhythmOperation ||
                 result.plan.contour != first.plan.contour ||
                 result.plan.operation != first.plan.operation ||
                 result.plan.onsets != first.plan.onsets ||
                 std::memcmp(result.plan.degreeOffsets,
                             first.plan.degreeOffsets,
                             sizeof(result.plan.degreeOffsets)) != 0) {
        sawDifferent = true;
      }
    }
    assert(sawDifferent);
  }

  return 0;
}
