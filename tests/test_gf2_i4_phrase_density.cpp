// GF2-I4 — density intent must coexist with the authoritative I3 phrase law.
//
// This is deliberately separate from the frozen I4 magnitude fixture. It uses
// shipped profiles and the production preparePhraseExecution path to prove:
//   1. density is resolved once in StrongRhythmFrozenSelection;
//   2. the prepared BarEvolution plan receives that exact target;
//   3. DevelopReturn, RepeatReply and SparseDrift still create temporal
//      bar-function/topology differences rather than being flattened by density;
//   4. the shipped eight-bar SparseDrift request keeps the existing four-bar
//      BarEvolution vocabulary seam and repeats/maps it through I3 execution;
//   5. at least one shipped phrase fixture is observably different from the
//      same evolution request with no explicit density intent.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "scenes.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/migration/phrase_execution.h"
#include "src/generation/rhythm/bar_evolution.h"
#include "src/generation/rhythm/reference_phrase_vocabulary.h"
#include "src/generation/rhythm/reference_vocabulary.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint8_t kFourBarPhraseBars = 4;
constexpr uint8_t kSparseDriftPhraseBars = 8;
constexpr uint16_t kMaxFixtureOrdinal = 96;

int g_failures = 0;

void expect(const char* label, bool condition) {
  if (condition) {
    std::printf("%-72s OK\n", label);
    return;
  }
  std::fprintf(stderr, "%-72s FAIL\n", label);
  ++g_failures;
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

uint8_t popcount16(uint16_t value) {
  uint8_t count = 0;
  while (value != 0u) {
    value = static_cast<uint16_t>(value & (value - 1u));
    ++count;
  }
  return count;
}

uint16_t topologyDifference(const DrumPatternSet& a,
                            const DrumPatternSet& b) {
  uint16_t difference = 0;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    uint16_t left = 0;
    uint16_t right = 0;
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      if (a.voices[voice].steps[step].hit)
        left = static_cast<uint16_t>(left | (1u << step));
      if (b.voices[voice].steps[step].hit)
        right = static_cast<uint16_t>(right | (1u << step));
    }
    difference = static_cast<uint16_t>(
        difference + popcount16(static_cast<uint16_t>(left ^ right)));
  }
  return difference;
}

struct PhraseFixture {
  bool found = false;
  GenreSettings settings{};
  uint16_t phraseIdentity = 0;
  uint8_t requestedPhraseBars = 0;
  PreparedPhraseExecution prepared{};
};

bool lawMatches(PhraseEvolutionLawId actual, PhraseEvolutionLawId wanted) {
  return actual == wanted;
}

PhraseFixture findFixture(PhraseEvolutionLawId wanted,
                          uint8_t requestedPhraseBars,
                          bool requireDensityDifferentFromPreferred) {
  static PhraseExecutionScratch scratch{};
  static PreparedPhraseExecution prepared{};

  for (uint8_t mode = 0; mode < kGenerativeModeCount; ++mode) {
    const GenerativeMode genre = static_cast<GenerativeMode>(mode);
    const uint8_t recipeCount = availableRecipeCount(genre);
    for (uint8_t recipeOrdinal = 0;
         recipeOrdinal < recipeCount;
         ++recipeOrdinal) {
      GenreRecipeId recipe = kBaseRecipeId;
      if (!availableRecipeAt(genre, recipeOrdinal, recipe)) continue;

      GenreSettings settings{};
      settings.generativeMode = mode;
      settings.recipe = recipe;
      settings.rhythmSelectionMode =
          static_cast<uint8_t>(RhythmSelectionMode::Auto);
      settings.rhythmArchetypeId = kNoArchetypeId;

      for (uint16_t identity = 0;
           identity < kMaxFixtureOrdinal;
           ++identity) {
        scratch = PhraseExecutionScratch{};
        prepared = PreparedPhraseExecution{};
        const PhraseExecutionStatus status = preparePhraseExecution(
            settings, materializationSettings(), identity, requestedPhraseBars,
            scratch, prepared);
        if (status != PhraseExecutionStatus::Ready ||
            prepared.phraseTrajectory == kNoTrajectoryId ||
            prepared.selection.composition.phraseBars != requestedPhraseBars ||
            !lawMatches(prepared.selection.composition.phraseLaw, wanted) ||
            prepared.selection.structuralDensityTarget ==
                kNoStructuralDensityTarget) {
          continue;
        }

        const ReferenceVocabulary::Definition* definition =
            ReferenceVocabulary::definitionForId(
                prepared.selection.composition.rhythmArchetypeId);
        const RhythmArchetype* archetype = definition == nullptr
            ? nullptr
            : ReferenceVocabulary::archetypeFor(definition->key);
        if (archetype == nullptr) continue;
        if (requireDensityDifferentFromPreferred &&
            prepared.selection.structuralDensityTarget ==
                archetype->density.structuralPreferred) {
          continue;
        }

        PhraseFixture fixture{};
        fixture.found = true;
        fixture.settings = settings;
        fixture.phraseIdentity = identity;
        fixture.requestedPhraseBars = requestedPhraseBars;
        fixture.prepared = prepared;
        return fixture;
      }
    }
  }
  return PhraseFixture{};
}

