#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/tonal/tonal_materializer.h"

using namespace GroovePuterRhythm;

namespace {

void testGlobalScaleAcrossHarmonicRoot() {
  TonalMaterializationRequest request{};
  request.rootPitchClass = 0;  // C
  request.scaleTypeValue = kScaleMajor;
  request.minMidi = 48;
  request.maxMidi = 71;
  request.maxAdjacentLeapSemitones = 12;

  request.onsets = stepBit(0);
  request.harmonicEventOnsets = stepBit(0);
  request.progression.id = ProgressionId::StaticModal;
  request.progression.eventCount = 1;
  request.progression.events[0] = HarmonicEvent{
      6, ChordQuality::Triad, 0};  // B in C major.

  // +1 is one degree in the global C-major scale. Relative to harmonic degree
  // B this must resolve to C, i.e. +1 semitone, not C# from a transposed
  // B-major-like interpretation of the ScaleType table.
  request.tonalOffsets[0] = 1;

  const TonalMaterializationResult result = materializeTonalIntent(request);
  assert(result.status == TonalMaterializationStatus::Ok);
  assert(result.projectionStatus == TonalProjectionStatus::Ok);
  assert(result.plan.onsetCount == 1);
  assert(result.plan.onsetSteps[0] == 0);
  assert(result.plan.midiNotes[0] == 60);  // C4.
  assert((result.plan.midiNotes[0] % 12) == 0);
}

void testIndependentHarmonicEventAnchors() {
  TonalMaterializationRequest request{};
  request.rootPitchClass = 0;  // C
  request.scaleTypeValue = kScaleMajor;
  request.minMidi = 48;
  request.maxMidi = 71;
  request.maxAdjacentLeapSemitones = 12;

  request.onsets = static_cast<StepMask>(stepBit(0) | stepBit(8));
  request.harmonicEventOnsets = request.onsets;
  request.progression.id = ProgressionId::BorrowedLift;
  request.progression.eventCount = 2;
  request.progression.events[0] = HarmonicEvent{
      0, ChordQuality::Triad, 0};
  request.progression.events[1] = HarmonicEvent{
      6, ChordQuality::Triad, 2};  // B + 2 semitones => C# event root.

  // Exact semitone intent is relative to the active event root. The second
  // event asks for an octave above C#. A whole-bar C-root anchor would require
  // offsets 0 and +25 and cannot fit 48..71, while independent event anchors
  // can represent both targets without octave folding.
  request.semitoneOffsetOrdinals = 0x0003u;
  request.tonalOffsets[0] = 0;
  request.tonalOffsets[1] = 12;

  const TonalMaterializationResult result = materializeTonalIntent(request);
  assert(result.status == TonalMaterializationStatus::Ok);
  assert(result.projectionStatus == TonalProjectionStatus::Ok);
  assert(result.plan.onsetCount == 2);
  assert(result.plan.onsetSteps[0] == 0);
  assert(result.plan.onsetSteps[1] == 8);
  assert(result.plan.midiNotes[0] >= 48 && result.plan.midiNotes[0] <= 71);
  assert(result.plan.midiNotes[1] >= 48 && result.plan.midiNotes[1] <= 71);
  assert((result.plan.midiNotes[0] % 12) == 0);   // C
  assert((result.plan.midiNotes[1] % 12) == 1);   // C#
}

}  // namespace

int main() {
  testGlobalScaleAcrossHarmonicRoot();
  testIndependentHarmonicEventAnchors();
  std::cout << "Stage 15 global-scale/event-local harmonic regression: OK\n";
  return 0;
}
