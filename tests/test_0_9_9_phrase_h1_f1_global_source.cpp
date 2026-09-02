#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/roles/chord_progression.h"

using namespace GroovePuterRhythm;

namespace {

bool sameEvent(const HarmonicEvent& left, const HarmonicEvent& right) {
  return left.degree == right.degree &&
         left.quality == right.quality &&
         left.rootOffsetSemitones == right.rootOffsetSemitones;
}

bool validStatus(ChordProgressionStatus status) {
  return status == ChordProgressionStatus::Ok ||
         status == ChordProgressionStatus::ValidButStatic;
}

GenerationContext fixedGeneration(uint32_t seed = 0x48314631u,
                                  uint16_t phraseOrdinal = 29) {
  GenerationContext generation{};
  generation.projectSeed = seed;
  generation.phraseOrdinal = phraseOrdinal;
  return generation;
}

ChordProgressionSourceRequest sourceRequestFor(
    ProgressionId id,
    RhythmFamily family,
    uint8_t phraseBars,
    GenerationContext generation = fixedGeneration()) {
  ChordProgressionSourceRequest request{};
  request.requestedId = id;
  request.family = family;
  request.generation = generation;
  request.phraseBars = phraseBars;
  return request;
}

ChordProgressionRequest planRequestFor(
    const ChordProgressionSourceRequest& sourceRequest,
    uint8_t eventCount) {
  ChordProgressionRequest request{};
  request.requestedId = sourceRequest.requestedId;
  request.family = sourceRequest.family;
  request.generation = sourceRequest.generation;
  request.harmonicEventCount = eventCount;
  request.phraseBars = sourceRequest.phraseBars;
  return request;
}

void assertSameSource(const ChordProgressionSource& left,
                      const ChordProgressionSource& right) {
  assert(left.id == right.id);
  assert(left.period == right.period);
  for (uint8_t index = 0; index < left.period; ++index)
    assert(sameEvent(left.events[index], right.events[index]));
}

void assertEventAt(const ChordProgressionSource& source,
                   uint32_t ordinal) {
  const ChordProgressionEventResult result =
      chordProgressionEventAt(source, ordinal);
  assert(validStatus(result.status));
  assert(sameEvent(result.event, source.events[ordinal % source.period]));
}

void assertSourceMatchesFinitePlan(
    const ChordProgressionSourceRequest& sourceRequest) {
  const ChordProgressionSourceResult sourceResult =
      realizeChordProgressionSource(sourceRequest);
  assert(validStatus(sourceResult.status));
  assert(sourceResult.source.id != ProgressionId::Auto);
  assert(sourceResult.source.period >= 1);
  assert(sourceResult.source.period <= kMaxChordProgressionSourceEvents);

  const ChordProgressionResult planResult =
      realizeChordProgression(planRequestFor(sourceRequest, kMaxHarmonicEvents));
  assert(validStatus(planResult.status));
  assert(planResult.plan.id == sourceResult.source.id);
  for (uint8_t index = 0; index < planResult.plan.eventCount; ++index) {
    assert(sameEvent(planResult.plan.events[index],
                     sourceResult.source.events[
                         index % sourceResult.source.period]));
  }
}

void testIntrinsicPeriodsAndArbitraryOrdinals() {
  const ChordProgressionSourceResult staticModal =
      realizeChordProgressionSource(sourceRequestFor(
          ProgressionId::StaticModal, RhythmFamily::FourFloor, 8));
  assert(staticModal.status == ChordProgressionStatus::ValidButStatic);
  assert(staticModal.source.period == 1);
  for (uint32_t ordinal = 0; ordinal <= 31; ++ordinal)
    assertEventAt(staticModal.source, ordinal);

  const ChordProgressionSourceResult pedalDrone =
      realizeChordProgressionSource(sourceRequestFor(
          ProgressionId::PedalDrone, RhythmFamily::DubPulse, 8));
  assert(pedalDrone.status == ChordProgressionStatus::ValidButStatic);
  assert(pedalDrone.source.period == 1);
  for (uint32_t ordinal = 0; ordinal <= 31; ++ordinal)
    assertEventAt(pedalDrone.source, ordinal);

  const ChordProgressionSourceResult popCycle =
      realizeChordProgressionSource(sourceRequestFor(
          ProgressionId::PopCycle, RhythmFamily::FourFloor, 8));
  assert(popCycle.status == ChordProgressionStatus::Ok);
  assert(popCycle.source.period == 4);
  constexpr uint32_t popOrdinals[] = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 31};
  for (uint32_t ordinal : popOrdinals) assertEventAt(popCycle.source, ordinal);

