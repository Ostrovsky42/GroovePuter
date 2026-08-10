#ifndef GROOVEPUTER_GENERATION_MIGRATION_STRONG_RHYTHM_LIVE_BRIDGE_H
#define GROOVEPUTER_GENERATION_MIGRATION_STRONG_RHYTHM_LIVE_BRIDGE_H

#include "strong_rhythm_migration.h"

class MiniAcid;

namespace GroovePuterRhythm {

// GENRE materialization boundary. Legacy generation runs first so pitch/timbre,
// Atlas compatibility and rollback remain authoritative; supported strong routes
// then atomically replace drums and semantic synth topology.
StrongRhythmMigrationResult regenerateWithStrongRhythmMigration(
    MiniAcid& engine);

// DRUMS-page G boundary. Regenerate only the drum pattern through the legacy
// generator first, then apply the selected strong rhythm + FEEL timing to drums.
// Synth A/B are intentionally untouched. Unsupported/failing strong routes keep
// the freshly generated legacy drum pattern as a safe fallback.
StrongRhythmMigrationResult regenerateDrumsWithStrongRhythmMigration(
    MiniAcid& engine);

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_MIGRATION_STRONG_RHYTHM_LIVE_BRIDGE_H
