#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "src/generation/migration/strong_rhythm_migration.h"

using namespace GroovePuterRhythm;

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

GenreSettings baseSettings(GenerativeMode mode) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(mode);
  settings.recipe = kBaseRecipeId;
  settings.morphTarget = kBaseRecipeId;
  settings.morphAmount = 0;
  return settings;
}

GenreSettings recipeSettings(uint8_t recipe) {
  GenerativeMode genre = GenerativeMode::Acid;
  if (recipe == 1 || recipe == 2 || recipe == 3 ||
      recipe == 8 || recipe == 9) {
    genre = GenerativeMode::Broken;
  } else if (recipe == 4) {
    genre = GenerativeMode::Rave;
  } else if (recipe == 5 || recipe == 10 || recipe == 11) {
    genre = GenerativeMode::Reggae;
  }
  GenreSettings settings = baseSettings(genre);
  settings.recipe = recipe;
  return settings;
}

bool equalDrumStep(const DrumStep& a, const DrumStep& b) {
  return a.hit == b.hit &&
         a.accent == b.accent &&
         a.velocity == b.velocity &&
         a.timing == b.timing &&
         a.fx == b.fx &&
         a.fxParam == b.fxParam &&
         a.probability == b.probability;
}

bool equalVoices(const DrumPatternSet& a, const DrumPatternSet& b) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      if (!equalDrumStep(a.voices[voice].steps[step],
                         b.voices[voice].steps[step])) {
        return false;
      }
    }
  }
  return true;
}

uint32_t voiceFingerprint(const DrumPatternSet& drums) {
  uint32_t hash = 2166136261u;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& event = drums.voices[voice].steps[step];
      hash = (hash ^ static_cast<uint32_t>(event.hit)) * 16777619u;
      hash = (hash ^ static_cast<uint32_t>(event.accent)) * 16777619u;
      hash = (hash ^ static_cast<uint32_t>(event.velocity)) * 16777619u;
    }
  }
  return hash;
}

DrumPatternSet sentinelPattern() {
  DrumPatternSet drums{};
  drums.groove.swing = 0.31f;
  drums.groove.humanize = 0.22f;
  drums.lanes[0].targetParam = 3;
  drums.lanes[0].nodeCount = 1;
  drums.lanes[0].nodes[0].step = 7;
  drums.lanes[0].nodes[0].value = 0.63f;
  drums.lanes[0].nodes[0].curveType = 2;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      DrumStep& event = drums.voices[voice].steps[step];
      event.hit = ((voice + step) % 3) == 0;
      event.accent = event.hit && ((voice + step) % 2) == 0;
      event.velocity = static_cast<uint8_t>(60 + ((voice * 7 + step) % 50));
      event.timing = static_cast<int8_t>((step % 5) - 2);
      event.fx = static_cast<uint8_t>((voice + step) % 3);
      event.fxParam = static_cast<uint8_t>(voice + step);
      event.probability = static_cast<uint8_t>(70 + (step % 30));
    }
  }
  return drums;
}

void testAllowList() {
  require(selectStrongRhythmRoute(baseSettings(GenerativeMode::Acid)) ==
              StrongRhythmRoute::AcidBase,
          "base Acid must migrate");
  require(selectStrongRhythmRoute(baseSettings(GenerativeMode::Darksynth)) ==
              StrongRhythmRoute::TechnoBase,
          "visible Techno base must migrate");
  require(selectStrongRhythmRoute(baseSettings(GenerativeMode::Rave)) ==
              StrongRhythmRoute::RaveBase,
          "base Rave must migrate");

  const GenerativeMode stage7Modes[] = {
      GenerativeMode::Outrun,
      GenerativeMode::Electro,
      GenerativeMode::Reggae,
      GenerativeMode::TripHop,
      GenerativeMode::Broken,
      GenerativeMode::Chip,
  };
  for (GenerativeMode mode : stage7Modes) {
    require(selectStrongRhythmRoute(baseSettings(mode)) ==
                StrongRhythmRoute::Stage7Composition,
            "valid base mode did not reach Stage 7 composition routing");
  }

  struct RecipeExpectation {
    uint8_t recipe;
    StrongRhythmRoute route;
  };
  const RecipeExpectation migrated[] = {
      {2, StrongRhythmRoute::DrumAndBass},
      {5, StrongRhythmRoute::DubTechno},
      {6, StrongRhythmRoute::ChicagoJack},
      {7, StrongRhythmRoute::RollingAcid},
      {10, StrongRhythmRoute::DeepChord},
  };
  for (const auto& item : migrated) {
    require(selectStrongRhythmRoute(recipeSettings(item.recipe)) == item.route,
            "approved recipe did not select its Stage 5 route");
  }

  const uint8_t stage7Recipes[] = {1, 3, 4, 8, 9, 11};
  for (uint8_t recipe : stage7Recipes) {
    require(selectStrongRhythmRoute(recipeSettings(recipe)) ==
                StrongRhythmRoute::Stage7Composition,
            "valid recipe did not reach Stage 7 composition routing");
  }

  GenreSettings crossMorph = recipeSettings(5);
  crossMorph.morphTarget = 10;
  crossMorph.morphAmount = 128;
  require(selectStrongRhythmRoute(crossMorph) == StrongRhythmRoute::Legacy,
          "cross-recipe morph must stay legacy");
}

