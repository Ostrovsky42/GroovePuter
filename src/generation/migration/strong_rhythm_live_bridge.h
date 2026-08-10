#ifndef GROOVEPUTER_GENERATION_MIGRATION_STRONG_RHYTHM_LIVE_BRIDGE_H
#define GROOVEPUTER_GENERATION_MIGRATION_STRONG_RHYTHM_LIVE_BRIDGE_H

#include "strong_rhythm_migration.h"

class MiniAcid;

namespace GroovePuterRhythm {

// Stage 5 live boundary: run the complete legacy generator first so synth pitch,
// tempo/Atlas compatibility and rollback remain authoritative, then replace
// drums only for the explicit strong-style allow-list. No backend state is
// persisted and unsupported/failing routes remain byte-for-byte legacy.
StrongRhythmMigrationResult regenerateWithStrongRhythmMigration(
    MiniAcid& engine);

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_MIGRATION_STRONG_RHYTHM_LIVE_BRIDGE_H
