#ifndef GROOVEPUTER_GENERATION_MIGRATION_QUANTIZED_GENERATION_COMMIT_H
#define GROOVEPUTER_GENERATION_MIGRATION_QUANTIZED_GENERATION_COMMIT_H

// Keep the accepted 0.9.9-A PREPARE/publication implementation intact while
// B1 replaces only its persistent COMMIT entry points. The renamed inline
// functions remain compile-time reference implementations and are not called
// through this public header.
#define commitQuantizedGenerationAtBarStart \
  legacyCommitQuantizedGenerationAtBarStart
#define regenerateWithQuantizedCommit legacyRegenerateWithQuantizedCommit
#define regenerateSynthWithQuantizedCommit \
  legacyRegenerateSynthWithQuantizedCommit
#include "quantized_generation_commit_impl.h"
#undef regenerateSynthWithQuantizedCommit
#undef regenerateWithQuantizedCommit
#undef commitQuantizedGenerationAtBarStart

#include "quantized_generation_undo_owner_impl.h"
#include "phrase_live_arrangement_activation.h"
#include "live_song_arrangement_activation.h"

#endif  // GROOVEPUTER_GENERATION_MIGRATION_QUANTIZED_GENERATION_COMMIT_H
