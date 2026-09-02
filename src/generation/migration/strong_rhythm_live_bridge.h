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

enum class PhraseAuditionStatus : uint8_t {
  AppliedEvolved = 0,
  AppliedVariationFallback,
  SelectionFailed,
  MaterializationFailed,
  Count,
};

// M1L uses the existing audition surface only. These literals are test-listening
// cases, not composition policy or persistent scene state.
enum class PhraseAuditionListeningCase : uint8_t {
  CurrentWired = 0,
  M1SparseControl,
  M1SparseWired,
  M1CallWired,
};

struct PhraseAuditionProbe {
  bool available = false;
  uint32_t commandDurationUs = 0;
  uint32_t maxReductionDurationUs = 0;
  uint32_t maxBreakDurationUs = 0;
  RhythmArchetypeId maxReductionArchetypeId = kNoArchetypeId;
  RhythmArchetypeId maxBreakArchetypeId = kNoArchetypeId;
  uint32_t stackBeforeWords = 0;
  uint32_t stackAfterWords = 0;
  uint32_t stackAfterBytes = 0;
  uint32_t freeInternalBefore = 0;
  uint32_t freeInternalAfter = 0;
  uint32_t largestInternalBefore = 0;
  uint32_t largestInternalAfter = 0;
};

struct PhraseAuditionResult {
  PhraseAuditionStatus status = PhraseAuditionStatus::SelectionFailed;
  StrongRhythmMigrationStatus selectionStatus =
      StrongRhythmMigrationStatus::Legacy;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  RealizationLevel level = RealizationLevel::P2Variation;
  uint8_t requestedBars = 1;
  uint8_t profileBars = 1;
  PhraseAuditionListeningCase listeningCase =
      PhraseAuditionListeningCase::CurrentWired;
  uint8_t synthBNoteCounts[4]{};
  TrajectoryId firstTrajectoryId = kNoTrajectoryId;
  TrajectoryId secondTrajectoryId = kNoTrajectoryId;
  PhraseAuditionProbe probe{};
};

// Explicit Stage 12 audition/probe command. It never replaces normal G.
//
// Contract:
// - reserves Bank B (current page) patterns 1..8 and Song B for audition;
// - locks one selected rhythm identity across the generated phrase;
// - uses the current session P1/P2/P3 request level for selection/evolution;
// - uses BarEvolution when that identity is admitted by phraseEvolutionCatalog;
// - otherwise writes deterministic one-bar strong variations for the requested
//   1/2/4/8-bar length, so every supported genre remains listenable;
// - keeps the existing MiniAcid song transport as the only playback owner;
// - uses the current Stage 15 tonal materialization context for synth A/B;
// - on Cardputer ADV, records stack/internal-heap timing metrics and benchmarks
//   Reduction/Break across every subtractive Stage 12 identity.
PhraseAuditionResult regeneratePhraseAuditionWithProbe(
    MiniAcid& engine,
    PhraseAuditionListeningCase listeningCase =
        PhraseAuditionListeningCase::CurrentWired);

const char* phraseAuditionStatusName(PhraseAuditionStatus status);

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_MIGRATION_STRONG_RHYTHM_LIVE_BRIDGE_H
