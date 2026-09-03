// GF2-I3 — phrase law execution.
//
// The generation profile has been selecting a PhraseEvolutionLawId and a bar
// count all along, and nothing in production has ever read either. A phrase
// therefore plays the same rhythm in every bar: the drum plan is realized once
// per bar with phraseBars = 1 and no trajectory, so only the bass/chord/melodic
// roles vary by bar ordinal.
//
// The contract: a non-Loop law must make the bars of one phrase structurally
// different from each other, the difference must land on salient positions, and
// Loop must stay bar-identical so an intentional zero is distinguishable from a
// broken one.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "scenes.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/migration/phrase_execution.h"
#include "src/generation/rhythm/reference_phrase_vocabulary.h"
#include "src/generation/rhythm/reference_vocabulary.h"

using namespace GroovePuterRhythm;

namespace {

// Expected-active fixture: Breaks / UK Garage declares a non-Loop law over four
// bars. Which archetype it draws depends on the generation context, and only
// some archetypes are admitted to multi-bar evolution, so the fixture searches
// for a phrase-enabled draw and pins it rather than assuming one.
constexpr uint8_t kUkGarageRecipe = 1;
constexpr uint8_t kPhraseBars = 4;
constexpr uint16_t kMaxFixtureOrdinal = 64;

// Minimum structural difference a non-Loop law must produce between the first
// bar and any later bar of the same phrase. PROVISIONAL until measured across
// the active corpus, per docs/gf2/GF2_MAGNITUDE_CONTRACT.md.
constexpr int kMinBarDifference = 2;

int g_failures = 0;

void expect(const char* label, bool condition) {
  if (condition) {
    std::printf("%-58s OK\n", label);
    return;
  }
  std::fprintf(stderr, "%-58s FAIL\n", label);
  ++g_failures;
}

GenreSettings settingsFor(GenerativeMode mode, uint8_t recipe) {
  GenreSettings value{};
  value.generativeMode = static_cast<uint8_t>(mode);
  value.recipe = recipe;
  value.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  value.rhythmArchetypeId = kNoArchetypeId;
  return value;
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

struct BarTopology {
  uint16_t voices[DrumPatternSet::kVoices]{};
};

BarTopology topologyOf(const DrumPatternSet& drums) {
  BarTopology topology{};
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      if (drums.voices[voice].steps[step].hit)
        topology.voices[voice] |= static_cast<uint16_t>(1u << step);
    }
  }
  return topology;
}

int popcount16(uint16_t value) {
  int count = 0;
  while (value != 0u) {
    value = static_cast<uint16_t>(value & (value - 1u));
    ++count;
  }
  return count;
}

// Onsets that differ between two bars, over all drum voices.
int barDifference(const BarTopology& a, const BarTopology& b) {
  int total = 0;
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice)
    total += popcount16(static_cast<uint16_t>(a.voices[voice] ^ b.voices[voice]));
  return total;
}

// The anchor rule: a development bar that leaves the kick and the backbeat
// untouched is not a development bar.
bool salientDifference(const BarTopology& a, const BarTopology& b) {
  return a.voices[0] != b.voices[0] || a.voices[1] != b.voices[1];
}

// Returns the first phrase ordinal whose drawn archetype admits multi-bar
// evolution, or kMaxFixtureOrdinal when the recipe never draws one.
uint16_t phraseEnabledOrdinal(const GenreSettings& settings) {
  for (uint16_t ordinal = 0; ordinal < kMaxFixtureOrdinal; ++ordinal) {
    GenerationContext generation{};
    generation.projectSeed = 1234;
    generation.phraseOrdinal = ordinal;
    const GenerationCompositionResult composition =
        resolveGenerationComposition(settings, generation);
    if (composition.status != GenerationCompositionStatus::Ok) continue;
    if (composition.phraseLaw == PhraseEvolutionLawId::Loop) continue;
    const auto* definition =
        ReferenceVocabulary::definitionForId(composition.rhythmArchetypeId);
    if (definition == nullptr) continue;
    if (ReferenceVocabulary::phraseEvolutionEnabled(definition->key))
      return ordinal;
  }
  return kMaxFixtureOrdinal;
}

