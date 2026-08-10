#include <cassert>
#include <cstdint>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/rhythm/reference_vocabulary.h"

using namespace GroovePuterRhythm;

namespace {

struct ProfileKey {
  GenerativeMode mode;
  uint8_t recipe;
};

constexpr ProfileKey kProductionProfiles[] = {
    {GenerativeMode::Acid, 0},     {GenerativeMode::Outrun, 0},
    {GenerativeMode::Darksynth, 0},{GenerativeMode::Electro, 0},
    {GenerativeMode::Rave, 0},     {GenerativeMode::Reggae, 0},
    {GenerativeMode::TripHop, 0},  {GenerativeMode::Broken, 0},
    {GenerativeMode::Chip, 0},     {GenerativeMode::Broken, 1},
    {GenerativeMode::Broken, 2},   {GenerativeMode::Broken, 3},
    {GenerativeMode::Rave, 4},     {GenerativeMode::Reggae, 5},
    {GenerativeMode::Acid, 6},     {GenerativeMode::Acid, 7},
    {GenerativeMode::Broken, 8},   {GenerativeMode::Broken, 9},
    {GenerativeMode::Reggae, 10},  {GenerativeMode::Reggae, 11},
};

GenreSettings settingsFor(ProfileKey key) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(key.mode);
  settings.recipe = key.recipe;
  settings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Auto);
  return settings;
}

GenerationContext generation(uint32_t seed, uint16_t ordinal) {
  GenerationContext value{};
  value.projectSeed = seed;
  value.phraseOrdinal = ordinal;
  return value;
}

bool contains(WeightedIdentityView view, uint8_t id) {
  for (uint8_t index = 0; index < view.count; ++index) {
    if (view.candidates[index].id == id &&
        view.candidates[index].weight != 0) return true;
  }
  return false;
}

bool equal(const GenerationCompositionResult& a,
           const GenerationCompositionResult& b) {
  return a.status == b.status &&
         a.rhythmSelectionMode == b.rhythmSelectionMode &&
         a.rhythmArchetypeId == b.rhythmArchetypeId &&
         a.normalizedRhythmToAuto == b.normalizedRhythmToAuto &&
         a.suggestedFeel == b.suggestedFeel &&
         a.bassRhythm == b.bassRhythm &&
         a.chordRhythm == b.chordRhythm &&
         a.melodicRhythm == b.melodicRhythm &&
         a.motifShape == b.motifShape &&
         a.phraseLaw == b.phraseLaw &&
         a.phraseBars == b.phraseBars &&
         a.secondaryRole == b.secondaryRole &&
         a.corridor.bpmMin == b.corridor.bpmMin &&
         a.corridor.bpmMax == b.corridor.bpmMax &&
         a.corridor.suggestedBpm == b.corridor.suggestedBpm;
}

void assertPlanBelongsToProfile(const GenerationProfileView& profile,
                                const GenerationCompositionResult& plan) {
  assert(plan.status == GenerationCompositionStatus::Ok);
  assert(contains(profile.feels, static_cast<uint8_t>(plan.suggestedFeel)));
  assert(contains(profile.bassRhythms,
                  static_cast<uint8_t>(plan.bassRhythm)));
  assert(contains(profile.chordRhythms,
                  static_cast<uint8_t>(plan.chordRhythm)));
  assert(contains(profile.melodicRhythms,
                  static_cast<uint8_t>(plan.melodicRhythm)));
  assert(contains(profile.motifShapes,
                  static_cast<uint8_t>(plan.motifShape)));
  const uint8_t packedPhrase = static_cast<uint8_t>(
      (static_cast<uint8_t>(plan.phraseLaw) << 4u) | plan.phraseBars);
  assert(contains(profile.phraseLaws, packedPhrase));
  assert(isRhythmCompatible(
      settingsFor({static_cast<GenerativeMode>(profile.generativeMode),
                   profile.recipe}),
      plan.rhythmArchetypeId));
}

