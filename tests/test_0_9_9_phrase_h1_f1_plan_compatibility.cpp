#include <cstdint>
#include <cstdio>

#include "../src/generation/roles/chord_progression.h"

namespace GroovePuterRhythm {
namespace {

void printEvent(const HarmonicEvent& event) {
  std::printf("%u,%u,%d",
              static_cast<unsigned>(event.degree),
              static_cast<unsigned>(event.quality),
              static_cast<int>(event.rootOffsetSemitones));
}

GenerationContext generationFor(uint32_t seed, uint16_t phrase) {
  GenerationContext generation{};
  generation.projectSeed = seed;
  generation.phraseOrdinal = phrase;
  return generation;
}

}  // namespace
}  // namespace GroovePuterRhythm

int main() {
  using namespace GroovePuterRhythm;

  constexpr ProgressionId ids[] = {
      ProgressionId::StaticModal, ProgressionId::PedalDrone,
      ProgressionId::PopCycle, ProgressionId::TwoFiveOne,
      ProgressionId::ParallelShift, ProgressionId::MinorFall,
      ProgressionId::BorrowedLift, ProgressionId::Auto};
  constexpr RhythmFamily families[] = {
      RhythmFamily::FourFloor, RhythmFamily::MachineSyncopation,
      RhythmFamily::Breakbeat, RhythmFamily::HipHopBackbeat,
      RhythmFamily::DubPulse, RhythmFamily::SparsePulse,
      RhythmFamily::Funk16};
  constexpr uint8_t bars[] = {1, 2, 4, 8};
  constexpr uint32_t seeds[] = {0u, 1u, 0x12345678u, 0x89abcdefu};

  for (const uint32_t seed : seeds) {
    for (const ProgressionId id : ids) {
      for (const RhythmFamily family : families) {
        for (const uint8_t phraseBars : bars) {
          for (uint8_t count = 0; count <= kMaxHarmonicEvents; ++count) {
            ChordProgressionRequest request{};
            request.requestedId = id;
            request.family = family;
            request.generation = generationFor(seed, 23);
            request.harmonicEventCount = count;
            request.phraseBars = phraseBars;

            const ChordProgressionResult result = realizeChordProgression(request);
            std::printf("seed=%08x id=%u family=%u bars=%u count=%u status=%u planId=%u planCount=%u",
                        static_cast<unsigned>(seed),
                        static_cast<unsigned>(id),
                        static_cast<unsigned>(family),
                        static_cast<unsigned>(phraseBars),
                        static_cast<unsigned>(count),
                        static_cast<unsigned>(result.status),
                        static_cast<unsigned>(result.plan.id),
                        static_cast<unsigned>(result.plan.eventCount));
            for (uint8_t i = 0; i < result.plan.eventCount; ++i) {
              std::printf(" e%u=", static_cast<unsigned>(i));
              printEvent(result.plan.events[i]);
            }
            std::printf("\n");
          }
        }
      }
    }
  }
  return 0;
}