  const ChordProgressionSourceResult twoFiveOne =
      realizeChordProgressionSource(sourceRequestFor(
          ProgressionId::TwoFiveOne, RhythmFamily::Breakbeat, 8));
  assert(twoFiveOne.status == ChordProgressionStatus::Ok);
  assert(twoFiveOne.source.period == 3);
  constexpr uint32_t twoFiveOneOrdinals[] = {
      0, 1, 2, 7, 8, 9, 11, 14, 15, 17, 31};
  for (uint32_t ordinal : twoFiveOneOrdinals)
    assertEventAt(twoFiveOne.source, ordinal);

  const ChordProgressionEventResult ordinal8 =
      chordProgressionEventAt(twoFiveOne.source, 8);
  assert(ordinal8.status == ChordProgressionStatus::Ok);
  assert(sameEvent(ordinal8.event, twoFiveOne.source.events[2]));
  const ChordProgressionResult finiteTwoFiveOne = realizeChordProgression(
      planRequestFor(sourceRequestFor(ProgressionId::TwoFiveOne,
                                     RhythmFamily::Breakbeat, 8),
                     kMaxHarmonicEvents));
  assert(finiteTwoFiveOne.status == ChordProgressionStatus::Ok);
  assert(finiteTwoFiveOne.plan.eventCount == kMaxHarmonicEvents);
  const HarmonicEvent& wrongModuloEight =
      finiteTwoFiveOne.plan.events[8 % finiteTwoFiveOne.plan.eventCount];
  assert(!sameEvent(ordinal8.event, wrongModuloEight));

  constexpr ProgressionId periodFourIds[] = {
      ProgressionId::ParallelShift,
      ProgressionId::MinorFall,
      ProgressionId::BorrowedLift,
  };
  for (ProgressionId id : periodFourIds) {
    const ChordProgressionSourceResult result =
        realizeChordProgressionSource(
            sourceRequestFor(id, RhythmFamily::FourFloor, 8));
    assert(result.status == ChordProgressionStatus::Ok);
    assert(result.source.period == 4);
    for (uint32_t ordinal = 0; ordinal <= 31; ++ordinal)
      assertEventAt(result.source, ordinal);
  }

  std::cout << "A periods=1/3/4 ordinal_range=0..31 TWO_FIVE_ONE_8=MODULO_3\n";
}

void testExplicitAndAutoSelectionDeterminism() {
  constexpr ProgressionId explicitIds[] = {
      ProgressionId::StaticModal,
      ProgressionId::PedalDrone,
      ProgressionId::PopCycle,
      ProgressionId::TwoFiveOne,
      ProgressionId::ParallelShift,
      ProgressionId::MinorFall,
      ProgressionId::BorrowedLift,
  };
  constexpr uint8_t phraseBarsValues[] = {1, 2, 4, 8};

  for (ProgressionId id : explicitIds) {
    for (uint8_t phraseBars : phraseBarsValues) {
      const ChordProgressionSourceRequest request = sourceRequestFor(
          id, RhythmFamily::FourFloor, phraseBars,
          fixedGeneration(0x13579BDFu, 41));
      const ChordProgressionSourceResult first =
          realizeChordProgressionSource(request);
      const ChordProgressionSourceResult second =
          realizeChordProgressionSource(request);
      assert(first.status == second.status);
      assert(validStatus(first.status));
      assertSameSource(first.source, second.source);
      assertSourceMatchesFinitePlan(request);
    }
  }

  for (uint8_t familyValue = 0;
       familyValue < static_cast<uint8_t>(RhythmFamily::Count);
       ++familyValue) {
    const RhythmFamily family = static_cast<RhythmFamily>(familyValue);
    for (uint8_t phraseBars : phraseBarsValues) {
      const ChordProgressionSourceRequest request = sourceRequestFor(
          ProgressionId::Auto, family, phraseBars,
          fixedGeneration(0x2468ACE0u, 53));
      const ChordProgressionSourceResult first =
          realizeChordProgressionSource(request);
      const ChordProgressionSourceResult second =
          realizeChordProgressionSource(request);
      assert(first.status == second.status);
      assert(validStatus(first.status));
      assert(first.source.id != ProgressionId::Auto);
      assertSameSource(first.source, second.source);
      assertSourceMatchesFinitePlan(request);
    }
  }

  std::cout << "B explicit_and_auto=DETERMINISTIC phrase_bars=1/2/4/8\n";
}

void testWhenCardinalityIsNotSourceIdentity() {
  const ChordProgressionSourceRequest sourceRequest = sourceRequestFor(
      ProgressionId::TwoFiveOne, RhythmFamily::Funk16, 8,
      fixedGeneration(0xA55AA55Au, 67));
  const ChordProgressionSourceResult source =
      realizeChordProgressionSource(sourceRequest);
  assert(source.status == ChordProgressionStatus::Ok);
  assert(source.source.period == 3);

  for (uint8_t eventCount : {uint8_t{0}, uint8_t{1}, uint8_t{2},
                             uint8_t{4}, uint8_t{8}}) {
    const ChordProgressionResult finite =
        realizeChordProgression(planRequestFor(sourceRequest, eventCount));
    assert(finite.status == ChordProgressionStatus::Ok);
    assert(finite.plan.id == source.source.id);
    for (uint8_t index = 0; index < finite.plan.eventCount; ++index) {
      assert(sameEvent(finite.plan.events[index],
                       source.source.events[index % source.source.period]));
    }
  }

  std::cout << "C harmonic_event_count=NOT_SOURCE_IDENTITY\n";
}

