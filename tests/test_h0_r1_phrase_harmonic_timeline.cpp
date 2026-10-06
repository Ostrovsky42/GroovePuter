#include <cassert>
#include <iostream>
#include <type_traits>

#include "../src/generation/composition/phrase_harmonic_clock_projection.h"
#include "../src/generation/composition/phrase_harmonic_policy.h"

using namespace GroovePuterRhythm;

namespace {

PhraseHarmonicTimeline prolongFixture() {
  const PhraseHarmonicEvent events[] = {{0, 0, 32}, {1, 32, 16}, {2, 48, 16}};
  return makePhraseHarmonicTimeline(4, events, 3);
}

void testHalfBarCompatibility() {
  for (uint8_t bars : {1, 2, 4, 8}) {
    const auto clock = projectPhraseHarmonicClock(bars, ProgressionId::PopCycle);
    assert(clock.status == PhraseHarmonicClockProjectionStatus::Ok);
    assert(clock.timeline.totalEventPositions == bars * 2);
    for (uint8_t i = 0; i < bars * 2; ++i) {
      const auto& event = clock.timeline.events[i];
      assert(event.sourceOrdinal == i);
      assert(event.onsetStep == i * 8);
      assert(event.durationSteps == 8);
    }
    for (uint8_t bar = 0; bar < bars; ++bar) {
      assert(clock.timeline.eventPositionsByBar[bar] == (stepBit(0) | stepBit(8)));
      const auto window = projectPhraseHarmonicBarMaterialization(clock.timeline, bar);
      assert(window.segmentCount == 2);
      assert(window.segmentOnsets == (stepBit(0) | stepBit(8)));
      assert(window.sourceOrdinals[0] == bar * 2);
      assert(window.sourceOrdinals[1] == bar * 2 + 1);
      assert(!window.entersFromPreviousBar);
    }
  }
}

void testExplicitCrossBarDuration() {
  const auto timeline = prolongFixture();
  assert(timeline.status == PhraseHarmonicTimelineStatus::Ok);
  assert(timeline.totalEventPositions == 3);
  assert(timeline.events[0].durationSteps == 32);
  assert(timeline.eventPositionsByBar[1] == 0);
}

void testStaticHalfBarCompatibility() {
  for (auto progression : {ProgressionId::StaticModal, ProgressionId::PedalDrone}) {
    for (uint8_t bars : {1, 2, 4, 8}) {
      const auto clock = projectPhraseHarmonicClock(bars, progression);
      assert(clock.status == PhraseHarmonicClockProjectionStatus::Ok);
      assert(clock.timeline.totalEventPositions == bars);
      for (uint8_t bar = 0; bar < bars; ++bar) {
        assert(clock.timeline.eventPositionsByBar[bar] == stepBit(0));
        assert(clock.timeline.events[bar].sourceOrdinal == bar);
        assert(clock.timeline.events[bar].onsetStep == bar * 16);
        assert(clock.timeline.events[bar].durationSteps == 16);
        const auto window = projectPhraseHarmonicBarMaterialization(clock.timeline, bar);
        assert(window.segmentCount == 1);
        assert(window.sourceOrdinals[0] == bar);
        assert(!window.entersFromPreviousBar);
      }
    }
  }
}

void testStaticPolicyTimeline() {
  const auto clock = projectPhraseHarmonicClock(
      4, ProgressionId::StaticModal, PhraseHarmonicPolicyId::Static);
  assert(clock.status == PhraseHarmonicClockProjectionStatus::Ok);
  assert(clock.harmonicRhythmRealizationCount == 0);
  assert(clock.timeline.totalEventPositions == 1);
  assert(clock.timeline.events[0].sourceOrdinal == 0);
  assert(clock.timeline.events[0].onsetStep == 0);
  assert(clock.timeline.events[0].durationSteps == 64);
  for (uint8_t bar = 0; bar < 4; ++bar) {
    const auto window = projectPhraseHarmonicBarMaterialization(
        clock.timeline, bar);
    assert(window.segmentCount == 1);
    assert(window.sourceOrdinals[0] == 0);
    assert(window.entersFromPreviousBar == (bar != 0));
  }
}

void testSlowPolicyFourBarTimeline() {
  const auto clock = projectPhraseHarmonicClock(
      4, ProgressionId::PopCycle, PhraseHarmonicPolicyId::Slow);
  assert(clock.status == PhraseHarmonicClockProjectionStatus::Ok);
  assert(clock.harmonicRhythmRealizationCount == 0);
  assert(clock.timeline.totalEventPositions == 4);
  for (uint8_t ordinal = 0; ordinal < 4; ++ordinal) {
    const auto& event = clock.timeline.events[ordinal];
    assert(event.sourceOrdinal == ordinal);
    assert(event.onsetStep == ordinal * 16);
    assert(event.durationSteps == 16);
    const auto window = projectPhraseHarmonicBarMaterialization(
        clock.timeline, ordinal);
    assert(window.segmentCount == 1);
    assert(window.segmentOnsets == stepBit(0));
    assert(window.sourceOrdinals[0] == ordinal);
    assert(!window.entersFromPreviousBar);
  }
}

void testProlongPolicyFourBarTimelineOnly() {
  const auto clock = projectPhraseHarmonicClock(
      4, ProgressionId::PopCycle, PhraseHarmonicPolicyId::Prolong);
  assert(clock.status == PhraseHarmonicClockProjectionStatus::Ok);
  assert(clock.harmonicRhythmRealizationCount == 0);
  assert(clock.timeline.totalEventPositions == 3);
  const uint8_t expectedOnsets[] = {0, 32, 48};
  const uint8_t expectedDurations[] = {32, 16, 16};
  for (uint8_t ordinal = 0; ordinal < 3; ++ordinal) {
    const auto& event = clock.timeline.events[ordinal];
    assert(event.sourceOrdinal == ordinal);
    assert(event.onsetStep == expectedOnsets[ordinal]);
    assert(event.durationSteps == expectedDurations[ordinal]);
  }
  const auto carryIn = projectPhraseHarmonicBarMaterialization(
      clock.timeline, 1);
  assert(carryIn.segmentCount == 1);
  assert(carryIn.sourceOrdinals[0] == 0);
  assert(carryIn.entersFromPreviousBar);
  assert(clock.timeline.totalEventPositions == 3);

  assert(projectPhraseHarmonicClock(
             2, ProgressionId::PopCycle, PhraseHarmonicPolicyId::Prolong)
             .status == PhraseHarmonicClockProjectionStatus::InvalidRequest);
  assert(projectPhraseHarmonicClock(
             8, ProgressionId::PopCycle, PhraseHarmonicPolicyId::Slow)
             .status == PhraseHarmonicClockProjectionStatus::InvalidRequest);
}

void testActiveEventAtEveryPhraseStep() {
  const auto timeline = prolongFixture();
  for (uint8_t step = 0; step < 64; ++step) {
    const auto* event = activePhraseHarmonicEventAt(timeline, step);
    assert(event != nullptr);
    const uint8_t expected = step < 32 ? 0 : (step < 48 ? 1 : 2);
    assert(event->sourceOrdinal == expected);
  }
  assert(activePhraseHarmonicEventAt(timeline, 64) == nullptr);
}

void testCarryInDoesNotAdvanceSourceOrdinal() {
  const auto timeline = prolongFixture();
  for (uint8_t bar = 0; bar < 4; ++bar) {
    const auto window = projectPhraseHarmonicBarMaterialization(timeline, bar);
    assert(window.segmentCount == 1);
    assert(window.segmentOnsets == stepBit(0));
    assert(window.sourceOrdinals[0] == (bar < 2 ? 0 : bar - 1));
    assert(window.entersFromPreviousBar == (bar == 1));
  }
  assert(timeline.totalEventPositions == 3); // Four windows, three phrase events.
  assert(phraseHarmonicEventRangeForBar(timeline, 1).eventCount == 0);
}

void testInvalidOverlapRejected() {
  const PhraseHarmonicEvent events[] = {{0, 0, 33}, {1, 32, 32}};
  assert(makePhraseHarmonicTimeline(4, events, 2).status != PhraseHarmonicTimelineStatus::Ok);
}

void testDurationPastPhraseRejected() {
  const PhraseHarmonicEvent events[] = {{0, 0, 65}};
  assert(makePhraseHarmonicTimeline(4, events, 1).status != PhraseHarmonicTimelineStatus::Ok);
}

void testThirtyTwoEventCapacity() {
  PhraseHarmonicEvent events[33]{};
  for (uint8_t i = 0; i < 32; ++i) events[i] = {i, static_cast<uint8_t>(i * 4), 4};
  const auto timeline = makePhraseHarmonicTimeline(8, events, 32);
  assert(timeline.status == PhraseHarmonicTimelineStatus::Ok);
  assert(timeline.totalEventPositions == 32);
  assert(activePhraseHarmonicEventAt(timeline, 127)->sourceOrdinal == 31);
  assert(makePhraseHarmonicTimeline(8, events, 33).status != PhraseHarmonicTimelineStatus::Ok);
  const PhraseHarmonicEvent full[] = {{0, 0, 128}};
  assert(makePhraseHarmonicTimeline(8, full, 1).status == PhraseHarmonicTimelineStatus::Ok);
}

void testGapsZeroDurationAndUnorderedRejected() {
  const PhraseHarmonicEvent gap[] = {{0, 0, 31}, {1, 32, 32}};
  const PhraseHarmonicEvent zero[] = {{0, 0, 0}};
  const PhraseHarmonicEvent late[] = {{0, 1, 63}};
  const PhraseHarmonicEvent unordered[] = {{1, 32, 32}, {0, 0, 32}};
  for (auto* events : {gap, unordered}) {
    assert(makePhraseHarmonicTimeline(4, events, 2).status != PhraseHarmonicTimelineStatus::Ok);
  }
  assert(makePhraseHarmonicTimeline(4, zero, 1).status != PhraseHarmonicTimelineStatus::Ok);
  assert(makePhraseHarmonicTimeline(4, late, 1).status != PhraseHarmonicTimelineStatus::Ok);
  assert(makePhraseHarmonicTimeline(4, late, 0).status != PhraseHarmonicTimelineStatus::Ok);
}

void testCarryInThenFourChanges() {
  const PhraseHarmonicEvent events[] = {
      {7, 0, 17}, {9, 17, 3}, {11, 20, 4}, {13, 24, 4}, {15, 28, 4}};
  const auto timeline = makePhraseHarmonicTimeline(2, events, 5);
  assert(timeline.status == PhraseHarmonicTimelineStatus::Ok);
  const auto window = projectPhraseHarmonicBarMaterialization(timeline, 1);
  assert(window.entersFromPreviousBar);
  assert(window.segmentCount == 5);
  assert(window.segmentOnsets == (stepBit(0) | stepBit(1) | stepBit(4) | stepBit(8) | stepBit(12)));
  const uint8_t expected[] = {7, 9, 11, 13, 15};
  for (uint8_t i = 0; i < 5; ++i) assert(window.sourceOrdinals[i] == expected[i]);
}

void testPhraseProgressionUsesExplicitCarryInSourceOrdinals() {
  const PhraseHarmonicEvent events[] = {{0, 0, 32}, {1, 32, 16}, {2, 48, 16}};
  const auto timeline = makePhraseHarmonicTimeline(4, events, 3);
  ChordProgressionSource source{};
  source.id = ProgressionId::PopCycle;
  source.period = 4;
  source.events[0] = {0, ChordQuality::Triad, 0};
  source.events[1] = {4, ChordQuality::Triad, 0};
  source.events[2] = {5, ChordQuality::Triad, 0};
  source.events[3] = {3, ChordQuality::Major7, 0};
  for (uint8_t bar = 0; bar < 4; ++bar) {
    const auto window = projectPhraseHarmonicBarMaterialization(timeline, bar);
    const auto progression = materializePhraseHarmonicProgression(source, window);
    assert(progression.status == (source.period == 1
        ? ChordProgressionStatus::ValidButStatic : ChordProgressionStatus::Ok));
    const uint8_t expectedSource = bar < 2 ? 0 : bar - 1;
    assert(progression.plan.eventCount == 1);
    assert(progression.plan.events[0].degree == source.events[expectedSource].degree);
  }

  const PhraseHarmonicEvent sparseOrdinals[] = {
      {7, 0, 17}, {9, 17, 3}, {11, 20, 4}, {13, 24, 4}, {15, 28, 4}};
  const auto fiveSegments = makePhraseHarmonicTimeline(2, sparseOrdinals, 5);
  const auto progression = materializePhraseHarmonicProgression(
      source, projectPhraseHarmonicBarMaterialization(fiveSegments, 1));
  const uint8_t expectedDegrees[] = {3, 4, 3, 4, 3};
  assert(progression.status == ChordProgressionStatus::Ok);
  assert(progression.plan.eventCount == 5);
  for (uint8_t i = 0; i < 5; ++i)
    assert(progression.plan.events[i].degree == expectedDegrees[i]);
}
}

int main() {
  static_assert(std::is_trivially_copyable<PhraseHarmonicTimeline>::value);
  testHalfBarCompatibility();
  testStaticHalfBarCompatibility();
  testStaticPolicyTimeline();
  testSlowPolicyFourBarTimeline();
  testProlongPolicyFourBarTimelineOnly();
  testExplicitCrossBarDuration();
  testActiveEventAtEveryPhraseStep();
  testCarryInDoesNotAdvanceSourceOrdinal();
  testInvalidOverlapRejected();
  testDurationPastPhraseRejected();
  testThirtyTwoEventCapacity();
  testGapsZeroDurationAndUnorderedRejected();
  testCarryInThenFourChanges();
  testPhraseProgressionUsesExplicitCarryInSourceOrdinals();
  std::cout << "H0-R1 explicit phrase timeline and carry-in: PASS\n";
}
