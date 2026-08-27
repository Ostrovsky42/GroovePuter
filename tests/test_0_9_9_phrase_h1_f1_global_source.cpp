#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../src/generation/roles/chord_progression.h"

namespace GroovePuterRhythm {
namespace {

bool sameEvent(const HarmonicEvent& a, const HarmonicEvent& b) {
  return a.degree == b.degree && a.quality == b.quality &&
         a.rootOffsetSemitones == b.rootOffsetSemitones;
}

bool sameSource(const ChordProgressionSource& a,
                const ChordProgressionSource& b) {
  if (a.id != b.id || a.period != b.period) return false;
  for (uint8_t i = 0; i < kMaxChordProgressionSourceEvents; ++i) {
    if (!sameEvent(a.events[i], b.events[i])) return false;
  }
  return true;
}

GenerationContext fixedGeneration() {
  GenerationContext generation{};
  generation.projectSeed = 0x48314631u;  // H1F1
  generation.phraseOrdinal = 17;
  return generation;
}

bool expectPeriodAndCycle(ProgressionId id, uint8_t expectedPeriod) {
  ChordProgressionSourceRequest request{};
  request.requestedId = id;
  request.family = RhythmFamily::FourFloor;
  request.generation = fixedGeneration();
  request.phraseBars = 8;

  const ChordProgressionSourceResult result =
      realizeChordProgressionSource(request);
  if (result.status == ChordProgressionStatus::InvalidRequest ||
      result.source.id != id || result.source.period != expectedPeriod) {
    std::printf("H1-F1 source failure id=%s period=%u\n",
                chordProgressionName(id),
                static_cast<unsigned>(result.source.period));
    return false;
  }

  for (uint16_t ordinal = 0; ordinal < 32; ++ordinal) {
    HarmonicEvent value{};
    if (!chordProgressionSourceEventAt(result.source, ordinal, value)) {
      return false;
    }
    if (!sameEvent(value, result.source.events[ordinal % expectedPeriod])) {
      return false;
    }
  }
  return true;
}

bool twoFiveOneBoundary() {
  ChordProgressionSourceRequest sourceRequest{};
  sourceRequest.requestedId = ProgressionId::TwoFiveOne;
  sourceRequest.family = RhythmFamily::HipHopBackbeat;
  sourceRequest.generation = fixedGeneration();
  sourceRequest.phraseBars = 8;
  const auto source = realizeChordProgressionSource(sourceRequest);
  if (source.status != ChordProgressionStatus::Ok || source.source.period != 3) {
    return false;
  }

  constexpr uint16_t required[] = {0, 1, 2, 7, 8, 9, 11, 14, 15, 17, 31};
  for (const uint16_t ordinal : required) {
    HarmonicEvent value{};
    if (!chordProgressionSourceEventAt(source.source, ordinal, value) ||
        !sameEvent(value, source.source.events[ordinal % 3])) {
      return false;
    }
  }

  HarmonicEvent at8{};
  if (!chordProgressionSourceEventAt(source.source, 8, at8) ||
      !sameEvent(at8, source.source.events[2])) {
    return false;
  }

  ChordProgressionRequest planRequest{};
  planRequest.requestedId = ProgressionId::TwoFiveOne;
  planRequest.family = sourceRequest.family;
  planRequest.generation = sourceRequest.generation;
  planRequest.harmonicEventCount = 8;
  planRequest.phraseBars = 8;
  const auto plan = realizeChordProgression(planRequest);
  const HarmonicEvent oldAt8 =
      plan.plan.events[8 % plan.plan.eventCount];
  if (sameEvent(oldAt8, at8)) {
    std::printf("H1-F1 old modulo-plan method unexpectedly matched ordinal 8\n");
    return false;
  }

  std::printf("H1-F1 TwoFiveOne ordinal8=new_source_event2 old_plan_mod8=rejected\n");
  return true;
}

bool countIndependenceAndPlanPrefix() {
  constexpr ProgressionId ids[] = {
      ProgressionId::StaticModal, ProgressionId::PedalDrone,
      ProgressionId::PopCycle, ProgressionId::TwoFiveOne,
      ProgressionId::ParallelShift, ProgressionId::MinorFall,
      ProgressionId::BorrowedLift, ProgressionId::Auto};
  constexpr RhythmFamily families[] = {
      RhythmFamily::FourFloor, RhythmFamily::HipHopBackbeat,
      RhythmFamily::SparsePulse};
  constexpr uint8_t bars[] = {1, 2, 4, 8};

  for (const ProgressionId id : ids) {
    for (const RhythmFamily family : families) {
      for (const uint8_t phraseBars : bars) {
        ChordProgressionSourceRequest sourceRequest{};
        sourceRequest.requestedId = id;
        sourceRequest.family = family;
        sourceRequest.generation = fixedGeneration();
        sourceRequest.phraseBars = phraseBars;
        const auto sourceA = realizeChordProgressionSource(sourceRequest);
        const auto sourceB = realizeChordProgressionSource(sourceRequest);
        if (sourceA.status == ChordProgressionStatus::InvalidRequest ||
            sourceB.status != sourceA.status ||
            !sameSource(sourceA.source, sourceB.source)) {
          return false;
        }

        for (uint8_t count = 0; count <= kMaxHarmonicEvents; ++count) {
          ChordProgressionRequest planRequest{};
          planRequest.requestedId = id;
          planRequest.family = family;
          planRequest.generation = sourceRequest.generation;
          planRequest.harmonicEventCount = count;
          planRequest.phraseBars = phraseBars;
          const auto plan = realizeChordProgression(planRequest);
          if (plan.status == ChordProgressionStatus::InvalidRequest ||
              plan.plan.id != sourceA.source.id) {
            return false;
          }
          if (count == 0) {
            if (plan.plan.eventCount != 0 ||
                plan.status != ChordProgressionStatus::Ok) {
              return false;
            }
            continue;
          }

          const bool isStatic =
              sourceA.source.id == ProgressionId::StaticModal ||
              sourceA.source.id == ProgressionId::PedalDrone;
          const uint8_t expectedCount = isStatic ? 1 : count;
          if (plan.plan.eventCount != expectedCount) return false;
          for (uint8_t index = 0; index < expectedCount; ++index) {
            HarmonicEvent expected{};
            if (!chordProgressionSourceEventAt(sourceA.source, index, expected) ||
                !sameEvent(expected, plan.plan.events[index])) {
              return false;
            }
          }
        }
      }
    }
  }
  return true;
}

}  // namespace
}  // namespace GroovePuterRhythm

