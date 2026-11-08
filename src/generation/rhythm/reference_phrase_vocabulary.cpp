#include "reference_phrase_vocabulary.h"
#include "reference_phrase_catalog_data.h"

namespace GroovePuterRhythm {
namespace ReferenceVocabulary {

bool phraseEvolutionEnabled(Archetype key) {
  const Definition* definition = definitionFor(key);
  return definition != nullptr &&
      PhraseCatalogDetail::stage12PhraseEnabledId(definition->archetypeId);
}

}  // namespace ReferenceVocabulary
}  // namespace GroovePuterRhythm
