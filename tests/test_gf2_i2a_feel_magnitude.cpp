// GF2-I2A — FEEL magnitude contract.
//
// GF2-I2 made the profile FEEL prior causal and passed every test while being
// inaudible: at the shipped default the whole profile character was one event
// displaced by 7 ms. docs/gf2/GF2_MAGNITUDE_CONTRACT.md now requires every
// semantic checkpoint to state how large its effect must be at shipped
// defaults, on a named corpus, and to pin that as a test.
//
// This is that test for FEEL. It consumes the production default directly from
// GeneratorParams so the contract tracks the shipped value rather than a copy.

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/dsp/mini_drumvoices.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/migration/strong_rhythm_migration.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

// Scene persistence is linked for the genre/recipe name tables and for the
// shipped GeneratorParams default this contract reads.
SerialMock Serial;
SDMock SD;

using namespace GroovePuterRhythm;

namespace {

// Thresholds taken from docs/research/GF2_I2A_FEEL_AMPLITUDE_CENSUS.tsv. Over
// the 22 expected-active recipes at 50% the weakest case is LAID BACK with 5
// displaced events and PUSH/PULL with 3, both at 2 ticks; at the previous 20%
// default PUSH/PULL displaced nothing at all. Three events and two ticks is
// therefore below the measured floor of a working amplitude and far above the
// value that shipped inaudible.
constexpr int kMinDisplacedEvents = 3;
constexpr int kMinOffsetTicks = 2;

constexpr int16_t kPatternAddress = 3;

// The named reference corpus. Every production genre/recipe is classified, so
// an intentional zero stays distinguishable from a broken one.
struct CorpusEntry {
  GenerativeMode genre;
  uint8_t recipe;
  bool expectedActive;
};

// Expected-absent by design: these vocabularies weight STRAIGHT strongly enough
// that AUTO resolves to it. Acid, techno, house, drum & bass and rave really are
// played straight; the profile prior is not missing, it is a statement.
constexpr CorpusEntry kExpectedAbsent[] = {
    {GenerativeMode::Acid, kBaseRecipeId, false},
    {GenerativeMode::Acid, 6, false},
    {GenerativeMode::Acid, 7, false},
    {GenerativeMode::Chip, kBaseRecipeId, false},
    {GenerativeMode::Darksynth, kBaseRecipeId, false},
    {GenerativeMode::DrumAndBass, kBaseRecipeId, false},
    {GenerativeMode::House, kBaseRecipeId, false},
    {GenerativeMode::Rave, kBaseRecipeId, false},
    {GenerativeMode::Rave, 4, false},
    {GenerativeMode::Outrun, kBaseRecipeId, false},
    {GenerativeMode::Techno, kBaseRecipeId, false},
};

int g_failures = 0;

void fail(const char* label, const char* genre, const char* recipe,
          const char* detail) {
  std::fprintf(stderr, "FAIL %-42s %s / %s: %s\n", label, genre, recipe, detail);
  ++g_failures;
}

void ok(const char* label) { std::printf("%-58s OK\n", label); }

bool expectedActive(GenerativeMode genre, uint8_t recipe) {
  for (const CorpusEntry& entry : kExpectedAbsent) {
    if (entry.genre == genre && entry.recipe == recipe) return false;
  }
  return true;
}

// Production converts the Scene float to the migration context the same way in
// every context builder; mirror it exactly so the contract tracks the shipped
// control rather than an approximation of it.
uint8_t shippedFeelAmount() {
  float amount = GeneratorParams().microTimingAmount;
  if (amount < 0.0f) amount = 0.0f;
  if (amount > 1.0f) amount = 1.0f;
  return static_cast<uint8_t>(amount * 100.0f + 0.5f);
}

struct Material {
  bool applied = false;
  int displaced = 0;
  int maxTicks = 0;
  uint32_t fingerprint = 2166136261u;
};

Material materialize(const GenreSettings& settings, FeelProfileId profile,
                     uint8_t amount) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = kPatternAddress;
  context.level = RealizationLevel::P2Variation;
  context.feelProfile = profile;
  context.feelAmount = amount;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = 0;
  context.scaleTypeValue = kScaleDorian;

  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
  Material material{};
  if (migrateStrongRhythmMaterial(settings, context, drums, synthA, synthB)
          .status != StrongRhythmMigrationStatus::Applied) {
    return material;
  }
  material.applied = true;