void testLawRemainsCausal(PhraseEvolutionLawId law,
                          const char* lawName,
                          uint8_t requestedPhraseBars) {
  const PhraseFixture fixture = findFixture(law, requestedPhraseBars, false);
  char label[128]{};
  std::snprintf(label, sizeof(label),
                "shipped %s/%u-bar fixture prepares with phrase trajectory",
                lawName, static_cast<unsigned>(requestedPhraseBars));
  expect(label, fixture.found);
  if (!fixture.found) return;

  expect("phrase fixture carries one frozen explicit profile density target",
         fixture.prepared.selection.structuralDensityTarget !=
             kNoStructuralDensityTarget);
  expect("requested shipped phrase length is preserved by preparation",
         fixture.prepared.length.effectivePhraseBars == requestedPhraseBars);
  expect("I3 BarEvolution vocabulary remains bounded to the existing seam",
         fixture.prepared.phrasePlan.barCount > 0 &&
             fixture.prepared.phrasePlan.barCount <= kFourBarPhraseBars);
  if (law == PhraseEvolutionLawId::SparseDrift) {
    expect("SparseDrift uses the shipped eight-bar request",
           requestedPhraseBars == kSparseDriftPhraseBars);
    expect("eight-bar SparseDrift retains the four-bar evolution vocabulary",
           fixture.prepared.phrasePlan.barCount == kFourBarPhraseBars);
  }

  DrumPatternSet first{};
  SynthPattern firstA{};
  SynthPattern firstB{};
  const StrongRhythmMigrationResult firstResult = materializePreparedPhraseBar(
      fixture.prepared, 0, 24, first, firstA, firstB);
  expect("prepared phrase statement bar materializes",
         firstResult.status == StrongRhythmMigrationStatus::Applied);
  if (firstResult.status != StrongRhythmMigrationStatus::Applied ||
      fixture.prepared.phrasePlan.barCount == 0) {
    return;
  }

  uint16_t maxDifference = 0;
  bool functionChanged = false;
  for (uint8_t bar = 1; bar < fixture.requestedPhraseBars; ++bar) {
    DrumPatternSet drums{};
    SynthPattern synthA{};
    SynthPattern synthB{};
    const StrongRhythmMigrationResult result = materializePreparedPhraseBar(
        fixture.prepared, bar, static_cast<int16_t>(24 + bar),
        drums, synthA, synthB);
    if (result.status != StrongRhythmMigrationStatus::Applied) continue;
    const uint16_t difference = topologyDifference(first, drums);
    if (difference > maxDifference) maxDifference = difference;
    const uint8_t planBar = static_cast<uint8_t>(
        bar % fixture.prepared.phrasePlan.barCount);
    if (fixture.prepared.phrasePlan.bars[planBar].function !=
        fixture.prepared.phrasePlan.bars[0].function) {
      functionChanged = true;
    }
  }

  std::printf(
      "  law=%s requested_bars=%u plan_bars=%u target=%u trajectory=%u "
      "max_bar_difference=%u\n",
      lawName,
      static_cast<unsigned>(fixture.requestedPhraseBars),
      static_cast<unsigned>(fixture.prepared.phrasePlan.barCount),
      static_cast<unsigned>(
          fixture.prepared.selection.structuralDensityTarget),
      static_cast<unsigned>(fixture.prepared.phraseTrajectory),
      static_cast<unsigned>(maxDifference));
  expect("phrase law still owns a non-Statement temporal bar function",
         functionChanged);
  expect("phrase law still produces a materialized bar-topology difference",
         maxDifference > 0);
}