void testAllProductionProfilesAreCompleteAndDeterministic() {
  for (const ProfileKey key : kProductionProfiles) {
    const GenreSettings settings = settingsFor(key);
    const GenerationProfileView profile = generationProfileFor(settings);
    assert(profile.generativeMode == static_cast<uint8_t>(key.mode));
    assert(profile.recipe == key.recipe);
    assert(isValidGenerationProfile(profile));
    for (uint16_t ordinal = 0; ordinal < 32; ++ordinal) {
      const GenerationContext context = generation(0x13000000u + ordinal,
                                                   ordinal);
      const GenerationCompositionResult first =
          resolveGenerationComposition(settings, context);
      const GenerationCompositionResult second =
          resolveGenerationComposition(settings, context);
      assert(equal(first, second));
      assertPlanBelongsToProfile(profile, first);
    }
  }
}

void testManualRhythmAndEveryRoleRemainComposable() {
  for (const ProfileKey key : kProductionProfiles) {
    GenreSettings settings = settingsFor(key);
    const GenerationProfileView profile = generationProfileFor(settings);
    for (uint8_t rhythmIndex = 0;
         rhythmIndex < profile.rhythms.count;
         ++rhythmIndex) {
      settings.rhythmSelectionMode =
          static_cast<uint8_t>(RhythmSelectionMode::Manual);
      settings.rhythmArchetypeId =
          profile.rhythms.candidates[rhythmIndex].archetypeId;
      const GenerationCompositionResult composition =
          resolveGenerationComposition(
              settings, generation(0xA5130000u, rhythmIndex));
      assert(composition.status == GenerationCompositionStatus::Ok);
      assert(composition.rhythmSelectionMode == RhythmSelectionMode::Manual);
      assert(composition.rhythmArchetypeId == settings.rhythmArchetypeId);

      const ReferenceVocabulary::Definition* definition =
          ReferenceVocabulary::definitionForId(composition.rhythmArchetypeId);
      assert(definition != nullptr);
      BassRhythmRequest bassRequest{};
      bassRequest.requestedId = composition.bassRhythm;
      bassRequest.family = definition->family;
      bassRequest.archetypeId = definition->archetypeId;
      bassRequest.kickOnsets = static_cast<StepMask>(stepBit(0) | stepBit(8));
      bassRequest.generation = generation(0xB5130000u, rhythmIndex);
      bassRequest.allowEmptyBar = true;
      const BassRhythmResult bass = realizeBassRhythm(bassRequest);
      assert(bass.status == BassRhythmStatus::Ok ||
             bass.status == BassRhythmStatus::ValidButEmpty);
      assert(bass.plan.id == composition.bassRhythm);

      ChordRhythmRequest chordRequest{};
      chordRequest.requestedId = composition.chordRhythm;
      chordRequest.family = definition->family;
      chordRequest.archetypeId = definition->archetypeId;
      chordRequest.bassOnsets = bass.plan.onsets;
      chordRequest.generation = bassRequest.generation;
      chordRequest.allowEmptyBar = true;
      const ChordRhythmResult chord = realizeChordRhythm(chordRequest);
      assert(chord.status == ChordRhythmStatus::Ok ||
             chord.status == ChordRhythmStatus::ValidButEmpty);
      assert(chord.plan.id == composition.chordRhythm);

      MelodicMotifRequest melodicRequest{};
      melodicRequest.requestedRhythm = composition.melodicRhythm;
      melodicRequest.requestedShape = composition.motifShape;
      melodicRequest.family = definition->family;
      melodicRequest.archetypeId = definition->archetypeId;
      melodicRequest.bassOnsets = bass.plan.onsets;
      melodicRequest.chordOnsets = chord.plan.onsets;
      melodicRequest.generation = bassRequest.generation;
      melodicRequest.allowEmptyBar = true;
      const MelodicMotifResult melodic = realizeMelodicMotif(melodicRequest);
      assert(melodic.status == MelodicMotifStatus::Ok ||
             melodic.status == MelodicMotifStatus::ValidButEmpty);
      assert(melodic.plan.rhythmId == composition.melodicRhythm);
      assert(melodic.plan.motif.shape == composition.motifShape);
    }
  }
}

