#include <cassert>
#include <cstdint>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/generation/rhythm/reference_vocabulary.h"

using namespace GroovePuterRhythm;

namespace {

struct GenreCase {
  GenerativeMode mode;
  const char* name;
};

constexpr GenreCase kVisibleGenres[] = {
    {GenerativeMode::Acid, "Acid"},
    {GenerativeMode::Outrun, "Outrun"},
    {GenerativeMode::Darksynth, "Darksynth"},
    {GenerativeMode::Electro, "Electro"},
    {GenerativeMode::Rave, "Rave"},
    {GenerativeMode::Reggae, "Reggae"},
    {GenerativeMode::TripHop, "TripHop"},
    {GenerativeMode::Broken, "Broken"},
    {GenerativeMode::Chip, "Chip"},
    {GenerativeMode::House, "House"},
    {GenerativeMode::Techno, "Techno"},
    {GenerativeMode::HipHop, "HipHop"},
    {GenerativeMode::FunkSoul, "FunkSoul"},
    {GenerativeMode::UkGarage, "UK Garage"},
    {GenerativeMode::DrumAndBass, "Drum & Bass"},
    {GenerativeMode::LoFi, "LoFi"},
};

static_assert(sizeof(kVisibleGenres) / sizeof(kVisibleGenres[0]) ==
              kGenerativeModeCount,
              "audition matrix must cover every visible GenerativeMode");

GenreSettings settingsFor(GenerativeMode mode) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = kBaseRecipeId;
  settings.morphTarget = kBaseRecipeId;
  settings.morphAmount = 0;
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

void testEveryVisibleGenreResolvesTheStrongAuditionBoundary() {
  for (uint8_t index = 0; index < kGenerativeModeCount; ++index) {
    const GenreCase& genre = kVisibleGenres[index];
    const GenreSettings settings = settingsFor(genre.mode);
    (void)genre.name;

    const StrongRhythmRoute route = selectStrongRhythmRoute(settings);
    assert(route != StrongRhythmRoute::Legacy);

    StrongRhythmMigrationContext context{};
    context.patternAddress = static_cast<int16_t>(index * 8u);
    context.level = RealizationLevel::P2Variation;
    context.feelProfile = FeelProfileId::Straight;
    context.feelAmount = 50;

    DrumPatternSet drums{};
    const StrongRhythmMigrationResult result =
        migrateStrongRhythmDrums(settings, context, drums);
    assert(result.status == StrongRhythmMigrationStatus::Applied);
    assert(result.route == route);
    assert(result.compositionStatus == GenerationCompositionStatus::Ok);
    assert(result.archetype != ReferenceVocabulary::Archetype::Count);

    const ReferenceVocabulary::Definition* definition =
        ReferenceVocabulary::definitionFor(result.archetype);
    assert(definition != nullptr);
    assert(isRhythmCompatible(settings, definition->archetypeId));
  }
}

}  // namespace

int main() {
  testEveryVisibleGenreResolvesTheStrongAuditionBoundary();
  return 0;
}
