#include <cassert>
#include <cstdint>
#include <cstdio>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/composition/phrase_harmonic_clock_projection.h"
#include "src/generation/composition/genre_structural_laws.h"

using namespace GroovePuterRhythm;

namespace {

GenreSettings settingsFor(GenerativeMode mode, uint8_t recipe = 0) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = recipe;
  return settings;
}

bool containsId(WeightedIdentityView view, uint8_t wanted) {
  for (uint8_t i = 0; i < view.count; ++i) {
    if (view.candidates[i].id == wanted) return true;
  }
  return false;
}

bool containsPhraseLaw(WeightedIdentityView view,
                       PhraseEvolutionLawId wantedLaw,
                       uint8_t maxBarsExclusive = 0) {
  for (uint8_t i = 0; i < view.count; ++i) {
    const uint8_t encoded = view.candidates[i].id;
    const auto law = static_cast<PhraseEvolutionLawId>(encoded >> 4u);
    const uint8_t bars = static_cast<uint8_t>(encoded & 0x0Fu);
    if (law != wantedLaw) continue;
    if (maxBarsExclusive == 0 || bars < maxBarsExclusive) return true;
  }
  return false;
}

void proveLoFiProfileProhibitions() {
  const GenerationProfileView lofi = generationProfileFor(
      settingsFor(GenerativeMode::LoFi));
  assert(isValidGenerationProfile(lofi));

  // RHYTHM: auto Lo-Fi may be loose/broken/sparse, but it must not collapse
  // into straight four-floor / rolling-machine drive merely by changing timbre.
  assert(!containsId(lofi.rhythms,
                     static_cast<uint8_t>(ReferenceVocabulary::Archetype::StraightDrive)));
  assert(!containsId(lofi.rhythms,
                     static_cast<uint8_t>(ReferenceVocabulary::Archetype::StackedQuarters)));
  assert(!containsId(lofi.rhythms,
                     static_cast<uint8_t>(ReferenceVocabulary::Archetype::MachineSyncopation)));

  // BASS: base Lo-Fi leaves space; root-pulse and rolling-drive are not Auto
  // identities. LoFiHouse is allowed to make a different musician decision.
  assert(!containsId(lofi.bassRhythms, static_cast<uint8_t>(BassRhythmId::RootPulse)));
  assert(!containsId(lofi.bassRhythms, static_cast<uint8_t>(BassRhythmId::RollingDrive)));

  // FORM: base Lo-Fi is not allowed to choose a 1/2-bar semantic phrase or a
  // pure LOOP law. Its existing profile vocabulary is 4/8-bar development.
  assert(!containsPhraseLaw(lofi.phraseLaws, PhraseEvolutionLawId::Loop));
  for (uint8_t i = 0; i < lofi.phraseLaws.count; ++i) {
    const uint8_t bars = static_cast<uint8_t>(lofi.phraseLaws.candidates[i].id & 0x0Fu);
    assert(bars >= 4);
  }

  // MELODY: the shipped Lo-Fi melodic palette is explicitly sparse by
  // construction; the old Stage14 characterization proves every identity is
  // <=3 onsets/bar. Here we pin the complementary profile prohibition against
  // the two continuous/high-activity identities.
  assert(!containsId(lofi.melodicRhythms,
                     static_cast<uint8_t>(MelodicRhythmId::SyncopatedMotif)));
  assert(!containsId(lofi.melodicRhythms,
                     static_cast<uint8_t>(MelodicRhythmId::RepeatedCell)));
}

void proveBeatBasedHarmonicDecision() {
  const GenerationProfileView lofi = generationProfileFor(
      settingsFor(GenerativeMode::LoFi));
  const GenerationProfileView hiphop = generationProfileFor(
      settingsFor(GenerativeMode::HipHop));
  const GenerationProfileView lofiHouse = generationProfileFor(
      settingsFor(GenerativeMode::LoFi, kLoFiHouseRecipeId));

  assert(harmonicChangeRateForProfile(lofi) ==
         HarmonicChangeRateId::Every4Beats);
  assert(harmonicChangeRateForProfile(hiphop) ==
         HarmonicChangeRateId::Every2Beats);
  assert(harmonicChangeRateForProfile(lofiHouse) ==
         HarmonicChangeRateId::Every2Beats);

  assert(harmonicChangeRateQuarterNotes(HarmonicChangeRateId::Every4Beats) == 4);
  assert(harmonicChangeRateQuarterNotes(HarmonicChangeRateId::Every2Beats) == 2);

  // Same musical decision, different physical time. This is deliberately
  // expressed in beats first and derived in milliseconds from tempo.
  assert(harmonicChangePeriodMilliseconds(HarmonicChangeRateId::Every4Beats, 54) == 4444);
  assert(harmonicChangePeriodMilliseconds(HarmonicChangeRateId::Every4Beats, 72) == 3333);
  assert(harmonicChangePeriodMilliseconds(HarmonicChangeRateId::Every4Beats, 90) == 2666);
}

void proveTimbreStrippedHarmonicDifferentiation() {
  const auto lofi = projectPhraseHarmonicClock(
      4, ProgressionId::PopCycle, HarmonicChangeRateId::Every4Beats);
  const auto hiphop = projectPhraseHarmonicClock(
      4, ProgressionId::PopCycle, HarmonicChangeRateId::Every2Beats);

  assert(lofi.status == PhraseHarmonicClockProjectionStatus::Ok);
  assert(hiphop.status == PhraseHarmonicClockProjectionStatus::Ok);
  assert(lofi.timeline.totalEventPositions == 4);
  assert(hiphop.timeline.totalEventPositions == 8);

  for (uint8_t bar = 0; bar < 4; ++bar) {
    assert(lofi.timeline.eventPositionsByBar[bar] == stepBit(0));
    assert(hiphop.timeline.eventPositionsByBar[bar] ==
           static_cast<StepMask>(stepBit(0) | stepBit(8)));
  }

  // Legacy H2R callers retain their exact two-beat behaviour.
  const auto legacy = projectPhraseHarmonicClock(4, ProgressionId::PopCycle);
  assert(legacy.timeline.totalEventPositions == 8);
}

void proveCorridorSingleSource() {
  const GenerationProfileView base = generationProfileFor(
      settingsFor(GenerativeMode::LoFi));
  assert(base.corridor.bpmMin == 54);
  assert(base.corridor.suggestedBpm == 72);
  assert(base.corridor.bpmMax == 90);
  assert(base.corridor.densityMin == 2);
  assert(base.corridor.densityMax == 8);
  assert(base.corridor.gridSteps == 16);

  const GenerationProfileView house = generationProfileFor(
      settingsFor(GenerativeMode::LoFi, kLoFiHouseRecipeId));
  assert(house.corridor.bpmMin == 92);
  assert(house.corridor.suggestedBpm == 106);
  assert(house.corridor.bpmMax == 118);
  assert(harmonicChangeRateForProfile(house) ==
         HarmonicChangeRateId::Every2Beats);
}

}  // namespace

int main() {
  proveLoFiProfileProhibitions();
  proveBeatBasedHarmonicDecision();
  proveTimbreStrippedHarmonicDifferentiation();
  proveCorridorSingleSource();
  std::puts("GF2 LOFI ADVERSARIAL SEMANTICS: PASS");
  return 0;
}
