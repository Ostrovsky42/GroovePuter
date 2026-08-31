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

void assertSameResult(const ChordProgressionResult& left,
                      const ChordProgressionResult& right) {
  assert(left.status == right.status);
  assert(left.plan.id == right.plan.id);
  assert(left.plan.eventCount == right.plan.eventCount);
  for (uint8_t index = 0; index < left.plan.eventCount; ++index)
    assert(sameEvent(left.plan.events[index], right.plan.events[index]));
}

GenerationContext generationFor(uint8_t familyValue, uint8_t phraseBars) {
  GenerationContext generation{};
  generation.projectSeed = 0xC011AB1Eu ^
      (static_cast<uint32_t>(familyValue) << 16u) ^ phraseBars;
  generation.phraseOrdinal = static_cast<uint16_t>(97u + familyValue * 11u +
                                                   phraseBars);
  return generation;
}

void dumpCase(const char* kind,
              uint8_t requestedId,
              uint8_t familyValue,
              uint8_t phraseBars,
              uint8_t eventCount,
              const ChordProgressionResult& result) {
  std::cout << kind
            << " req=" << static_cast<unsigned>(requestedId)
            << " family=" << static_cast<unsigned>(familyValue)
            << " bars=" << static_cast<unsigned>(phraseBars)
            << " requested_count=" << static_cast<unsigned>(eventCount)
            << " status=" << static_cast<unsigned>(result.status)
            << " id=" << static_cast<unsigned>(result.plan.id)
            << " count=" << static_cast<unsigned>(result.plan.eventCount);
  for (uint8_t index = 0; index < result.plan.eventCount; ++index) {
    const HarmonicEvent& value = result.plan.events[index];
    std::cout << " e" << static_cast<unsigned>(index)
              << "=" << static_cast<unsigned>(value.degree)
              << ":" << static_cast<unsigned>(value.quality)
              << ":" << static_cast<int>(value.rootOffsetSemitones);
  }
  std::cout << "\n";
}

ChordProgressionResult runCase(ProgressionId id,
                               RhythmFamily family,
                               uint8_t phraseBars,
                               uint8_t eventCount) {
  ChordProgressionRequest request{};
  request.requestedId = id;
  request.family = family;
  request.generation = generationFor(static_cast<uint8_t>(family), phraseBars);
  request.harmonicEventCount = eventCount;
  request.phraseBars = phraseBars;

  const ChordProgressionResult first = realizeChordProgression(request);
  const ChordProgressionResult second = realizeChordProgression(request);
  assertSameResult(first, second);
  return first;
}

}  // namespace

int main() {
  std::cout << "SIZE ChordProgressionPlan=" << sizeof(ChordProgressionPlan)
            << " ChordProgressionResult=" << sizeof(ChordProgressionResult)
            << "\n";

  constexpr uint8_t phraseBarsValues[] = {1, 2, 4, 8};
  constexpr ProgressionId explicitIds[] = {
      ProgressionId::StaticModal,
      ProgressionId::PedalDrone,
      ProgressionId::PopCycle,
      ProgressionId::TwoFiveOne,
      ProgressionId::ParallelShift,
      ProgressionId::MinorFall,
      ProgressionId::BorrowedLift,
  };

  for (ProgressionId id : explicitIds) {
    for (uint8_t phraseBars : phraseBarsValues) {
      for (uint8_t eventCount = 0; eventCount <= kMaxHarmonicEvents;
           ++eventCount) {
        const ChordProgressionResult result = runCase(
            id, RhythmFamily::FourFloor, phraseBars, eventCount);
        dumpCase("EXPLICIT", static_cast<uint8_t>(id),
                 static_cast<uint8_t>(RhythmFamily::FourFloor), phraseBars,
                 eventCount, result);
      }
    }
  }

  for (uint8_t familyValue = 0;
       familyValue < static_cast<uint8_t>(RhythmFamily::Count);
       ++familyValue) {
    const RhythmFamily family = static_cast<RhythmFamily>(familyValue);
    for (uint8_t phraseBars : phraseBarsValues) {
      for (uint8_t eventCount = 0; eventCount <= kMaxHarmonicEvents;
           ++eventCount) {
        const ChordProgressionResult result = runCase(
            ProgressionId::Auto, family, phraseBars, eventCount);
        dumpCase("AUTO", static_cast<uint8_t>(ProgressionId::Auto),
                 familyValue, phraseBars, eventCount, result);
      }
    }
  }

  std::cout << "PHRASE-H1-F1 finite-plan compatibility corpus: PASS\n";
  return 0;
}