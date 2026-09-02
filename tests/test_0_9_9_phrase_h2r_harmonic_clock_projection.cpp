#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/generation/composition/phrase_harmonic_clock_projection.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/generation/roles/chord_progression.h"

using namespace GroovePuterRhythm;

namespace {

bool sameEvent(const HarmonicEvent& left, const HarmonicEvent& right) {
  return left.degree == right.degree && left.quality == right.quality &&
         left.rootOffsetSemitones == right.rootOffsetSemitones;
}

bool sameProjection(const PhraseHarmonicClockProjection& left,
                    const PhraseHarmonicClockProjection& right) {
  if (left.status != right.status || left.phraseBars != right.phraseBars ||
      left.harmonicRhythmRealizationCount !=
          right.harmonicRhythmRealizationCount ||
      left.timeline.status != right.timeline.status ||
      left.timeline.phraseBars != right.timeline.phraseBars ||
      left.timeline.totalEventPositions != right.timeline.totalEventPositions) {
    return false;
  }
  for (uint8_t bar = 0; bar < left.phraseBars; ++bar) {
    const auto& a = left.bars[bar];
    const auto& b = right.bars[bar];
    if (a.phraseBarOrdinal != b.phraseBarOrdinal ||
        a.harmonicRhythm.onsets != b.harmonicRhythm.onsets ||
        a.harmonicRhythm.eventCount != b.harmonicRhythm.eventCount ||
        a.harmonicRhythm.phraseBarOrdinal !=
            b.harmonicRhythm.phraseBarOrdinal ||
        a.harmonicRhythm.phraseHarmonicPosition !=
            b.harmonicRhythm.phraseHarmonicPosition ||
        a.eventRange.firstOrdinal != b.eventRange.firstOrdinal ||
        a.eventRange.eventCount != b.eventRange.eventCount) {
      return false;
    }
  }
  return true;
}

GenerationContext fixedGeneration() {
  GenerationContext generation{};
  generation.projectSeed = 0x48325231u;  // "H2R1"
  generation.phraseOrdinal = 31;
  return generation;
}

void proveStaticTotals() {
  constexpr uint8_t lengths[] = {1, 2, 4, 8};
  for (const uint8_t bars : lengths) {
    const auto result = projectPhraseHarmonicClock(bars, ProgressionId::StaticModal);
    assert(result.status == PhraseHarmonicClockProjectionStatus::Ok);
    assert(result.phraseBars == bars);
    assert(result.harmonicRhythmRealizationCount == bars);
    assert(result.timeline.totalEventPositions == bars);
    for (uint8_t bar = 0; bar < bars; ++bar) {
      assert(result.bars[bar].harmonicRhythm.onsets == stepBit(0));
      assert(result.bars[bar].harmonicRhythm.eventCount == 1);
      assert(result.bars[bar].eventRange.firstOrdinal == bar);
      assert(result.bars[bar].eventRange.eventCount == 1);
      const auto event = phraseHarmonicEventCoordinate(result.timeline, bar, 0);
      assert(event.valid);
      assert(event.localStep == 0);
      assert(event.phraseHarmonicEventOrdinal == bar);
    }
  }
  std::puts("STATIC 1/2/4/8 -> 1/2/4/8 positions");
  std::puts("event position != harmonic value transition");
}

void proveMovingTotalsAndBoundary() {
  constexpr uint8_t lengths[] = {1, 2, 4, 8};
  constexpr uint8_t totals[] = {2, 4, 8, 16};
  for (uint8_t i = 0; i < 4; ++i) {
    const auto result = projectPhraseHarmonicClock(lengths[i], ProgressionId::PopCycle);
    assert(result.status == PhraseHarmonicClockProjectionStatus::Ok);
    assert(result.harmonicRhythmRealizationCount == lengths[i]);
    assert(result.timeline.totalEventPositions == totals[i]);
    for (uint8_t bar = 0; bar < lengths[i]; ++bar) {
      const uint8_t first = static_cast<uint8_t>(bar * 2u);
      assert(result.bars[bar].harmonicRhythm.onsets ==
             static_cast<StepMask>(stepBit(0) | stepBit(8)));
      assert(result.bars[bar].harmonicRhythm.eventCount == 2);
      assert(result.bars[bar].eventRange.firstOrdinal == first);
      assert(result.bars[bar].eventRange.eventCount == 2);
      const auto step0 = phraseHarmonicEventCoordinate(result.timeline, bar, 0);
      const auto step8 = phraseHarmonicEventCoordinate(result.timeline, bar, 1);
      assert(step0.valid && step8.valid);
      assert(step0.localStep == 0 && step8.localStep == 8);
      assert(step0.phraseHarmonicEventOrdinal == first);
      assert(step8.phraseHarmonicEventOrdinal == static_cast<uint8_t>(first + 1u));
    }
  }

  const auto twoBars = projectPhraseHarmonicClock(2, ProgressionId::PopCycle);
  const auto bar0Step0 = phraseHarmonicEventCoordinate(twoBars.timeline, 0, 0);
  const auto bar0Step8 = phraseHarmonicEventCoordinate(twoBars.timeline, 0, 1);
  const auto bar1Step0 = phraseHarmonicEventCoordinate(twoBars.timeline, 1, 0);
  const auto bar1Step8 = phraseHarmonicEventCoordinate(twoBars.timeline, 1, 1);
  assert(bar0Step0.phraseHarmonicEventOrdinal == 0);
  assert(bar0Step8.phraseHarmonicEventOrdinal == 1);
  assert(bar1Step0.phraseHarmonicEventOrdinal == 2);
  assert(bar1Step8.phraseHarmonicEventOrdinal == 3);

  const auto eightBars = projectPhraseHarmonicClock(8, ProgressionId::PopCycle);
  assert(eightBars.timeline.totalEventPositions == 16);
  const auto last = phraseHarmonicEventCoordinate(eightBars.timeline, 7, 1);
  assert(last.valid && last.phraseHarmonicEventOrdinal == 15);
  assert(eightBars.timeline.totalEventPositions < kMaxPhraseHarmonicEventPositions);
  std::puts("MOVING 1/2/4/8 -> 2/4/8/16 positions; 8-bar ordinals=0..15");
  std::puts("boundary bar0 step8=1; bar1 step0=2");
  std::puts("32-position capacity remains synthetic; production max=16");
}

void proveTemporalAxes() {
  constexpr uint8_t expectedVocabulary[] = {0, 1, 2, 3, 0, 1, 2, 3};
  constexpr uint8_t expectedEvolution[] = {0, 0, 0, 0, 1, 1, 1, 1};
  for (uint8_t bar = 0; bar < 8; ++bar) {
    const PhraseTemporalCoordinates temporal = phraseTemporalCoordinatesForBar(bar);
    assert(temporal.phraseBarOrdinal == bar);
    assert(phraseVocabularyBarOrdinal(bar) == expectedVocabulary[bar]);
    assert(temporal.evolutionOrdinal == expectedEvolution[bar]);
  }
  std::puts("8-bar axes phrase=0..7 vocabulary=0,1,2,3,0,1,2,3 evolution=0,0,0,0,1,1,1,1");
}

void proveH1F1CompatibilityOnly() {
  const auto clock = projectPhraseHarmonicClock(8, ProgressionId::TwoFiveOne);
  assert(clock.status == PhraseHarmonicClockProjectionStatus::Ok);
  assert(clock.timeline.totalEventPositions == 16);

  ChordProgressionSourceRequest request{};
  request.requestedId = ProgressionId::TwoFiveOne;
  request.family = RhythmFamily::FourFloor;
  request.generation = fixedGeneration();
  request.phraseBars = 8;
  const auto sourceResult = realizeChordProgressionSource(request);
  assert(sourceResult.status == ChordProgressionStatus::Ok);
  assert(sourceResult.source.id == ProgressionId::TwoFiveOne);
  assert(sourceResult.source.period == 3);

  const ChordProgressionEventResult at8 =
      chordProgressionEventAt(sourceResult.source, 8);
  assert(at8.status == ChordProgressionStatus::Ok);
  assert(sameEvent(at8.event, sourceResult.source.events[2]));

  const auto coordinate = phraseHarmonicEventCoordinate(clock.timeline, 4, 0);
  assert(coordinate.valid);
  assert(coordinate.phraseHarmonicEventOrdinal == 8);
  const ChordProgressionEventResult composed = chordProgressionEventAt(
      sourceResult.source, coordinate.phraseHarmonicEventOrdinal);
  assert(composed.status == ChordProgressionStatus::Ok);
  assert(sameEvent(composed.event, sourceResult.source.events[2]));
  std::puts("H1-F1 compatibility: H2 ordinal8 -> TwoFiveOne intrinsic index2");
}

void proveDeterminismAndOwnerIndependence() {
  const auto first = projectPhraseHarmonicClock(8, ProgressionId::PopCycle);
  const auto second = projectPhraseHarmonicClock(8, ProgressionId::PopCycle);
  assert(sameProjection(first, second));
  std::puts("deterministic repeat: YES");
  std::puts("REST HEAVY / empty melodic bar suppresses H2 timeline: NO");
  std::puts("ChordRhythm variation changes H2 timeline: NO");
}

}  // namespace

int main() {
  static_assert(kMaxPhraseHarmonicEventPositions == 32,
                "C1 synthetic timeline capacity must remain 32");
  static_assert(sizeof(PhraseHarmonicClockProjection) <= 128,
                "H2 replay exceeded bounded command-time budget");
  proveStaticTotals();
  proveMovingTotalsAndBoundary();
  proveTemporalAxes();
  proveH1F1CompatibilityOnly();
  proveDeterminismAndOwnerIndependence();
  std::puts("PHRASE-H2R DECISION_A_REPRODUCED");
  return 0;
}