void testWeightedSelectionIsOrderInvariant() {
  constexpr WeightedIdentityCandidate forward[] = {
      {7, 20}, {2, 50}, {9, 40}, {2, 30},
  };
  constexpr WeightedIdentityCandidate reverse[] = {
      {2, 30}, {9, 40}, {2, 50}, {7, 20},
  };
  for (uint16_t ordinal = 0; ordinal < 512; ++ordinal) {
    uint8_t first = 0;
    uint8_t second = 0;
    const GenerationContext context = generation(0x13C0FFEEu, ordinal);
    assert(selectWeightedIdentityFromView(
        {forward, 4}, GenerationDomain::BassRhythmSelection,
        407, 0x13070000u, context, first));
    assert(selectWeightedIdentityFromView(
        {reverse, 4}, GenerationDomain::BassRhythmSelection,
        407, 0x13070000u, context, second));
    assert(first == second);
  }

  WeightedIdentityCandidate overflow[17]{};
  for (uint8_t index = 0; index < 17; ++index) {
    overflow[index] = {index, 1};
  }
  uint8_t untouched = 99;
  assert(!selectWeightedIdentityFromView(
      {overflow, 17}, GenerationDomain::BassRhythmSelection,
      407, 0, generation(13, 0), untouched));
  assert(untouched == 99);
}

void testSlowAndBrokenFalsificationProfiles() {
  const GenerationProfileView tripHop = generationProfileFor(
      settingsFor({GenerativeMode::TripHop, 0}));
  assert(tripHop.corridor.bpmMax <= 100);
  assert(contains(tripHop.feels,
                  static_cast<uint8_t>(FeelProfileId::LaidBack)));
  assert(contains(tripHop.bassRhythms,
                  static_cast<uint8_t>(BassRhythmId::SparseAnchor)));
  assert(contains(tripHop.chordRhythms,
                  static_cast<uint8_t>(ChordRhythmId::WholeBarHold)));
  assert(contains(tripHop.melodicRhythms,
                  static_cast<uint8_t>(MelodicRhythmId::RestHeavy)));
  assert(tripHop.secondaryRole == CompositionSecondaryRole::Chord);

  const GenreSettings dnbSettings =
      settingsFor({GenerativeMode::Broken, 2});
  const GenerationProfileView dnb = generationProfileFor(dnbSettings);
  for (uint8_t index = 0; index < dnb.rhythms.count; ++index) {
    const ReferenceVocabulary::Definition* definition =
        ReferenceVocabulary::definitionForId(
            dnb.rhythms.candidates[index].archetypeId);
    assert(definition != nullptr);
    assert(definition->family != RhythmFamily::FourFloor);
  }
}

void testFallbackAndInvalidProfileIntent() {
  GenreSettings mismatched = settingsFor({GenerativeMode::TripHop, 10});
  const GenerationProfileView fallback = generationProfileFor(mismatched);
  assert(fallback.generativeMode ==
         static_cast<uint8_t>(GenerativeMode::TripHop));
  assert(fallback.recipe == 0);
  assert(isValidGenerationProfile(fallback));
  assert(resolveGenerationComposition(
             mismatched, generation(13, 13)).status ==
         GenerationCompositionStatus::Ok);

  mismatched.generativeMode = 99;
  const GenerationProfileView invalid = generationProfileFor(mismatched);
  assert(invalid.rhythms.candidates == nullptr);
  assert(resolveGenerationComposition(
             mismatched, generation(13, 13)).status ==
         GenerationCompositionStatus::NoProfile);
}

}  // namespace

int main() {
  testAllProductionProfilesAreCompleteAndDeterministic();
  testManualRhythmAndEveryRoleRemainComposable();
  testWeightedSelectionIsOrderInvariant();
  testSlowAndBrokenFalsificationProfiles();
  testFallbackAndInvalidProfileIntent();
  return 0;
}
