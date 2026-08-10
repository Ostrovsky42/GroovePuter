#ifndef GROOVEPUTER_GENERATION_RHYTHM_REFERENCE_PHRASE_VOCABULARY_H
#define GROOVEPUTER_GENERATION_RHYTHM_REFERENCE_PHRASE_VOCABULARY_H

#include "reference_vocabulary.h"

namespace GroovePuterRhythm {
namespace ReferenceVocabulary {

// Stage 12 candidate catalog. It preserves the production one-bar catalog and
// overlays bounded 2/4-bar trajectories only on archetypes whose current
// relationship contracts admit phrase evolution. Individual archetypes may
// expose narrower trajectory/mutation capabilities when their lane minima do
// not provide legal headroom for a transform. Production generation must keep
// using catalog() until the ESP32-S3 Stage 6.1 stack/heap/runtime gate is
// recorded and accepted.
const RhythmCatalogView& phraseEvolutionCatalog();

// True when the archetype is admitted to the Stage 12 candidate overlay.
// This is capability metadata only; it does not select a phrase length or make
// BarEvolution production-reachable.
bool phraseEvolutionEnabled(Archetype key);

}  // namespace ReferenceVocabulary
}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_RHYTHM_REFERENCE_PHRASE_VOCABULARY_H
