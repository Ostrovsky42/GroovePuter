#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/tonal/tonal_materializer.h"

using namespace GroovePuterRhythm;

int main() {
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

  std::cout << "Stage 15 global-scale harmonic event regression: OK\n";
  return 0;
}
