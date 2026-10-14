#ifndef GROOVEPUTER_GENERATION_HARMONY_PHRASE_GRAPH_ROUTE_BUILDER_H
#define GROOVEPUTER_GENERATION_HARMONY_PHRASE_GRAPH_ROUTE_BUILDER_H

#include "harmonic_route.h"
#include "song_variant_genome.h"

struct GenreSettings;

namespace GroovePuterHarmony {

enum class PhraseGraphRouteBuildStatus : uint8_t {
  Ready = 0,
  Disabled,
  UnsupportedGenre,
  UnsupportedScale,
  UnsupportedHome,
  UnsupportedProjection,
  PlannerFailure,
  InvalidContext,
};

struct PreparedPhraseGraphMetadata {
  HarmonicStyleId style = HarmonicStyleId::Acid;
  SongVariantGenome genome{};
  HarmonicState home{};
  TonalContext homeTonal{};
  HarmonicState terminal{};
  TonalContext terminalTonal{};
};

// Control/UI-side capability projection only. It does not allocate route
// scratch or promise that a particular seeded route will be found.
PhraseGraphRouteBuildStatus graphRouteCapability(
    const ::GenreSettings& settings, uint8_t scaleId);

PhraseGraphRouteBuildStatus buildPhraseGraphRoute(
    const ::GenreSettings& settings,
    uint8_t rootPitchClass,
    uint8_t scaleId,
    uint8_t eventCount,
    uint32_t projectSeed,
    uint32_t songIdentity,
    uint32_t materialId,
    uint32_t publicationGeneration,
    const RecentHarmonyMemory& recent,
    RouteScratch& routeScratch,
    RouteCandidateWorkspace& candidateWorkspace,
    PreparedGraphHarmony& graphOut,
    PreparedPhraseGraphMetadata& metadataOut,
    const HarmonicState* current = nullptr,
    const TonalContext* currentTonal = nullptr,
    SectionIntent section = SectionIntent::Develop);

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_PHRASE_GRAPH_ROUTE_BUILDER_H
