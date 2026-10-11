#include <cassert>
#include <cstring>
#include <cstdio>

#include "src/generation/roles/bass_pitch_behavior.h"
#include "src/generation/migration/tonal_pattern_adapter.h"
#include "src/dsp/musical_development.h"

using namespace GroovePuterRhythm;

int main() {
  BassRhythmRequest rhythmRequest{};
  rhythmRequest.requestedId = BassRhythmId::ConnectedHook;
  rhythmRequest.archetypeId = 401;
  const auto rhythm = realizeBassRhythm(rhythmRequest);
  assert(rhythm.status == BassRhythmStatus::Ok);
  const StepMask expectedOnsets = stepBit(0) | stepBit(3) | stepBit(7) |
      stepBit(10) | stepBit(14);
  const StepMask expectedHolds = stepBit(1) | stepBit(2) | stepBit(8) |
      stepBit(9);
  assert(rhythm.plan.onsets == expectedOnsets);
  assert(rhythm.plan.continuations == expectedHolds);

  BassPitchBehaviorRequest pitchRequest{};
  pitchRequest.rhythmPlan = rhythm.plan;
  pitchRequest.archetypeId = rhythmRequest.archetypeId;
  pitchRequest.policy.allowedContours = kAllBassPitchContours;
  pitchRequest.policy.allowedArticulations = kAllBassArticulationStyles;
  pitchRequest.requestedContour = BassPitchContourId::StepApproach;
  pitchRequest.requestedArticulation = BassArticulationStyleId::LegatoApproach;
  const auto pitch = realizeBassPitchBehavior(pitchRequest);
  assert(pitch.status == BassPitchBehaviorStatus::Ok);
  assert(pitch.plan.onsets == expectedOnsets);
  assert(pitch.plan.continuations == expectedHolds);
  assert(pitch.plan.slideIntoOnsets == stepBit(10));

  // A semantic slide must survive tonal realization and the actual adapter.
  for (uint8_t root = 0; root < 12; ++root) {
    TonalMaterializationRequest tonalRequest{};
    tonalRequest.onsets = pitch.plan.onsets;
    tonalRequest.continuations = pitch.plan.continuations;
    tonalRequest.progression.id = ProgressionId::StaticModal;
    tonalRequest.progression.eventCount = 1;
    tonalRequest.rootPitchClass = root;
    tonalRequest.scaleTypeValue = kScaleDorian;
    tonalRequest.minMidi = 24;
    tonalRequest.maxMidi = 47;
    std::memcpy(tonalRequest.tonalOffsets, pitch.plan.tonalOffsets,
                sizeof(tonalRequest.tonalOffsets));
    const auto tonal = materializeTonalIntent(tonalRequest);
    assert(tonal.status == TonalMaterializationStatus::Ok);
    SynthPattern source{}, physical{};
    assert(adaptTonalPlanToSynthPattern(source, tonal.plan,
        pitch.plan.accentOnsets, pitch.plan.slideIntoOnsets, physical) ==
        TonalPatternAdaptStatus::Ok);
    for (uint8_t step = 0; step < 16; ++step) {
      const bool occupied = ((expectedOnsets | expectedHolds) & stepBit(step)) != 0;
      assert((physical.steps[step].note >= 0) == occupied);
      if ((pitch.plan.slideIntoOnsets & stepBit(step)) != 0) {
        assert(step > 0 && physical.steps[step - 1].note >= 0);
        assert(physical.steps[step].slide);
        assert(physical.steps[step].note != physical.steps[step - 1].note);
      }
    }
    // Pattern projection counts held steps as runtime events. The cell must
    // remain compatible with the existing default THIN density-drop policy.
    PhraseRuntime::RuntimeSynthEventBuffer runtime{};
    PhraseRuntime::PatternProjectionSettings projection{};
    assert(PhraseRuntime::projectPatternToRuntimeEvents(
        physical, projection, runtime) == PhraseRuntime::PatternProjectionStatus::Ready);
    GroovePuterDevelopment::DevelopmentRequest development{};
    development.transformation = GroovePuterDevelopment::TransformationKind::Thin;
    const auto thinned = GroovePuterDevelopment::developCandidate(runtime, development);
    if (!thinned.success) std::fprintf(stderr,
        "THIN: source=%u candidate=%u drop=%d reason=%s\n",
        unsigned(runtime.count), unsigned(thinned.candidate.count),
        int(thinned.evidence.rhythm.densityDelta),
        thinned.classification.failureReason ? thinned.classification.failureReason : "none");
    assert(thinned.success);
  }

  // A removed anchor cannot leave orphaned continuations or fake connectivity.
  rhythmRequest.protectedSpace = stepBit(7) | stepBit(12);
  const auto protectedRhythm = realizeBassRhythm(rhythmRequest);
  assert((protectedRhythm.plan.onsets & stepBit(7)) == 0);
  assert((protectedRhythm.plan.continuations &
      (stepBit(8) | stepBit(9) | stepBit(12) | stepBit(13))) == 0);
  pitchRequest.rhythmPlan = protectedRhythm.plan;
  const auto protectedPitch = realizeBassPitchBehavior(pitchRequest);
  assert(protectedPitch.status == BassPitchBehaviorStatus::Ok);
  assert((protectedPitch.plan.slideIntoOnsets &
      (stepBit(10) | stepBit(14))) == 0);
}