  auto account = [&material](int timing) {
    material.fingerprint ^= static_cast<uint32_t>(static_cast<uint8_t>(timing));
    material.fingerprint *= 16777619u;
    if (timing == 0) return;
    ++material.displaced;
    const int magnitude = timing < 0 ? -timing : timing;
    if (magnitude > material.maxTicks) material.maxTicks = magnitude;
  };
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      if (drums.voices[voice].steps[step].hit)
        account(drums.voices[voice].steps[step].timing);
    }
  }
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (synthA.steps[step].note >= 0) account(synthA.steps[step].timing);
    if (synthB.steps[step].note >= 0) account(synthB.steps[step].timing);
  }
  return material;
}

GenreSettings settingsFor(GenerativeMode genre, uint8_t recipe) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(genre);
  settings.recipe = recipe;
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

// ---------------------------------------------------------------------------

void testProfilesAreAudibleAtTheShippedDefault() {
  const uint8_t amount = shippedFeelAmount();
  std::printf("shipped FEEL AMOUNT: %u%%  (threshold: >=%d events, >=%d ticks)\n",
              static_cast<unsigned>(amount), kMinDisplacedEvents,
              kMinOffsetTicks);

  int active = 0;
  int absent = 0;
  for (int genreIndex = 0; genreIndex < kGenerativeModeCount; ++genreIndex) {
    const auto genre = static_cast<GenerativeMode>(genreIndex);
    const char* genreName = GenreCatalog::generativeModeName(genre);
    const uint8_t recipeCount = availableRecipeCount(genre);
    for (uint8_t ordinal = 0; ordinal < recipeCount; ++ordinal) {
      GenreRecipeId recipe = 0;
      if (!availableRecipeAt(genre, ordinal, recipe)) continue;
      const char* recipeName = GenreCatalog::recipeName(recipe);
      const GenreSettings settings = settingsFor(genre, static_cast<uint8_t>(recipe));

      const Material straight =
          materialize(settings, FeelProfileId::Straight, amount);
      if (!straight.applied) {
        fail("corpus materializes", genreName, recipeName, "STRAIGHT not applied");
        continue;
      }
      if (straight.displaced != 0) {
        fail("STRAIGHT is exactly zero", genreName, recipeName,
             "STRAIGHT displaced events");
      }

      // Deliberately inert: SwingCompatible is defined as compatible with the
      // independently applied swingPct and does not add displacement of its own.
      const Material swing =
          materialize(settings, FeelProfileId::SwingCompatible, amount);
      if (swing.displaced != 0) {
        fail("SWING COMPAT stays an expected zero", genreName, recipeName,
             "unexpected displacement");
      }

      const Material laid =
          materialize(settings, FeelProfileId::LaidBack, amount);
      const Material push =
          materialize(settings, FeelProfileId::PushPullControlled, amount);
      const Material automatic = materialize(settings, FeelProfileId::Auto, amount);

      if (!expectedActive(genre, static_cast<uint8_t>(recipe))) {
        ++absent;
        if (automatic.fingerprint != straight.fingerprint) {
          fail("expected-absent recipe stays straight", genreName, recipeName,
               "AUTO no longer resolves to STRAIGHT - update the corpus table");
        }
        continue;
      }

      ++active;
      char detail[96];
      if (laid.displaced < kMinDisplacedEvents || laid.maxTicks < kMinOffsetTicks) {
        std::snprintf(detail, sizeof(detail), "LAID BACK %d events, %d ticks",
                      laid.displaced, laid.maxTicks);
        fail("LAID BACK reaches the magnitude threshold", genreName, recipeName,
             detail);
      }
      if (push.displaced < kMinDisplacedEvents || push.maxTicks < kMinOffsetTicks) {
        std::snprintf(detail, sizeof(detail), "PUSH/PULL %d events, %d ticks",
                      push.displaced, push.maxTicks);
        fail("PUSH/PULL reaches the magnitude threshold", genreName, recipeName,
             detail);
      }
      if (laid.fingerprint == push.fingerprint ||
          laid.fingerprint == straight.fingerprint ||
          push.fingerprint == straight.fingerprint) {
        fail("profiles stay mutually distinct", genreName, recipeName,
             "two profiles produce identical timing");
      }
      if (automatic.fingerprint == straight.fingerprint) {
        fail("active recipe resolves AUTO to a real profile", genreName,
             recipeName, "AUTO is a no-op here - update the corpus table");
      }
    }
  }
  std::printf("corpus: %d expected-active, %d expected-absent\n", active, absent);
  if (active == 0 || absent == 0) {
    std::fprintf(stderr,
                 "FAIL corpus must contain both active and absent cases\n");
    ++g_failures;
  }
  if (g_failures == 0) ok("every active recipe is audible at the default");
}