void testInvalidRequestsAndSourcesFailClosed() {
  ChordProgressionSourceRequest request = sourceRequestFor(
      ProgressionId::PopCycle, RhythmFamily::FourFloor, 4);

  request.requestedId = ProgressionId::Count;
  assert(realizeChordProgressionSource(request).status ==
         ChordProgressionStatus::InvalidRequest);
  request.requestedId = static_cast<ProgressionId>(0xFFu);
  assert(realizeChordProgressionSource(request).status ==
         ChordProgressionStatus::InvalidRequest);

  request = sourceRequestFor(ProgressionId::PopCycle,
                             RhythmFamily::FourFloor, 4);
  request.family = RhythmFamily::Count;
  assert(realizeChordProgressionSource(request).status ==
         ChordProgressionStatus::InvalidRequest);
  request.family = static_cast<RhythmFamily>(0xFFu);
  assert(realizeChordProgressionSource(request).status ==
         ChordProgressionStatus::InvalidRequest);

  for (uint8_t invalidBars : {uint8_t{0}, uint8_t{3}, uint8_t{16}}) {
    request = sourceRequestFor(ProgressionId::PopCycle,
                               RhythmFamily::FourFloor, invalidBars);
    assert(realizeChordProgressionSource(request).status ==
           ChordProgressionStatus::InvalidRequest);
  }

  ChordProgressionSource source =
      realizeChordProgressionSource(sourceRequestFor(
          ProgressionId::TwoFiveOne, RhythmFamily::Breakbeat, 4)).source;
  assert(source.period == 3);

  ChordProgressionSource invalid = source;
  invalid.id = ProgressionId::Auto;
  assert(chordProgressionEventAt(invalid, 0).status ==
         ChordProgressionStatus::InvalidRequest);

  invalid = source;
  invalid.period = 0;
  assert(chordProgressionEventAt(invalid, 0).status ==
         ChordProgressionStatus::InvalidRequest);
  invalid = source;
  invalid.period = 4;
  assert(chordProgressionEventAt(invalid, 0).status ==
         ChordProgressionStatus::InvalidRequest);
  invalid = source;
  invalid.period = static_cast<uint8_t>(kMaxChordProgressionSourceEvents + 1u);
  assert(chordProgressionEventAt(invalid, 0).status ==
         ChordProgressionStatus::InvalidRequest);

  invalid = source;
  invalid.events[0].degree = 7;
  assert(chordProgressionEventAt(invalid, 0).status ==
         ChordProgressionStatus::InvalidRequest);
  invalid = source;
  invalid.events[0].quality = ChordQuality::Count;
  assert(chordProgressionEventAt(invalid, 0).status ==
         ChordProgressionStatus::InvalidRequest);
  invalid = source;
  invalid.events[0].rootOffsetSemitones = 3;
  assert(chordProgressionEventAt(invalid, 0).status ==
         ChordProgressionStatus::InvalidRequest);

  invalid = realizeChordProgressionSource(sourceRequestFor(
      ProgressionId::PopCycle, RhythmFamily::FourFloor, 4)).source;
  invalid.events[0].rootOffsetSemitones = 1;
  assert(chordProgressionEventAt(invalid, 0).status ==
         ChordProgressionStatus::InvalidRequest);

  std::cout << "D invalid_request_source_event=FAIL_CLOSED\n";
}

void printSizes() {
  std::cout << "SIZE HarmonicEvent=" << sizeof(HarmonicEvent)
            << " ChordProgressionSource=" << sizeof(ChordProgressionSource)
            << " ChordProgressionSourceRequest="
            << sizeof(ChordProgressionSourceRequest)
            << " ChordProgressionSourceResult="
            << sizeof(ChordProgressionSourceResult)
            << " ChordProgressionEventResult="
            << sizeof(ChordProgressionEventResult)
            << " ChordProgressionPlan=" << sizeof(ChordProgressionPlan)
            << " ChordProgressionResult=" << sizeof(ChordProgressionResult)
            << "\n";
}

}  // namespace

int main() {
  testIntrinsicPeriodsAndArbitraryOrdinals();
  testExplicitAndAutoSelectionDeterminism();
  testWhenCardinalityIsNotSourceIdentity();
  testInvalidRequestsAndSourcesFailClosed();
  printSizes();
  std::cout << "PHRASE-H1-F1 global progression source: PASS\n";
  return 0;
}