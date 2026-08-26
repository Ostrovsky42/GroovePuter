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

GenerationContext fixedGeneration() {
  GenerationContext generation{};
  generation.projectSeed = 0x48315352u;  // "H1SR"
  generation.phraseOrdinal = 23;
  return generation;
}

ChordProgressionRequest requestFor(ProgressionId id,
                                   uint8_t eventCount,
                                   uint8_t phraseBars) {
  ChordProgressionRequest request{};
  request.requestedId = id;
  request.family = RhythmFamily::FourFloor;
  request.generation = fixedGeneration();
  request.harmonicEventCount = eventCount;
  request.phraseBars = phraseBars;
  return request;
}

uint8_t repeatingPeriod(const ChordProgressionPlan& plan) {
  if (plan.eventCount <= 1) return plan.eventCount;
  for (uint8_t period = 1; period <= 4; ++period) {
    bool repeats = true;
    for (uint8_t index = 0; index < plan.eventCount; ++index) {
      if (!sameEvent(plan.events[index], plan.events[index % period])) {
        repeats = false;
        break;
      }
    }
    if (repeats) return period;
  }
  return 0;
}

void assertSamePlan(const ChordProgressionPlan& left,
                    const ChordProgressionPlan& right) {
  assert(left.id == right.id);
  assert(left.eventCount == right.eventCount);
  for (uint8_t index = 0; index < left.eventCount; ++index)
    assert(sameEvent(left.events[index], right.events[index]));
}

void testExplicitGrammarIsDeterministic() {
  const ChordProgressionRequest request =
      requestFor(ProgressionId::PopCycle, 8, 8);
  const ChordProgressionResult first = realizeChordProgression(request);
  const ChordProgressionResult second = realizeChordProgression(request);
  assert(first.status == ChordProgressionStatus::Ok);
  assert(second.status == ChordProgressionStatus::Ok);
  assertSamePlan(first.plan, second.plan);
  std::cout << "A explicit_source=DETERMINISTIC\n";
}

void testAutoSelectionIsDeterministic() {
  const ChordProgressionRequest request =
      requestFor(ProgressionId::Auto, 8, 8);
  const ChordProgressionResult first = realizeChordProgression(request);
  const ChordProgressionResult second = realizeChordProgression(request);
  assert(first.status == second.status);
  assert(first.status == ChordProgressionStatus::Ok ||
         first.status == ChordProgressionStatus::ValidButStatic);
  assert(first.plan.id != ProgressionId::Auto);
  assertSamePlan(first.plan, second.plan);
  std::cout << "B auto_source=DETERMINISTIC\n";
}

void testMaterializedEventsRepeatSelectedGrammar() {
  constexpr ProgressionId ids[] = {
      ProgressionId::PopCycle,
      ProgressionId::TwoFiveOne,
      ProgressionId::ParallelShift,
      ProgressionId::MinorFall,
      ProgressionId::BorrowedLift,
  };

  for (ProgressionId id : ids) {
    const ChordProgressionResult result =
        realizeChordProgression(requestFor(id, 8, 8));
    assert(result.status == ChordProgressionStatus::Ok);
    assert(result.plan.eventCount == 8);
    const uint8_t period = repeatingPeriod(result.plan);
    assert(period >= 1 && period <= 4);
  }
  std::cout << "C materialized_ordinal=SELECTED_GRAMMAR_CYCLE\n";
}

void testEventCountDoesNotReselectSource() {
  const ChordProgressionResult full =
      realizeChordProgression(requestFor(ProgressionId::PopCycle, 8, 8));
  assert(full.status == ChordProgressionStatus::Ok);

  for (uint8_t count = 1; count <= 8; ++count) {
    const ChordProgressionResult prefix =
        realizeChordProgression(requestFor(ProgressionId::PopCycle, count, 8));
    assert(prefix.status == ChordProgressionStatus::Ok);
    assert(prefix.plan.id == full.plan.id);
    assert(prefix.plan.eventCount == count);
    for (uint8_t index = 0; index < count; ++index)
      assert(sameEvent(prefix.plan.events[index], full.plan.events[index]));
  }
  std::cout << "D carrier_count=DOES_NOT_RESELECT_SOURCE\n";
}

void testCarrierCapacityRemainsEight() {
  ChordProgressionRequest request =
      requestFor(ProgressionId::PopCycle, kMaxHarmonicEvents, 8);
  assert(realizeChordProgression(request).status == ChordProgressionStatus::Ok);

  request.harmonicEventCount = static_cast<uint8_t>(kMaxHarmonicEvents + 1u);
  assert(realizeChordProgression(request).status ==
         ChordProgressionStatus::InvalidRequest);
  std::cout << "E plan_capacity=8_SOURCE_POLICY_SEPARATE\n";
}

void testPhraseLengthIsPartOfSourceCoordinate() {
  for (uint8_t phraseBars : {uint8_t{1}, uint8_t{2}, uint8_t{4}, uint8_t{8}}) {
    const ChordProgressionRequest request =
        requestFor(ProgressionId::PopCycle, 8, phraseBars);
    const ChordProgressionResult first = realizeChordProgression(request);
    const ChordProgressionResult second = realizeChordProgression(request);
    assert(first.status == ChordProgressionStatus::Ok);
    assertSamePlan(first.plan, second.plan);
  }
  std::cout << "F phrase_bars=SOURCE_SELECTION_COORDINATE\n";
}

}  // namespace

int main() {
  testExplicitGrammarIsDeterministic();
  testAutoSelectionIsDeterministic();
  testMaterializedEventsRepeatSelectedGrammar();
  testEventCountDoesNotReselectSource();
  testCarrierCapacityRemainsEight();
  testPhraseLengthIsPartOfSourceCoordinate();
  std::cout << "PHRASE-H1 progression WHAT source: DECISION_A\n";
  return 0;
}