int main() {
  using namespace GroovePuterRhythm;

  static_assert(kMaxHarmonicEvents == 8, "legacy plan capacity changed");
  static_assert(kMaxChordProgressionSourceEvents == 4,
                "intrinsic source capacity changed");

  if (!expectPeriodAndCycle(ProgressionId::StaticModal, 1) ||
      !expectPeriodAndCycle(ProgressionId::PedalDrone, 1) ||
      !expectPeriodAndCycle(ProgressionId::PopCycle, 4) ||
      !expectPeriodAndCycle(ProgressionId::TwoFiveOne, 3) ||
      !expectPeriodAndCycle(ProgressionId::ParallelShift, 4) ||
      !expectPeriodAndCycle(ProgressionId::MinorFall, 4) ||
      !expectPeriodAndCycle(ProgressionId::BorrowedLift, 4) ||
      !twoFiveOneBoundary() ||
      !countIndependenceAndPlanPrefix()) {
    std::printf("H1-F1 FAIL\n");
    return 1;
  }

  std::printf("H1-F1 sizes HarmonicEvent=%zu Source=%zu SourceRequest=%zu SourceResult=%zu Plan=%zu Result=%zu\n",
              sizeof(HarmonicEvent), sizeof(ChordProgressionSource),
              sizeof(ChordProgressionSourceRequest),
              sizeof(ChordProgressionSourceResult), sizeof(ChordProgressionPlan),
              sizeof(ChordProgressionResult));
  std::printf("H1-F1 periods static=1 pedal=1 pop=4 twofiveone=3 parallel=4 minorfall=4 borrowed=4\n");
  std::printf("0.9.9-PHRASE-H1-F1 global progression source representation: OK\n");
  return 0;
}
