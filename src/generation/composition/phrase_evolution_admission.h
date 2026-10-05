#ifndef GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_EVOLUTION_ADMISSION_H
#define GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_EVOLUTION_ADMISSION_H

#include <cstdint>

#include "../../../scenes.h"
#include "../../dsp/genre_manager.h"
#include "../rhythm/reference_phrase_vocabulary.h"

namespace GroovePuterRhythm {

// Phrase evolution is admitted per archetype AND per scenario. An archetype can be shared by
// several genre routes, so listing it in the catalog whitelist is not enough to decide where
// bar functions may act. P0 excludes Acid and House explicitly (spec section 4): widening the
// musical task there is a separate decision. Product code and the M0 tooling both call this one
// function, so a measurement can never admit something production would refuse.
inline bool phraseEvolutionAdmitted(const GenreSettings& settings,
                                    ReferenceVocabulary::Archetype key) {
  if (settings.generativeMode == static_cast<uint8_t>(GenerativeMode::Acid) ||
      settings.generativeMode == static_cast<uint8_t>(GenerativeMode::House)) {
    return false;
  }
  return ReferenceVocabulary::phraseEvolutionEnabled(key);
}

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_EVOLUTION_ADMISSION_H
