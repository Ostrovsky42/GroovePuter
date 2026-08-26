#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/roles/chord_progression.h"
#include "src/generation/tonal/tonal_materializer.h"

using namespace GroovePuterRhythm;

namespace {

bool sameEvent(const HarmonicEvent& a, const HarmonicEvent& b) {
  return a.degree == b.degree && a.quality == b.quality &&
         a.rootOffsetSemitones == b.rootOffsetSemitones;
}

GenerationContext fixedGeneration() {
  GenerationContext generation{};
  generation.projectSeed = 0x4d334131u;
  generation.phraseOrdinal = 17;
  return generation;
}

ChordProgressionRequest popCycleRequest(uint8_t eventCount) {
  ChordProgressionRequest request{};
  request.requestedId = ProgressionId::PopCycle;
  request.family = RhythmFamily::FourFloor;
  request.generation = fixedGeneration();
  request.harmonicEventCount = eventCount;
  request.phraseBars = 1;
  return request;
}

TonalMaterializationRequest tonalBase() {
  TonalMaterializationRequest request{};
  request.rootPitchClass = 0;
  request.scaleTypeValue = kScaleMajor;
  request.minMidi = 36;
  request.maxMidi = 71;
  request.maxAdjacentLeapSemitones = 24;
  return request;
}

void testStaticFourBarControl() {
  TonalMaterializationRequest request = tonalBase();
  request.onsets = static_cast<StepMask>(stepBit(0) | stepBit(8));
  request.progression.id = ProgressionId::StaticModal;
  request.progression.eventCount = 1;
  request.progression.events[0] = HarmonicEvent{0, ChordQuality::Triad, 0};

  uint8_t firstNotes[2]{};
  for (uint8_t bar = 0; bar < 4; ++bar) {
    const TonalMaterializationResult result = materializeTonalIntent(request);
    assert(result.status == TonalMaterializationStatus::Ok);
    assert(result.plan.onsetCount == 2);
    if (bar == 0) {
      firstNotes[0] = result.plan.midiNotes[0];
      firstNotes[1] = result.plan.midiNotes[1];
    } else {
      assert(result.plan.midiNotes[0] == firstNotes[0]);
      assert(result.plan.midiNotes[1] == firstNotes[1]);
    }
  }
  std::cout << "A static_4bar=PASS\n";
}

void testMovingOneEventPerBarRestartsAtEventZero() {
  const ChordProgressionResult phraseShape =
      realizeChordProgression(popCycleRequest(4));
  assert(phraseShape.status == ChordProgressionStatus::Ok);
  assert(phraseShape.plan.eventCount == 4);

  bool laterSourceExists = false;
  for (uint8_t index = 1; index < phraseShape.plan.eventCount; ++index) {
    if (!sameEvent(phraseShape.plan.events[0], phraseShape.plan.events[index])) {
      laterSourceExists = true;
      break;
    }
  }
  assert(laterSourceExists);

  for (uint8_t bar = 0; bar < 4; ++bar) {
    const ChordProgressionResult local =
        realizeChordProgression(popCycleRequest(1));
    assert(local.status == ChordProgressionStatus::Ok);
    assert(local.plan.eventCount == 1);
    assert(sameEvent(local.plan.events[0], phraseShape.plan.events[0]));
  }
  std::cout << "B one_event_per_bar=RESET_TO_LOCAL_EVENT_0\n";
}

void testInsideBarAndExactBoundaryUseLocalTimeline() {
  TonalMaterializationRequest request = tonalBase();
  request.onsets = static_cast<StepMask>(stepBit(7) | stepBit(8));
  request.harmonicEventOnsets =
      static_cast<StepMask>(stepBit(0) | stepBit(8));
  request.progression.id = ProgressionId::PopCycle;
  request.progression.eventCount = 2;
  request.progression.events[0] = HarmonicEvent{0, ChordQuality::Triad, 0};
  request.progression.events[1] = HarmonicEvent{4, ChordQuality::Triad, 0};

  const TonalMaterializationResult result = materializeTonalIntent(request);
  assert(result.status == TonalMaterializationStatus::Ok);
  assert(result.plan.onsetCount == 2);
  assert(result.plan.onsetSteps[0] == 7);
  assert(result.plan.onsetSteps[1] == 8);
  assert(result.plan.midiNotes[0] == 48);
  assert(result.plan.midiNotes[1] == 55);
  std::cout << "C inside_bar=PASS\n";
  std::cout << "D exact_change_onset=NEW_SOURCE\n";
}

void testContinuationIsNotRepitchedAtHarmonicChange() {
  TonalMaterializationRequest request = tonalBase();
  request.onsets = stepBit(4);
  request.continuations = static_cast<StepMask>(
      stepBit(5) | stepBit(6) | stepBit(7) | stepBit(8) |
      stepBit(9) | stepBit(10));
  request.harmonicEventOnsets =
      static_cast<StepMask>(stepBit(0) | stepBit(8));
  request.progression.id = ProgressionId::PopCycle;
  request.progression.eventCount = 2;
  request.progression.events[0] = HarmonicEvent{0, ChordQuality::Triad, 0};
  request.progression.events[1] = HarmonicEvent{4, ChordQuality::Triad, 0};

  const TonalMaterializationResult result = materializeTonalIntent(request);
  assert(result.status == TonalMaterializationStatus::Ok);
  assert(result.plan.onsetCount == 1);
  assert(result.plan.onsetSteps[0] == 4);
  assert(result.plan.midiNotes[0] == 48);
  assert(result.plan.continuations == request.continuations);
  std::cout << "E continuation_crossing=ONSET_SOURCE_STABLE\n";
}

void testEmptyMelodicBarDoesNotEncodeHarmonicAdvance() {
  TonalMaterializationRequest request = tonalBase();
  request.harmonicEventOnsets =
      static_cast<StepMask>(stepBit(0) | stepBit(8));
  request.progression.id = ProgressionId::PopCycle;
  request.progression.eventCount = 2;
  request.progression.events[0] = HarmonicEvent{0, ChordQuality::Triad, 0};
  request.progression.events[1] = HarmonicEvent{4, ChordQuality::Triad, 0};

  const TonalMaterializationResult result = materializeTonalIntent(request);
  assert(result.status == TonalMaterializationStatus::ValidButEmpty);
  assert(result.plan.onsetCount == 0);
  std::cout << "F empty_melodic_bar=VALID_BUT_NO_CROSSBAR_SOURCE_STATE\n";
}

}  // namespace

int main() {
  testStaticFourBarControl();
  testMovingOneEventPerBarRestartsAtEventZero();
  testInsideBarAndExactBoundaryUseLocalTimeline();
  testContinuationIsNotRepitchedAtHarmonicChange();
  testEmptyMelodicBarDoesNotEncodeHarmonicAdvance();
  std::cout << "M3-A1 focused harmonic crossing characterization: OK\n";
  return 0;
}
