#include <cassert>
#include <cstdint>
#include <set>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/rhythm/reference_vocabulary.h"

using namespace GroovePuterRhythm;

namespace {

static_assert(static_cast<uint8_t>(GenerativeMode::Acid) == 0);
static_assert(static_cast<uint8_t>(GenerativeMode::Outrun) == 1);
static_assert(static_cast<uint8_t>(GenerativeMode::Darksynth) == 2);
static_assert(static_cast<uint8_t>(GenerativeMode::Electro) == 3);
static_assert(static_cast<uint8_t>(GenerativeMode::Rave) == 4);
static_assert(static_cast<uint8_t>(GenerativeMode::Reggae) == 5);
static_assert(static_cast<uint8_t>(GenerativeMode::TripHop) == 6);
static_assert(static_cast<uint8_t>(GenerativeMode::Broken) == 7);
static_assert(static_cast<uint8_t>(GenerativeMode::Chip) == 8);
static_assert(static_cast<uint8_t>(GenerativeMode::House) == 9);
static_assert(static_cast<uint8_t>(GenerativeMode::Techno) == 10);
static_assert(static_cast<uint8_t>(GenerativeMode::HipHop) == 11);
static_assert(static_cast<uint8_t>(GenerativeMode::FunkSoul) == 12);
static_assert(static_cast<uint8_t>(GenerativeMode::UkGarage) == 13);
static_assert(static_cast<uint8_t>(GenerativeMode::DrumAndBass) == 14);
static_assert(static_cast<uint8_t>(GenerativeMode::LoFi) == 15);
static_assert(kGenerativeModeCount == 16);

GenreSettings settingsFor(GenerativeMode mode, uint8_t recipe = 0) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = recipe;
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

GenerationContext context(uint16_t ordinal) {
  GenerationContext generation{};
  generation.projectSeed = 0x14C0FFEEu;
  generation.phraseOrdinal = ordinal;
  return generation;
}

bool contains(WeightedIdentityView view, uint8_t id) {
  for (uint8_t index = 0; index < view.count; ++index) {
    if (view.candidates[index].id == id && view.candidates[index].weight != 0) {
      return true;
    }
  }
  return false;
}

uint8_t popcount16(StepMask mask) {
  uint8_t count = 0;
  while (mask != 0) {
    count = static_cast<uint8_t>(count + (mask & 1u));
    mask = static_cast<StepMask>(mask >> 1u);
  }
  return count;
}

uint64_t audibleCompositionSignature(const GenerationCompositionResult& value) {
  uint64_t result = value.rhythmArchetypeId;
  result = (result << 4u) | static_cast<uint8_t>(value.suggestedFeel);
  result = (result << 4u) | static_cast<uint8_t>(value.bassRhythm);
  result = (result << 4u) | static_cast<uint8_t>(value.chordRhythm);
  result = (result << 4u) | static_cast<uint8_t>(value.melodicRhythm);
  result = (result << 4u) | static_cast<uint8_t>(value.motifShape);
  result = (result << 3u) | static_cast<uint8_t>(value.secondaryRole);
  return result;
}

void testProductionVocabularyRemainsEvidenceGated() {
  assert(ReferenceVocabulary::definitionCount() == 24);
  assert(ReferenceVocabulary::catalog().archetypeCount == 24);
  assert(ReferenceVocabulary::definitionForId(701) == nullptr);
  assert(ReferenceVocabulary::definitionForId(702) == nullptr);
  assert(ReferenceVocabulary::definitionForId(703) == nullptr);
  assert(ReferenceVocabulary::definitionForId(711) != nullptr);
  assert(ReferenceVocabulary::definitionForId(712) != nullptr);
  assert(ReferenceVocabulary::definitionForId(713) != nullptr);
  assert(ReferenceVocabulary::definitionForId(714) != nullptr);
}

void testNewBaseGenreProfilesUseProductionVocabularyOnly() {
  constexpr GenerativeMode modes[] = {
      GenerativeMode::House,
      GenerativeMode::Techno,
      GenerativeMode::HipHop,
      GenerativeMode::FunkSoul,
      GenerativeMode::UkGarage,
      GenerativeMode::DrumAndBass,
      GenerativeMode::LoFi,
  };
  for (GenerativeMode mode : modes) {
    const GenreSettings settings = settingsFor(mode);
    const GenerationProfileView profile = generationProfileFor(settings);
    assert(profile.generativeMode == static_cast<uint8_t>(mode));
    assert(profile.recipe == 0);
    assert(isValidGenerationProfile(profile));
    for (uint8_t index = 0; index < profile.rhythms.count; ++index) {
      const RhythmArchetypeId id = profile.rhythms.candidates[index].archetypeId;
      assert(ReferenceVocabulary::definitionForId(id) != nullptr);
      assert(id != 701 && id != 702 && id != 703);
    }
    for (uint16_t ordinal = 0; ordinal < 64; ++ordinal) {
      const GenerationCompositionResult result =
          resolveGenerationComposition(settings, context(ordinal));
      assert(result.status == GenerationCompositionStatus::Ok);
      assert(isRhythmCompatible(settings, result.rhythmArchetypeId));
    }
  }
}

void testLoFiVariantsAndCorridors() {
  struct Expected {
    uint8_t recipe;
    uint16_t min;
    uint16_t suggested;
    uint16_t max;
    uint8_t densityMin;
    uint8_t densityMax;
  };
  constexpr Expected expected[] = {
      {0, 54, 72, 90, 2, 8},
      {kClassicChillRecipeId, 58, 72, 82, 2, 7},
      {kDrunkenGrooveRecipeId, 66, 82, 92, 3, 9},
      {kLoFiHouseRecipeId, 92, 106, 118, 4, 11},
      {kMinimalSleepRecipeId, 42, 54, 66, 1, 5},
  };
  for (const Expected& item : expected) {
    const GenreSettings settings = settingsFor(GenerativeMode::LoFi, item.recipe);
    const GenerationProfileView profile = generationProfileFor(settings);
    assert(isValidGenerationProfile(profile));
    assert(profile.corridor.bpmMin == item.min);
    assert(profile.corridor.suggestedBpm == item.suggested);
    assert(profile.corridor.bpmMax == item.max);
    assert(profile.corridor.densityMin == item.densityMin);
    assert(profile.corridor.densityMax == item.densityMax);
    assert(profile.secondaryRole == CompositionSecondaryRole::ChordWithMelodicFill);
    assert(contains(profile.chordRhythms,
                    static_cast<uint8_t>(ChordRhythmId::WholeBarHold)));
    assert(contains(profile.melodicRhythms,
                    static_cast<uint8_t>(MelodicRhythmId::RestHeavy)));
  }
}

void testLoFiMelodicPaletteIsSparseByConstruction() {
  const GenerationProfileView profile =
      generationProfileFor(settingsFor(GenerativeMode::LoFi));
  assert(!contains(profile.melodicRhythms,
                   static_cast<uint8_t>(MelodicRhythmId::SyncopatedMotif)));
  assert(!contains(profile.melodicRhythms,
                   static_cast<uint8_t>(MelodicRhythmId::RepeatedCell)));

  for (uint8_t index = 0; index < profile.melodicRhythms.count; ++index) {
    MelodicMotifRequest request{};
    request.requestedRhythm = static_cast<MelodicRhythmId>(
        profile.melodicRhythms.candidates[index].id);
    request.requestedShape = MotifShapeId::SourceOrder;
    request.family = RhythmFamily::Breakbeat;
    request.archetypeId = 416;
    request.allowEmptyBar = true;
    request.generation = context(index);
    request.barOrdinal = index;
    const MelodicMotifResult result = realizeMelodicMotif(request);
    assert(result.status == MelodicMotifStatus::Ok ||
           result.status == MelodicMotifStatus::ValidButEmpty);
    assert(popcount16(result.plan.onsets) <= 3);
  }
}

void testLoFiAutoIsDeterministicAndAudiblyVariable() {
  const GenreSettings settings = settingsFor(GenerativeMode::LoFi);
  std::set<RhythmArchetypeId> rhythms;
  std::set<uint64_t> audible;
  for (uint16_t ordinal = 0; ordinal < 256; ++ordinal) {
    const GenerationCompositionResult first =
        resolveGenerationComposition(settings, context(ordinal));
    const GenerationCompositionResult repeat =
        resolveGenerationComposition(settings, context(ordinal));
    assert(first.status == GenerationCompositionStatus::Ok);
    assert(audibleCompositionSignature(first) == audibleCompositionSignature(repeat));
    assert(first.phraseLaw == repeat.phraseLaw);
    assert(first.phraseBars == repeat.phraseBars);
    rhythms.insert(first.rhythmArchetypeId);
    audible.insert(audibleCompositionSignature(first));
  }
  assert(rhythms.size() >= 4);
  assert(audible.size() >= 24);
}

void testManualRhythmFixesTopologyNotWholeComposition() {
  GenreSettings settings = settingsFor(GenerativeMode::LoFi);
  const RhythmArchetypeId manual = compatibleRhythmId(settings, 0);
  assert(manual != kNoArchetypeId);
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Manual);
  settings.rhythmArchetypeId = manual;