struct Phrase {
  bool ready = false;
  BarTopology bars[kPhraseBars]{};
};

Phrase renderPhrase(const GenreSettings& settings, uint16_t phraseOrdinal) {
  static PhraseExecutionScratch scratch{};
  static PreparedPhraseExecution prepared{};
  Phrase phrase{};

  const PhraseExecutionStatus status = preparePhraseExecution(
      settings, materializationSettings(), phraseOrdinal, kPhraseBars, scratch,
      prepared);
  if (status != PhraseExecutionStatus::Ready) return phrase;

  for (uint8_t bar = 0; bar < kPhraseBars; ++bar) {
    DrumPatternSet drums{};
    SynthPattern synthA{};
    SynthPattern synthB{};
    const StrongRhythmMigrationResult result = materializePreparedPhraseBar(
        prepared, bar, static_cast<int16_t>(16 + bar), drums, synthA, synthB);
    if (result.status != StrongRhythmMigrationStatus::Applied) return phrase;
    phrase.bars[bar] = topologyOf(drums);
  }
  phrase.ready = true;
  return phrase;
}

void testDeclaredLawIsNonLoop() {
  const GenreSettings settings =
      settingsFor(GenerativeMode::Broken, kUkGarageRecipe);
  const uint16_t ordinal = phraseEnabledOrdinal(settings);
  expect("fixture: the recipe can draw a phrase-enabled archetype",
         ordinal < kMaxFixtureOrdinal);
  if (ordinal >= kMaxFixtureOrdinal) return;
  GenerationContext generation{};
  generation.projectSeed = 1234;
  generation.phraseOrdinal = ordinal;
  const GenerationCompositionResult composition =
      resolveGenerationComposition(settings, generation);
  expect("fixture: the profile resolves a composition",
         composition.status == GenerationCompositionStatus::Ok);
  expect("fixture: UK Garage declares a non-Loop phrase law",
         composition.phraseLaw != PhraseEvolutionLawId::Loop);
  expect("fixture: the archetype admits multi-bar evolution",
         ReferenceVocabulary::phraseEvolutionEnabled(
             ReferenceVocabulary::definitionForId(composition.rhythmArchetypeId)
                 ->key));
  std::printf("  phrase ordinal=%u law=%u bars=%u archetype=%u\n",
              static_cast<unsigned>(ordinal),
              static_cast<unsigned>(composition.phraseLaw),
              static_cast<unsigned>(composition.phraseBars),
              static_cast<unsigned>(composition.rhythmArchetypeId));
}

void testNonLoopLawDifferentiatesBars() {
  const GenreSettings settings =
      settingsFor(GenerativeMode::Broken, kUkGarageRecipe);
  const uint16_t ordinal = phraseEnabledOrdinal(settings);
  if (ordinal >= kMaxFixtureOrdinal) return;
  const Phrase phrase = renderPhrase(settings, ordinal);
  expect("phrase materializes four bars", phrase.ready);
  if (!phrase.ready) return;

  int maxDifference = 0;
  bool salient = false;
  for (uint8_t bar = 1; bar < kPhraseBars; ++bar) {
    const int difference = barDifference(phrase.bars[0], phrase.bars[bar]);
    if (difference > maxDifference) maxDifference = difference;
    if (salientDifference(phrase.bars[0], phrase.bars[bar])) salient = true;
    std::printf("  bar 1 vs bar %u: %d differing onsets%s\n",
                static_cast<unsigned>(bar + 1), difference,
                salientDifference(phrase.bars[0], phrase.bars[bar])
                    ? " (salient)" : "");
  }
  expect("a non-Loop law differentiates the bars of its phrase",
         maxDifference >= kMinBarDifference);
  expect("the difference lands on the kick or the backbeat", salient);
}

}  // namespace

int main() {
  testDeclaredLawIsNonLoop();
  testNonLoopLawDifferentiatesBars();

  if (g_failures != 0) {
    std::fprintf(stderr, "GF2-I3 phrase law execution: %d failure(s)\n",
                 g_failures);
    return 1;
  }
  std::printf("GF2-I3 phrase law execution: PASS\n");
  return 0;
}
