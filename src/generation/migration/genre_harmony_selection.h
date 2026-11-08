#ifndef GROOVEPUTER_GENERATION_MIGRATION_GENRE_HARMONY_SELECTION_H
#define GROOVEPUTER_GENERATION_MIGRATION_GENRE_HARMONY_SELECTION_H

#include "strong_rhythm_migration.h"
#include "src/generation/harmony/phrase_graph_route_builder.h"

namespace GroovePuterRhythm {

struct GenreHarmonySelection {
  GenreSettings settings{}; // concrete Legacy0/Graph1 for existing executors
  GroovePuterHarmony::PhraseGraphRouteBuildStatus graphCapability{};
};

// AUTO belongs to next-Phrase source selection, not to the materializer enum.
// Old/default Legacy and explicit Graph keep their meanings and error behavior.
inline GenreHarmonySelection resolveGenreHarmony(
    const GenreSettings& requested, uint8_t scaleId) {
  using Status = GroovePuterHarmony::PhraseGraphRouteBuildStatus;
  GenreHarmonySelection selection{};
  selection.settings = requested;
  GenreSettings graph = requested;
  graph.harmonicStrategy = 1;
  selection.graphCapability = selectStrongRhythmRoute(graph) == StrongRhythmRoute::Legacy
      ? Status::UnsupportedGenre : GroovePuterHarmony::graphRouteCapability(graph, scaleId);
  if (requested.harmonicAuto)
    selection.settings.harmonicStrategy = selection.graphCapability == Status::Ready ? 1 : 0;
  return selection;
}

} // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_MIGRATION_GENRE_HARMONY_SELECTION_H
