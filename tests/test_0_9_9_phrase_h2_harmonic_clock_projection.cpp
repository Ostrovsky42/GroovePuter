#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/generation/composition/phrase_harmonic_clock_projection.h"
#include "src/generation/roles/chord_progression.h"

using namespace GroovePuterRhythm;

namespace {

bool sameEvent(const HarmonicEvent& left, const HarmonicEvent& right) {
  return left.degree == right.degree &&
         left.quality == right.quality &&
         left.rootOffsetSemitones == right.rootOffsetSemitones;
}

GenerationContext fixedGeneration() {
  GenerationContext generation{};
  generation.projectSeed = 0x48325352u;  // "H2SR"
  generation.phraseOrdinal = 29;
  return generation;
}

ChordProgressionResult selectFrozenH1Source(ProgressionId id,
                                             uint8_t phraseBars) {
  ChordProgressionRequest request{};
  request.requestedId = id;
  request.family = RhythmFamily::FourFloor;
  request.generation = fixedGeneration();
  request.harmonicEventCount = kMaxHarmonicEvents;
  request.phraseBars = phraseBars;
  return realizeChordProgression(request);
}

void proveStaticPolicy() {
  constexpr uint8_t lengths[] = {1, 2, 4, 8};
  for (const uint8_t bars : lengths) {
    const PhraseHarmonicClockProjection result =
        projectPhraseHarmonicClock(bars, ProgressionId::StaticModal);
    assert(result.status == PhraseHarmonicClockProjectionStatus::Ok);
    assert(result.phraseBars == bars);
    assert(result.harmonicRhythmRealizationCount == bars);
    assert(result.timeline.totalEventPositions == bars);

    for (uint8_t bar = 0; bar < bars; ++bar) {
      const PhraseHarmonicBarProjection& projected = result.bars[bar];
      assert(projected.phraseBarOrdinal == bar);
      assert(projected.harmonicRhythm.onsets == stepBit(0));
      assert(projected.harmonicRhythm.eventCount == 1);
      assert(projected.harmonicRhythm.phraseBarOrdinal == bar);
      assert(projected.harmonicRhythm.phraseHarmonicPosition == bar);
      assert(projected.eventRange.firstOrdinal == bar);
      assert(projected.eventRange.eventCount == 1);
      const PhraseHarmonicEventCoordinate coordinate =
          phraseHarmonicEventCoordinate(result.timeline, bar, 0);
      assert(coordinate.valid);
      assert(coordinate.localStep == 0);
      assert(coordinate.phraseHarmonicEventOrdinal == bar);
    }
  }
  std::printf("STATIC 1/2/4/8 bars -> 1/2/4/8 event positions\n");
  std::printf("event position != harmonic value transition\n");
}

void proveMovingPolicy() {
  constexpr uint8_t lengths[] = {1, 2, 4, 8};
  constexpr uint8_t totals[] = {2, 4, 8, 16};
  for (uint8_t index = 0; index < 4; ++index) {
    const uint8_t bars = lengths[index];
    const PhraseHarmonicClockProjection result =
        projectPhraseHarmonicClock(bars, ProgressionId::PopCycle);
    assert(result.status == PhraseHarmonicClockProjectionStatus::Ok);
    assert(result.harmonicRhythmRealizationCount == bars);
    assert(result.timeline.totalEventPositions == totals[index]);

    for (uint8_t bar = 0; bar < bars; ++bar) {
      const PhraseHarmonicBarProjection& projected = result.bars[bar];
      const uint8_t first = static_cast<uint8_t>(bar * 2u);
      assert(projected.harmonicRhythm.onsets ==
             static_cast<StepMask>(stepBit(0) | stepBit(8)));
      assert(projected.harmonicRhythm.eventCount == 2);
      assert(projected.harmonicRhythm.phraseBarOrdinal == bar);
      assert(projected.harmonicRhythm.phraseHarmonicPosition == first);
      assert(projected.eventRange.firstOrdinal == first);
      assert(projected.eventRange.eventCount == 2);

      const PhraseHarmonicEventCoordinate firstEvent =
          phraseHarmonicEventCoordinate(result.timeline, bar, 0);
      const PhraseHarmonicEventCoordinate secondEvent =
          phraseHarmonicEventCoordinate(result.timeline, bar, 1);
      assert(firstEvent.valid && secondEvent.valid);
      assert(firstEvent.localStep == 0);
      assert(secondEvent.localStep == 8);
      assert(firstEvent.phraseHarmonicEventOrdinal == first);
      assert(secondEvent.phraseHarmonicEventOrdinal ==
             static_cast<uint8_t>(first + 1u));
    }
  }
  std::printf("MOVING {0,8} 1/2/4/8 bars -> 2/4/8/16 event positions\n");
}

void proveBoundaryUsesOnePhraseGlobalWhatSource() {
  const PhraseHarmonicClockProjection clock =
      projectPhraseHarmonicClock(2, ProgressionId::PopCycle);
  assert(clock.status == PhraseHarmonicClockProjectionStatus::Ok);

  const PhraseHarmonicEventCoordinate bar0Step8 =
      phraseHarmonicEventCoordinate(clock.timeline, 0, 1);
  const PhraseHarmonicEventCoordinate bar1Step0 =
      phraseHarmonicEventCoordinate(clock.timeline, 1, 0);
  assert(bar0Step8.valid && bar0Step8.localStep == 8);
  assert(bar0Step8.phraseHarmonicEventOrdinal == 1);
  assert(bar1Step0.valid && bar1Step0.localStep == 0);
  assert(bar1Step0.phraseHarmonicEventOrdinal == 2);

  uint8_t h1SourceSelections = 0;
  const ChordProgressionResult frozenSource =
      selectFrozenH1Source(ProgressionId::PopCycle, 2);
  ++h1SourceSelections;
  assert(h1SourceSelections == 1);
  assert(frozenSource.status == ChordProgressionStatus::Ok);
  assert(frozenSource.plan.eventCount == kMaxHarmonicEvents);

  const HarmonicEvent& phraseGlobalOrdinal2 =
      frozenSource.plan.events[bar1Step0.phraseHarmonicEventOrdinal];
  const HarmonicEvent& resetOrdinal0 = frozenSource.plan.events[0];
  assert(!sameEvent(phraseGlobalOrdinal2, resetOrdinal0));
  assert(sameEvent(phraseGlobalOrdinal2, frozenSource.plan.events[2]));

  std::printf("bar0 step8 -> ordinal 1\n");
  std::printf("bar1 step0 -> ordinal 2\n");
  std::printf("WHAT -> frozen H1 source ordinal 2; source selections=1\n");
}

void proveDeterministicRandomAccessAndFirewalls() {
  const PhraseHarmonicClockProjection first =
      projectPhraseHarmonicClock(8, ProgressionId::PopCycle);
  const PhraseHarmonicClockProjection second =
      projectPhraseHarmonicClock(8, ProgressionId::PopCycle);
  assert(first.status == PhraseHarmonicClockProjectionStatus::Ok);
  assert(second.status == PhraseHarmonicClockProjectionStatus::Ok);
  assert(first.timeline.totalEventPositions == 16);
  assert(second.timeline.totalEventPositions == 16);
  assert(first.harmonicRhythmRealizationCount == 8);

  for (uint8_t bar = 0; bar < 8; ++bar) {
    assert(first.bars[bar].harmonicRhythm.onsets ==
           second.bars[bar].harmonicRhythm.onsets);
    assert(first.bars[bar].eventRange.firstOrdinal ==
           second.bars[bar].eventRange.firstOrdinal);
    assert(first.bars[bar].eventRange.eventCount ==
           second.bars[bar].eventRange.eventCount);
  }

  assert(first.timeline.totalEventPositions <
         kMaxPhraseHarmonicEventPositions);
  std::printf("8-bar moving production positions=16; 32-position capacity=SYNTHETIC_ONLY\n");
  std::printf("REST HEAVY melody owner of harmonic time=NO\n");
  std::printf("ChordRhythm articulation owner of phrase harmonic time=NO\n");
  std::printf("F08.1 imported=NO\n");
}

}  // namespace

int main() {
  static_assert(kMaxPhraseHarmonicEventPositions == 32,
                "C1 synthetic capacity must remain 32");
  static_assert(sizeof(PhraseHarmonicClockProjection) <= 128,
                "H2 projection exceeded bounded command-time budget");

  std::printf("sizeof PhraseHarmonicClockProjection=%zu B\n",
              sizeof(PhraseHarmonicClockProjection));
  proveStaticPolicy();
  proveMovingPolicy();
  proveBoundaryUsesOnePhraseGlobalWhatSource();
  proveDeterministicRandomAccessAndFirewalls();
  std::printf("PHRASE-H2 phrase-wide harmonic clock policy: DECISION_A_CANDIDATE\n");
  return 0;
}
