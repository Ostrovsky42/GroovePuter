#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/generation/roles/melodic_pitch_intent.h"

using namespace GroovePuterRhythm;

namespace {

StepMask mask(std::initializer_list<uint8_t> steps) {
  StepMask result = 0;
  for (uint8_t step : steps)
    result = static_cast<StepMask>(result | stepBit(step));
  return result;
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
  request.minDegreeOffset = -7;
  request.maxDegreeOffset = 7;
  request.maxLeapDegrees = 4;
  request.maxOnsets = kStepsPerBar;
  return request;
}

void assertSame(const MelodicPitchIntentResult& a,
                const MelodicPitchIntentResult& b) {
  assert(std::memcmp(&a, &b, sizeof(a)) == 0);
}

void assertBounds(const MelodicPitchIntentPlan& plan,
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
    MelodicPitchIntentRequest request = requestFor(0);
    request.requestedContour = MelodicContourId::LeapReturn;
    request.requestedOperation = MelodicMotifOperationId::ChangeTerminal;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::ValidButEmpty);
    assert(result.plan.onsetCount == 0);
    assert(result.plan.onsets == 0);
    assert(result.plan.continuations == 0);
  }

  {
    const MelodicPitchIntentRequest request = requestFor(mask({1, 5, 10, 14}));
    assertSame(realizeMelodicPitchIntent(request),
               realizeMelodicPitchIntent(request));
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4, 8, 12}));
    request.requestedContour = MelodicContourId::StepUp;
    request.requestedOperation = MelodicMotifOperationId::None;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.onsetCount == 4);
    assert(result.plan.degreeOffsets[0] == 0);
    assert(result.plan.degreeOffsets[1] == 1);
    assert(result.plan.degreeOffsets[2] == 2);
    assert(result.plan.degreeOffsets[3] == 3);
    assert(result.plan.onsets == request.rhythmPlan.onsets);
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
    request.rhythmPlan.continuations = mask({1, 2, 5});
    request.requestedContour = MelodicContourId::RepeatThenUp;
    request.requestedOperation = MelodicMotifOperationId::TerminalReturn;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::Ok);
    assert(result.plan.onsets == request.rhythmPlan.onsets);
    assert(result.plan.continuations == request.rhythmPlan.continuations);
    assert(result.plan.onsetSteps[0] == 0);
    assert(result.plan.onsetSteps[1] == 4);
    assert(result.plan.onsetSteps[2] == 8);
    assert(result.plan.onsetSteps[3] == 12);
    assert(result.plan.degreeOffsets[3] == 0);
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
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::InvalidRequest);
  }

  {
    MelodicPitchIntentRequest request = requestFor(mask({0, 4, 8}));
    request.maxOnsets = 2;
    const auto result = realizeMelodicPitchIntent(request);
    assert(result.status == MelodicPitchIntentStatus::InvalidRequest);
  }

  {
    bool sawDifferentShape = false;
    MelodicPitchIntentResult first{};
    for (uint16_t ordinal = 0; ordinal < 128; ++ordinal) {
      MelodicPitchIntentRequest request = requestFor(mask({1, 5, 10, 14}));
      request.generation.phraseOrdinal = ordinal;
      const auto result = realizeMelodicPitchIntent(request);
      assert(result.status == MelodicPitchIntentStatus::Ok);
      assertBounds(result.plan, -7, 7, 4);
      if (ordinal == 0) {
        first = result;
      } else if (result.plan.contour != first.plan.contour ||
                 result.plan.operation != first.plan.operation ||
                 std::memcmp(result.plan.degreeOffsets,
                             first.plan.degreeOffsets,
                             sizeof(result.plan.degreeOffsets)) != 0) {
        sawDifferentShape = true;
      }
    }
    assert(sawDifferentShape);
  }

  return 0;
}
