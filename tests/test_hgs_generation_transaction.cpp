#include <cassert>
#include <cstring>
#include <iostream>
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/generation/harmony/harmonic_source.h"
#include "src/generation/harmony/harmonic_quality.h"
#include "src/generation/harmony/harmonic_scale.h"
using namespace GroovePuterRhythm;
using namespace GroovePuterHarmony;
int main() {
  GenreSettings settings{};
  settings.generativeMode = 13; // UKG has an idiom after tonal materialization.
  settings.harmonicStrategy = 1;
  PreparedGraphHarmony graph{};
  graph.materialId = graph.publicationGeneration = graph.modelVersion = 1;
  graph.authority = HarmonicAuthority::Graph;
  graph.eventCount = 1;
  graph.events[0].state = {kQualityMajor, qualityDefinition(kQualityMajor)->intervalMask, 0, kNoBassPc};
  graph.events[0].tonal = {1, 1, scaleDefinition(1)->intervalMask,
      scaleDefinition(1)->intervalMask, 0, 0};
  HarmonicSourceView source{HarmonicStrategy::Graph, nullptr, &graph};
  HarmonicRhythmPlan rhythm{};
  rhythm.onsets = stepBit(0); rhythm.eventCount = 1;
  rhythm.phraseHarmonicPosition = 1; // rejected by the late idiom projector
  StrongRhythmPhraseExecutionOverride execution{};
  execution.harmonicRhythm = &rhythm;
  execution.harmonicSourceView = &source;
  execution.firstGlobalHarmonicOrdinal = 0;
  StrongRhythmMigrationContext context{};
  context.level = RealizationLevel::P2Variation;
  context.tonalMaterializationEnabled = true;
  context.scaleTypeValue = kScaleMajor;
  context.phraseExecutionOverride = &execution;
  DrumPatternSet drums{}; SynthPattern a{}, b{};
  drums.voices[0].steps[0].hit = true;
  drums.voices[0].steps[0].velocity = 13;
  a.steps[0].note = 42; b.steps[0].note = 65;
  const auto originalDrums = drums; const auto originalA = a; const auto originalB = b;
  const auto result = migrateStrongRhythmMaterial(settings, context, drums, a, b);
  std::cout << "late GRAPH status=" << static_cast<int>(result.status) << std::endl;
  assert(result.status == StrongRhythmMigrationStatus::GraphHarmonyProjectionFailed);
  assert(std::memcmp(&drums, &originalDrums, sizeof(drums)) == 0);
  assert(std::memcmp(&a, &originalA, sizeof(a)) == 0);
  assert(std::memcmp(&b, &originalB, sizeof(b)) == 0);
  std::cout << "HGS late projection transaction: PASS\n";
}