  std::set<uint64_t> audible;
  for (uint16_t ordinal = 0; ordinal < 128; ++ordinal) {
    const GenerationCompositionResult result =
        resolveGenerationComposition(settings, context(ordinal));
    assert(result.status == GenerationCompositionStatus::Ok);
    assert(result.rhythmSelectionMode == RhythmSelectionMode::Manual);
    assert(result.rhythmArchetypeId == manual);
    audible.insert(audibleCompositionSignature(result));
  }
  assert(audible.size() >= 12);
}

void testBoomBapVariantsUseHybridSecondaryRole() {
  constexpr uint8_t recipes[] = {kGoldenEraRecipeId, kDustyJazzRecipeId};
  for (uint8_t recipe : recipes) {
    const GenreSettings settings = settingsFor(GenerativeMode::HipHop, recipe);
    const GenerationProfileView profile = generationProfileFor(settings);
    assert(profile.recipe == recipe);
    assert(isValidGenerationProfile(profile));
    assert(profile.secondaryRole == CompositionSecondaryRole::ChordWithMelodicFill);
    assert(contains(profile.melodicRhythms,
                    static_cast<uint8_t>(MelodicRhythmId::RestHeavy)));
  }
}

}  // namespace

int main() {
  testProductionVocabularyRemainsEvidenceGated();
  testNewBaseGenreProfilesUseProductionVocabularyOnly();
  testLoFiVariantsAndCorridors();
  testLoFiMelodicPaletteIsSparseByConstruction();
  testLoFiAutoIsDeterministicAndAudiblyVariable();
  testManualRhythmFixesTopologyNotWholeComposition();
  testBoomBapVariantsUseHybridSecondaryRole();
  return 0;
}