void testDensityIsCausalInsidePhrasePrepare() {
  PhraseFixture fixture = findFixture(
      PhraseEvolutionLawId::DevelopReturn, kFourBarPhraseBars, true);
  if (!fixture.found) {
    fixture = findFixture(
        PhraseEvolutionLawId::RepeatReply, kFourBarPhraseBars, true);
  }
  if (!fixture.found) {
    fixture = findFixture(
        PhraseEvolutionLawId::SparseDrift, kSparseDriftPhraseBars, true);
  }
  expect("a shipped phrase fixture has target distinct from archetype preferred",
         fixture.found);
  if (!fixture.found) return;

  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(
          fixture.prepared.selection.composition.rhythmArchetypeId);
  expect("density-sensitive phrase fixture resolves an archetype definition",
         definition != nullptr);
  if (definition == nullptr || fixture.prepared.phrasePlan.barCount == 0) return;

  BarEvolutionRequest explicitRequest{};
  explicitRequest.catalog = &ReferenceVocabulary::phraseEvolutionCatalog();
  explicitRequest.archetypeId = definition->archetypeId;
  explicitRequest.phraseBars = fixture.prepared.phrasePlan.barCount;
  explicitRequest.level = fixture.prepared.materialization.level;
  explicitRequest.generation =
      fixture.prepared.selection.realizationGeneration;
  explicitRequest.structuralDensityTarget =
      fixture.prepared.selection.structuralDensityTarget;
  explicitRequest.requestedTrajectoryId = fixture.prepared.phraseTrajectory;

  const BarEvolutionResult explicitResult = evolveRhythmPhrase(explicitRequest);
  expect("explicit frozen density target reproduces prepared phrase evolution",
         explicitResult.status == BarEvolutionStatus::Ok &&
             std::memcmp(&explicitResult.plan,
                         &fixture.prepared.phrasePlan,
                         sizeof(explicitResult.plan)) == 0);

  BarEvolutionRequest legacyRequest = explicitRequest;
  legacyRequest.structuralDensityTarget = kNoStructuralDensityTarget;
  const BarEvolutionResult legacyResult = evolveRhythmPhrase(legacyRequest);
  expect("same phrase evolution without density intent still realizes",
         legacyResult.status == BarEvolutionStatus::Ok);
  if (explicitResult.status != BarEvolutionStatus::Ok ||
      legacyResult.status != BarEvolutionStatus::Ok) {
    return;
  }
  expect("profile density remains causal before phrase-law mutation",
         std::memcmp(&explicitResult.plan,
                     &legacyResult.plan,
                     sizeof(explicitResult.plan)) != 0);
}

}  // namespace

int main() {
  testLawRemainsCausal(
      PhraseEvolutionLawId::DevelopReturn, "DevelopReturn", kFourBarPhraseBars);
  testLawRemainsCausal(
      PhraseEvolutionLawId::RepeatReply, "RepeatReply", kFourBarPhraseBars);
  testLawRemainsCausal(
      PhraseEvolutionLawId::SparseDrift, "SparseDrift", kSparseDriftPhraseBars);
  testDensityIsCausalInsidePhrasePrepare();

  if (g_failures != 0) {
    std::fprintf(stderr,
                 "GF2-I4 phrase-density coexistence: %d failure(s)\n",
                 g_failures);
    return 1;
  }
  std::printf("GF2-I4 phrase-density coexistence: PASS\n");
  return 0;
}
