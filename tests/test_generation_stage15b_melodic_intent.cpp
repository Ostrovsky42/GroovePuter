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
    if ((onsets & bit) != 0) active = true;
    else if ((continuations & bit) != 0) {
      if (!active) return false;
    } else active = false;
  }
  return true;
}

MelodicPitchIntentRequest requestFor(StepMask onsets) {
  MelodicPitchIntentRequest request{};
  request.rhythmPlan.rhythmId = MelodicRhythmId::SyncopatedMotif;
  request.rhythmPlan.motif.shape = MotifShapeId::SourceOrder;
  request.rhythmPlan.onsets = onsets;
  request.family = RhythmFamily::HipHopBackbeat;
  request.archetypeId = 401;
  request.generation.projectSeed = 0x12345678u;
  request.generation.phraseOrdinal = 9;
  request.barOrdinal = 3;
  request.requestedRhythmOperation = MelodicRhythmOperationId::Preserve;
  request.minDegreeOffset = -7;
  request.maxDegreeOffset = 7;
  request.maxLeapDegrees = 4;
  request.maxOnsets = kStepsPerBar;
  return request;
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
  {
    MelodicPitchIntentRequest request = requestFor(0);
    request.requestedContour = MelodicContourId::LeapReturn;
    request.requestedOperation = MelodicMotifOperationId::ChangeTerminal;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::ValidButEmpty);
    assert(result.plan.onsets == 0);
    assert(result.plan.continuations == 0);
  }

  {
    const auto request = requestFor(mask({1, 5, 10, 14}));
    assertSame(realizeMelodicPitchIntent(request),
               realizeMelodicPitchIntent(request));
  }

  // Stage 14 onset blocking and continuation legality are separate contracts.
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

  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4, 10}));
    request.rhythmPlan.continuations = mask({5, 6, 11});
    request.requestedRhythmOperation = MelodicRhythmOperationId::ControlledRest;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.rhythmOperation == MelodicRhythmOperationId::ControlledRest);
    assert(result.plan.onsetCount == 2);
    assert((result.plan.onsets & stepBit(0)) != 0);
    assert(validTopology(result.plan.onsets, result.plan.continuations));
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({8}));
    request.requestedRhythmOperation = MelodicRhythmOperationId::ControlledRest;
    request.allowEmptyBar = true;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::ValidButEmpty);
    assert(result.plan.rhythmOperation == MelodicRhythmOperationId::ControlledRest);
    assert(result.plan.onsets == 0);
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4, 10}));
    request.rhythmPlan.continuations = mask({5, 6});
    request.requestedRhythmOperation = MelodicRhythmOperationId::ShiftInteriorLater;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.rhythmOperation == MelodicRhythmOperationId::ShiftInteriorLater);
    assert(result.plan.onsetCount == 3);
    assert(result.plan.onsets != request.rhythmPlan.onsets);
    assert((result.plan.onsets & stepBit(0)) != 0);
    assert(validTopology(result.plan.onsets, result.plan.continuations));
  }

  // Compatibility wins when an onset cannot move into the legal onset mask.
  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4}));
    request.requestedRhythmOperation = MelodicRhythmOperationId::ShiftInteriorLater;
    request.allowedOnsetSteps = request.rhythmPlan.onsets;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.rhythmOperation == MelodicRhythmOperationId::Preserve);
    assert(result.plan.onsets == request.rhythmPlan.onsets);
  }

  // A shifted held note must also fit the distinct continuation mask.
  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4}));
    request.rhythmPlan.continuations = mask({5, 6});
    request.requestedRhythmOperation = MelodicRhythmOperationId::ShiftInteriorLater;
    request.allowedOnsetSteps = mask({0, 4, 5});
    request.allowedContinuationSteps = mask({5, 6});
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.rhythmOperation == MelodicRhythmOperationId::Preserve);
    assert(result.plan.onsets == request.rhythmPlan.onsets);
    assert(result.plan.continuations == request.rhythmPlan.continuations);
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 8}));
    request.requestedRhythmOperation = MelodicRhythmOperationId::TerminalEcho;
    request.maxOnsets = 3;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.rhythmOperation == MelodicRhythmOperationId::TerminalEcho);
    assert(result.plan.onsetCount == 3);
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 8, 12}));
    request.requestedRhythmOperation = MelodicRhythmOperationId::TerminalEcho;
    request.maxOnsets = 3;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.rhythmOperation == MelodicRhythmOperationId::Preserve);
  }

  // Terminal echo cannot land on a held continuation cell.
  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 8}));
    request.rhythmPlan.continuations = mask({9, 10, 11, 12, 13, 14, 15});
    request.requestedRhythmOperation = MelodicRhythmOperationId::TerminalEcho;
    request.allowedOnsetSteps = static_cast<StepMask>(stepBit(0) | stepBit(8) | stepBit(10));
    request.maxOnsets = 3;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.rhythmOperation == MelodicRhythmOperationId::Preserve);
    assert(result.plan.onsets == request.rhythmPlan.onsets);
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4, 8, 12}));
    request.requestedContour = MelodicContourId::StepUp;
    request.requestedOperation = MelodicMotifOperationId::None;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.degreeOffsets[0] == 0);
    assert(result.plan.degreeOffsets[1] == 1);
    assert(result.plan.degreeOffsets[2] == 2);
    assert(result.plan.degreeOffsets[3] == 3);
    assertBounds(result.plan, -7, 7, 4);
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4, 8, 12}));
    request.requestedContour = MelodicContourId::StepDown;
    request.requestedOperation = MelodicMotifOperationId::None;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.degreeOffsets[0] == 0);
    assert(result.plan.degreeOffsets[1] == -1);
    assert(result.plan.degreeOffsets[2] == -2);
    assert(result.plan.degreeOffsets[3] == -3);
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 3, 6, 9, 12}));
    request.requestedContour = MelodicContourId::Arch;
    request.requestedOperation = MelodicMotifOperationId::None;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.degreeOffsets[0] == 0);
    assert(result.plan.degreeOffsets[1] == 1);
    assert(result.plan.degreeOffsets[2] == 2);
    assert(result.plan.degreeOffsets[3] == 1);
    assert(result.plan.degreeOffsets[4] == 0);
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 6, 12}));
    request.requestedContour = MelodicContourId::LeapReturn;
    request.requestedOperation = MelodicMotifOperationId::None;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.degreeOffsets[0] == 0);
    assert(result.plan.degreeOffsets[1] == 4);
    assert(result.plan.degreeOffsets[2] == 0);
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 3, 6, 9, 12}));
    request.requestedContour = MelodicContourId::Neighbor;
    request.requestedOperation = MelodicMotifOperationId::None;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.degreeOffsets[0] == 0);
    assert(result.plan.degreeOffsets[1] == 1);
    assert(result.plan.degreeOffsets[2] == 0);
    assert(result.plan.degreeOffsets[3] == -1);
    assert(result.plan.degreeOffsets[4] == 0);
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4, 8, 12}));
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

  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4}));
    request.allowedOnsetSteps = mask({0});
    assert(realizeMelodicPitchIntent(request).status ==
           MelodicPitchIntentStatus::InvalidRequest);
  }

  {
    bool sawDifferentRhythm = false;
    bool sawDifferentPitch = false;
    MelodicPitchIntentResult first{};
    for (uint16_t ordinal = 0; ordinal < 128; ++ordinal) {
      MelodicPitchIntentRequest request = requestFor(mask({1, 5, 10}));
      request.requestedRhythmOperation = MelodicRhythmOperationId::Auto;
      request.maxOnsets = 4;
      request.generation.phraseOrdinal = ordinal;
      const auto result = realizeMelodicPitchIntent(request);
      assert(result.status == MelodicPitchIntentStatus::Ok);
      assert(result.plan.onsetCount <= request.maxOnsets);
      assert(validTopology(result.plan.onsets, result.plan.continuations));
      assert((result.plan.onsets & static_cast<StepMask>(~request.allowedOnsetSteps)) == 0);
      assert((result.plan.continuations &
              static_cast<StepMask>(~request.allowedContinuationSteps)) == 0);
      assertBounds(result.plan, -7, 7, 4);
      if (ordinal == 0) {
        first = result;
        continue;
      }
      if (result.plan.rhythmOperation != first.plan.rhythmOperation ||
          result.plan.onsets != first.plan.onsets ||
          result.plan.continuations != first.plan.continuations)
        sawDifferentRhythm = true;
      if (result.plan.contour != first.plan.contour ||
          result.plan.operation != first.plan.operation ||
          std::memcmp(result.plan.degreeOffsets, first.plan.degreeOffsets,
                      sizeof(result.plan.degreeOffsets)) != 0)
        sawDifferentPitch = true;
    }
    assert(sawDifferentRhythm);
    assert(sawDifferentPitch);
  }

  return 0;
}
