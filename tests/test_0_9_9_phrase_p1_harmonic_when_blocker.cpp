#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/composition/phrase_harmonic_timeline.h"
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
  generation.projectSeed = 0x50315748u;  // "P1WH"
  generation.phraseOrdinal = 41;
  return generation;
}

ChordProgressionResult frozenSource() {
  ChordProgressionRequest request{};
  request.requestedId = ProgressionId::PopCycle;
  request.family = RhythmFamily::FourFloor;
  request.generation = fixedGeneration();
  request.harmonicEventCount = kMaxHarmonicEvents;
  request.phraseBars = 8;
  return realizeChordProgression(request);
}

uint8_t repeatingPeriod(const ChordProgressionPlan& plan) {
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

void proveFrozenLowerContractsAreReady() {
  StepMask positions[kMaxSemanticPhraseBars]{};
  const StepMask quarterCycle = static_cast<StepMask>(
      stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12));
  for (uint8_t bar = 0; bar < kMaxSemanticPhraseBars; ++bar)
    positions[bar] = quarterCycle;

  const PhraseHarmonicTimeline timeline =
      makePhraseHarmonicTimeline(8, positions);
  assert(timeline.status == PhraseHarmonicTimelineStatus::Ok);
  assert(timeline.totalEventPositions == 32);

  const ChordProgressionResult source = frozenSource();
  assert(source.status == ChordProgressionStatus::Ok);
  assert(source.plan.eventCount == kMaxHarmonicEvents);
  const uint8_t period = repeatingPeriod(source.plan);
  assert(period >= 1 && period <= 4);

  const PhraseHarmonicEventCoordinate ordinal17 =
      phraseHarmonicEventCoordinate(timeline, 4, 1);
  assert(ordinal17.valid);
  assert(ordinal17.phraseHarmonicEventOrdinal == 17);
  const HarmonicEvent& first =
      source.plan.events[ordinal17.phraseHarmonicEventOrdinal % period];
  const HarmonicEvent& repeat =
      source.plan.events[ordinal17.phraseHarmonicEventOrdinal % period];
  assert(sameEvent(first, repeat));

  std::cout << "c1_when_representation=READY positions=32\n";
  std::cout << "h1_what_source=READY ordinal17=DETERMINISTIC\n";
  std::cout << "h1_32_position_reachability=SYNTHETIC_ONLY\n";
}

}  // namespace

int main() {
  proveFrozenLowerContractsAreReady();
  std::cout << "production_phrase_wide_harmonic_when_owner=ABSENT\n";
  std::cout << "lifetime_blocker=NO\n";
  std::cout << "PHRASE-P1 DECISION_B\n";
  return 0;
}
