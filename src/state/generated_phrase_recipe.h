#ifndef GROOVEPUTER_SRC_STATE_GENERATED_PHRASE_RECIPE_H
#define GROOVEPUTER_SRC_STATE_GENERATED_PHRASE_RECIPE_H

#include <cstdint>
#include <type_traits>

#include "../../scenes.h"
#include "../generation/migration/phrase_execution.h"

// P0: the inputs needed to rebuild the kept generated phrase under a different
// phrase law. Session-local (never persisted), published by GeneratedPhraseSong
// together with the origin sidecar and cleared by the same events.
//
// `genre` and `materialization` are exactly what preparePhraseExecution consumed;
// together with the attempt ordinal inside `materialization` they reproduce the
// prepared execution. `contextFingerprint` covers what materialization reads from
// the engine instead (pitch source: genre manager, flavor, BPM) and is compared
// again when the cycle is requested (R1).
namespace GroovePuterMaterial {

struct GeneratedPhraseRecipe {
  GenreSettings genre{};
  GroovePuterRhythm::PhraseExecutionMaterializationSettings materialization{};
  uint16_t phraseGenerationIdentity =
      GroovePuterRhythm::kUnspecifiedPhraseGenerationIdentity;
  uint32_t contextFingerprint = 0;
  int16_t pageIndex = -1;
  int16_t songSlot = -1;
  int16_t songStart = -1;
  int16_t firstLocalSlot = -1;
  // First Song row of the published DEVELOP+BREAK cycle; -1 when none.
  int16_t cycleSongStart = -1;
  uint8_t bars = 0;
  bool valid = false;
};

static_assert(std::is_trivially_copyable<GeneratedPhraseRecipe>::value,
              "generated phrase recipe must remain fixed value state");
static_assert(sizeof(GeneratedPhraseRecipe) <= 64,
              "generated phrase recipe must stay compact session state");

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_SRC_STATE_GENERATED_PHRASE_RECIPE_H