void testLegacyAndFailureAreTransactional() {
  DrumPatternSet legacy = sentinelPattern();
  DrumPatternSet destination = legacy;
  StrongRhythmMigrationContext context{};
  context.patternAddress = 3;

  GenreSettings unsupported = baseSettings(GenerativeMode::Reggae);
  unsupported.recipe = 255;
  const StrongRhythmMigrationResult legacyResult = migrateStrongRhythmDrums(
      unsupported, context, destination);
  require(legacyResult.status == StrongRhythmMigrationStatus::Legacy,
          "unknown recipe must report legacy route");
  require(equalVoices(destination, legacy),
          "legacy route changed drum voices");
  require(destination.groove.swing == legacy.groove.swing &&
              destination.groove.humanize == legacy.groove.humanize,
          "legacy route changed PatternGroove");

  destination = legacy;
  context.patternAddress = -1;
  const StrongRhythmMigrationResult invalidResult = migrateStrongRhythmDrums(
      baseSettings(GenerativeMode::Acid), context, destination);
  require(invalidResult.status == StrongRhythmMigrationStatus::InvalidContext,
          "negative address must be rejected");
  require(equalVoices(destination, legacy),
          "invalid context changed drum voices");
}

void testAppliedRoutesAndCompatibilityState() {
  const GenreSettings routes[] = {
      baseSettings(GenerativeMode::Acid),
      baseSettings(GenerativeMode::Darksynth),
      baseSettings(GenerativeMode::Rave),
      recipeSettings(2),
      recipeSettings(5),
      recipeSettings(6),
      recipeSettings(7),
      recipeSettings(10),
  };

  for (const GenreSettings& settings : routes) {
    uint32_t firstFingerprint = 0;
    bool sawDifferentFingerprint = false;
    for (int address = 0; address < 16; ++address) {
      StrongRhythmMigrationContext context{};
      context.patternAddress = static_cast<int16_t>(address);
      context.level = RealizationLevel::P2Variation;

      DrumPatternSet first = sentinelPattern();
      const PatternGroove legacyGroove = first.groove;
      const AutomationLane legacyLane = first.lanes[0];
      const StrongRhythmMigrationResult firstResult =
          migrateStrongRhythmDrums(settings, context, first);
      require(firstResult.status == StrongRhythmMigrationStatus::Applied,
              "approved route failed migration");
      require(firstResult.realizationStatus == RealizationStatus::Ok ||
                  firstResult.realizationStatus == RealizationStatus::ValidButSparse,
              "approved route failed realization");
      require(firstResult.materializationStatus == PatternMaterializeStatus::Ok,
              "approved route failed materialization");
      require(first.groove.swing == legacyGroove.swing &&
                  first.groove.humanize == legacyGroove.humanize,
              "Stage 5 overwrote PatternGroove");
      require(first.lanes[0].targetParam == legacyLane.targetParam &&
                  first.lanes[0].nodeCount == legacyLane.nodeCount &&
                  first.lanes[0].nodes[0].step == legacyLane.nodes[0].step &&
                  first.lanes[0].nodes[0].value == legacyLane.nodes[0].value &&
                  first.lanes[0].nodes[0].curveType == legacyLane.nodes[0].curveType,
              "Stage 5 overwrote automation lane");

      DrumPatternSet second = sentinelPattern();
      const StrongRhythmMigrationResult secondResult =
          migrateStrongRhythmDrums(settings, context, second);
      require(secondResult.status == firstResult.status &&
                  secondResult.route == firstResult.route &&
                  secondResult.archetype == firstResult.archetype,
              "same context changed migration decision");
      require(equalVoices(first, second),
              "same context produced different drum realization");

      const uint32_t fingerprint = voiceFingerprint(first);
      if (address == 0) {
        firstFingerprint = fingerprint;
      } else if (fingerprint != firstFingerprint) {
        sawDifferentFingerprint = true;
      }
    }
    require(sawDifferentFingerprint,
            "pattern address did not provide deterministic route variation");
  }
}

