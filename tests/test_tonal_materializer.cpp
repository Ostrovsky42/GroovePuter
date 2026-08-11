#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/tonal/tonal_materializer.h"

using namespace GroovePuterRhythm;

namespace {

TonalMaterializationRequest baseRequest() {
  TonalMaterializationRequest request{};
  request.rootPitchClass = 0;
  request.scaleTypeValue = kScaleDorian;
  request.minMidi = 36;
  request.maxMidi = 71;
  request.maxAdjacentLeapSemitones = 24;
  request.progression.id = ProgressionId::StaticModal;
  request.progression.eventCount = 1;
  request.progression.events[0] = HarmonicEvent{0, ChordQuality::Triad, 0};
  return request;
}

void testStaticDegreeOffsets() {
  TonalMaterializationRequest request = baseRequest();
  request.onsets = static_cast<StepMask>(stepBit(0) | stepBit(4) | stepBit(8));
  request.tonalOffsets[0] = 0;
  request.tonalOffsets[1] = 1;
  request.tonalOffsets[2] = -1;

  const TonalMaterializationResult result = materializeTonalIntent(request);
  assert(result.status == TonalMaterializationStatus::Ok);
  assert(result.projectionStatus == TonalProjectionStatus::Ok);
  assert(result.plan.onsetCount == 3);
  assert(result.plan.onsetSteps[0] == 0);
  assert(result.plan.onsetSteps[1] == 4);
  assert(result.plan.onsetSteps[2] == 8);
  assert(result.plan.midiNotes[0] == 48);
  assert(result.plan.midiNotes[1] == 50);
  assert(result.plan.midiNotes[2] == 46);
}

void testHarmonicTimelineSelectsEvents() {
  TonalMaterializationRequest request = baseRequest();
  request.scaleTypeValue = kScaleMajor;
  request.onsets = static_cast<StepMask>(
      stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12));
  request.harmonicEventOnsets =
      static_cast<StepMask>(stepBit(0) | stepBit(8));
  request.progression.id = ProgressionId::PopCycle;
  request.progression.eventCount = 2;
  request.progression.events[0] = HarmonicEvent{0, ChordQuality::Triad, 0};
  request.progression.events[1] = HarmonicEvent{4, ChordQuality::Triad, 0};

  const TonalMaterializationResult result = materializeTonalIntent(request);
  assert(result.status == TonalMaterializationStatus::Ok);
  // C3=48 is the feasible C root nearest the center of [36,71].
  assert(result.plan.midiNotes[0] == 48);
  assert(result.plan.midiNotes[1] == 48);
  assert(result.plan.midiNotes[2] == 55);
  assert(result.plan.midiNotes[3] == 55);
}

void testChromaticRootOffsetIsExact() {
  TonalMaterializationRequest request = baseRequest();
  request.scaleTypeValue = kScaleMinor;
  request.onsets = static_cast<StepMask>(stepBit(0) | stepBit(8));
  request.harmonicEventOnsets =
      static_cast<StepMask>(stepBit(0) | stepBit(8));
  request.progression.id = ProgressionId::ParallelShift;
  request.progression.eventCount = 2;
  request.progression.events[0] = HarmonicEvent{0, ChordQuality::Minor7, 0};
  request.progression.events[1] = HarmonicEvent{0, ChordQuality::Minor7, 1};

  const TonalMaterializationResult result = materializeTonalIntent(request);
  assert(result.status == TonalMaterializationStatus::Ok);
  assert(result.plan.midiNotes[1] == result.plan.midiNotes[0] + 1);
}

void testTaggedBassFifthIsExact() {
  TonalMaterializationRequest request = baseRequest();
  request.scaleTypeValue = kScaleLocrian;
  request.onsets = static_cast<StepMask>(stepBit(0) | stepBit(4));
  request.tonalOffsets[0] = 0;
  request.tonalOffsets[1] = 7;
  request.semitoneOffsetOrdinals = static_cast<uint16_t>(1u << 1u);

  const TonalMaterializationResult result = materializeTonalIntent(request);
  assert(result.status == TonalMaterializationStatus::Ok);
  assert(result.plan.midiNotes[1] == result.plan.midiNotes[0] + 7);
}

void testEmptyPlanIsValid() {
  TonalMaterializationRequest request = baseRequest();
  const TonalMaterializationResult result = materializeTonalIntent(request);
  assert(result.status == TonalMaterializationStatus::ValidButEmpty);
  assert(result.plan.onsetCount == 0);
}

void testDynamicTimelineMustMatchEventCount() {
  TonalMaterializationRequest request = baseRequest();
  request.onsets = stepBit(0);
  request.harmonicEventOnsets = stepBit(0);
  request.progression.id = ProgressionId::PopCycle;
  request.progression.eventCount = 2;
  request.progression.events[0] = HarmonicEvent{0, ChordQuality::Triad, 0};
  request.progression.events[1] = HarmonicEvent{4, ChordQuality::Triad, 0};
  const TonalMaterializationResult result = materializeTonalIntent(request);
  assert(result.status == TonalMaterializationStatus::InvalidRequest);
}

void testRegisterFailureIsAtomic() {
  TonalMaterializationRequest request = baseRequest();
  request.minMidi = 36;
  request.maxMidi = 40;
  request.onsets = static_cast<StepMask>(stepBit(0) | stepBit(4));
  request.tonalOffsets[1] = 7;
  request.semitoneOffsetOrdinals = static_cast<uint16_t>(1u << 1u);
  const TonalMaterializationResult result = materializeTonalIntent(request);
  assert(result.status == TonalMaterializationStatus::ProjectionFailed);
  assert(result.projectionStatus == TonalProjectionStatus::NoteOutOfRegister);
  assert(result.plan.midiNotes[0] == 0);
  assert(result.plan.midiNotes[1] == 0);
}

}  // namespace

int main() {
  testStaticDegreeOffsets();
  testHarmonicTimelineSelectsEvents();
  testChromaticRootOffsetIsExact();
  testTaggedBassFifthIsExact();
  testEmptyPlanIsValid();
  testDynamicTimelineMustMatchEventCount();
  testRegisterFailureIsAtomic();
  std::cout << "Stage 15 Tonal Materializer host matrix: OK\n";
  return 0;
}
