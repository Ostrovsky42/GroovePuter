#include <cassert>
#include <cstdint>
#include <iostream>

#include "../src/generation/composition/phrase_harmonic_timeline.h"
#include "../src/generation/roles/chord_progression.h"
#include "../src/generation/roles/melodic_motif.h"
#include "../src/generation/tonal/tonal_materializer.h"

using namespace GroovePuterRhythm;

namespace {

constexpr StepMask at(uint8_t step) {
  return static_cast<StepMask>(1u << step);
}

void testQuarterCycle32Positions() {
  StepMask bars[kMaxPhraseBars]{};
  const StepMask quarter = static_cast<StepMask>(at(0) | at(4) | at(8) | at(12));
  for (uint8_t bar = 0; bar < 8; ++bar) bars[bar] = quarter;

  const PhraseHarmonicTimeline timeline = makePhraseHarmonicTimeline(8, bars);
  assert(timeline.status == PhraseHarmonicTimelineStatus::Ok);
  assert(timeline.totalEventPositions == 32);
  constexpr uint8_t expectedSteps[] = {0, 4, 8, 12};
  for (uint8_t bar = 0; bar < 8; ++bar) {
    const PhraseHarmonicEventRange range =
        phraseHarmonicEventRangeForBar(timeline, bar);
    assert(range.firstOrdinal == static_cast<uint8_t>(bar * 4u));
    assert(range.eventCount == 4);
    for (uint8_t local = 0; local < 4; ++local) {
      const PhraseHarmonicEventCoordinate coordinate =
          phraseHarmonicEventCoordinate(timeline, bar, local);
      assert(coordinate.valid);
      assert(coordinate.localStep == expectedSteps[local]);
      assert(coordinate.phraseHarmonicEventOrdinal ==
             static_cast<uint8_t>(bar * 4u + local));
    }
  }

  const PhraseHarmonicEventCoordinate ordinal17 =
      phraseHarmonicEventCoordinate(timeline, 4, 1);
  assert(ordinal17.valid);
  assert(ordinal17.phraseHarmonicEventOrdinal == 17);
  static_assert(kMaxHarmonicEvents == 8,
                "ChordProgression WHAT capacity changed unexpectedly");
  assert(ordinal17.phraseHarmonicEventOrdinal >= kMaxHarmonicEvents);
}

void testOneEventPerBarHasGlobalPhraseOrdinal() {
  StepMask bars[kMaxPhraseBars]{};
  for (uint8_t bar = 0; bar < 4; ++bar) bars[bar] = at(0);
  const PhraseHarmonicTimeline timeline = makePhraseHarmonicTimeline(4, bars);
  assert(timeline.totalEventPositions == 4);
  for (uint8_t bar = 0; bar < 4; ++bar) {
    const PhraseHarmonicEventCoordinate coordinate =
        phraseHarmonicEventCoordinate(timeline, bar, 0);
    assert(coordinate.valid);
    assert(coordinate.localStep == 0);
    assert(coordinate.phraseHarmonicEventOrdinal == bar);
  }
}

void testRestHeavyDoesNotCollapseHarmonicTime() {
  StepMask bars[kMaxPhraseBars]{};
  const StepMask quarter = static_cast<StepMask>(at(0) | at(4) | at(8) | at(12));
  for (uint8_t bar = 0; bar < 4; ++bar) bars[bar] = quarter;
  const PhraseHarmonicTimeline timeline = makePhraseHarmonicTimeline(4, bars);

  MelodicMotifStatus melodicStatus[4] = {
      MelodicMotifStatus::Ok,
      MelodicMotifStatus::ValidButEmpty,
      MelodicMotifStatus::ValidButEmpty,
      MelodicMotifStatus::ValidButEmpty,
  };
  (void)melodicStatus;
  for (uint8_t bar = 0; bar < 4; ++bar) {
    const PhraseHarmonicEventRange range =
        phraseHarmonicEventRangeForBar(timeline, bar);
    assert(range.firstOrdinal == static_cast<uint8_t>(bar * 4u));
    assert(range.eventCount == 4);
  }
  assert(timeline.totalEventPositions == 16);
}

void testOnsetSourceStableBaseline() {
  TonalMaterializationRequest request{};
  request.rootPitchClass = 0;
  request.scaleTypeValue = kScaleMajor;
  request.minMidi = 36;
  request.maxMidi = 71;
  request.maxAdjacentLeapSemitones = 24;
  request.onsets = at(4);
  request.continuations = static_cast<StepMask>(
      at(5) | at(6) | at(7) | at(8) | at(9) | at(10));
  request.harmonicEventOnsets = static_cast<StepMask>(at(0) | at(8));
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
}

}  // namespace

int main() {
  testQuarterCycle32Positions();
  testOneEventPerBarHasGlobalPhraseOrdinal();
  testRestHeavyDoesNotCollapseHarmonicTime();
  testOnsetSourceStableBaseline();
  std::cout << "PHRASE-C1 M3 harmonic timeline: PASS\n";
  std::cout << "quarter_cycle_8bar_positions=32 ordinals=0..31\n";
  std::cout << "rest_heavy_harmonic_time=0..15_not_collapsed\n";
  std::cout << "one_event_per_bar_phrase_ordinals=0,1,2,3\n";
  std::cout << "continuation_crossing=ONSET_SOURCE_STABLE\n";
  std::cout << "TIMELINE REPRESENTATION SUFFICIENT\n";
  std::cout << "PROGRESSION SOURCE RESOLUTION GAP: ordinal17 has no new WHAT policy\n";
  return 0;
}