// Amount zero must stay exactly neutral whatever the default becomes.
void testAmountZeroStaysNeutral() {
  bool clean = true;
  for (int genreIndex = 0; genreIndex < kGenerativeModeCount; ++genreIndex) {
    const auto genre = static_cast<GenerativeMode>(genreIndex);
    const uint8_t recipeCount = availableRecipeCount(genre);
    for (uint8_t ordinal = 0; ordinal < recipeCount; ++ordinal) {
      GenreRecipeId recipe = 0;
      if (!availableRecipeAt(genre, ordinal, recipe)) continue;
      const GenreSettings settings = settingsFor(genre, static_cast<uint8_t>(recipe));
      const Material straight = materialize(settings, FeelProfileId::Straight, 0);
      for (uint8_t raw = 1;
           raw < static_cast<uint8_t>(FeelProfileId::Auto); ++raw) {
        const Material other =
            materialize(settings, static_cast<FeelProfileId>(raw), 0);
        if (other.fingerprint != straight.fingerprint || other.displaced != 0) {
          clean = false;
        }
      }
    }
  }
  if (clean) {
    ok("FEEL amount zero is neutral for every profile");
  } else {
    std::fprintf(stderr, "FAIL amount zero displaced events\n");
    ++g_failures;
  }
}

// The presets must straddle the threshold rather than crowd below it.
void testPresetsAreRecognisablyDifferent() {
  const GenreSettings settings = settingsFor(GenerativeMode::Reggae, 11);
  const Material tight = materialize(settings, FeelProfileId::LaidBack, 15);
  const Material human = materialize(settings, FeelProfileId::LaidBack, 50);
  const Material loose = materialize(settings, FeelProfileId::LaidBack, 80);
  if (tight.displaced < human.displaced && human.displaced < loose.displaced) {
    std::printf("%-58s OK   (%d < %d < %d events)\n",
                "TIGHT / HUMAN / LOOSE stay recognisably different",
                tight.displaced, human.displaced, loose.displaced);
  } else {
    std::fprintf(stderr,
                 "FAIL presets not strictly increasing: %d / %d / %d events\n",
                 tight.displaced, human.displaced, loose.displaced);
    ++g_failures;
  }
}

}  // namespace

int main() {
  testProfilesAreAudibleAtTheShippedDefault();
  testAmountZeroStaysNeutral();
  testPresetsAreRecognisablyDifferent();

  if (g_failures != 0) {
    std::fprintf(stderr, "GF2-I2A feel magnitude: %d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("GF2-I2A feel magnitude: PASS\n");
  return 0;
}