void testHardwareIdentityCorrections() {
  using Archetype = ReferenceVocabulary::Archetype;
  int structurallyDifferentAcidVariants = 0;

  for (int address = 0; address < 16; ++address) {
    StrongRhythmMigrationContext context{};
    context.patternAddress = static_cast<int16_t>(address);
    context.level = RealizationLevel::P2Variation;

    DrumPatternSet chicago = sentinelPattern();
    const StrongRhythmMigrationResult chicagoResult = migrateStrongRhythmDrums(
        recipeSettings(6), context, chicago);
    require(chicagoResult.status == StrongRhythmMigrationStatus::Applied,
            "Chicago Jack correction failed migration");
    require(chicagoResult.archetype == Archetype::StraightAcid ||
                chicagoResult.archetype == Archetype::SparseAcid,
            "Chicago Jack escaped its jack/sparse identity pool");

    DrumPatternSet rolling = sentinelPattern();
    const StrongRhythmMigrationResult rollingResult = migrateStrongRhythmDrums(
        recipeSettings(7), context, rolling);
    require(rollingResult.status == StrongRhythmMigrationStatus::Applied,
            "Rolling Acid correction failed migration");
    require(rollingResult.archetype == Archetype::RollingAcid ||
                rollingResult.archetype == Archetype::SyncopatedAcid,
            "Rolling Acid escaped its rolling/syncopated identity pool");
    require(chicagoResult.archetype != rollingResult.archetype,
            "Chicago Jack and Rolling Acid archetype pools overlapped");

    if (voiceFingerprint(chicago) != voiceFingerprint(rolling)) {
      ++structurallyDifferentAcidVariants;
    }

    DrumPatternSet deepChord = sentinelPattern();
    const StrongRhythmMigrationResult deepChordResult = migrateStrongRhythmDrums(
        recipeSettings(10), context, deepChord);
    require(deepChordResult.status == StrongRhythmMigrationStatus::Applied,
            "Deep Chord correction failed migration");
    require(deepChordResult.archetype == Archetype::ChordResponse,
            "Deep Chord must stay on chord-response grammar");
    require(deepChordResult.chordOnsets != 0,
            "Deep Chord chord-response grammar produced no stab onsets");
  }

  require(structurallyDifferentAcidVariants >= 12,
          "Chicago Jack and Rolling Acid remain structurally too similar");
}

void testAllPLevelsRemainLegal() {
  GenreSettings settings = recipeSettings(2);
  for (uint8_t level = 0;
       level < static_cast<uint8_t>(RealizationLevel::Count);
       ++level) {
    StrongRhythmMigrationContext context{};
    context.patternAddress = 9;
    context.level = static_cast<RealizationLevel>(level);
    DrumPatternSet destination = sentinelPattern();
    const StrongRhythmMigrationResult result =
        migrateStrongRhythmDrums(settings, context, destination);
    require(result.status == StrongRhythmMigrationStatus::Applied,
            "Stage 5 rejected a legal P-level");
  }
}

}  // namespace

int main() {
  testAllowList();
  testLegacyAndFailureAreTransactional();
  testAppliedRoutesAndCompatibilityState();
  testHardwareIdentityCorrections();
  testAllPLevelsRemainLegal();
  std::puts("Groove Vocabulary Stage 5 strong migration: OK");
  return 0;
}
