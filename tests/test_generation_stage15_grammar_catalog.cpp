#include <cassert>
#include <cstdint>

#include "src/generation/roles/chord_progression.h"

using namespace GroovePuterRhythm;

namespace {

struct ExpectedEvent {
  uint8_t degree;
  ChordQuality quality;
  int8_t offset;
};

struct ExpectedVariant {
  ExpectedEvent events[4];
  uint8_t count;
};

constexpr ExpectedEvent expected(uint8_t degree,
                                 ChordQuality quality,
                                 int8_t offset = 0) {
  return {degree, quality, offset};
}

bool eventEqual(const HarmonicEvent& actual, const ExpectedEvent& wanted) {
  return actual.degree == wanted.degree &&
         actual.quality == wanted.quality &&
         actual.rootOffsetSemitones == wanted.offset;
}

bool planMatches(const ChordProgressionPlan& plan,
                 const ExpectedVariant& variant) {
  if (variant.count == 0 || plan.eventCount == 0) return false;
  for (uint8_t index = 0; index < plan.eventCount; ++index) {
    if (!eventEqual(plan.events[index],
                    variant.events[index % variant.count])) {
      return false;
    }
  }
  return true;
}

GenerationContext contextFor(ProgressionId id, uint16_t ordinal) {
  GenerationContext generation{};
  generation.projectSeed =
      0x15C00000u | (static_cast<uint32_t>(static_cast<uint8_t>(id)) << 8u);
  generation.phraseOrdinal = ordinal;
  return generation;
}

ChordProgressionResult realize(ProgressionId id, uint16_t ordinal) {
  ChordProgressionRequest request{};
  request.requestedId = id;
  request.family = RhythmFamily::FourFloor;
  request.generation = contextFor(id, ordinal);
  request.harmonicEventCount = 8;
  request.phraseBars = 4;
  return realizeChordProgression(request);
}

void assertCatalog(ProgressionId id,
                   const ExpectedVariant* variants,
                   uint8_t variantCount) {
  bool observed[2]{};
  assert(variantCount >= 1 && variantCount <= 2);

  for (uint16_t ordinal = 0; ordinal < 512; ++ordinal) {
    const ChordProgressionResult result = realize(id, ordinal);
    assert(result.status == ChordProgressionStatus::Ok ||
           result.status == ChordProgressionStatus::ValidButStatic);
    assert(result.plan.id == id);

    bool matched = false;
    for (uint8_t variant = 0; variant < variantCount; ++variant) {
      if (planMatches(result.plan, variants[variant])) {
        observed[variant] = true;
        matched = true;
        break;
      }
    }
    assert(matched);
  }

  for (uint8_t variant = 0; variant < variantCount; ++variant) {
    assert(observed[variant]);
  }
}

void testStaticModalCatalog() {
  constexpr ExpectedVariant variants[] = {{
      {expected(0, ChordQuality::Triad), {}, {}, {}}, 1}};
  assertCatalog(ProgressionId::StaticModal, variants, 1);
}

void testPedalDroneCatalog() {
  constexpr ExpectedVariant variants[] = {{
      {expected(0, ChordQuality::Sus4), {}, {}, {}}, 1}};
  assertCatalog(ProgressionId::PedalDrone, variants, 1);
}

void testPopCycleCatalog() {
  constexpr ExpectedVariant variants[] = {
      {{expected(0, ChordQuality::Triad),
        expected(4, ChordQuality::Triad),
        expected(5, ChordQuality::Minor7),
        expected(3, ChordQuality::Major7)}, 4},
      {{expected(0, ChordQuality::Triad),
        expected(5, ChordQuality::Minor7),
        expected(3, ChordQuality::Major7),
        expected(4, ChordQuality::Dominant7)}, 4},
  };
  assertCatalog(ProgressionId::PopCycle, variants, 2);
}

void testTwoFiveOneCatalog() {
  constexpr ExpectedVariant variants[] = {
      {{expected(1, ChordQuality::Minor7),
        expected(4, ChordQuality::Dominant7),
        expected(0, ChordQuality::Major7), {}}, 3},
      {{expected(1, ChordQuality::Minor9),
        expected(4, ChordQuality::Dominant7),
        expected(0, ChordQuality::Major9), {}}, 3},
  };
  assertCatalog(ProgressionId::TwoFiveOne, variants, 2);
}

void testParallelShiftCatalog() {
  constexpr ExpectedVariant variants[] = {
      {{expected(0, ChordQuality::Minor9),
        expected(0, ChordQuality::Minor9, 1),
        expected(0, ChordQuality::Minor9),
        expected(0, ChordQuality::Minor9, -1)}, 4},
      {{expected(0, ChordQuality::Minor7),
        expected(0, ChordQuality::Minor7, -2),
        expected(0, ChordQuality::Minor7),
        expected(0, ChordQuality::Minor7, 2)}, 4},
  };
  assertCatalog(ProgressionId::ParallelShift, variants, 2);
}

void testMinorFallCatalog() {
  constexpr ExpectedVariant variants[] = {
      {{expected(0, ChordQuality::Minor7),
        expected(5, ChordQuality::Major7),
        expected(2, ChordQuality::Major7),
        expected(6, ChordQuality::Major7)}, 4},
      {{expected(0, ChordQuality::Minor7),
        expected(5, ChordQuality::Triad),
        expected(2, ChordQuality::Triad),
        expected(6, ChordQuality::Triad)}, 4},
  };
  assertCatalog(ProgressionId::MinorFall, variants, 2);
}

void testBorrowedLiftCatalog() {
  constexpr ExpectedVariant variants[] = {
      {{expected(0, ChordQuality::Minor7),
        expected(3, ChordQuality::Major7),
        expected(4, ChordQuality::Dominant7),
        expected(3, ChordQuality::Major7, 1)}, 4},
      {{expected(0, ChordQuality::Triad),
        expected(4, ChordQuality::Dominant7),
        expected(3, ChordQuality::Major7, 1),
        expected(0, ChordQuality::Triad)}, 4},
  };
  assertCatalog(ProgressionId::BorrowedLift, variants, 2);
}

}  // namespace

int main() {
  testStaticModalCatalog();
  testPedalDroneCatalog();
  testPopCycleCatalog();
  testTwoFiveOneCatalog();
  testParallelShiftCatalog();
  testMinorFallCatalog();
  testBorrowedLiftCatalog();
  return 0;
}
