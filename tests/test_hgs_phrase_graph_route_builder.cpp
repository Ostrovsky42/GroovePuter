#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "scenes.h"
#include "src/generation/harmony/harmonic_quality.h"
#include "src/generation/harmony/harmonic_scale.h"
#include "src/generation/harmony/phrase_graph_route_builder.h"

using namespace GroovePuterHarmony;

namespace {
PhraseGraphRouteBuildStatus build(const GenreSettings& settings,
                                 uint8_t root, uint8_t scale,
                                 PreparedGraphHarmony& graph,
                                 PreparedPhraseGraphMetadata& metadata,
                                 uint8_t events = 8) {
  static RouteScratch scratch{};
  static RouteCandidateWorkspace candidates{};
  static RecentHarmonyMemory recent{};
  return buildPhraseGraphRoute(settings, root, scale, events, 0x31415926u,
                               0x10203040u, 71, 93, recent, scratch,
                               candidates, graph, metadata);
}
}  // namespace

int main() {
  GenreSettings capabilitySettings{};
  capabilitySettings.generativeMode = 0;
  capabilitySettings.recipe = 7;
  capabilitySettings.harmonicStrategy = 1;
  assert(graphRouteCapability(capabilitySettings, 0) ==
         PhraseGraphRouteBuildStatus::Ready);
  capabilitySettings.generativeMode = 2;
  capabilitySettings.recipe = 0;
  assert(graphRouteCapability(capabilitySettings, 0) ==
         PhraseGraphRouteBuildStatus::UnsupportedGenre);
  capabilitySettings.generativeMode = 0;
  assert(graphRouteCapability(capabilitySettings, 31) ==
         PhraseGraphRouteBuildStatus::UnsupportedScale);

  GenreSettings settings{};
  settings.generativeMode = 0;  // Acid
  settings.recipe = 7;          // Rolling Acid has the production P1R route.
  settings.harmonicStrategy = 1;

  PreparedGraphHarmony graphA{};
  PreparedGraphHarmony graphB{};
  PreparedPhraseGraphMetadata metadataA{};
  PreparedPhraseGraphMetadata metadataB{};
  assert(build(settings, 0, 0, graphA, metadataA) ==
         PhraseGraphRouteBuildStatus::Ready);
  PreparedGraphHarmony fourEventGraph{};
  PreparedPhraseGraphMetadata fourEventMetadata{};
  assert(build(settings, 0, 0, fourEventGraph, fourEventMetadata, 4) ==
         PhraseGraphRouteBuildStatus::Ready);
  PreparedGraphHarmony fourDorianEventGraph{};
  PreparedPhraseGraphMetadata fourDorianEventMetadata{};
  assert(build(settings, 0, 2, fourDorianEventGraph,
               fourDorianEventMetadata, 4) ==
         PhraseGraphRouteBuildStatus::Ready);
  assert(build(settings, 0, 0, graphB, metadataB) ==
         PhraseGraphRouteBuildStatus::Ready);
  assert(graphA.eventCount == 8);
  assert(graphA.materialId == 71 && graphA.publicationGeneration == 93);
  assert(graphA.authority == HarmonicAuthority::Graph);
  assert(metadataA.style == HarmonicStyleId::Acid);
  assert(metadataA.home.rootPc == 0);
  assert(metadataA.home.qualityId == kQualityMinor7);
  for (uint8_t i = 0; i < graphA.eventCount; ++i) {
    GroovePuterRhythm::HarmonicEvent projected{};
    assert(projectHarmonicState(graphA.events[i].state,
                                graphA.events[i].tonal, projected) ==
           HarmonyStatus::Ready);
  }
  assert(std::memcmp(&metadataA.genome, &metadataB.genome,
                     sizeof(metadataA.genome)) == 0);
  for (uint8_t i = 0; i < graphA.eventCount; ++i) {
    assert(graphA.events[i].timingSlotOrdinal == i);
    assert(graphA.events[i].state.rootPc == graphB.events[i].state.rootPc);
    assert(graphA.events[i].state.qualityId == graphB.events[i].state.qualityId);
    assert(graphA.events[i].tonal.scaleId == graphB.events[i].tonal.scaleId);
  }

  RouteScratch continuationScratch{};
  RouteCandidateWorkspace continuationCandidates{};
  RecentHarmonyMemory recent{};
  PreparedGraphHarmony returned{};
  PreparedPhraseGraphMetadata continuationMetadata{};
  const auto& terminal = graphA.events[graphA.eventCount - 1u];
  assert(buildPhraseGraphRoute(
             settings, 0, 0, 8, 0x31415926u, 0x10203040u, 72, 94,
             recent, continuationScratch, continuationCandidates, returned,
             continuationMetadata, &terminal.state, &terminal.tonal,
             SectionIntent::Return) == PhraseGraphRouteBuildStatus::Ready);
  assert(continuationMetadata.home.rootPc == metadataA.home.rootPc);
  assert(continuationMetadata.home.qualityId == metadataA.home.qualityId);
  assert(returned.eventCount == 8);
  assert(returned.events[returned.eventCount - 1u].state.rootPc ==
         continuationMetadata.home.rootPc);

  const PreparedGraphHarmony protectedGraph = graphA;
  const PreparedPhraseGraphMetadata protectedMetadata = metadataA;
  settings.harmonicStrategy = 0;
  assert(build(settings, 0, 0, graphA, metadataA) ==
         PhraseGraphRouteBuildStatus::Disabled);
  assert(std::memcmp(&graphA, &protectedGraph, sizeof(graphA)) == 0);

  settings.harmonicStrategy = 1;
  assert(build(settings, 0, 31, graphA, metadataA) ==
         PhraseGraphRouteBuildStatus::UnsupportedScale);
  assert(std::memcmp(&graphA, &protectedGraph, sizeof(graphA)) == 0);
  assert(std::memcmp(&metadataA, &protectedMetadata, sizeof(metadataA)) == 0);

  GenreSettings unsupported{};
  unsupported.harmonicStrategy = 1;
  unsupported.generativeMode = 2;  // no shipped harmonic style mapping
  assert(build(unsupported, 0, 0, graphA, metadataA) ==
         PhraseGraphRouteBuildStatus::UnsupportedGenre);

  // A saturated discovery pool must not discard an already-queued legal
  // exact-home path (review Synthwave/LoFi BREAK terminal reproductions).
  for (uint8_t styleId : {uint8_t{16}, uint8_t{7}}) {
    GenreSettings returning{};
    returning.harmonicStrategy = 1;
    returning.generativeMode = styleId == 16 ? 1 : 15;
    const uint8_t root = styleId == 16 ? 4 : 7;
    const QualityId quality = styleId == 16 ? kQualityDiminished : kQualityMajor;
    const uint8_t tonic = styleId == 16 ? 5 : 10;
    HarmonicState current{quality,
        transposePitchSet(qualityDefinition(quality)->intervalMask, root),
        root, kNoBassPc};
    TonalContext currentTonal{1, 0,
        transposePitchSet(scaleDefinition(1)->intervalMask, tonic),
        scaleDefinition(0)->intervalMask, tonic, 0};
    PreparedGraphHarmony result{};
    PreparedPhraseGraphMetadata metadata{};
    RouteScratch boundedScratch{};
    RouteCandidateWorkspace candidates{};
    const uint8_t slots = styleId == 16 ? 8 : 16;
    assert(buildPhraseGraphRoute(returning, 0, 0, slots, 1,
        0x48475300u ^ (uint32_t{styleId} << 16u) ^ 1u, 1, 1, recent,
        boundedScratch, candidates, result, metadata, &current, &currentTonal,
        SectionIntent::Return) == PhraseGraphRouteBuildStatus::Ready);
    assert(boundedScratch.nodeCount <= kMaxReturnSearchNodes);
    assert(result.eventCount == slots);
    const auto& last = result.events[result.eventCount - 1];
    assert(last.state.rootPc == 0 && last.state.qualityId == kQualityMinor7);
    assert(last.tonal.scaleId == 0 && last.tonal.tonicPc == 0);
  }

  std::cout << "HGS phrase graph route builder tests passed\n";
  return 0;
}
