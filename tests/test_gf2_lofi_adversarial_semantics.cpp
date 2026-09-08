#include <cassert>
#include <cstdint>
#include <cstdio>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/composition/genre_structural_laws.h"
#include "src/generation/composition/phrase_harmonic_clock_projection.h"
#include "src/generation/migration/phrase_execution.h"

using namespace GroovePuterRhythm;

namespace {

GenreSettings settingsFor(GenerativeMode mode, uint8_t recipe = 0) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = recipe;
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

PhraseExecutionMaterializationSettings materializationSettings() {
  PhraseExecutionMaterializationSettings value{};
  value.level = RealizationLevel::P2Variation;
  value.generationAttemptOrdinal = 0;
  value.feelProfile = FeelProfileId::Straight;
  value.feelAmount = 0;
  value.tonalMaterializationEnabled = true;
  value.rootPitchClass = 0;
  value.scaleTypeValue = kScaleDorian;
  return value;
}

bool containsId(WeightedIdentityView view, uint8_t wanted) {
  for (uint8_t i = 0; i < view.count; ++i) {
    if (view.candidates[i].id == wanted) return true;
  }
  return false;
}

bool containsRhythmArchetype(RhythmCompatibilityView view,
                             ReferenceVocabulary::Archetype wanted) {
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionFor(wanted);
  assert(definition != nullptr);
  for (uint8_t i = 0; i < view.count; ++i) {
    if (view.candidates[i].archetypeId == definition->archetypeId) return true;
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

PreparedPhraseExecution findReadyMovingPhrase(GenerativeMode mode,
                                               uint8_t recipe,
                                               uint8_t bars) {
  for (uint16_t identity = 1; identity < 256; ++identity) {
    PhraseExecutionScratch scratch{};
    PreparedPhraseExecution prepared{};
    const PhraseExecutionStatus status = preparePhraseExecution(
        settingsFor(mode, recipe), materializationSettings(), identity,
        bars, scratch, prepared);
    if (status != PhraseExecutionStatus::Ready) continue;
    if (!isStaticHarmonicProgression(prepared.selection.composition.progression)) {
      return prepared;
    }
  }
  assert(false && "expected at least one ready moving-harmony phrase");
  return {};
}

PreparedPhraseExecution findReadyDevelopingLoFiPhrase(uint8_t bars) {
  for (uint16_t identity = 1; identity < 256; ++identity) {
    PhraseExecutionScratch scratch{};
    PreparedPhraseExecution prepared{};
    const PhraseExecutionStatus status = preparePhraseExecution(
        settingsFor(GenerativeMode::LoFi), materializationSettings(), identity,
        bars, scratch, prepared);
    if (status != PhraseExecutionStatus::Ready) continue;
    if (prepared.phraseTrajectory == kNoTrajectoryId) continue;
    if (prepared.phrasePlan.barCount <= 1) continue;
    return prepared;
  }
  assert(false && "expected at least one ready developing Lo-Fi phrase");
  return {};
}

struct ContourClass {
  uint8_t up = 0;
  uint8_t down = 0;
  uint8_t repeat = 0;
};

struct StructuralBar {
  StepMask drums[DrumPatternSet::kVoices]{};
  StepMask synthAOnsets = 0;
  StepMask synthBOnsets = 0;
  ContourClass synthAContour{};
  ContourClass synthBContour{};
};

ContourClass contourOf(const SynthPattern& synth) {
  ContourClass contour{};
  bool havePrevious = false;
  int8_t previous = 0;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    const int8_t note = synth.steps[step].note;
    if (note < 0) continue;
    if (havePrevious) {
      if (note > previous) {
        ++contour.up;
      } else if (note < previous) {
        ++contour.down;
      } else {
        ++contour.repeat;
      }
    }
    previous = note;
    havePrevious = true;
  }
  return contour;
}

StepMask synthOnsets(const SynthPattern& synth) {
  StepMask onsets = 0;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if (synth.steps[step].note >= 0) {
      onsets = static_cast<StepMask>(onsets | stepBit(step));
    }
  }
  return onsets;
}

StructuralBar stripTimbre(const DrumPatternSet& drums,
                          const SynthPattern& synthA,
                          const SynthPattern& synthB) {
  StructuralBar result{};
  for (uint8_t voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (uint8_t step = 0; step < DrumPattern::kSteps; ++step) {
      if (drums.voices[voice].steps[step].hit) {
        result.drums[voice] = static_cast<StepMask>(
            result.drums[voice] | stepBit(step));
      }
    }
  }
  result.synthAOnsets = synthOnsets(synthA);
  result.synthBOnsets = synthOnsets(synthB);
  result.synthAContour = contourOf(synthA);
  result.synthBContour = contourOf(synthB);
  return result;
}

bool sameContour(const ContourClass& a, const ContourClass& b) {
  return a.up == b.up && a.down == b.down && a.repeat == b.repeat;
}

bool sameStructuralBar(const StructuralBar& a, const StructuralBar& b) {
  for (uint8_t voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    if (a.drums[voice] != b.drums[voice]) return false;
  }
  return a.synthAOnsets == b.synthAOnsets &&
         a.synthBOnsets == b.synthBOnsets &&
         sameContour(a.synthAContour, b.synthAContour) &&
         sameContour(a.synthBContour, b.synthBContour);
}

StepMask activeSteps(const StructuralBar& bar) {
  StepMask result = static_cast<StepMask>(bar.synthAOnsets | bar.synthBOnsets);
  for (uint8_t voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    result = static_cast<StepMask>(result | bar.drums[voice]);
  }
  return result;
}

uint8_t countBits(StepMask mask) {
  uint8_t count = 0;
  while (mask != 0) {
    mask = static_cast<StepMask>(mask & static_cast<StepMask>(mask - 1u));
    ++count;
  }
  return count;
}

void proveLoFiProfileProhibitions() {
  const GenerationProfileView lofi = generationProfileFor(
      settingsFor(GenerativeMode::LoFi));
  assert(isValidGenerationProfile(lofi));

  // RHYTHM: auto Lo-Fi may be loose/broken/sparse, but it must not collapse
  // into straight four-floor / rolling-machine drive merely by changing timbre.
  assert(!containsRhythmArchetype(
      lofi.rhythms, ReferenceVocabulary::Archetype::StraightDrive));
  assert(!containsRhythmArchetype(
      lofi.rhythms, ReferenceVocabulary::Archetype::StackedQuarters));
  assert(!containsRhythmArchetype(
      lofi.rhythms, ReferenceVocabulary::Archetype::MachineSyncopation));

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

  // MELODY: pin the complementary profile prohibition against the two
  // continuous/high-activity identities. Stage14 separately proves every
  // allowed Lo-Fi melodic identity realizes <=3 onsets/bar.
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

  // Same musical decision, different physical time. Express the decision in
  // beats; derive wall-clock duration only when tempo matters.
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

void provePhraseExecutionConsumesProfileRate() {
  const PreparedPhraseExecution lofi =
      findReadyMovingPhrase(GenerativeMode::LoFi, kBaseRecipeId, 8);
  assert(lofi.harmonicClock.timeline.totalEventPositions == 8);
  for (uint8_t bar = 0; bar < 8; ++bar) {
    assert(lofi.harmonicClock.bars[bar].harmonicRhythm.eventCount == 1);
    assert(lofi.harmonicClock.timeline.eventPositionsByBar[bar] == stepBit(0));
  }

  const PreparedPhraseExecution hiphop =
      findReadyMovingPhrase(GenerativeMode::HipHop, kBaseRecipeId, 4);
  assert(hiphop.harmonicClock.timeline.totalEventPositions == 8);
  for (uint8_t bar = 0; bar < 4; ++bar) {
    assert(hiphop.harmonicClock.bars[bar].harmonicRhythm.eventCount == 2);
    assert(hiphop.harmonicClock.timeline.eventPositionsByBar[bar] ==
           static_cast<StepMask>(stepBit(0) | stepBit(8)));
  }
}

void proveMaterializedMultiBarStructure() {
  constexpr uint8_t kBars = 4;
  const PreparedPhraseExecution prepared = findReadyDevelopingLoFiPhrase(kBars);
  assert(prepared.status == PhraseExecutionStatus::Ready);
  assert(prepared.phraseTrajectory != kNoTrajectoryId);
  assert(prepared.phrasePlan.barCount > 1);

  StructuralBar structural[kBars]{};
  bool hasIntentionalSpace = false;
  for (uint8_t bar = 0; bar < kBars; ++bar) {
    DrumPatternSet drums{};
    SynthPattern synthA{};
    SynthPattern synthB{};
    const StrongRhythmMigrationResult materialized = materializePreparedPhraseBar(
        prepared, bar, static_cast<int16_t>(16 + bar), drums, synthA, synthB);
    assert(materialized.status == StrongRhythmMigrationStatus::Applied);
    structural[bar] = stripTimbre(drums, synthA, synthB);

    // Structural space is measured after removing velocity, accent, timing,
    // groove, FX and instrument identity. At least one of sixteen step slots is
    // silent across all physical lanes in a sparse/developing Lo-Fi phrase.
    if (countBits(activeSteps(structural[bar])) < kStepsPerBar) {
      hasIntentionalSpace = true;
    }
  }

  bool differsFromStatement = false;
  for (uint8_t bar = 1; bar < kBars; ++bar) {
    if (!sameStructuralBar(structural[0], structural[bar])) {
      differsFromStatement = true;
    }
  }
  assert(differsFromStatement && "Lo-Fi developing phrase collapsed to bar0 x4");
  assert(hasIntentionalSpace && "Lo-Fi phrase has no structural silent step");
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
  provePhraseExecutionConsumesProfileRate();
  proveMaterializedMultiBarStructure();
  proveCorridorSingleSource();
  std::puts("GF2 LOFI ADVERSARIAL SEMANTICS: PASS");
  return 0;
}
