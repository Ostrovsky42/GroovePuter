#include <cstdint>
#include <cstdio>

#include "../src/generation/roles/chord_progression.h"
#include "../src/generation/roles/harmonic_rhythm.h"

namespace GroovePuterRhythm {
namespace {

bool sameEvent(const HarmonicEvent& left, const HarmonicEvent& right) {
  return left.degree == right.degree &&
         left.quality == right.quality &&
         left.rootOffsetSemitones == right.rootOffsetSemitones;
}

int fail(const char* message) {
  std::fprintf(stderr, "W1R H1-F1 compatibility FAIL: %s\n", message);
  return 1;
}

}  // namespace
}  // namespace GroovePuterRhythm

int main() {
  using namespace GroovePuterRhythm;

  ChordProgressionSourceRequest sourceRequest{};
  sourceRequest.requestedId = ProgressionId::TwoFiveOne;
  sourceRequest.family = RhythmFamily::FourFloor;
  sourceRequest.generation.projectSeed = 0x57315248u;  // "W1RH"
  sourceRequest.generation.phraseOrdinal = 17;
  sourceRequest.phraseBars = 8;

  const ChordProgressionSourceResult before =
      realizeChordProgressionSource(sourceRequest);
  if (before.status != ChordProgressionStatus::Ok) {
    return fail("TwoFiveOne source did not resolve");
  }
  if (before.source.id != ProgressionId::TwoFiveOne ||
      before.source.period != 3) {
    return fail("TwoFiveOne intrinsic source period is not 3");
  }

  HarmonicEvent ordinal8{};
  if (!chordProgressionSourceEventAt(before.source, 8, ordinal8)) {
    return fail("source accessor rejected ordinal 8");
  }
  if (!sameEvent(ordinal8, before.source.events[2])) {
    return fail("ordinal 8 did not resolve through intrinsic period 3");
  }

  HarmonicRhythmRequest whenRequest{};
  whenRequest.progression = ProgressionId::TwoFiveOne;
  const HarmonicRhythmResult when = realizeHarmonicRhythm(whenRequest);
  if (when.status != HarmonicRhythmStatus::Ok ||
      when.plan.eventCount != 2 ||
      when.plan.onsets != static_cast<StepMask>(stepBit(0) | stepBit(8))) {
    return fail("accepted moving F08 WHEN bootstrap changed");
  }

  ChordProgressionRequest finiteRequest{};
  finiteRequest.requestedId = ProgressionId::TwoFiveOne;
  finiteRequest.family = sourceRequest.family;
  finiteRequest.generation = sourceRequest.generation;
  finiteRequest.phraseBars = sourceRequest.phraseBars;
  finiteRequest.harmonicEventCount = when.plan.eventCount;
  const ChordProgressionResult finite = realizeChordProgression(finiteRequest);
  if (finite.status != ChordProgressionStatus::Ok ||
      finite.plan.eventCount != when.plan.eventCount) {
    return fail("finite plan cardinality no longer follows HarmonicRhythm WHEN");
  }

  const ChordProgressionSourceResult after =
      realizeChordProgressionSource(sourceRequest);
  if (after.status != before.status ||
      after.source.id != before.source.id ||
      after.source.period != before.source.period) {
    return fail("W1 realization changed H1-F1 source identity");
  }
  for (uint8_t index = 0; index < before.source.period; ++index) {
    if (!sameEvent(before.source.events[index], after.source.events[index])) {
      return fail("W1 realization changed H1-F1 source events");
    }
  }

  HarmonicEvent ordinal8After{};
  if (!chordProgressionSourceEventAt(after.source, 8, ordinal8After) ||
      !sameEvent(ordinal8, ordinal8After)) {
    return fail("ordinal 8 source truth changed after W1 realization");
  }

  std::printf("W1R H1-F1 source period=3 preserved=YES\n");
  std::printf("W1R TwoFiveOne ordinal8=intrinsic-event-2 preserved=YES\n");
  std::printf("W1R moving F08 fingerprint=0101/2\n");
  std::printf("W1R harmonicEventCount owner=HarmonicRhythm\n");
  std::printf("W1R arbitrary source accessor consumed by W1=NO\n");
  return 0;
}
