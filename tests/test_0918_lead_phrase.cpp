#include <cassert>
#include <cstring>

#include "src/generation/roles/melodic_pitch_intent.h"
#include "src/generation/tonal/tonal_materializer.h"

using namespace GroovePuterRhythm;

int main() {
  MelodicPitchIntentRequest request{};
  request.archetypeId = 401;
  request.generation.projectSeed = 0x918;
  request.rhythmPlan.rhythmId = MelodicRhythmId::EighthArp;
  request.rhythmPlan.onsets = 0x5555;
  request.policy.allowedContours |= melodicContourBit(MelodicContourId::MotifAnswer);
  request.requestedContour = MelodicContourId::MotifAnswer;
  const auto result = realizeMelodicPitchIntent(request);
  assert(result.status == MelodicPitchIntentStatus::Ok);
  assert(result.plan.onsets == 0x5555 && result.plan.continuations == 0);
  assert(result.plan.onsetCount == 8);
  // Repeat the opening gesture, change the answer, return to the anchor.
  const int8_t expected[] = {0, 1, 3, 1, 0, 1, 2, 0};
  assert(std::memcmp(result.plan.degreeOffsets, expected, sizeof(expected)) == 0);

  // The same gesture follows every project root/scale through the real tonal
  // materializer; it must not smuggle C-major MIDI notes into other keys.
  for (uint8_t scale = 0; scale < kScaleTypeCount; ++scale) {
    for (uint8_t root = 0; root < 12; ++root) {
      TonalMaterializationRequest tonalRequest{};
      tonalRequest.onsets = result.plan.onsets;
      tonalRequest.progression.id = ProgressionId::StaticModal;
      tonalRequest.progression.eventCount = 1;
      tonalRequest.rootPitchClass = root;
      tonalRequest.scaleTypeValue = scale;
      tonalRequest.minMidi = 48;
      tonalRequest.maxMidi = 71;
      std::memcpy(tonalRequest.tonalOffsets, result.plan.degreeOffsets,
                  sizeof(tonalRequest.tonalOffsets));
      const auto tonal = materializeTonalIntent(tonalRequest);
      assert(tonal.status == TonalMaterializationStatus::Ok);
      assert(tonal.plan.onsets == result.plan.onsets);
      const auto definition = scaleDefinitionFor(scale);
      for (uint8_t i = 0; i < tonal.plan.onsetCount; ++i) {
        const uint8_t pc = (tonal.plan.midiNotes[i] + 12 - root) % 12;
        bool inScale = false;
        for (uint8_t degree = 0; degree < definition.count; ++degree)
          inScale |= pc == definition.intervals[degree];
        assert(inScale);
      }
    }
  }

  // All supported densities and leap bounds must preserve timing and determinism.
  for (unsigned count = 1; count <= 16; ++count) {
    request.rhythmPlan.onsets = static_cast<StepMask>((1u << count) - 1u);
    for (unsigned leap = 0; leap <= 4; ++leap) {
      request.maxLeapDegrees = leap;
      const auto a = realizeMelodicPitchIntent(request);
      const auto b = realizeMelodicPitchIntent(request);
      assert(a.status == MelodicPitchIntentStatus::Ok);
      assert(a.plan.onsets == request.rhythmPlan.onsets);
      assert(a.plan.onsetCount == count);
      assert(std::memcmp(a.plan.degreeOffsets, b.plan.degreeOffsets,
                         sizeof(a.plan.degreeOffsets)) == 0);
      for (unsigned i = 1; i < count; ++i) {
        const int delta = a.plan.degreeOffsets[i] - a.plan.degreeOffsets[i - 1];
        assert(delta >= -static_cast<int>(leap) && delta <= static_cast<int>(leap));
      }
    }
  }
}
