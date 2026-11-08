// Pins TODAY's sharing of kGenerativePresets slots (genre_manager.cpp).
// Stage14 modes 9..15 reuse base-mode presets instead of owning one. Wave 2
// changes this on purpose; this test must fail then so the change is visible.
#include "src/dsp/genre_manager.h"

#include <cassert>
#include <cstdio>

SerialMock Serial;
SDMock SD;

namespace {

bool sameParams(const GenerativeParams& a, const GenerativeParams& b) {
  return a.minNotes == b.minNotes && a.maxNotes == b.maxNotes &&
         a.minOctave == b.minOctave && a.maxOctave == b.maxOctave &&
         a.slideProbability == b.slideProbability &&
         a.accentProbability == b.accentProbability &&
         a.gateLengthMultiplier == b.gateLengthMultiplier &&
         a.swingAmount == b.swingAmount &&
         a.microTimingAmount == b.microTimingAmount &&
         a.velocityMin == b.velocityMin && a.velocityMax == b.velocityMax &&
         a.preferDownbeats == b.preferDownbeats &&
         a.allowRepeats == b.allowRepeats && a.rootNoteBias == b.rootNoteBias &&
         a.ghostProbability == b.ghostProbability &&
         a.chromaticProbability == b.chromaticProbability &&
         a.sparseKick == b.sparseKick && a.sparseHats == b.sparseHats &&
         a.noAccents == b.noAccents && a.fillProbability == b.fillProbability &&
         a.drumSyncopation == b.drumSyncopation &&
         a.drumPreferOffbeat == b.drumPreferOffbeat &&
         a.drumVoiceCount == b.drumVoiceCount;
}

// Owner slot of each mode's preset today (index of the base mode it shares).
constexpr int kOwnerToday[kGenerativeModeCount] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8,   // base modes own their preset
    1,                           // House     -> Outrun
    2,                           // Techno    -> Darksynth
    6,                           // HipHop    -> TripHop
    6,                           // FunkSoul  -> TripHop
    7,                           // UkGarage  -> Broken
    7,                           // DnB       -> Broken
    6,                           // LoFi      -> TripHop
};

}  // namespace

int main() {
  // The 9 base presets are pairwise distinct, so equality below means sharing.
  for (int i = 0; i < 9; ++i)
    for (int j = i + 1; j < 9; ++j)
      assert(!sameParams(kGenerativePresets[i], kGenerativePresets[j]));

  // Full partition: slots i,j are equal exactly when they share an owner.
  for (int i = 0; i < kGenerativeModeCount; ++i) {
    for (int j = 0; j < kGenerativeModeCount; ++j) {
      const bool shared = kOwnerToday[i] == kOwnerToday[j];
      if (sameParams(kGenerativePresets[i], kGenerativePresets[j]) != shared) {
        std::printf("preset slot sharing changed: modes %d and %d\n", i, j);
        return 1;
      }
    }
  }
  std::printf("genre preset slot map pinned: PASS\n");
  return 0;
}
